/*
** Linux bring-up stub — replace BinkVideoPlayer.cpp when -Ddisable_bink=true.
*/
#include "VideoDevice/Bink/BinkVideoPlayer.h"

BinkVideoStream::BinkVideoStream() : m_handle(nullptr), m_memFile(nullptr) {}
BinkVideoStream::~BinkVideoStream() {}

void BinkVideoStream::update() {}
Bool BinkVideoStream::isFrameReady() { return FALSE; }
void BinkVideoStream::frameDecompress() {}
void BinkVideoStream::frameRender(VideoBuffer *) {}
void BinkVideoStream::frameNext() {}
Int BinkVideoStream::frameIndex() { return 0; }
Int BinkVideoStream::frameCount() { return 0; }
void BinkVideoStream::frameGoto(Int) {}
Int BinkVideoStream::height() { return 0; }
Int BinkVideoStream::width() { return 0; }

BinkVideoPlayer::BinkVideoPlayer() {}
BinkVideoPlayer::~BinkVideoPlayer() {}

void BinkVideoPlayer::init() {}
void BinkVideoPlayer::reset() {}
void BinkVideoPlayer::update() {}
void BinkVideoPlayer::deinit() {}
void BinkVideoPlayer::loseFocus() {}
void BinkVideoPlayer::regainFocus() {}

VideoStreamInterface *BinkVideoPlayer::createStream(HBINK) { return nullptr; }
VideoStreamInterface *BinkVideoPlayer::open(AsciiString) { return nullptr; }
VideoStreamInterface *BinkVideoPlayer::load(AsciiString) { return nullptr; }

void BinkVideoPlayer::notifyVideoPlayerOfNewProvider(Bool) {}
void BinkVideoPlayer::initializeBinkWithMiles() {}
