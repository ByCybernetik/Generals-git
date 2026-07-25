# FFmpeg via pkg-config — any distro-provided 4.4+ (no pinned SONAME/version).

_ffmpeg_want = not get_option('disable_bink') or not get_option('disable_audio')
_ffmpeg_libs = []

if _ffmpeg_want
  _ffmpeg_libs += ['libavcodec', 'libavformat', 'libavutil', 'libswresample']
endif
if not get_option('disable_bink')
  _ffmpeg_libs += ['libswscale']
endif

_ffmpeg_seen = {}
_ffmpeg_dep_list = []
foreach _lib : _ffmpeg_libs
  if not _ffmpeg_seen.has_key(_lib)
    _ffmpeg_seen += {_lib : true}
    _dep = dependency(_lib, required : false)
    if not _dep.found()
      error('FFmpeg library not found: @0@. Install the matching -dev package (e.g. libavcodec-dev, libavformat-dev, libavutil-dev, libswresample-dev' + (get_option('disable_bink') ? ').' : ', libswscale-dev).'))
    endif
    _ffmpeg_dep_list += _dep
  endif
endforeach
