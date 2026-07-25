#!/usr/bin/env bash
# Bundle generals with SDL3 + FFmpeg shared libs for distro-agnostic releases.
set -euo pipefail

BINARY="${1:?usage: package_linux_release.sh <binary> <output.tar.gz>}"
ARCHIVE="${2:?usage: package_linux_release.sh <binary> <output.tar.gz>}"

ROOT="$(mktemp -d)"
trap 'rm -rf "$ROOT"' EXIT

DIST="$ROOT/generals-linux"
mkdir -p "$DIST/lib"

cp "$BINARY" "$DIST/generals"
chmod +x "$DIST/generals"
cp "$(dirname "$0")/run-generals.sh" "$DIST/run-generals.sh"
chmod +x "$DIST/run-generals.sh"

should_bundle() {
	case "$(basename "$1")" in
	libSDL3.so*|libav*.so*|libsw*.so*) return 0 ;;
	esac
	return 1
}

bundle_lib() {
	local lib="$1"
	local base
	base="$(basename "$lib")"
	if [[ ! -f "$DIST/lib/$base" ]]; then
		cp -L "$lib" "$DIST/lib/$base"
	fi
}

collect_deps() {
	local target="$1"
	local lib dep

	while IFS= read -r line; do
		lib="$(awk '/=>/ { print $3 }' <<<"$line")"
		[[ -z "$lib" || ! -f "$lib" ]] && continue
		if should_bundle "$lib"; then
			bundle_lib "$lib"
		fi
	done < <(ldd "$target")

	for lib in "$DIST/lib"/*.so*; do
		[[ -f "$lib" ]] || continue
		while IFS= read -r line; do
			dep="$(awk '/=>/ { print $3 }' <<<"$line")"
			[[ -z "$dep" || ! -f "$dep" ]] && continue
			if should_bundle "$dep"; then
				bundle_lib "$dep"
			fi
		done < <(ldd "$lib")
	done
}

collect_deps "$DIST/generals"

# Unversioned symlinks so the binary can load libavcodec.so (any SONAME via launcher).
for lib in "$DIST/lib"/libav*.so.* "$DIST/lib"/libsw*.so.*; do
	[[ -f "$lib" ]] || continue
	base="$(basename "$lib")"
	unversioned="${base%%.so.*}.so"
	ln -sfn "$base" "$DIST/lib/$unversioned"
done

if command -v patchelf >/dev/null 2>&1; then
	patchelf --set-rpath '$ORIGIN/lib' "$DIST/generals"
	while IFS= read -r needed; do
		case "$needed" in
		libav*.so.*|libsw*.so.*)
			unversioned="${needed%%.so.*}.so"
			patchelf --replace-needed "$needed" "$unversioned" "$DIST/generals" 2>/dev/null || true
			;;
		esac
	done < <(patchelf --print-needed "$DIST/generals")
fi

cat > "$DIST/run-generals.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUNDLED="$DIR/lib"
RUNTIME="$DIR/.runtime-libs"
USE_SYSTEM="${GENERALS_USE_SYSTEM_FFMPEG:-1}"

find_ffmpeg_lib() {
	local name="$1"
	local best="" best_ver=-1 path entry ver dir

	while read -r entry path; do
		[[ -z "$entry" || -z "$path" ]] && continue
		[[ "$entry" == "${name}.so."* ]] || continue
		ver="${entry#"${name}.so."}"
		[[ "$ver" =~ ^[0-9]+$ ]] || continue
		if (( ver > best_ver )); then
			best_ver=$ver
			best="$path"
		fi
	done < <(ldconfig -p 2>/dev/null | awk '{print $1, $NF}')

	for dir in /usr/lib /usr/lib64 /lib /lib64; do
		[[ -d "$dir" ]] || continue
		for path in "$dir/${name}.so."*; do
			[[ -f "$path" ]] || continue
			ver="$(basename "$path")"
			ver="${ver#"${name}.so."}"
			[[ "$ver" =~ ^[0-9]+$ ]] || continue
			if (( ver > best_ver )); then
				best_ver=$ver
				best="$path"
			fi
		done
	done

	printf '%s' "$best"
}

setup_system_ffmpeg() {
	local name path
	rm -rf "$RUNTIME"
	mkdir -p "$RUNTIME"
	for name in libavcodec libavformat libavutil libswresample libswscale; do
		path="$(find_ffmpeg_lib "$name")"
		if [[ -z "$path" || ! -f "$path" ]]; then
			return 1
		fi
		ln -sf "$path" "$RUNTIME/${name}.so"
	done
	return 0
}

if [[ "$USE_SYSTEM" != "0" ]] && setup_system_ffmpeg; then
	export LD_LIBRARY_PATH="$RUNTIME:$BUNDLED${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
else
	export LD_LIBRARY_PATH="$BUNDLED${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

exec "$DIR/generals" "$@"
EOF
chmod +x "$DIST/run-generals.sh"

cat > "$DIST/README.txt" <<'EOF'
Generals Linux preview build

Run:
  ./run-generals.sh

The launcher prefers your system FFmpeg (libavcodec.so.*) when available.
Set GENERALS_USE_SYSTEM_FFMPEG=0 to force bundled libraries in lib/.

Bundled SDL3 is always taken from lib/ if not installed system-wide.
You still need Vulkan drivers and normal desktop libraries (X11/Wayland).
EOF

tar -czf "$ARCHIVE" -C "$ROOT" generals-linux
echo "Created $ARCHIVE"
tar -tzf "$ARCHIVE" | head -40
