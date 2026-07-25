#!/usr/bin/env bash
# Launcher for prebuilt generals Linux release.
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUNDLED="$DIR/lib"
RUNTIME="$DIR/.runtime-libs"
USE_SYSTEM="${GENERALS_USE_SYSTEM_FFMPEG:-1}"

find_ffmpeg_lib() {
	local name="$1"
	local best="" best_ver=-1 path entry base ver dir

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
			base="$(basename "$path")"
			ver="${base#"${name}.so."}"
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

setup_library_path() {
	if [[ "$USE_SYSTEM" != "0" ]] && setup_system_ffmpeg; then
		export LD_LIBRARY_PATH="$RUNTIME:$BUNDLED${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
	else
		export LD_LIBRARY_PATH="$BUNDLED${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
	fi
}

setup_library_path
exec "$DIR/generals" "$@"
