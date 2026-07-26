#include "PreRTS.h"
#include "sdlaudio_manager.h"

#include "Common/AudioEventRTS.h"
#include "Common/AudioEventInfo.h"
#include "Common/AudioHandleSpecialValues.h"
#include "Common/AudioRequest.h"
#include "Common/AudioSettings.h"
#include "Common/File.h"
#include "Common/FileSystem.h"
#include "Common/GameEngine.h"
#include "Common/GameMusic.h"
#include "Common/GameSounds.h"
#include "Common/MiscAudio.h"
#include "GameClient/Drawable.h"
#include "GameClient/View.h"
#include "GameLogic/GameLogic.h"
#include "GameLogic/Object.h"

#include <cmath>
#include <cstring>
#include <strings.h>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include <SDL3/SDL.h>
#include <cstdio>

namespace {

/*
 * Soft per-frame budget for heavy FFmpeg work on the main thread.
 * Spikes of 100–400ms (full-file decode / music refill) were the main
 * cause of sub-30 FPS dips in GENERALS_PROFILE runs.
 */
static UnsignedInt s_audioFrameStartMs = 0;
static const UnsignedInt kAudioSoftBudgetMs = 4;
static const UnsignedInt kAudioHardBudgetMs = 8;

static void Audio_Budget_Begin()
{
	s_audioFrameStartMs = timeGetTime();
}

static UnsignedInt Audio_Budget_Spent()
{
	return timeGetTime() - s_audioFrameStartMs;
}

static Bool Audio_Budget_Soft_Ok()
{
	return Audio_Budget_Spent() < kAudioSoftBudgetMs;
}

static Bool Audio_Budget_Hard_Ok()
{
	return Audio_Budget_Spent() < kAudioHardBudgetMs;
}

struct PcmCacheEntry
{
	std::string key;
	Uint8 *data;
	Uint32 size;
	Int rate;
	Int channels;
	UnsignedInt lastUseMs;
};

static const size_t kPcmCacheMaxBytes = 48u * 1024u * 1024u;
static const size_t kPcmCacheMaxEntries = 96;
static std::vector<PcmCacheEntry> s_pcmCache;
static size_t s_pcmCacheBytes = 0;

static void Pcm_Cache_Evict_One()
{
	if (s_pcmCache.empty())
		return;
	size_t victim = 0;
	for (size_t i = 1; i < s_pcmCache.size(); ++i) {
		if (s_pcmCache[i].lastUseMs < s_pcmCache[victim].lastUseMs)
			victim = i;
	}
	free(s_pcmCache[victim].data);
	s_pcmCacheBytes -= (size_t)s_pcmCache[victim].size;
	s_pcmCache.erase(s_pcmCache.begin() + (std::ptrdiff_t)victim);
}

static Bool Pcm_Cache_Lookup(const AsciiString &filename,
	Uint8 *&outData, Uint32 &outSize, Int &outRate, Int &outChannels)
{
	const char *key = filename.str();
	if (!key || !key[0])
		return false;
	for (size_t i = 0; i < s_pcmCache.size(); ++i) {
		if (s_pcmCache[i].key != key)
			continue;
		Uint8 *copy = (Uint8 *)malloc(s_pcmCache[i].size);
		if (!copy)
			return false;
		memcpy(copy, s_pcmCache[i].data, s_pcmCache[i].size);
		outData = copy;
		outSize = s_pcmCache[i].size;
		outRate = s_pcmCache[i].rate;
		outChannels = s_pcmCache[i].channels;
		s_pcmCache[i].lastUseMs = timeGetTime();
		return true;
	}
	return false;
}

static void Pcm_Cache_Store(const AsciiString &filename,
	const Uint8 *data, Uint32 size, Int rate, Int channels)
{
	const char *key = filename.str();
	if (!key || !key[0] || !data || size == 0)
		return;
	if (size > kPcmCacheMaxBytes / 4)
		return; /* single entry too large — skip */

	for (size_t i = 0; i < s_pcmCache.size(); ++i) {
		if (s_pcmCache[i].key == key) {
			s_pcmCache[i].lastUseMs = timeGetTime();
			return;
		}
	}

	while ((!s_pcmCache.empty())
		&& (s_pcmCache.size() >= kPcmCacheMaxEntries
			|| s_pcmCacheBytes + (size_t)size > kPcmCacheMaxBytes)) {
		Pcm_Cache_Evict_One();
	}

	Uint8 *copy = (Uint8 *)malloc(size);
	if (!copy)
		return;
	memcpy(copy, data, size);
	PcmCacheEntry e;
	e.key = key;
	e.data = copy;
	e.size = size;
	e.rate = rate;
	e.channels = channels;
	e.lastUseMs = timeGetTime();
	s_pcmCache.push_back(e);
	s_pcmCacheBytes += (size_t)size;
}

static const AVInputFormat *mem_audio_guess_format_by_extension(const char *filenameHint)
{
	if (!filenameHint || !filenameHint[0])
		return NULL;

	const char *ext = strrchr(filenameHint, '.');
	if (!ext || !ext[1])
		return NULL;
	++ext;

	static const struct {
		const char *ext;
		const char *fmt;
	} kMap[] = {
		{ "mp3", "mp3" },
		{ "ogg", "ogg" },
		{ "wav", "wav" },
		{ "webm", "webm" },
		{ "mkv", "matroska" },
		{ NULL, NULL },
	};

	for (int i = 0; kMap[i].ext; ++i) {
		if (strcasecmp(ext, kMap[i].ext) == 0)
			return av_find_input_format(kMap[i].fmt);
	}
	return NULL;
}

static const AVInputFormat *mem_audio_probe_format(const MemAudioIO *io, const char *filenameHint)
{
	if (!io || !io->data || io->size <= 0)
		return NULL;

	const AVInputFormat *byExt = mem_audio_guess_format_by_extension(filenameHint);
	if (byExt)
		return byExt;

	AVProbeData probeData = {};
	probeData.buf = (unsigned char *)io->data;
	probeData.buf_size = io->size < 32768 ? io->size : 32768;
	probeData.filename = (filenameHint && filenameHint[0]) ? filenameHint : "";
	probeData.mime_type = NULL;
	return av_probe_input_format(&probeData, 1);
}

static int audio_resolve_sample_rate(const AVCodecParameters *params,
	const AVCodecContext *ctx, const AVFrame *frame)
{
	if (frame && frame->sample_rate > 0)
		return frame->sample_rate;
	if (ctx && ctx->sample_rate > 0)
		return ctx->sample_rate;
	if (params && params->sample_rate > 0)
		return params->sample_rate;
	return 0;
}

static AVSampleFormat audio_resolve_sample_fmt(const AVCodecParameters *params,
	const AVCodecContext *ctx, const AVFrame *frame, const AVCodec *codec)
{
	if (frame && frame->format != AV_SAMPLE_FMT_NONE)
		return (AVSampleFormat)frame->format;
	if (ctx && ctx->sample_fmt != AV_SAMPLE_FMT_NONE)
		return ctx->sample_fmt;
	if (params && params->format != AV_SAMPLE_FMT_NONE)
		return (AVSampleFormat)params->format;
	if (codec && codec->sample_fmts)
		return codec->sample_fmts[0];
	return AV_SAMPLE_FMT_FLTP;
}

static void audio_resolve_in_chlayout(AVChannelLayout *out,
	const AVCodecParameters *params, const AVCodecContext *ctx, const AVFrame *frame)
{
	if (frame && frame->ch_layout.nb_channels > 0) {
		av_channel_layout_copy(out, &frame->ch_layout);
		return;
	}
	if (ctx && ctx->ch_layout.nb_channels > 0) {
		av_channel_layout_copy(out, &ctx->ch_layout);
		return;
	}
	if (params && params->ch_layout.nb_channels > 0) {
		av_channel_layout_copy(out, &params->ch_layout);
		return;
	}
	av_channel_layout_default(out, 2);
}

static SwrContext *audio_create_swr(const AVCodecParameters *params,
	AVCodecContext *codecCtx, const AVFrame *frame, const AVCodec *codec,
	int outSampleRate, int outChannels)
{
	AVChannelLayout inLayout;
	AVChannelLayout outLayout;
	audio_resolve_in_chlayout(&inLayout, params, codecCtx, frame);

	const int inChannels = inLayout.nb_channels > 0 ? (int)inLayout.nb_channels : 2;
	const int outCh = outChannels > 0 ? outChannels : inChannels;
	av_channel_layout_default(&outLayout, outCh);

	const int inRate = audio_resolve_sample_rate(params, codecCtx, frame);
	if (inRate <= 0) {
		av_channel_layout_uninit(&inLayout);
		av_channel_layout_uninit(&outLayout);
		return NULL;
	}
	if (outSampleRate <= 0)
		outSampleRate = inRate;

	const AVSampleFormat inFmt = audio_resolve_sample_fmt(params, codecCtx, frame, codec);

	SwrContext *swr = swr_alloc();
	if (!swr) {
		av_channel_layout_uninit(&inLayout);
		av_channel_layout_uninit(&outLayout);
		return NULL;
	}

	av_opt_set_chlayout(swr, "in_chlayout", &inLayout, 0);
	av_opt_set_chlayout(swr, "out_chlayout", &outLayout, 0);
	av_opt_set_int(swr, "in_sample_rate", inRate, 0);
	av_opt_set_int(swr, "out_sample_rate", outSampleRate, 0);
	av_opt_set_sample_fmt(swr, "in_sample_fmt", inFmt, 0);
	av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

	av_channel_layout_uninit(&inLayout);
	av_channel_layout_uninit(&outLayout);

	if (swr_init(swr) < 0) {
		swr_free(&swr);
		return NULL;
	}
	return swr;
}

static Bool mem_audio_open_input(AVFormatContext **fmt, MemAudioIO *memIO, const char *filenameHint)
{
	const AVInputFormat *inputFmt = mem_audio_probe_format(memIO, filenameHint);
	const char *openName = (filenameHint && filenameHint[0]) ? filenameHint : NULL;

	if (inputFmt) {
		if (avformat_open_input(fmt, openName, inputFmt, NULL) >= 0)
			return true;
		if (openName && avformat_open_input(fmt, NULL, inputFmt, NULL) >= 0)
			return true;
	}

	if (openName && avformat_open_input(fmt, openName, NULL, NULL) >= 0)
		return true;

	return false;
}

} // namespace

static int mem_audio_read(void *opaque, uint8_t *buf, int buf_size)
{
	MemAudioIO *io = (MemAudioIO *)opaque;
	if (!io || !buf || buf_size <= 0 || io->pos >= io->size)
		return AVERROR_EOF;
	int remain = io->size - io->pos;
	int to_read = (remain < buf_size) ? remain : buf_size;
	memcpy(buf, io->data + io->pos, to_read);
	io->pos += to_read;
	return to_read;
}

static int64_t mem_audio_seek(void *opaque, int64_t offset, int whence)
{
	MemAudioIO *io = (MemAudioIO *)opaque;
	if (!io) return AVERROR(EINVAL);
	switch (whence) {
	case AVSEEK_SIZE:
		return io->size;
	case SEEK_SET:
		io->pos = (offset < 0) ? 0 : (int)(offset > io->size ? io->size : offset);
		break;
	case SEEK_CUR:
		io->pos = (int)(io->pos + offset);
		if (io->pos > io->size) io->pos = io->size;
		if (io->pos < 0) io->pos = 0;
		break;
	case SEEK_END:
		io->pos = io->size + (int)offset;
		if (io->pos > io->size) io->pos = io->size;
		if (io->pos < 0) io->pos = 0;
		break;
	}
	return io->pos;
}

SDLAudioManager::SDLAudioManager() :
	m_playingList(NULL),
	m_stoppedList(NULL),
	m_binkHandle(NULL),
	m_musicPlaying(false),
	m_musicCompletedCount(0),
	m_selectedProvider(0),
	m_selectedSpeakerType(0),
	m_targetSampleRate(44100),
	m_targetChannels(2)
{
}

SDLAudioManager::~SDLAudioManager()
{
	closeDevice();
}

void SDLAudioManager::init()
{
	AudioManager::init();
	openDevice();
}

void SDLAudioManager::postProcessLoad()
{
	AudioManager::postProcessLoad();
}

void SDLAudioManager::reset()
{
	AudioManager::reset();
	stopAllAudio();
	m_currentTrackName.clear();
	m_musicPlaying = false;
	m_musicCompletedCount = 0;
}

void SDLAudioManager::update()
{
	Audio_Budget_Begin();
	AudioManager::update();
	setDeviceListenerPosition();
	processRequestList();
	updateStreaming();
	processFadingMusic();
	processPlayingList();
	processStoppedList();
}

void SDLAudioManager::openDevice()
{
}

void SDLAudioManager::closeDevice()
{
	stopAllAudio();
}

static Bool sdlPlayingIsMusic(const SDLPlayingAudio *pa)
{
	if (!pa || !pa->m_event)
		return false;
	const AudioEventInfo *info = pa->m_event->getAudioEventInfo();
	return info && info->m_soundType == AT_Music;
}

static Bool sdlPlayingIsSpeech(const SDLPlayingAudio *pa)
{
	if (!pa || !pa->m_event)
		return false;
	const AudioEventInfo *info = pa->m_event->getAudioEventInfo();
	return info && info->m_soundType == AT_Streaming;
}

void SDLAudioManager::stopAudio(AudioAffect which)
{
	SDLPlayingAudio *pa = m_playingList;
	while (pa) {
		if ((which & AudioAffect_Sound) && !pa->m_is3D && pa->m_type == SPT_Sample)
			pa->m_shouldStop = true;
		if ((which & AudioAffect_Sound3D) && pa->m_is3D && pa->m_type == SPT_Sample)
			pa->m_shouldStop = true;
		if ((which & AudioAffect_Music) && pa->m_type == SPT_Stream && sdlPlayingIsMusic(pa))
			pa->m_shouldStop = true;
		if ((which & AudioAffect_Speech) && pa->m_type == SPT_Stream && sdlPlayingIsSpeech(pa))
			pa->m_shouldStop = true;
		pa = pa->m_next;
	}
}

void SDLAudioManager::pauseAudio(AudioAffect which)
{
	SDLPlayingAudio *pa = m_playingList;
	while (pa) {
		Bool match = false;
		if ((which & AudioAffect_Sound) && !pa->m_is3D && pa->m_type == SPT_Sample)
			match = true;
		if ((which & AudioAffect_Sound3D) && pa->m_is3D && pa->m_type == SPT_Sample)
			match = true;
		if ((which & AudioAffect_Music) && pa->m_type == SPT_Stream && sdlPlayingIsMusic(pa))
			match = true;
		if ((which & AudioAffect_Speech) && pa->m_type == SPT_Stream && sdlPlayingIsSpeech(pa))
			match = true;
		if (match && pa->m_isPlaying && pa->m_stream) {
			SDL_PauseAudioStreamDevice(pa->m_stream);
			pa->m_isPlaying = false;
		}
		pa = pa->m_next;
	}
}

void SDLAudioManager::resumeAudio(AudioAffect which)
{
	SDLPlayingAudio *pa = m_playingList;
	while (pa) {
		Bool match = false;
		if ((which & AudioAffect_Sound) && !pa->m_is3D && pa->m_type == SPT_Sample)
			match = true;
		if ((which & AudioAffect_Sound3D) && pa->m_is3D && pa->m_type == SPT_Sample)
			match = true;
		if ((which & AudioAffect_Music) && pa->m_type == SPT_Stream && sdlPlayingIsMusic(pa))
			match = true;
		if ((which & AudioAffect_Speech) && pa->m_type == SPT_Stream && sdlPlayingIsSpeech(pa))
			match = true;
		if (match && !pa->m_isPlaying && pa->m_stream) {
			SDL_ResumeAudioStreamDevice(pa->m_stream);
			pa->m_isPlaying = true;
		}
		pa = pa->m_next;
	}
}

void SDLAudioManager::pauseAmbient(Bool shouldPause)
{
}

void SDLAudioManager::stopAllAmbientsBy(Object *obj)
{
	if (!obj) return;
	UnsignedInt objID = obj->getID();
	SDLPlayingAudio *pa = m_playingList;
	while (pa) {
		if (pa->m_ownerType == 2 && pa->m_ownerID == objID)
			pa->m_shouldStop = true;
		pa = pa->m_next;
	}
}

void SDLAudioManager::stopAllAmbientsBy(Drawable *draw)
{
	if (!draw) return;
	UnsignedInt drawID = draw->getID();
	SDLPlayingAudio *pa = m_playingList;
	while (pa) {
		if (pa->m_ownerType == 1 && pa->m_ownerID == drawID)
			pa->m_shouldStop = true;
		pa = pa->m_next;
	}
}

void SDLAudioManager::killAudioEventImmediately(AudioHandle audioEvent)
{
	stopAudioEvent(audioEvent);
}

void SDLAudioManager::nextMusicTrack()
{
	AsciiString trackName = m_currentTrackName;
	SDLPlayingAudio *pa = m_playingList;
	while (pa) {
		if (pa->m_type == SPT_Stream && sdlPlayingIsMusic(pa) && pa->m_event)
			trackName = pa->m_event->getEventName();
		pa = pa->m_next;
	}

	TheAudio->removeAudioEvent(AHSV_StopTheMusic);

	trackName = nextTrackName(trackName);
	if (trackName.isEmpty())
		return;

	AudioEventRTS newTrack(trackName);
	TheAudio->addAudioEvent(&newTrack);
}

void SDLAudioManager::prevMusicTrack()
{
	AsciiString trackName = m_currentTrackName;
	SDLPlayingAudio *pa = m_playingList;
	while (pa) {
		if (pa->m_type == SPT_Stream && sdlPlayingIsMusic(pa) && pa->m_event)
			trackName = pa->m_event->getEventName();
		pa = pa->m_next;
	}

	TheAudio->removeAudioEvent(AHSV_StopTheMusic);

	trackName = prevTrackName(trackName);
	if (trackName.isEmpty())
		return;

	AudioEventRTS newTrack(trackName);
	TheAudio->addAudioEvent(&newTrack);
}

Bool SDLAudioManager::isMusicPlaying() const
{
	Bool playing = false;
	int musicCount = 0;
	const SDLPlayingAudio *pa = m_playingList;
	while (pa) {
		if (pa->m_type == SPT_Stream && sdlPlayingIsMusic(pa)) {
			musicCount++;
			if (pa->m_isPlaying)
				playing = true;
		}
		pa = pa->m_next;
	}
	return playing;
}

Bool SDLAudioManager::hasMusicTrackCompleted(const AsciiString& trackName, Int numberOfTimes) const
{
	Bool result = (m_currentTrackName == trackName)
		&& (m_musicCompletedCount >= numberOfTimes);
	return result;
}

AsciiString SDLAudioManager::getMusicTrackName() const
{
	return m_currentTrackName;
}

Bool SDLAudioManager::isCurrentlyPlaying(AudioHandle handle)
{
	/* Base AudioManager::isCurrentlyPlaying() always returns true (Miles stub).
	   EVA, ambient drawables, turrets, etc. gate playback on this — without a
	   real override EVA never starts because it thinks speech is already playing. */
	if (handle < AHSV_FirstHandle)
		return false;

	for (SDLPlayingAudio *pa = m_playingList; pa; pa = pa->m_next) {
		if (pa->m_event && pa->m_event->getPlayingHandle() == handle && pa->m_isPlaying)
			return true;
	}

	for (std::list<AudioRequest *>::const_iterator ait = m_audioRequests.begin();
		 ait != m_audioRequests.end(); ++ait) {
		AudioRequest *req = *ait;
		if (req && req->m_usePendingEvent && req->m_pendingEvent
			&& req->m_pendingEvent->getPlayingHandle() == handle)
			return true;
	}

	return false;
}

void SDLAudioManager::notifyOfAudioCompletion(UnsignedInt audioCompleted, UnsignedInt flags)
{
}

AsciiString SDLAudioManager::getProviderName(UnsignedInt providerNum) const
{
	return AsciiString("SDL Audio");
}

UnsignedInt SDLAudioManager::getProviderIndex(AsciiString providerName) const
{
	return 0;
}

void SDLAudioManager::selectProvider(UnsignedInt providerNdx)
{
	m_selectedProvider = providerNdx;
}

void SDLAudioManager::unselectProvider()
{
	m_selectedProvider = 0;
}

UnsignedInt SDLAudioManager::getSelectedProvider() const
{
	return m_selectedProvider;
}

void SDLAudioManager::setSpeakerType(UnsignedInt speakerType)
{
	m_selectedSpeakerType = speakerType;
}

UnsignedInt SDLAudioManager::getSpeakerType()
{
	return m_selectedSpeakerType;
}

void *SDLAudioManager::getHandleForBink()
{
	return NULL;
}

void SDLAudioManager::releaseHandleForBink()
{
}

void SDLAudioManager::friend_forcePlayAudioEventRTS(const AudioEventRTS *eventToPlay)
{
	if (!eventToPlay)
		return;

	/*
	 * Miles uses AIL_quick_load_and_play for mission briefings (Speech volume,
	 * center pan). Match that path: resolve info, honour Speech mute, apply
	 * script volume overrides, then play as a speech stream.
	 */
	if (!eventToPlay->getAudioEventInfo()) {
		getInfoForAudioEvent(eventToPlay);
		if (!eventToPlay->getAudioEventInfo()) {
			DEBUG_CRASH(("No info for forced audio event '%s'\n", eventToPlay->getEventName().str()));
			return;
		}
	}

	switch (eventToPlay->getAudioEventInfo()->m_soundType) {
	case AT_Music:
		if (!isOn(AudioAffect_Music))
			return;
		break;
	case AT_SoundEffect:
		if (!isOn(AudioAffect_Sound) || !isOn(AudioAffect_Sound3D))
			return;
		break;
	case AT_Streaming:
		if (!isOn(AudioAffect_Speech))
			return;
		break;
	default:
		break;
	}

	AudioEventRTS *ev = MSGNEW("AudioEventRTS") AudioEventRTS(*eventToPlay);
	ev->setPlayingHandle(allocateNewHandle());
	ev->generateFilename();
	ev->generatePlayInfo();

	for (std::list<std::pair<AsciiString, Real> >::iterator it = m_adjustedVolumes.begin();
		 it != m_adjustedVolumes.end(); ++it) {
		if (it->first == ev->getEventName()) {
			ev->setVolume(it->second);
			break;
		}
	}

	playAudioEvent(ev);
}

UnsignedInt SDLAudioManager::getNum2DSamples() const
{
	/* Miles returns pool *capacity*, not currently-playing count.
	   SoundManager::canPlayNow caches this once as m_num2DSamples and
	   compares against m_numPlaying2DSamples — returning "playing" here
	   left capacity at 0 and culled every SFX / EVA / ambient forever. */
	const AudioSettings *settings = getAudioSettings();
	return settings ? (UnsignedInt)settings->m_sampleCount2D : 0;
}

UnsignedInt SDLAudioManager::getNum3DSamples() const
{
	const AudioSettings *settings = getAudioSettings();
	return settings ? (UnsignedInt)settings->m_sampleCount3D : 0;
}

UnsignedInt SDLAudioManager::getNumStreams() const
{
	const AudioSettings *settings = getAudioSettings();
	return settings ? (UnsignedInt)settings->m_streamCount : 0;
}

Bool SDLAudioManager::doesViolateLimit(AudioEventRTS *event) const
{
	if (!event) return true;
	const AudioEventInfo *info = event->getAudioEventInfo();
	if (!info) return true;

	Int limit = info->m_limit;
	if (limit == 0)
		return false;

	Int totalCount = 0;
	Int totalRequestCount = 0;
	const Bool isStream = (info->m_soundType == AT_Music || info->m_soundType == AT_Streaming);
	const Bool is3D = event->isPositionalAudio();

	for (SDLPlayingAudio *pa = m_playingList; pa; pa = pa->m_next) {
		if (!pa->m_event)
			continue;
		if (pa->m_event->getEventName() != event->getEventName())
			continue;
		if (isStream) {
			if (pa->m_type != SPT_Stream)
				continue;
		} else if (pa->m_type != SPT_Sample || pa->m_is3D != is3D) {
			continue;
		}
		if (totalCount == 0)
			event->setHandleToKill(pa->m_event->getPlayingHandle());
		++totalCount;
	}

	for (std::list<AudioRequest *>::const_iterator arIt = m_audioRequests.begin();
		 arIt != m_audioRequests.end(); ++arIt) {
		AudioRequest *req = *arIt;
		if (!req || !req->m_usePendingEvent || !req->m_pendingEvent)
			continue;
		if (req->m_pendingEvent->getEventName() == event->getEventName()) {
			++totalRequestCount;
			++totalCount;
		}
	}

	if (info->m_control & AC_INTERRUPT) {
		if (totalRequestCount < limit) {
			Int totalPlayingCount = totalCount - totalRequestCount;
			if (totalRequestCount + totalPlayingCount < limit) {
				event->setHandleToKill(0);
				return false;
			}
			return false;
		}
	}

	if (totalCount < limit) {
		event->setHandleToKill(0);
		return false;
	}

	return true;
}

Bool SDLAudioManager::isPlayingLowerPriority(AudioEventRTS *event) const
{
	if (!event) return false;
	const AudioEventInfo *info = event->getAudioEventInfo();
	if (!info) return false;

	SDLPlayingAudio *pa = m_playingList;
	while (pa) {
		if (pa->m_type == SPT_Sample && pa->m_event) {
			const AudioEventInfo *pi = pa->m_event->getAudioEventInfo();
			if (pi && pi->m_priority < info->m_priority)
				return true;
		}
		pa = pa->m_next;
	}
	return false;
}

Bool SDLAudioManager::isPlayingAlready(AudioEventRTS *event) const
{
	if (!event) return false;
	SDLPlayingAudio *pa = m_playingList;
	while (pa) {
		if (pa->m_event && pa->m_event->getEventName() == event->getEventName() && pa->m_isPlaying)
			return true;
		pa = pa->m_next;
	}
	return false;
}

Bool SDLAudioManager::isObjectPlayingVoice(UnsignedInt objID) const
{
	if (objID == 0)
		return false;
	SDLPlayingAudio *pa = m_playingList;
	while (pa) {
		if (pa->m_isPlaying && pa->m_event && pa->m_event->getObjectID() == objID) {
			const AudioEventInfo *info = pa->m_event->getAudioEventInfo();
			if (info && BitTest(info->m_type, ST_VOICE))
				return true;
		}
		pa = pa->m_next;
	}
	return false;
}

void SDLAudioManager::adjustVolumeOfPlayingAudio(AsciiString eventName, Real newVolume)
{
	SDLPlayingAudio *pa = m_playingList;
	while (pa) {
		if (pa->m_event && pa->m_event->getEventName() == eventName) {
			pa->m_baseVolume = newVolume;
			pa->m_event->setVolume(newVolume);
			if (pa->m_stream)
				applyPlayingGain(pa);
		}
		pa = pa->m_next;
	}
}

void SDLAudioManager::removePlayingAudio(AsciiString eventName)
{
	SDLPlayingAudio *pa = m_playingList;
	while (pa) {
		if (pa->m_event && pa->m_event->getEventName() == eventName)
			pa->m_shouldStop = true;
		pa = pa->m_next;
	}
}

void SDLAudioManager::removeAllDisabledAudio()
{
	Real minVol = getAudioSettings() ? getAudioSettings()->m_minVolume : 0.0f;
	SDLPlayingAudio *pa = m_playingList;
	while (pa) {
		if (pa->m_event && pa->m_event->getVolume() < minVol)
			pa->m_shouldStop = true;
		pa = pa->m_next;
	}
}

Real SDLAudioManager::getFileLengthMS(AsciiString strToLoad) const
{
	if (strToLoad.isEmpty()) return 0.0f;

	File *fp = TheFileSystem->openFile(strToLoad.str(), File::READ);
	if (!fp) return 0.0f;

	Int fileSize = fp->size();
	char *fileData = fp->readEntireAndClose();
	if (!fileData) return 0.0f;

	MemAudioIO memIO;
	memIO.data = fileData;
	memIO.size = fileSize;
	memIO.pos = 0;

	AVFormatContext *fmt = NULL;
	AVIOContext *avio = NULL;
	unsigned char *avioBuf = (unsigned char *)av_malloc(4096);
	if (!avioBuf) { delete[] fileData; return 0.0f; }

	avio = avio_alloc_context(avioBuf, 4096, 0, &memIO, mem_audio_read, NULL, mem_audio_seek);
	if (!avio) { av_free(avioBuf); delete[] fileData; return 0.0f; }

	fmt = avformat_alloc_context();
	if (!fmt) { avio_context_free(&avio); delete[] fileData; return 0.0f; }

	fmt->pb = avio;
	fmt->flags |= AVFMT_FLAG_CUSTOM_IO;

	if (!mem_audio_open_input(&fmt, &memIO, strToLoad.str())) {
		avformat_close_input(&fmt);
		delete[] fileData;
		return 0.0f;
	}

	if (avformat_find_stream_info(fmt, NULL) < 0) {
		avformat_close_input(&fmt);
		delete[] fileData;
		return 0.0f;
	}

	Real lengthMs = 0.0f;
	if (fmt->duration > 0)
		lengthMs = (Real)(fmt->duration / 1000.0);

	for (unsigned i = 0; i < fmt->nb_streams; i++) {
		if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			if (fmt->streams[i]->duration > 0) {
				AVRational tb = fmt->streams[i]->time_base;
				lengthMs = (Real)((double)fmt->streams[i]->duration * tb.num * 1000.0 / tb.den);
			}
			break;
		}
	}

	avformat_close_input(&fmt);
	delete[] fileData;
	return lengthMs;
}

void SDLAudioManager::closeAnySamplesUsingFile(const void *fileToClose)
{
}

#if defined(_DEBUG) || defined(_INTERNAL)
void SDLAudioManager::audioDebugDisplay(DebugDisplayInterface *dd, void *userData, FILE *fp)
{
	if (fp) {
		fprintf(fp, "SDLAudioManager:\n");
		fprintf(fp, "  2D samples: %d\n", (int)getNum2DSamples());
		fprintf(fp, "  3D samples: %d\n", (int)getNum3DSamples());
		fprintf(fp, "  Streams: %d\n", (int)getNumStreams());
	}
}
#endif

void SDLAudioManager::setDeviceListenerPosition()
{
	/* Pose is stored on AudioManager each GameAudio::update(). Distance and
	 * stereo pan read m_listenerPosition / m_listenerOrientation live. */
}

Real SDLAudioManager::calcStereoPan(SDLPlayingAudio *pa) const
{
	if (!pa || !pa->m_is3D || !pa->m_event)
		return 0.0f;

	const Coord3D *pos = pa->m_event->getCurrentPosition();
	if (!pos)
		return 0.0f;

	/*
	 * Miles Fast 2D: listener forward + up=(0,0,-1). Pan from lateral
	 * component. Soften with asin so mid-field sources fade instead of
	 * slamming to hard L/R (raw cos-azimuth hits ±1 very early).
	 */
	Coord3D forward = m_listenerOrientation;
	const Real flen = forward.length();
	if (flen < 1.0e-4f)
		return 0.0f;
	forward.normalize();

	Coord3D up;
	up.set(0.0f, 0.0f, -1.0f);
	Coord3D right;
	Coord3D::crossProduct(&forward, &up, &right);
	const Real rlen = right.length();
	if (rlen < 1.0e-4f)
		return 0.0f;
	right.normalize();

	Coord3D toSrc;
	toSrc.set(pos);
	toSrc.sub(&m_listenerPosition);
	const Real dlen = toSrc.length();
	if (dlen < 1.0e-4f)
		return 0.0f;
	toSrc.normalize();

	/* Negate: RH forward×up points world-left; +pan = screen-right = R. */
	Real lateral = -(toSrc.x * right.x + toSrc.y * right.y + toSrc.z * right.z);
	if (lateral < -1.0f) lateral = -1.0f;
	if (lateral > 1.0f) lateral = 1.0f;
	/* Angle-linear pan in [-1,1]: 0° ahead → 0, 90° side → ±1. */
	Real pan = (Real)(std::asin((double)lateral) / 1.5707963267948966);
	if (pan < -1.0f) pan = -1.0f;
	if (pan > 1.0f) pan = 1.0f;
	return pan;
}

Real SDLAudioManager::calcPlayingGain(SDLPlayingAudio *pa) const
{
	if (!pa || !pa->m_event)
		return 0.0f;

	const AudioEventInfo *info = pa->m_event->getAudioEventInfo();
	if (!info)
		return 0.0f;

	Real volume = pa->m_event->getVolume() * pa->m_event->getVolumeShift();

	if (info->m_soundType == AT_Music) {
		volume *= getVolume(AudioAffect_Music);
	} else if (info->m_soundType == AT_Streaming) {
		volume *= getVolume(AudioAffect_Speech);
	} else if (pa->m_is3D) {
		/* Do NOT multiply by m_relative2DVolume — that INI bias is applied when
		   Options sets preferred 2D/3D slider levels, not per-sample (Miles never
		   did). Relative2DVolume defaults to -10%, which clamped all 3D to silence. */
		volume *= getVolume(AudioAffect_Sound3D);

		const Coord3D *pos = pa->m_event->getCurrentPosition();
		if (pos) {
			Coord3D distance = m_listenerPosition;
			distance.sub(pos);

			Real objMinDistance;
			Real objMaxDistance;
			if (info->m_type & ST_GLOBAL) {
				objMinDistance = (Real)getAudioSettings()->m_globalMinRange;
				objMaxDistance = (Real)getAudioSettings()->m_globalMaxRange;
			} else {
				objMinDistance = info->m_minDistance;
				objMaxDistance = info->m_maxDistance;
			}

			Real objDistance = distance.length();
			if (objDistance > objMinDistance)
				volume *= 1.0f / (objDistance / objMinDistance);
			if (objDistance >= objMaxDistance)
				volume = 0.0f;
		} else {
			/* Miles stops the sample when position disappears. */
			volume = 0.0f;
		}
	} else {
		volume *= getVolume(AudioAffect_Sound);
	}

	if (volume < 0.0f) volume = 0.0f;
	if (volume > 1.0f) volume = 1.0f;
	return volume;
}

void SDLAudioManager::applyPlayingGain(SDLPlayingAudio *pa)
{
	if (!pa || !pa->m_stream)
		return;
	SDL_SetAudioStreamGain(pa->m_stream, calcPlayingGain(pa));
}

/*
 * Bake mono/stereo source into a stereo buffer with constant-power pan (and
 * optional off-screen low-pass). Used for one-shot submission — avoids the
 * underruns we got from ~40ms chunked feeds under UI/game hitch.
 *
 * Stereo sources are downmixed to mono before pan (Miles refuses stereo 3D).
 * Pan gains use equal-power law (center ≈ 0.707 / 0.707).
 */
static Bool Bake_Stereo_Panned(
	const Sint16 *src,
	Uint32 srcBytes,
	Int srcCh,
	Real gL,
	Real gR,
	Bool applyLp,
	Real lpAlpha,
	Real &lpState,
	std::vector<Sint16> &outStereo)
{
	if (!src || srcCh < 1 || srcBytes < (Uint32)(2 * srcCh))
		return false;

	const int frames = (int)(srcBytes / (Uint32)(2 * srcCh));
	if (frames <= 0)
		return false;

	outStereo.resize((size_t)frames * 2u);
	for (int i = 0; i < frames; ++i) {
		Real s;
		if (srcCh == 1) {
			s = (Real)src[i];
		} else {
			/* Miles 3D path requires mono — average channels then pan. */
			s = 0.0f;
			for (Int c = 0; c < srcCh; ++c)
				s += (Real)src[i * srcCh + c];
			s /= (Real)srcCh;
		}
		if (applyLp) {
			lpState += lpAlpha * (s - lpState);
			s = lpState;
		}
		Real oL = s * gL;
		Real oR = s * gR;
		if (oL > 32767.0f) oL = 32767.0f;
		if (oL < -32768.0f) oL = -32768.0f;
		if (oR > 32767.0f) oR = 32767.0f;
		if (oR < -32768.0f) oR = -32768.0f;
		outStereo[(size_t)i * 2u] = (Sint16)oL;
		outStereo[(size_t)i * 2u + 1u] = (Sint16)oR;
	}
	return true;
}

/* ~8ms linear fade-in to hide random-sample discontinuities at loop seams. */
static void Fade_In_S16(Sint16 *samples, int sampleCount, int channels, int sampleRate)
{
	if (!samples || sampleCount <= 0 || channels < 1 || sampleRate < 1)
		return;
	int fadeFrames = sampleRate / 125; /* ~8ms */
	if (fadeFrames < 32)
		fadeFrames = 32;
	const int frames = sampleCount / channels;
	if (fadeFrames > frames)
		fadeFrames = frames;
	for (int i = 0; i < fadeFrames; ++i) {
		const Real g = (Real)i / (Real)fadeFrames;
		for (int c = 0; c < channels; ++c) {
			const int idx = i * channels + c;
			samples[idx] = (Sint16)((Real)samples[idx] * g);
		}
	}
}

/*
 * Equal-power stereo pan (Miles Fast 2D Positional Audio).
 * Center: cos(π/4)=sin(π/4)=√2/2 ≈ 0.707 — total power conserved.
 * Do NOT apply √2 boost to make center 1,1: that is ~+3 dB louder than Miles
 * and makes AmbientLoops (jungle, heli, map props) drown the mix.
 */
static void Stereo_Pan_Gains(Real pan, Real &gL, Real &gR)
{
	if (pan < -1.0f) pan = -1.0f;
	if (pan > 1.0f) pan = 1.0f;
	const Real angle = (pan + 1.0f) * (Real)(0.7853981633974483); /* 0..pi/2 */
	gL = std::cos(angle);
	gR = std::sin(angle);
	if (gL < 0.0f) gL = 0.0f;
	if (gR < 0.0f) gR = 0.0f;
}

static Real Smooth_Pan(SDLPlayingAudio *pa, Real target)
{
	if (!pa->m_panSmoothInit) {
		pa->m_panSmooth = target;
		pa->m_panSmoothInit = true;
		return target;
	}
	/* ~4–5 frames to catch camera/unit motion without L/R clicks. */
	pa->m_panSmooth += (target - pa->m_panSmooth) * 0.28f;
	return pa->m_panSmooth;
}

/* Mark when this PCM buffer should finish (device may empty the SDL stream early). */
static void Mark_Sample_Play_End(SDLPlayingAudio *pa, Uint32 pcmBytes, Int channels, int sampleRate)
{
	if (!pa)
		return;
	const Int ch = channels > 0 ? channels : 1;
	const int rate = sampleRate > 0 ? sampleRate : 22050;
	const Uint32 frames = pcmBytes / (Uint32)(2 * ch);
	Real durationMs = (rate > 0) ? (1000.0f * (Real)frames / (Real)rate) : 0.0f;
	if (pa->m_event) {
		Real pitch = pa->m_event->getPitchShift();
		if (pitch > 0.01f)
			durationMs /= pitch;
	}
	/*
	 * Small device-period slack only. Large slack + queue-wait used to stack into
	 * ~1–2s silent holes between AmbientLoop iterations.
	 */
	pa->m_playEndMs = timeGetTime() + (UnsignedInt)(durationMs + 40.0f);
}

/* Estimate ms still in the SDL stream (queued-only for device-bound streams). */
static int Stream_Remain_Ms(const SDLPlayingAudio *pa, int queued, int available)
{
	if (!pa)
		return 0;
	const int ch = pa->m_streamChannels > 0 ? pa->m_streamChannels
		: (pa->m_is3D ? 2 : (pa->m_channels > 0 ? pa->m_channels : 1));
	const int rate = pa->m_sampleRate > 0 ? pa->m_sampleRate : 22050;
	const int frameBytes = 2 * (ch > 0 ? ch : 1);
	const int bytes = (queued > 0) ? queued : ((available > 0) ? available : 0);
	if (frameBytes <= 0 || rate <= 0 || bytes <= 0)
		return 0;
	const int frames = bytes / frameBytes;
	Real pitch = (pa->m_event) ? pa->m_event->getPitchShift() : 1.0f;
	if (pitch < 0.01f)
		pitch = 1.0f;
	return (int)(1000.0f * (Real)frames / ((Real)rate * pitch));
}

/* Install prefetched mono body as the new source (gapless 3D chunk path). */
static Bool Install_Prefetch_Mono(SDLPlayingAudio *pa)
{
	if (!pa || !pa->m_prefetchReady || !pa->m_prefetchData || !pa->m_event)
		return false;
	if (pa->m_sampleRate != pa->m_prefetchRate || pa->m_channels != pa->m_prefetchChannels)
		return false;

	AudioEventRTS *event = pa->m_event;
	const AudioEventInfo *info = event->getAudioEventInfo();
	const Int curPortion = (Int)event->getNextPlayPortion();

	if (pa->m_looping && curPortion == PP_Sound && info && (info->m_control & AC_LOOP)) {
		event->decreaseLoopCount();
		if (!event->hasMoreLoops()) {
			return false;
		}
		event->setNextPlayPortion(PP_Sound);
	} else if (curPortion == PP_Attack) {
		event->advanceNextPlayPortion();
		if (event->getNextPlayPortion() != PP_Sound)
			return false;
	} else {
		return false;
	}

	if (pa->m_pcmData)
		free(pa->m_pcmData);
	pa->m_pcmData = pa->m_prefetchData;
	pa->m_pcmSize = pa->m_prefetchSize;
	pa->m_prefetchData = NULL;
	pa->m_prefetchSize = 0;
	pa->m_prefetchReady = false;
	pa->m_pcmOffset = 0;
	pa->m_feedFlushed = false;
	pa->m_feedStartMs = timeGetTime();

	const Int ch = pa->m_channels > 0 ? pa->m_channels : 1;
	Real durMs = 1000.0f * (Real)(pa->m_pcmSize / (Uint32)(2 * ch))
		/ (Real)(pa->m_sampleRate > 0 ? pa->m_sampleRate : 22050);
	Real pitch = event->getPitchShift();
	if (pitch > 0.01f)
		durMs /= pitch;
	const int remain = pa->m_stream
		? Stream_Remain_Ms(pa, SDL_GetAudioStreamQueued(pa->m_stream),
			SDL_GetAudioStreamAvailable(pa->m_stream))
		: 0;
	pa->m_playEndMs = timeGetTime() + (UnsignedInt)(remain + durMs + 20.0f);
	return true;
}

void SDLAudioManager::feedSampleChunks(SDLPlayingAudio *pa)
{
	/*
	 * 3D: stream mono source in short stereo chunks with *current* pan each
	 * Put. Baking the whole file once froze L/R for ~1s so motion sounded like
	 * hard channel switching instead of a fade.
	 */
	if (!pa || !pa->m_stream || !pa->m_pcmData || pa->m_pcmSize == 0 || pa->m_feedFlushed)
		return;
	if (!pa->m_is3D || pa->m_streamChannels < 2)
		return;

	const Int srcCh = pa->m_channels > 0 ? pa->m_channels : 1;
	const Int srcFrameBytes = 2 * srcCh;
	const Int dstFrameBytes = 4;
	const int rate = pa->m_sampleRate > 0 ? pa->m_sampleRate : 22050;
	Real pitch = pa->m_event ? pa->m_event->getPitchShift() : 1.0f;
	if (pitch < 0.01f)
		pitch = 1.0f;

	/* Prefetch BEFORE draining source — otherwise Flush leaves a gap in AmbientLoop. */
	if (!pa->m_prefetchReady && pa->m_pcmSize > pa->m_pcmOffset) {
		const Uint32 framesLeft = (pa->m_pcmSize - pa->m_pcmOffset) / (Uint32)srcFrameBytes;
		const int srcRemainMs = (int)(1000.0f * (Real)framesLeft / ((Real)rate * pitch));
		if (srcRemainMs <= 320)
			prepareSamplePrefetch(pa);
	}

	/*
	 * Keep ~350ms of queued stereo, scaled by pitch so FrequencyRatio > 1
	 * cannot empty the device buffer between frames (heli PitchShift ±5%).
	 */
	const int targetQueued = (int)((Real)(rate * dstFrameBytes) * 0.35f * pitch);
	int queued = SDL_GetAudioStreamQueued(pa->m_stream);

	static thread_local std::vector<Sint16> s_chunk;
	/* ~50ms chunks — pan tracks motion; deep queue covers frame spikes. */
	const int chunkFrames = (rate / 20) > 512 ? (rate / 20) : 512;

	for (int pass = 0; pass < 2; ++pass) {
		while (queued < targetQueued && pa->m_pcmOffset < pa->m_pcmSize) {
			const Real pan = Smooth_Pan(pa, calcStereoPan(pa));
			Real gL = 1.0f, gR = 1.0f;
			Stereo_Pan_Gains(pan, gL, gR);

			Bool applyLp = false;
			Real lpAlpha = 1.0f;
			if (pa->m_event) {
				const AudioEventInfo *info = pa->m_event->getAudioEventInfo();
				const Coord3D *pos = pa->m_event->getCurrentPosition();
				if (info && pos && info->m_lowPassFreq > 0.0f && TheTacticalView) {
					ICoord2D dummy;
					if (!TheTacticalView->worldToScreen(pos, &dummy)) {
						applyLp = true;
						lpAlpha = info->m_lowPassFreq;
						if (lpAlpha < 0.05f) lpAlpha = 0.05f;
						if (lpAlpha > 1.0f) lpAlpha = 1.0f;
					}
				}
			}

			const Uint32 remainBytes = pa->m_pcmSize - pa->m_pcmOffset;
			int frames = (int)(remainBytes / (Uint32)srcFrameBytes);
			if (frames <= 0)
				break;
			if (frames > chunkFrames)
				frames = chunkFrames;

			const Sint16 *src = reinterpret_cast<const Sint16 *>(pa->m_pcmData + pa->m_pcmOffset);
			if (!Bake_Stereo_Panned(src, (Uint32)(frames * srcFrameBytes), srcCh, gL, gR,
					applyLp, lpAlpha, pa->m_lpState, s_chunk))
				break;

			const int putBytes = frames * dstFrameBytes;
			if (!SDL_PutAudioStreamData(pa->m_stream, s_chunk.data(), putBytes))
				break;

			pa->m_pcmOffset += (Uint32)(frames * srcFrameBytes);
			queued = SDL_GetAudioStreamQueued(pa->m_stream);
		}

		if (pa->m_pcmOffset < pa->m_pcmSize)
			return;

		/* Source exhausted — try gapless install of prefetch, else flush. */
		if (pa->m_prefetchReady && Install_Prefetch_Mono(pa)) {
			queued = SDL_GetAudioStreamQueued(pa->m_stream);
			continue;
		}

		/*
		 * No prefetch yet: do NOT Flush while queued audio remains — that gap
		 * was the heli drone chop. Retry prefetch; if the device queue is also
		 * empty, flush so advance/Decay can proceed.
		 */
		if (pa->m_looping && pa->m_event) {
			const AudioEventInfo *info = pa->m_event->getAudioEventInfo();
			if (info && (info->m_control & AC_LOOP)
				&& pa->m_event->getNextPlayPortion() == PP_Sound) {
				prepareSamplePrefetch(pa);
				if (pa->m_prefetchReady && Install_Prefetch_Mono(pa)) {
					queued = SDL_GetAudioStreamQueued(pa->m_stream);
					continue;
				}
				queued = SDL_GetAudioStreamQueued(pa->m_stream);
				if (queued > 0)
					return;
			}
		}

		if (!pa->m_feedFlushed) {
			SDL_FlushAudioStream(pa->m_stream);
			pa->m_feedFlushed = true;
		}
		return;
	}
}

AsciiString SDLAudioManager::filenameForPortion(AudioEventRTS *event) const
{
	if (!event)
		return AsciiString::TheEmptyString;
	switch (event->getNextPlayPortion()) {
	case PP_Attack:
		return event->getAttackFilename();
	case PP_Decay:
		return event->getDecayFilename();
	case PP_Sound:
	default:
		return event->getFilename();
	}
}

void SDLAudioManager::clearSamplePrefetch(SDLPlayingAudio *pa)
{
	if (!pa)
		return;
	if (pa->m_prefetchData) {
		free(pa->m_prefetchData);
		pa->m_prefetchData = NULL;
	}
	pa->m_prefetchSize = 0;
	pa->m_prefetchRate = 0;
	pa->m_prefetchChannels = 0;
	pa->m_prefetchReady = false;
}

Bool SDLAudioManager::prepareSamplePrefetch(SDLPlayingAudio *pa)
{
	if (!pa || !pa->m_event || pa->m_prefetchReady)
		return false;
	/* Defer full-file decode to next frames when this update already spent budget. */
	if (!Audio_Budget_Soft_Ok())
		return false;

	AudioEventRTS *event = pa->m_event;
	const AudioEventInfo *info = event->getAudioEventInfo();
	if (!info)
		return false;

	AsciiString filename;
	const Int portion = (Int)event->getNextPlayPortion();
	if (pa->m_looping && portion == PP_Sound && (info->m_control & AC_LOOP)
		&& event->hasMoreLoops()) {
		/* Next Sound body (random). generateFilename now; advance must not re-roll. */
		event->generateFilename();
		filename = event->getFilename();
	} else if (portion == PP_Attack) {
		/* Attack → Sound uses the filename already chosen at play start. */
		filename = event->getFilename();
	} else {
		return false;
	}

	if (filename.isEmpty())
		return false;

	Uint8 *pcmData = NULL;
	Uint32 pcmSize = 0;
	int sampleRate = 0, channels = 0;
	if (!loadAndDecodeAudio(filename, pcmData, pcmSize, sampleRate, channels))
		return false;

	pa->m_prefetchData = pcmData;
	pa->m_prefetchSize = pcmSize;
	pa->m_prefetchRate = sampleRate;
	pa->m_prefetchChannels = channels;
	pa->m_prefetchReady = true;

	return true;
}

Bool SDLAudioManager::feedSampleFromPrefetch(SDLPlayingAudio *pa)
{
	if (!pa || !pa->m_prefetchReady || !pa->m_prefetchData)
		return false;

	Uint8 *pcmData = pa->m_prefetchData;
	Uint32 pcmSize = pa->m_prefetchSize;
	int sampleRate = pa->m_prefetchRate;
	int channels = pa->m_prefetchChannels;
	pa->m_prefetchData = NULL;
	pa->m_prefetchSize = 0;
	pa->m_prefetchReady = false;

	if (pa->m_pcmData) {
		free(pa->m_pcmData);
		pa->m_pcmData = NULL;
		pa->m_pcmSize = 0;
	}

	const Bool needNewStream = !pa->m_stream
		|| pa->m_sampleRate != sampleRate
		|| pa->m_channels != channels
		|| (pa->m_is3D && pa->m_streamChannels != 2)
		|| (!pa->m_is3D && pa->m_streamChannels != channels);
	if (needNewStream) {
		if (pa->m_stream) {
			SDL_DestroyAudioStream(pa->m_stream);
			pa->m_stream = NULL;
		}
		SDL_AudioSpec spec;
		SDL_zero(spec);
		spec.freq = sampleRate;
		spec.format = SDL_AUDIO_S16LE;
		const Int streamCh = pa->m_is3D ? 2 : channels;
		spec.channels = (Sint8)streamCh;
		pa->m_stream = SDL_OpenAudioDeviceStream(
			SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
		if (!pa->m_stream) {
			free(pcmData);
			return false;
		}
		pa->m_streamChannels = streamCh;
		Real pitch = pa->m_event ? pa->m_event->getPitchShift() : 1.0f;
		if (pitch > 0.01f && pitch != 1.0f)
			SDL_SetAudioStreamFrequencyRatio(pa->m_stream, (float)pitch);
		applyPlayingGain(pa);
	}

	pa->m_pcmData = pcmData;
	pa->m_pcmSize = pcmSize;
	pa->m_sampleRate = sampleRate;
	pa->m_channels = channels;
	pa->m_pcmOffset = 0;
	pa->m_lpState = 0.0f;
	pa->m_feedFlushed = false;
	pa->m_idleFrames = 0;
	pa->m_feedStartMs = timeGetTime();

	if (pa->m_stream) {
		const int q = SDL_GetAudioStreamQueued(pa->m_stream);
		const int a = SDL_GetAudioStreamAvailable(pa->m_stream);
		if (q > 0 || a > 0)
			SDL_ClearAudioStream(pa->m_stream);
	}

	if (pa->m_is3D) {
		pa->m_panSmoothInit = false;
		Mark_Sample_Play_End(pa, pcmSize, channels, sampleRate);
		feedSampleChunks(pa);
		SDL_ResumeAudioStreamDevice(pa->m_stream);
		pa->m_isPlaying = true;
		return true;
	}

	if (!SDL_PutAudioStreamData(pa->m_stream, pcmData, (int)pcmSize)) {
		free(pcmData);
		pa->m_pcmData = NULL;
		pa->m_pcmSize = 0;
		return false;
	}

	const Bool keepPcm = pa->m_looping && pa->m_event
		&& pa->m_event->getNextPlayPortion() == PP_Sound;
	if (!keepPcm) {
		free(pcmData);
		pa->m_pcmData = NULL;
		pa->m_pcmSize = 0;
	}

	Mark_Sample_Play_End(pa, pcmSize, channels, sampleRate);
	SDL_FlushAudioStream(pa->m_stream);
	pa->m_feedFlushed = true;
	SDL_ResumeAudioStreamDevice(pa->m_stream);
	pa->m_isPlaying = true;
	return true;
}

/*
 * Append prefetched PCM onto a still-playing stream (no Clear). Eliminates the
 * ~1-frame underrun when empty→refeed happens on the next audio tick.
 */
Bool SDLAudioManager::appendSamplePrefetch(SDLPlayingAudio *pa)
{
	if (!pa || !pa->m_prefetchReady || !pa->m_prefetchData || !pa->m_stream || !pa->m_event)
		return false;

	if (pa->m_is3D) {
		if (!Install_Prefetch_Mono(pa))
			return false;
		/* Keep smoothed pan across loop seam so L/R don't jump. */
		feedSampleChunks(pa);
		SDL_ResumeAudioStreamDevice(pa->m_stream);
		pa->m_isPlaying = true;
		return true;
	}

	AudioEventRTS *event = pa->m_event;
	const AudioEventInfo *info = event->getAudioEventInfo();
	const Int curPortion = (Int)event->getNextPlayPortion();

	if (pa->m_looping && curPortion == PP_Sound && info && (info->m_control & AC_LOOP)) {
		event->decreaseLoopCount();
		if (!event->hasMoreLoops()) {
			clearSamplePrefetch(pa);
			return false;
		}
		event->setNextPlayPortion(PP_Sound);
	} else if (curPortion == PP_Attack) {
		event->advanceNextPlayPortion();
		if (event->getNextPlayPortion() != PP_Sound) {
			clearSamplePrefetch(pa);
			return false;
		}
	} else {
		return false;
	}

	Uint8 *pcmData = pa->m_prefetchData;
	Uint32 pcmSize = pa->m_prefetchSize;
	int sampleRate = pa->m_prefetchRate;
	int channels = pa->m_prefetchChannels;
	pa->m_prefetchData = NULL;
	pa->m_prefetchSize = 0;
	pa->m_prefetchReady = false;

	/* Format change cannot append — fall back to stop+feed path. */
	if (pa->m_sampleRate != sampleRate || pa->m_channels != channels
		|| pa->m_streamChannels != channels) {
		pa->m_prefetchData = pcmData;
		pa->m_prefetchSize = pcmSize;
		pa->m_prefetchRate = sampleRate;
		pa->m_prefetchChannels = channels;
		pa->m_prefetchReady = true;
		return false;
	}

	const int q0 = SDL_GetAudioStreamQueued(pa->m_stream);
	const int a0 = SDL_GetAudioStreamAvailable(pa->m_stream);
	const int remainBefore = Stream_Remain_Ms(pa, q0, a0);

	const Int chLog = channels > 0 ? channels : 1;
	const int rateLog = sampleRate > 0 ? sampleRate : 22050;
	Real newDurMs = 1000.0f * (Real)(pcmSize / (Uint32)(2 * chLog)) / (Real)rateLog;
	Real pitch = event->getPitchShift();
	if (pitch > 0.01f)
		newDurMs /= pitch;

	static thread_local std::vector<Sint16> s_mono;
	const int nSamp = (int)(pcmSize / sizeof(Sint16));
	s_mono.resize((size_t)nSamp);
	memcpy(s_mono.data(), pcmData, pcmSize);
	Fade_In_S16(s_mono.data(), nSamp, channels > 0 ? channels : 1, sampleRate);
	if (!SDL_PutAudioStreamData(pa->m_stream, s_mono.data(), (int)(s_mono.size() * sizeof(Sint16)))) {
		free(pcmData);
		return false;
	}

	SDL_FlushAudioStream(pa->m_stream);
	pa->m_feedFlushed = true;
	pa->m_sampleRate = sampleRate;
	pa->m_channels = channels;
	pa->m_idleFrames = 0;
	pa->m_feedStartMs = timeGetTime();
	pa->m_playEndMs = pa->m_feedStartMs
		+ (UnsignedInt)(remainBefore + newDurMs + 20.0f);

	if (pa->m_pcmData) {
		free(pa->m_pcmData);
		pa->m_pcmData = NULL;
		pa->m_pcmSize = 0;
	}
	const Bool keepPcm = pa->m_looping && event->getNextPlayPortion() == PP_Sound;
	if (keepPcm) {
		pa->m_pcmData = pcmData;
		pa->m_pcmSize = pcmSize;
		pa->m_pcmOffset = pcmSize;
	} else {
		free(pcmData);
		pa->m_pcmOffset = 0;
	}

	SDL_ResumeAudioStreamDevice(pa->m_stream);
	pa->m_isPlaying = true;
	return true;
}

Bool SDLAudioManager::feedSamplePortion(SDLPlayingAudio *pa, Bool createStream)
{
	if (!pa || !pa->m_event)
		return false;

	clearSamplePrefetch(pa);

	AsciiString filename = filenameForPortion(pa->m_event);
	if (filename.isEmpty() && pa->m_event->getNextPlayPortion() == PP_Attack) {
		/* Empty Attack list → jump to Sound (Miles openFile would fail and drop). */
		pa->m_event->setNextPlayPortion(PP_Sound);
		filename = filenameForPortion(pa->m_event);
	}
	if (filename.isEmpty())
		return false;

	Uint8 *pcmData = NULL;
	Uint32 pcmSize = 0;
	int sampleRate = 0, channels = 0;
	if (!loadAndDecodeAudio(filename, pcmData, pcmSize, sampleRate, channels))
		return false;

	if (pa->m_pcmData) {
		free(pa->m_pcmData);
		pa->m_pcmData = NULL;
		pa->m_pcmSize = 0;
	}

	const Bool needNewStream = createStream || !pa->m_stream
		|| pa->m_sampleRate != sampleRate
		|| pa->m_channels != channels
		|| (pa->m_is3D && pa->m_streamChannels != 2)
		|| (!pa->m_is3D && pa->m_streamChannels != channels);
	if (needNewStream) {
		if (pa->m_stream) {
			SDL_DestroyAudioStream(pa->m_stream);
			pa->m_stream = NULL;
		}
		SDL_AudioSpec spec;
		SDL_zero(spec);
		spec.freq = sampleRate;
		spec.format = SDL_AUDIO_S16LE;
		/* 3D always stereo so we can bake constant-power L/R pan. */
		const Int streamCh = pa->m_is3D ? 2 : channels;
		spec.channels = (Sint8)streamCh;
		pa->m_stream = SDL_OpenAudioDeviceStream(
			SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
		if (!pa->m_stream) {
			free(pcmData);
			return false;
		}
		pa->m_streamChannels = streamCh;
		Real pitch = pa->m_event->getPitchShift();
		if (pitch > 0.01f && pitch != 1.0f) {
			SDL_SetAudioStreamFrequencyRatio(pa->m_stream, (float)pitch);
		}
		applyPlayingGain(pa);
	}

	pa->m_pcmData = pcmData;
	pa->m_pcmSize = pcmSize;
	pa->m_sampleRate = sampleRate;
	pa->m_channels = channels;
	pa->m_pcmOffset = 0;
	pa->m_lpState = 0.0f;
	pa->m_feedFlushed = false;
	pa->m_idleFrames = 0;
	pa->m_feedStartMs = timeGetTime();

	/*
	 * Clear only if the stream still holds unread data. Clearing an already-drained
	 * stream is fine, but Clear+Destroy paths above already handle format changes.
	 */
	if (pa->m_stream) {
		const int q = SDL_GetAudioStreamQueued(pa->m_stream);
		const int a = SDL_GetAudioStreamAvailable(pa->m_stream);
		if (q > 0 || a > 0)
			SDL_ClearAudioStream(pa->m_stream);
	}

	if (pa->m_is3D) {
		pa->m_panSmoothInit = false;
		Mark_Sample_Play_End(pa, pcmSize, channels, sampleRate);
		feedSampleChunks(pa);
		SDL_ResumeAudioStreamDevice(pa->m_stream);
		pa->m_isPlaying = true;
		return true;
	}

	if (!SDL_PutAudioStreamData(pa->m_stream, pcmData, (int)pcmSize)) {
		free(pcmData);
		pa->m_pcmData = NULL;
		pa->m_pcmSize = 0;
		return false;
	}

	/* One-shot portions don't need to keep PCM unless looping the Sound body. */
	const Bool keepPcm = pa->m_looping && pa->m_event->getNextPlayPortion() == PP_Sound;
	if (!keepPcm) {
		free(pcmData);
		pa->m_pcmData = NULL;
		pa->m_pcmSize = 0;
	}

	Mark_Sample_Play_End(pa, pcmSize, channels, sampleRate);
	SDL_FlushAudioStream(pa->m_stream);
	pa->m_feedFlushed = true;
	SDL_ResumeAudioStreamDevice(pa->m_stream);
	pa->m_isPlaying = true;
	return true;
}

Bool SDLAudioManager::advanceSampleAfterIdle(SDLPlayingAudio *pa)
{
	if (!pa || !pa->m_event)
		return false;

	AudioEventRTS *event = pa->m_event;
	const AudioEventInfo *info = event->getAudioEventInfo();

	/* Miles: on LOOP while in PP_Sound, decrease loop and replay Sound body. */
	if (pa->m_looping && event->getNextPlayPortion() == PP_Sound && info
		&& (info->m_control & AC_LOOP)) {
		event->decreaseLoopCount();
		if (event->hasMoreLoops()) {
			if (pa->m_prefetchReady) {
				event->setNextPlayPortion(PP_Sound);
				if (feedSampleFromPrefetch(pa))
					return true;
			}
			event->generateFilename();
			event->setNextPlayPortion(PP_Sound);
			if (feedSamplePortion(pa, false))
				return true;
		}
		clearSamplePrefetch(pa);
	}

	event->advanceNextPlayPortion();
	if (event->getNextPlayPortion() == PP_Done)
		return false;

	if (pa->m_prefetchReady && event->getNextPlayPortion() == PP_Sound) {
		if (feedSampleFromPrefetch(pa))
			return true;
	}

	if (feedSamplePortion(pa, false))
		return true;

	/* Skip empty/missing portions until Done or something plays. */
	while (event->getNextPlayPortion() != PP_Done) {
		event->advanceNextPlayPortion();
		if (event->getNextPlayPortion() == PP_Done)
			break;
		if (feedSamplePortion(pa, false))
			return true;
	}
	return false;
}

void SDLAudioManager::flushStreamDecoderTail(SDLPlayingAudio *pa)
{
	if (!pa || !pa->m_codecCtx || !pa->m_swr || !pa->m_stream || !pa->m_aframe)
		return;

	/* Drain codec after EOF (send NULL packet). */
	if (avcodec_send_packet(pa->m_codecCtx, NULL) >= 0) {
		while (avcodec_receive_frame(pa->m_codecCtx, pa->m_aframe) == 0) {
			int outSamples = swr_get_out_samples(pa->m_swr, pa->m_aframe->nb_samples);
			int bytesPerSample = 2 * m_targetChannels;
			if (outSamples < 1) outSamples = 1;
			std::vector<Uint8> pcm((size_t)outSamples * (size_t)bytesPerSample);
			uint8_t *outBuf[1] = { pcm.data() };
			int converted = swr_convert(pa->m_swr, outBuf, outSamples,
				(const uint8_t **)pa->m_aframe->data, pa->m_aframe->nb_samples);
			if (converted > 0)
				SDL_PutAudioStreamData(pa->m_stream, pcm.data(), converted * bytesPerSample);
			av_frame_unref(pa->m_aframe);
		}
	}

	/* Drain swr delay buffer. */
	{
		int outSamples = swr_get_out_samples(pa->m_swr, 0);
		if (outSamples < 256) outSamples = 256;
		int bytesPerSample = 2 * m_targetChannels;
		std::vector<Uint8> pcm((size_t)outSamples * (size_t)bytesPerSample);
		uint8_t *outBuf[1] = { pcm.data() };
		int converted = swr_convert(pa->m_swr, outBuf, outSamples, NULL, 0);
		if (converted > 0)
			SDL_PutAudioStreamData(pa->m_stream, pcm.data(), converted * bytesPerSample);
	}

	SDL_FlushAudioStream(pa->m_stream);
	pa->m_feedFlushed = true;
}

void SDLAudioManager::processRequestList()
{
	while (!m_audioRequests.empty()) {
		AudioRequest *req = m_audioRequests.front();
		m_audioRequests.pop_front();
		if (!req) continue;

		if (req->m_usePendingEvent && req->m_pendingEvent) {
			if (req->m_request == AR_Play) {
				AudioHandle kill = req->m_pendingEvent->getHandleToKill();
				if (kill)
					stopAudioEvent(kill);
				playAudioEvent(req->m_pendingEvent);
			}
		} else if (!req->m_usePendingEvent) {
			if (req->m_request == AR_Stop)
				stopAudioEvent(req->m_handleToInteractOn);
		}

		releaseAudioRequest(req);
	}
}

static void unlinkAudioFromList(SDLPlayingAudio *&list, SDLPlayingAudio *pa)
{
	if (!list || !pa) return;
	SDLPlayingAudio **pp = &list;
	while (*pp) {
		if (*pp == pa) {
			*pp = pa->m_next;
			return;
		}
		pp = &(*pp)->m_next;
	}
}

void SDLAudioManager::processPlayingList()
{
	SDLPlayingAudio *pa = m_playingList;
	while (pa) {
		SDLPlayingAudio *next = pa->m_next;

		if (pa->m_shouldStop) {
			pa->m_isPlaying = false;
			if (pa->m_stream) {
				SDL_ClearAudioStream(pa->m_stream);
				SDL_DestroyAudioStream(pa->m_stream);
				pa->m_stream = NULL;
			}
			unlinkAudioFromList(m_playingList, pa);
			pa->m_next = m_stoppedList;
			m_stoppedList = pa;
			pa = next;
			continue;
		}

		/* 3D samples need per-frame distance + stereo pan updates.
		   2D/streams only need a refresh when the global volume sliders change.
		   Fading music owns its gain via processFadingMusic. */
		if (pa->m_stream && pa->m_event && !pa->m_isFading && (pa->m_is3D || m_volumeHasChanged)) {
			if (pa->m_is3D && pa->m_type == SPT_Sample) {
				if (pa->m_event->isDead()) {
					pa->m_shouldStop = true;
					pa = next;
					continue;
				}
				Real gain = calcPlayingGain(pa);
				Bool playAnyways = BitTest(pa->m_event->getAudioEventInfo()->m_type, ST_GLOBAL)
					|| pa->m_event->getAudioEventInfo()->m_priority == AP_CRITICAL;
				/*
				 * Miles culls when effective volume < MinSampleVolume. Keeping
				 * out-of-range AmbientLoops alive at gain 0 (previous experiment)
				 * left Chinook etc. silently occupying Limit slots forever
				 * (logs: dist 650 > MaxRange 600, gain 0.000 every tick).
				 */
				if (gain < getAudioSettings()->m_minVolume && !playAnyways) {
					pa->m_shouldStop = true;
					pa = next;
					continue;
				}
				SDL_SetAudioStreamGain(pa->m_stream, gain);
				feedSampleChunks(pa);
			} else {
				applyPlayingGain(pa);
			}
		}

		if (pa->m_type == SPT_Sample && pa->m_stream && pa->m_isPlaying) {
			const UnsignedInt nowMs = timeGetTime();
			int queued = SDL_GetAudioStreamQueued(pa->m_stream);
			int available = SDL_GetAudioStreamAvailable(pa->m_stream);
			Bool shouldAdvance = false;

			const Bool drained = (queued <= 0 && available <= 0 && pa->m_feedFlushed);
			const int remainQMs = Stream_Remain_Ms(pa, queued, available);
			const int clockRemain = pa->m_playEndMs ? (int)(pa->m_playEndMs - nowMs) : 9999;
			int srcRemainMs = 0;
			if (pa->m_is3D && pa->m_pcmData && pa->m_pcmSize > pa->m_pcmOffset) {
				const Int ch = pa->m_channels > 0 ? pa->m_channels : 1;
				const int rate = pa->m_sampleRate > 0 ? pa->m_sampleRate : 22050;
				const Uint32 framesLeft = (pa->m_pcmSize - pa->m_pcmOffset) / (Uint32)(2 * ch);
				Real pitch = pa->m_event ? pa->m_event->getPitchShift() : 1.0f;
				if (pitch < 0.01f) pitch = 1.0f;
				srcRemainMs = (int)(1000.0f * (Real)framesLeft / ((Real)rate * pitch));
			}
			const int remainMs = (remainQMs > 0 && clockRemain > 0)
				? ((remainQMs < clockRemain) ? remainQMs : clockRemain)
				: ((remainQMs > 0) ? remainQMs : ((clockRemain > 0) ? clockRemain : 0));

			if (!pa->m_prefetchReady) {
				if ((remainMs > 0 && remainMs <= 400)
					|| (clockRemain > 0 && clockRemain <= 400)
					|| (srcRemainMs > 0 && srcRemainMs <= 400))
					prepareSamplePrefetch(pa);
			}

			/* 2D gapless append only — 3D installs at source end inside feedSampleChunks. */
			if (pa->m_prefetchReady && !drained && remainMs > 0 && remainMs <= 160
				&& !pa->m_is3D) {
				if (appendSamplePrefetch(pa)) {
					pa = next;
					continue;
				}
			}

			if (drained) {
				shouldAdvance = true;
			} else if (pa->m_playEndMs != 0 && nowMs >= pa->m_playEndMs) {
				if (pa->m_is3D && (!pa->m_feedFlushed || remainQMs > 0))
					shouldAdvance = false;
				else
					shouldAdvance = true;
			} else {
				pa->m_idleFrames = 0;
			}

			if (shouldAdvance) {
				const UnsignedInt advT0 = timeGetTime();
				if (advanceSampleAfterIdle(pa)) {
					pa->m_idleFrames = 0;
				} else {
					pa->m_isPlaying = false;
					if (pa->m_stream) {
						SDL_DestroyAudioStream(pa->m_stream);
						pa->m_stream = NULL;
					}
					unlinkAudioFromList(m_playingList, pa);
					pa->m_next = m_stoppedList;
					m_stoppedList = pa;
				}
			}
		}

		pa = next;
	}

	if (m_volumeHasChanged)
		m_volumeHasChanged = false;
}

void SDLAudioManager::processStoppedList()
{
	SDLPlayingAudio *pa = m_stoppedList;
	while (pa) {
		SDLPlayingAudio *next = pa->m_next;
		releasePlayingAudio(pa);
		pa = next;
	}
	m_stoppedList = NULL;
}

void SDLAudioManager::playAudioEvent(AudioEventRTS *event)
{
	if (!event) return;

	const AudioEventInfo *info = event->getAudioEventInfo();
	if (!info) return;

	SDLPlayingAudio *pa = allocatePlayingAudio();
	if (!pa) return;

	pa->m_event = event;
	pa->m_handle = event->getPlayingHandle();
	pa->m_baseVolume = event->getVolume();
	pa->m_volume = 1.0f;

	const Coord3D *pos = event->getCurrentPosition();
	if (pos) {
		pa->m_is3D = event->isPositionalAudio();
		pa->m_ownerType = (int)event->getOwnerType();
		if (pa->m_ownerType == 2)
			pa->m_ownerID = event->getObjectID();
		else if (pa->m_ownerType == 1)
			pa->m_ownerID = event->getDrawableID();
	}

	if (info->m_soundType == AT_Music || info->m_soundType == AT_Streaming) {
		pa->m_type = SPT_Stream;
		/* Miles forces infinite loop for music regardless of Control flags. */
		if (info->m_soundType == AT_Music) {
			pa->m_looping = true;
			pa->m_loopCount = -1;
		} else {
			pa->m_looping = (info->m_control & AC_LOOP) != 0;
			pa->m_loopCount = pa->m_looping ? -1 : 0;
		}
	}

	pa->m_next = m_playingList;
	m_playingList = pa;

	if (pa->m_type == SPT_Stream) {
		Bool opened = openStreamForMusic(event);
		if (opened) {
			if (info->m_soundType == AT_Music) {
				m_musicPlaying = true;
				m_currentTrackName = event->getEventName();
				m_musicCompletedCount = 0;
			}
		} else {
			unlinkAudioFromList(m_playingList, pa);
			releasePlayingAudio(pa);
		}
		return;
	}

	pa->m_type = SPT_Sample;
	pa->m_looping = (info->m_control & AC_LOOP) != 0;
	pa->m_loopCount = pa->m_looping ? -1 : 0;

	if (!feedSamplePortion(pa, true)) {
		unlinkAudioFromList(m_playingList, pa);
		releasePlayingAudio(pa);
		return;
	}

	if (m_sound) {
		if (pa->m_is3D)
			m_sound->notifyOf3DSampleStart();
		else
			m_sound->notifyOf2DSampleStart();
		pa->m_countedSampleChannel = true;
	}
}

void SDLAudioManager::stopAudioEvent(AudioHandle handle)
{
	if (handle == AHSV_StopTheMusic || handle == AHSV_StopTheMusicFade) {
		const Bool doFade = (handle == AHSV_StopTheMusicFade);
		SDLPlayingAudio *pa = m_playingList;
		while (pa) {
			if (pa->m_type == SPT_Stream && sdlPlayingIsMusic(pa) && !pa->m_shouldStop) {
				if (doFade && pa->m_isPlaying && pa->m_stream) {
					/* Miles moves the track to m_fadingAudio instead of killing it.
					 * Keep looping so the stream survives the fade window. */
					if (!pa->m_isFading) {
						pa->m_isFading = true;
						pa->m_framesFaded = 0;
					}
				} else {
					pa->m_shouldStop = true;
					pa->m_isFading = false;
				}
			}
			pa = pa->m_next;
		}
		m_musicPlaying = false;
		return;
	}

	SDLPlayingAudio *pa = findPlayingAudio(handle);
	if (pa)
		pa->m_shouldStop = true;
}

void SDLAudioManager::processFadingMusic()
{
	const AudioSettings *settings = getAudioSettings();
	const Int fadeFrames = settings && settings->m_fadeAudioFrames > 0
		? settings->m_fadeAudioFrames : 1;

	SDLPlayingAudio *pa = m_playingList;
	while (pa) {
		SDLPlayingAudio *next = pa->m_next;
		if (pa->m_isFading && pa->m_stream && !pa->m_shouldStop) {
			++pa->m_framesFaded;
			if (pa->m_framesFaded >= fadeFrames) {
				pa->m_shouldStop = true;
				pa->m_isFading = false;
			} else {
				Real volume = calcPlayingGain(pa);
				volume *= (1.0f - (Real)pa->m_framesFaded / (Real)fadeFrames);
				if (volume < 0.0f) volume = 0.0f;
				SDL_SetAudioStreamGain(pa->m_stream, volume);
			}
		}
		pa = next;
	}
}

Bool SDLAudioManager::loadAndDecodeAudio(const AsciiString& filename,
	Uint8 *&outData, Uint32 &outSize, Int &outSampleRate, Int &outChannels)
{
	outData = NULL;
	outSize = 0;
	outSampleRate = 0;
	outChannels = 0;

	if (filename.isEmpty()) return false;

	if (Pcm_Cache_Lookup(filename, outData, outSize, outSampleRate, outChannels))
		return true;

	File *fp = TheFileSystem->openFile(filename.str(), File::READ);
	if (!fp) return false;

	Int fileSize = fp->size();
	char *fileData = fp->readEntireAndClose();
	if (!fileData) return false;

	MemAudioIO memIO;
	memIO.data = fileData;
	memIO.size = fileSize;
	memIO.pos = 0;

	AVFormatContext *fmt = NULL;
	AVIOContext *avio = NULL;
	unsigned char *avioBuf = (unsigned char *)av_malloc(4096);
	if (!avioBuf) { delete[] fileData; return false; }

	avio = avio_alloc_context(avioBuf, 4096, 0, &memIO, mem_audio_read, NULL, mem_audio_seek);
	if (!avio) { av_free(avioBuf); delete[] fileData; return false; }

	fmt = avformat_alloc_context();
	if (!fmt) { avio_context_free(&avio); delete[] fileData; return false; }

	fmt->pb = avio;
	fmt->flags |= AVFMT_FLAG_CUSTOM_IO;

	if (!mem_audio_open_input(&fmt, &memIO, filename.str())) {
		avformat_close_input(&fmt);
		delete[] fileData;
		return false;
	}

	if (avformat_find_stream_info(fmt, NULL) < 0) {
		avformat_close_input(&fmt);
		delete[] fileData;
		return false;
	}

	int audioStream = -1;
	for (unsigned i = 0; i < fmt->nb_streams; i++) {
		if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			audioStream = (int)i;
			break;
		}
	}

	if (audioStream < 0) {
		avformat_close_input(&fmt);
		delete[] fileData;
		return false;
	}

	AVCodecParameters *params = fmt->streams[audioStream]->codecpar;
	const AVCodec *codec = avcodec_find_decoder(params->codec_id);
	if (!codec) {
		avformat_close_input(&fmt);
		delete[] fileData;
		return false;
	}

	AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
	if (!codecCtx || avcodec_parameters_to_context(codecCtx, params) < 0) {
		if (codecCtx) avcodec_free_context(&codecCtx);
		avformat_close_input(&fmt);
		delete[] fileData;
		return false;
	}

	if (avcodec_open2(codecCtx, codec, NULL) < 0) {
		avcodec_free_context(&codecCtx);
		avformat_close_input(&fmt);
		delete[] fileData;
		return false;
	}

	outSampleRate = audio_resolve_sample_rate(params, codecCtx, NULL);
	outChannels = codecCtx->ch_layout.nb_channels;
	if (outChannels <= 0)
		outChannels = params->ch_layout.nb_channels;
	if (outChannels <= 0)
		outChannels = 1;

	SwrContext *swr = NULL;
	if (outSampleRate > 0)
		swr = audio_create_swr(params, codecCtx, NULL, codec, outSampleRate, outChannels);

	AVFrame *frame = av_frame_alloc();
	AVPacket *pkt = av_packet_alloc();
	if (!frame || !pkt) {
		if (frame) av_frame_free(&frame);
		if (pkt) av_packet_free(&pkt);
		swr_free(&swr);
		avcodec_free_context(&codecCtx);
		avformat_close_input(&fmt);
		delete[] fileData;
		return false;
	}

	std::vector<Uint8> allPcm;
	allPcm.reserve(1024 * 1024);

	while (av_read_frame(fmt, pkt) >= 0) {
		if (pkt->stream_index == audioStream) {
			if (avcodec_send_packet(codecCtx, pkt) >= 0) {
				while (avcodec_receive_frame(codecCtx, frame) == 0) {
					if (!swr) {
						swr = audio_create_swr(params, codecCtx, frame, codec, 0, outChannels);
						if (!swr) {
							av_frame_unref(frame);
							continue;
						}
						if (outSampleRate <= 0)
							outSampleRate = audio_resolve_sample_rate(params, codecCtx, frame);
					}
					int outSamples = swr_get_out_samples(swr, frame->nb_samples);
					size_t oldSize = allPcm.size();
					size_t needed = oldSize + (size_t)outSamples * (size_t)outChannels * 2;
					allPcm.resize(needed);

					uint8_t *outBuf[1] = { allPcm.data() + oldSize };
					int converted = swr_convert(swr, outBuf, outSamples,
						(const uint8_t **)frame->data, frame->nb_samples);
					if (converted > 0)
						allPcm.resize(oldSize + (size_t)converted * (size_t)outChannels * 2);
					else
						allPcm.resize(oldSize);
					av_frame_unref(frame);
				}
			}
		}
		av_packet_unref(pkt);
	}

	avcodec_send_packet(codecCtx, NULL);
	while (avcodec_receive_frame(codecCtx, frame) == 0) {
		if (!swr) {
			swr = audio_create_swr(params, codecCtx, frame, codec, 0, outChannels);
			if (!swr) {
				av_frame_unref(frame);
				continue;
			}
			if (outSampleRate <= 0)
				outSampleRate = audio_resolve_sample_rate(params, codecCtx, frame);
		}
		int outSamples = swr_get_out_samples(swr, frame->nb_samples);
		size_t oldSize = allPcm.size();
		size_t needed = oldSize + (size_t)outSamples * (size_t)outChannels * 2;
		allPcm.resize(needed);

		uint8_t *outBuf[1] = { allPcm.data() + oldSize };
		int converted = swr_convert(swr, outBuf, outSamples,
			(const uint8_t **)frame->data, frame->nb_samples);
		if (converted > 0)
			allPcm.resize(oldSize + (size_t)converted * (size_t)outChannels * 2);
		else
			allPcm.resize(oldSize);
		av_frame_unref(frame);
	}

	/* Flush swr delay so the last resampled frames are not dropped. */
	if (swr) {
		int outSamples = swr_get_out_samples(swr, 0);
		if (outSamples < 256) outSamples = 256;
		size_t oldSize = allPcm.size();
		allPcm.resize(oldSize + (size_t)outSamples * (size_t)outChannels * 2);
		uint8_t *outBuf[1] = { allPcm.data() + oldSize };
		int converted = swr_convert(swr, outBuf, outSamples, NULL, 0);
		if (converted > 0)
			allPcm.resize(oldSize + (size_t)converted * (size_t)outChannels * 2);
		else
			allPcm.resize(oldSize);
	}

	av_frame_free(&frame);
	av_packet_free(&pkt);
	swr_free(&swr);
	avcodec_free_context(&codecCtx);
	avformat_close_input(&fmt);
	delete[] fileData;

	if (allPcm.empty()) return false;
	if (outSampleRate <= 0)
		outSampleRate = 44100;

	outSize = (Uint32)allPcm.size();
	outData = (Uint8 *)malloc(outSize);
	if (!outData) return false;

	memcpy(outData, allPcm.data(), outSize);
	Pcm_Cache_Store(filename, outData, outSize, outSampleRate, outChannels);
	return true;
}

Bool SDLAudioManager::openStreamForMusic(AudioEventRTS *event)
{
	if (!event) return false;

	AsciiString filename = event->getFilename();
	if (filename.isEmpty()) return false;

	SDLPlayingAudio *pa = findPlayingAudio(event->getPlayingHandle());
	if (!pa) return false;

	File *fp = TheFileSystem->openFile(filename.str(), File::READ);
	if (!fp) return false;

	Int fileSize = fp->size();
	char *fileData = fp->readEntireAndClose();
	if (!fileData) return false;

	MemAudioIO *memIO = &pa->m_memIO;
	memIO->data = fileData;
	memIO->size = fileSize;
	memIO->pos = 0;

	AVFormatContext *fmt = NULL;
	AVIOContext *avio = NULL;
	unsigned char *avioBuf = (unsigned char *)av_malloc(65536);
	if (!avioBuf) { delete[] fileData; return false; }

	avio = avio_alloc_context(avioBuf, 65536, 0, memIO, mem_audio_read, NULL, mem_audio_seek);
	if (!avio) { av_free(avioBuf); delete[] fileData; return false; }

	fmt = avformat_alloc_context();
	if (!fmt) { avio_context_free(&avio); delete[] fileData; return false; }

	fmt->pb = avio;
	fmt->flags |= AVFMT_FLAG_CUSTOM_IO;

	if (!mem_audio_open_input(&fmt, memIO, filename.str())) {
		avformat_close_input(&fmt);
		delete[] fileData;
		return false;
	}

	if (avformat_find_stream_info(fmt, NULL) < 0) {
		avformat_close_input(&fmt);
		delete[] fileData;
		return false;
	}

	int audioStream = -1;
	for (unsigned i = 0; i < fmt->nb_streams; i++) {
		if (fmt->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			audioStream = (int)i;
			break;
		}
	}

	if (audioStream < 0) {
		avformat_close_input(&fmt);
		delete[] fileData;
		return false;
	}

	AVCodecParameters *params = fmt->streams[audioStream]->codecpar;
	const AVCodec *codec = avcodec_find_decoder(params->codec_id);
	if (!codec) {
		avformat_close_input(&fmt);
		delete[] fileData;
		return false;
	}

	AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
	if (!codecCtx || avcodec_parameters_to_context(codecCtx, params) < 0) {
		if (codecCtx) avcodec_free_context(&codecCtx);
		avformat_close_input(&fmt);
		delete[] fileData;
		return false;
	}

	if (avcodec_open2(codecCtx, codec, NULL) < 0) {
		avcodec_free_context(&codecCtx);
		avformat_close_input(&fmt);
		delete[] fileData;
		return false;
	}

	SwrContext *swr = NULL;
	if (audio_resolve_sample_rate(params, codecCtx, NULL) > 0) {
		swr = audio_create_swr(params, codecCtx, NULL, codec,
			m_targetSampleRate, m_targetChannels);
	}

	pa->m_formatCtx = fmt;
	pa->m_codecCtx = codecCtx;
	pa->m_swr = swr;
	pa->m_avStreamIndex = audioStream;
	pa->m_pkt = av_packet_alloc();
	pa->m_aframe = av_frame_alloc();
	pa->m_streamEOF = false;
	pa->m_sampleRate = audio_resolve_sample_rate(params, codecCtx, NULL);
	if (pa->m_sampleRate <= 0)
		pa->m_sampleRate = m_targetSampleRate;

	if (!pa->m_pkt || !pa->m_aframe) {
		if (pa->m_pkt) av_packet_free(&pa->m_pkt);
		if (pa->m_aframe) av_frame_free(&pa->m_aframe);
		swr_free(&swr);
		avcodec_free_context(&codecCtx);
		avformat_close_input(&fmt);
		delete[] fileData;
		return false;
	}

	SDL_AudioSpec streamSpec;
	SDL_zero(streamSpec);
	streamSpec.freq = m_targetSampleRate;
	streamSpec.format = SDL_AUDIO_S16LE;
	streamSpec.channels = (Sint8)m_targetChannels;

	pa->m_stream = SDL_OpenAudioDeviceStream(
		SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &streamSpec, NULL, NULL);
	if (!pa->m_stream) {
		av_packet_free(&pa->m_pkt);
		av_frame_free(&pa->m_aframe);
		swr_free(&swr);
		avcodec_free_context(&codecCtx);
		avformat_close_input(&fmt);
		delete[] fileData;
		return false;
	}

	pa->m_rawFileData = fileData;
	pa->m_rawFileSize = fileSize;

	Real affectVol = sdlPlayingIsMusic(pa) ? getVolume(AudioAffect_Music) : getVolume(AudioAffect_Speech);
	Real vol = pa->m_baseVolume * affectVol;
	if (vol < 0.0f) vol = 0.0f;
	if (vol > 1.0f) vol = 1.0f;
	SDL_SetAudioStreamGain(pa->m_stream, vol);

	pushStreamData(pa);
	SDL_ResumeAudioStreamDevice(pa->m_stream);
	pa->m_isPlaying = true;

	return true;
}

void SDLAudioManager::pushStreamData(SDLPlayingAudio *pa)
{
	if (!pa || !pa->m_formatCtx || !pa->m_codecCtx)
		return;

	/* Allow re-entry after EOF so looping music can seek and refill. */
	if (pa->m_streamEOF) {
		if (!(pa->m_looping && pa->m_loopCount != 0))
			return;
		if (sdlPlayingIsMusic(pa))
			m_musicCompletedCount++;
		if (pa->m_loopCount > 0)
			pa->m_loopCount--;
		if (av_seek_frame(pa->m_formatCtx, -1, 0, AVSEEK_FLAG_BACKWARD) < 0) {
			if (pa->m_formatCtx->pb)
				avio_seek(pa->m_formatCtx->pb, 0, SEEK_SET);
		}
		avcodec_flush_buffers(pa->m_codecCtx);
		pa->m_memIO.pos = 0;
		pa->m_streamEOF = false;
	}

	/*
	 * Soft refill: decode a small number of packets per frame.
	 * Keep ~1s cushion; only dig deeper when the queue is critically low
	 * (map-load stalls / first fill). Cap work with the frame audio budget.
	 */
	static thread_local std::vector<Uint8> s_pcmScratch;
	const int queuedNow = pa->m_stream ? SDL_GetAudioStreamQueued(pa->m_stream) : 0;
	const int targetQueued = (queuedNow < 48000) ? 288000 : 192000; /* ~1.5s / ~1.0s */
	int packetsToRead = 8;
	if (queuedNow < 48000)
		packetsToRead = 24;
	else if (queuedNow < 144000)
		packetsToRead = 12;
	Bool hitEof = false;
	while (packetsToRead > 0 && !pa->m_streamEOF) {
		if (pa->m_stream && SDL_GetAudioStreamQueued(pa->m_stream) >= targetQueued)
			break;
		/* Never starve a near-empty queue, but stop early once healthy. */
		if (queuedNow >= 48000 && !Audio_Budget_Hard_Ok())
			break;

		int ret = av_read_frame(pa->m_formatCtx, pa->m_pkt);
		if (ret < 0) {
			hitEof = true;
			pa->m_streamEOF = true;
			break;
		}

		if (pa->m_pkt->stream_index == pa->m_avStreamIndex) {
			if (avcodec_send_packet(pa->m_codecCtx, pa->m_pkt) >= 0) {
				while (avcodec_receive_frame(pa->m_codecCtx, pa->m_aframe) == 0) {
					if (!pa->m_swr) {
						AVCodecParameters *params =
							pa->m_formatCtx->streams[pa->m_avStreamIndex]->codecpar;
						pa->m_swr = audio_create_swr(params, pa->m_codecCtx, pa->m_aframe, NULL,
							m_targetSampleRate, m_targetChannels);
						if (!pa->m_swr) {
							av_frame_unref(pa->m_aframe);
							continue;
						}
						const int resolved = audio_resolve_sample_rate(params, pa->m_codecCtx, pa->m_aframe);
						if (resolved > 0)
							pa->m_sampleRate = resolved;
					}
					int outSamples = swr_get_out_samples(pa->m_swr, pa->m_aframe->nb_samples);
					int bytesPerSample = 2 * m_targetChannels;
					const size_t need = (size_t)outSamples * (size_t)bytesPerSample;
					if (s_pcmScratch.size() < need)
						s_pcmScratch.resize(need);

					uint8_t *outBuf[1] = { s_pcmScratch.data() };
					int converted = swr_convert(pa->m_swr, outBuf, outSamples,
						(const uint8_t **)pa->m_aframe->data, pa->m_aframe->nb_samples);
					if (converted > 0) {
						int bytes = converted * bytesPerSample;
						SDL_PutAudioStreamData(pa->m_stream, s_pcmScratch.data(), bytes);
					}
					av_frame_unref(pa->m_aframe);
				}
			}
		}
		av_packet_unref(pa->m_pkt);
		packetsToRead--;
	}

	if (hitEof)
		flushStreamDecoderTail(pa);
}

void SDLAudioManager::updateStreaming()
{
	SDLPlayingAudio *pa = m_playingList;
	while (pa) {
		if (pa->m_type == SPT_Stream && pa->m_isPlaying && pa->m_stream) {
			int queued = SDL_GetAudioStreamQueued(pa->m_stream);
			/* Refill before underrun — keep ~0.75s cushion under load. */
			if (queued < 144000 && !pa->m_streamEOF) {
				pushStreamData(pa);
			}

			/* Count a completed pass only when playback reaches the loop boundary,
			 * then seek/refill. Miles reports completion from playback, not decode. */
			if (pa->m_streamEOF && queued <= 4096 && pa->m_looping && pa->m_loopCount != 0) {
				pushStreamData(pa);
			}

			if (pa->m_streamEOF && !pa->m_isFading) {
				queued = SDL_GetAudioStreamQueued(pa->m_stream);
				int available = SDL_GetAudioStreamAvailable(pa->m_stream);
				if (queued <= 0 && available <= 0) {
					pa->m_idleFrames++;
				} else {
					pa->m_idleFrames = 0;
				}
				if (pa->m_idleFrames >= 8) {
					pa->m_isPlaying = false;
					pa->m_shouldStop = true;
					if (sdlPlayingIsMusic(pa)) {
						m_musicPlaying = false;
					}
				}
			}
		}
		pa = pa->m_next;
	}
}

void SDLAudioManager::clearRequests()
{
	while (!m_audioRequests.empty()) {
		AudioRequest *req = m_audioRequests.front();
		m_audioRequests.pop_front();
		releaseAudioRequest(req);
	}
}

void SDLAudioManager::stopAllAudio()
{

	SDLPlayingAudio *pa = m_playingList;
	while (pa) {
		pa->m_shouldStop = true;
		if (pa->m_stream) {
			SDL_ClearAudioStream(pa->m_stream);
			SDL_DestroyAudioStream(pa->m_stream);
			pa->m_stream = NULL;
		}
		pa->m_isPlaying = false;
		pa = pa->m_next;
	}

	while (m_playingList) {
		SDLPlayingAudio *next = m_playingList->m_next;
		releasePlayingAudio(m_playingList);
		m_playingList = next;
	}

	while (m_stoppedList) {
		SDLPlayingAudio *next = m_stoppedList->m_next;
		releasePlayingAudio(m_stoppedList);
		m_stoppedList = next;
	}

	m_binkHandle = NULL;
	clearRequests();

}

SDLPlayingAudio *SDLAudioManager::allocatePlayingAudio()
{
	return new SDLPlayingAudio();
}

void SDLAudioManager::releasePlayingAudio(SDLPlayingAudio *pa)
{
	if (!pa) return;

	if (pa->m_countedSampleChannel && m_sound) {
		if (pa->m_is3D)
			m_sound->notifyOf3DSampleCompletion();
		else
			m_sound->notifyOf2DSampleCompletion();
		pa->m_countedSampleChannel = false;
	}

	if (pa->m_stream) {
		SDL_DestroyAudioStream(pa->m_stream);
		pa->m_stream = NULL;
	}

	if (pa->m_pcmData) {
		free(pa->m_pcmData);
		pa->m_pcmData = NULL;
	}

	clearSamplePrefetch(pa);

	if (pa->m_formatCtx)
		avformat_close_input(&pa->m_formatCtx);
	if (pa->m_codecCtx)
		avcodec_free_context(&pa->m_codecCtx);
	if (pa->m_swr)
		swr_free(&pa->m_swr);
	if (pa->m_pkt)
		av_packet_free(&pa->m_pkt);
	if (pa->m_aframe)
		av_frame_free(&pa->m_aframe);

	if (pa->m_rawFileData) {
		delete[] pa->m_rawFileData;
		pa->m_rawFileData = NULL;
	}

	if (pa->m_event)
		releaseAudioEventRTS(pa->m_event);

	delete pa;
}

SDLPlayingAudio *SDLAudioManager::findPlayingAudio(AudioHandle handle)
{
	SDLPlayingAudio *pa = m_playingList;
	while (pa) {
		if (pa->m_handle == handle)
			return pa;
		pa = pa->m_next;
	}
	return NULL;
}
