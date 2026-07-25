#pragma once
#ifndef SDL_AUDIO_MANAGER_H
#define SDL_AUDIO_MANAGER_H

#include "Common/GameAudio.h"
#include <SDL3/SDL.h>

struct AVFormatContext;
struct AVCodecContext;
struct SwrContext;
struct AVFrame;
struct AVPacket;

struct MemAudioIO {
	const char *data;
	int size;
	int pos;
};

enum SDLPlayingType
{
	SPT_Sample,
	SPT_Stream,
	SPT_INVALID
};

struct SDLPlayingAudio
{
	SDL_AudioStream *m_stream;
	AudioEventRTS *m_event;
	AudioHandle m_handle;
	SDLPlayingType m_type;
	volatile Bool m_isPlaying;
	Bool m_shouldStop;
	Bool m_isFading;
	Int m_framesFaded;

	Uint8 *m_pcmData;
	Uint32 m_pcmSize;
	Int m_sampleRate;
	Int m_channels;
	Bool m_looping;
	Int m_loopCount;

	AVFormatContext *m_formatCtx;
	AVCodecContext *m_codecCtx;
	SwrContext *m_swr;
	int m_avStreamIndex;
	AVPacket *m_pkt;
	AVFrame *m_aframe;
	Bool m_streamEOF;
	MemAudioIO m_memIO;
	char *m_rawFileData;
	int m_rawFileSize;

	Bool m_is3D;
	Int m_ownerType;
	UnsignedInt m_ownerID;
	Real m_volume;
	Real m_baseVolume;
	int m_idleFrames;
	Bool m_countedSampleChannel;
	Bool m_feedFlushed; /* SDL_FlushAudioStream after last Put */
	Uint32 m_pcmOffset; /* chunked 3D feed cursor into m_pcmData */
	Int m_streamChannels; /* SDL stream channel count (2 for 3D stereo) */
	Real m_lpState; /* one-pole LP state for off-screen filter */
	UnsignedInt m_playEndMs; /* wall-clock when current PCM should finish */
	UnsignedInt m_feedStartMs; /* when current portion was queued */
	Real m_panSmooth; /* smoothed stereo pan for live chunks [-1,1] */
	Bool m_panSmoothInit;
	/* Next portion decoded while current still plays (kills loop-seam silence). */
	Uint8 *m_prefetchData;
	Uint32 m_prefetchSize;
	Int m_prefetchRate;
	Int m_prefetchChannels;
	Bool m_prefetchReady;

	SDLPlayingAudio *m_next;

	SDLPlayingAudio() :
		m_stream(NULL), m_event(NULL), m_handle(0), m_type(SPT_INVALID),
		m_isPlaying(false), m_shouldStop(false),
		m_isFading(false), m_framesFaded(0),
		m_pcmData(NULL), m_pcmSize(0),
		m_sampleRate(0), m_channels(0), m_looping(false), m_loopCount(0),
		m_formatCtx(NULL), m_codecCtx(NULL), m_swr(NULL),
		m_avStreamIndex(-1), m_pkt(NULL), m_aframe(NULL), m_streamEOF(false),
		m_rawFileData(NULL), m_rawFileSize(0),
		m_is3D(false), m_ownerType(0), m_ownerID(0),
		m_volume(1.0f), m_baseVolume(1.0f), m_idleFrames(0),
		m_countedSampleChannel(false), m_feedFlushed(false),
		m_pcmOffset(0), m_streamChannels(0), m_lpState(0.0f),
		m_playEndMs(0), m_feedStartMs(0),
		m_panSmooth(0.0f), m_panSmoothInit(false),
		m_prefetchData(NULL), m_prefetchSize(0),
		m_prefetchRate(0), m_prefetchChannels(0), m_prefetchReady(false),
		m_next(NULL)
	{}
};

class SDLAudioManager : public AudioManager
{
public:
	SDLAudioManager();
	virtual ~SDLAudioManager();

	virtual void init();
	virtual void postProcessLoad();
	virtual void reset();
	virtual void update();

	virtual void openDevice();
	virtual void closeDevice();
	virtual void *getDevice() { return NULL; }

	virtual void stopAudio(AudioAffect which);
	virtual void pauseAudio(AudioAffect which);
	virtual void resumeAudio(AudioAffect which);
	virtual void pauseAmbient(Bool shouldPause);

	virtual void stopAllAmbientsBy(Object *obj);
	virtual void stopAllAmbientsBy(Drawable *draw);

	virtual void killAudioEventImmediately(AudioHandle audioEvent);

	virtual void nextMusicTrack();
	virtual void prevMusicTrack();
	virtual Bool isMusicPlaying() const;
	virtual Bool hasMusicTrackCompleted(const AsciiString& trackName, Int numberOfTimes) const;
	virtual AsciiString getMusicTrackName() const;
	virtual Bool isCurrentlyPlaying(AudioHandle handle);

	virtual void notifyOfAudioCompletion(UnsignedInt audioCompleted, UnsignedInt flags);

	virtual UnsignedInt getProviderCount() const { return 1; }
	virtual AsciiString getProviderName(UnsignedInt providerNum) const;
	virtual UnsignedInt getProviderIndex(AsciiString providerName) const;
	virtual void selectProvider(UnsignedInt providerNdx);
	virtual void unselectProvider();
	virtual UnsignedInt getSelectedProvider() const;
	virtual void setSpeakerType(UnsignedInt speakerType);
	virtual UnsignedInt getSpeakerType();

	virtual void *getHandleForBink();
	virtual void releaseHandleForBink();

	virtual void friend_forcePlayAudioEventRTS(const AudioEventRTS *eventToPlay);

	virtual UnsignedInt getNum2DSamples() const;
	virtual UnsignedInt getNum3DSamples() const;
	virtual UnsignedInt getNumStreams() const;

	virtual Bool doesViolateLimit(AudioEventRTS *event) const;
	virtual Bool isPlayingLowerPriority(AudioEventRTS *event) const;
	virtual Bool isPlayingAlready(AudioEventRTS *event) const;
	virtual Bool isObjectPlayingVoice(UnsignedInt objID) const;

	virtual void adjustVolumeOfPlayingAudio(AsciiString eventName, Real newVolume);
	virtual void removePlayingAudio(AsciiString eventName);
	virtual void removeAllDisabledAudio();

	virtual void setPreferredProvider(AsciiString providerNdx) { m_prefProvider = providerNdx; }
	virtual void setPreferredSpeaker(AsciiString speakerType) { m_prefSpeaker = speakerType; }

	virtual Real getFileLengthMS(AsciiString strToLoad) const;
	virtual void closeAnySamplesUsingFile(const void *fileToClose);

#if defined(_DEBUG) || defined(_INTERNAL)
	virtual void audioDebugDisplay(DebugDisplayInterface *dd, void *userData, FILE *fp = NULL);
#endif

protected:
	virtual void setDeviceListenerPosition();

	void processRequestList();
	void processPlayingList();
	void processStoppedList();
	void processFadingMusic();

	void playAudioEvent(AudioEventRTS *event);
	void stopAudioEvent(AudioHandle handle);

	/** Miles-equivalent gain for a playing sample (includes 3D distance falloff). */
	Real calcPlayingGain(SDLPlayingAudio *pa) const;
	/** Stereo pan in [-1,1] from listener orientation (Miles Fast 2D–style). */
	Real calcStereoPan(SDLPlayingAudio *pa) const;
	void applyPlayingGain(SDLPlayingAudio *pa);
	/** Push next stereo-panned PCM chunks for positional samples. */
	void feedSampleChunks(SDLPlayingAudio *pa);

	AsciiString filenameForPortion(AudioEventRTS *event) const;
	Bool feedSamplePortion(SDLPlayingAudio *pa, Bool createStream);
	Bool feedSampleFromPrefetch(SDLPlayingAudio *pa);
	Bool appendSamplePrefetch(SDLPlayingAudio *pa);
	void clearSamplePrefetch(SDLPlayingAudio *pa);
	Bool prepareSamplePrefetch(SDLPlayingAudio *pa);
	Bool advanceSampleAfterIdle(SDLPlayingAudio *pa);
	void flushStreamDecoderTail(SDLPlayingAudio *pa);

	Bool loadAndDecodeAudio(const AsciiString& filename, Uint8 *&outData, Uint32 &outSize,
							Int &outSampleRate, Int &outChannels);
	Bool openStreamForMusic(AudioEventRTS *event);
	void pushStreamData(SDLPlayingAudio *pa);
	void updateStreaming();

	SDLPlayingAudio *allocatePlayingAudio();
	void releasePlayingAudio(SDLPlayingAudio *pa);
	SDLPlayingAudio *findPlayingAudio(AudioHandle handle);

	void clearRequests();
	void stopAllAudio();

protected:
	AsciiString m_prefProvider;
	AsciiString m_prefSpeaker;

	SDLPlayingAudio *m_playingList;
	SDLPlayingAudio *m_stoppedList;
	SDLPlayingAudio *m_binkHandle;
	AsciiString m_currentTrackName;
	Bool m_musicPlaying;
	Int m_musicCompletedCount;
	UnsignedInt m_selectedProvider;
	UnsignedInt m_selectedSpeakerType;

	int m_targetSampleRate;
	int m_targetChannels;
};

#endif
