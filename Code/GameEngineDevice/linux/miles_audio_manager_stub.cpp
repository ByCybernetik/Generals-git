/*
** Linux bring-up stub — replace MilesAudioManager.cpp when -Ddisable_audio=true.
*/
#include "MilesAudioDevice/MilesAudioManager.h"
#include "Common/GameAudio.h"

MilesAudioManager::MilesAudioManager()
	: m_providerCount(0), m_selectedProvider(0), m_lastProvider(0), m_selectedSpeakerType(0),
	  m_digitalHandle(nullptr), m_listener(nullptr), m_delayFilter(nullptr),
	  m_audioCache(nullptr), m_binkHandle(nullptr), m_num2DSamples(0), m_num3DSamples(0), m_numStreams(0)
{
}

MilesAudioManager::~MilesAudioManager() {}

void MilesAudioManager::init()
{
	AudioManager::init();
}

void MilesAudioManager::postProcessLoad()
{
	AudioManager::postProcessLoad();
}

void MilesAudioManager::reset()
{
	AudioManager::reset();
}

void MilesAudioManager::update() {}

void MilesAudioManager::nextMusicTrack() {}
void MilesAudioManager::prevMusicTrack() {}
Bool MilesAudioManager::isMusicPlaying() const { return FALSE; }
Bool MilesAudioManager::hasMusicTrackCompleted(const AsciiString &, Int) const { return TRUE; }
AsciiString MilesAudioManager::getMusicTrackName() const { return AsciiString(); }

void MilesAudioManager::openDevice() {}
void MilesAudioManager::closeDevice() {}

void MilesAudioManager::stopAudio(AudioAffect) {}
void MilesAudioManager::pauseAudio(AudioAffect) {}
void MilesAudioManager::resumeAudio(AudioAffect) {}
void MilesAudioManager::pauseAmbient(Bool) {}

void MilesAudioManager::killAudioEventImmediately(AudioHandle) {}
void MilesAudioManager::stopAllAmbientsBy(Object *) {}
void MilesAudioManager::stopAllAmbientsBy(Drawable *) {}
Bool MilesAudioManager::isCurrentlyPlaying(AudioHandle) { return FALSE; }

void MilesAudioManager::notifyOfAudioCompletion(UnsignedInt, UnsignedInt) {}
PlayingAudio *MilesAudioManager::findPlayingAudioFrom(UnsignedInt, UnsignedInt) { return nullptr; }

UnsignedInt MilesAudioManager::getProviderCount() const { return 0; }
AsciiString MilesAudioManager::getProviderName(UnsignedInt) const { return AsciiString(); }
UnsignedInt MilesAudioManager::getProviderIndex(AsciiString) const { return 0; }
void MilesAudioManager::selectProvider(UnsignedInt) {}
void MilesAudioManager::unselectProvider() {}
UnsignedInt MilesAudioManager::getSelectedProvider() const { return 0; }
void MilesAudioManager::setSpeakerType(UnsignedInt) {}
UnsignedInt MilesAudioManager::getSpeakerType() { return 0; }

void *MilesAudioManager::getHandleForBink() { return nullptr; }
void MilesAudioManager::releaseHandleForBink() {}

void MilesAudioManager::friend_forcePlayAudioEventRTS(const AudioEventRTS *) {}

UnsignedInt MilesAudioManager::getNum2DSamples() const { return 0; }
UnsignedInt MilesAudioManager::getNum3DSamples() const { return 0; }
UnsignedInt MilesAudioManager::getNumStreams() const { return 0; }

Bool MilesAudioManager::doesViolateLimit(AudioEventRTS *) const { return FALSE; }
Bool MilesAudioManager::isPlayingLowerPriority(AudioEventRTS *) const { return FALSE; }
Bool MilesAudioManager::isPlayingAlready(AudioEventRTS *) const { return FALSE; }
Bool MilesAudioManager::isObjectPlayingVoice(UnsignedInt) const { return FALSE; }
Bool MilesAudioManager::killLowestPrioritySoundImmediately(AudioEventRTS *) { return FALSE; }
AudioEventRTS *MilesAudioManager::findLowestPrioritySound(AudioEventRTS *) { return nullptr; }

void MilesAudioManager::adjustVolumeOfPlayingAudio(AsciiString, Real) {}
void MilesAudioManager::removePlayingAudio(AsciiString) {}
void MilesAudioManager::removeAllDisabledAudio() {}

void MilesAudioManager::processRequestList() {}
void MilesAudioManager::processPlayingList() {}
void MilesAudioManager::processFadingList() {}
void MilesAudioManager::processStoppedList() {}

Bool MilesAudioManager::shouldProcessRequestThisFrame(AudioRequest *) const { return FALSE; }
void MilesAudioManager::adjustRequest(AudioRequest *) {}
Bool MilesAudioManager::checkForSample(AudioRequest *) { return FALSE; }

void MilesAudioManager::setHardwareAccelerated(Bool) {}
void MilesAudioManager::setSpeakerSurround(Bool) {}

Real MilesAudioManager::getFileLengthMS(AsciiString) const { return 0.0f; }
void MilesAudioManager::closeAnySamplesUsingFile(const void *) {}

void MilesAudioManager::setDeviceListenerPosition(void) {}

#if defined(_DEBUG) || defined(_INTERNAL)
void MilesAudioManager::audioDebugDisplay(DebugDisplayInterface *, void *, FILE *) {}
AudioHandle MilesAudioManager::addAudioEvent(const AudioEventRTS *) { return AHSV_Error; }
#endif
