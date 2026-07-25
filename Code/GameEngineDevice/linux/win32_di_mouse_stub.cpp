#include "Win32Device/GameClient/Win32DIMouse.h"

DirectInputMouse::DirectInputMouse()
	: m_pDirectInput(NULL), m_pMouseDevice(NULL)
{
}

DirectInputMouse::~DirectInputMouse() {}

void DirectInputMouse::init() {}
void DirectInputMouse::reset() {}
void DirectInputMouse::update() {}
void DirectInputMouse::setPosition(Int, Int) {}
void DirectInputMouse::setMouseLimits() {}
void DirectInputMouse::setCursor(MouseCursor) {}
void DirectInputMouse::capture() {}
void DirectInputMouse::releaseCapture() {}

UnsignedByte DirectInputMouse::getMouseEvent(MouseIO *result, Bool)
{
	if (result) {
		result->leftEvent = MOUSE_EVENT_NONE;
	}
	return 0;
}

void DirectInputMouse::openMouse() {}
void DirectInputMouse::closeMouse() {}
