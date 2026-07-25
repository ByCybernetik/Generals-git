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
	local lib dep base

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

if command -v patchelf >/dev/null 2>&1; then
	patchelf --set-rpath '$ORIGIN/lib' "$DIST/generals"
fi

cat > "$DIST/run-generals.sh" <<'EOF'
#!/usr/bin/env bash
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="$DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$DIR/generals" "$@"
EOF
chmod +x "$DIST/run-generals.sh"

cat > "$DIST/README.txt" <<'EOF'
Generals Linux preview build

Run:
  ./run-generals.sh

Or (if rpath is set):
  ./generals

Bundled libraries: SDL3 and FFmpeg (no matching system packages required).
You still need Vulkan drivers and normal desktop libraries (X11/Wayland, etc.).
EOF

tar -czf "$ARCHIVE" -C "$ROOT" generals-linux
echo "Created $ARCHIVE"
tar -tzf "$ARCHIVE" | head -30
