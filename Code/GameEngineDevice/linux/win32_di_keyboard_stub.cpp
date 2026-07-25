#include "Win32Device/GameClient/Win32DIKeyboard.h"
#include "GameClient/KeyDefs.h"
#include "sdl3_host.h"
#include "linux/dinput_keys.h"
#include "linux/winuser_extra.h"

DirectInputKeyboard::DirectInputKeyboard()
	: m_pDirectInput(NULL), m_pKeyboardDevice(NULL)
{
}

DirectInputKeyboard::~DirectInputKeyboard() {}

void DirectInputKeyboard::init()
{
	Keyboard::init();
}

void DirectInputKeyboard::reset()
{
	Keyboard::reset();
	Linux_Keyboard_ResetTransitionState();
}

void DirectInputKeyboard::update()
{
	Keyboard::update();
}
Bool DirectInputKeyboard::getCapsState()
{
	return Platform_Get_Async_Key(VK_CAPITAL) ? TRUE : FALSE;
}

static UnsignedByte vk_to_dik(int vk)
{
	/* VK_F1..VK_F12 (0x70..0x7B) overlap lowercase ASCII ('p'..'z'). */
	if (vk >= 0x70 && vk <= 0x7B) {
		return (UnsignedByte)(DIK_F1 + (vk - 0x70));
	}

	if (vk >= 'a' && vk <= 'z') {
		vk = vk - 'a' + 'A';
	}

	if (vk >= 'A' && vk <= 'Z') {
		return (UnsignedByte)(DIK_A + (vk - 'A'));
	}
	if (vk >= '1' && vk <= '9') {
		return (UnsignedByte)(DIK_1 + (vk - '1'));
	}
	if (vk == '0') {
		return DIK_0;
	}

	switch (vk) {
	case VK_ESCAPE: return DIK_ESCAPE;
	case VK_RETURN: return DIK_RETURN;
	case VK_SPACE: return DIK_SPACE;
	case VK_TAB: return DIK_TAB;
	case VK_BACK: return DIK_BACK;
	case VK_DELETE: return DIK_DELETE;
	case VK_HOME: return DIK_HOME;
	case VK_END: return DIK_END;
	case VK_PRIOR: return DIK_PRIOR;
	case VK_NEXT: return DIK_NEXT;
	case VK_UP: return DIK_UP;
	case VK_DOWN: return DIK_DOWN;
	case VK_LEFT: return DIK_LEFT;
	case VK_RIGHT: return DIK_RIGHT;
	case VK_INSERT: return DIK_INSERT;
	case VK_CAPITAL: return DIK_CAPITAL;
	case VK_PAUSE: return DIK_PAUSE;
	case 0x90: return DIK_NUMLOCK;
	case 0x91: return DIK_SCROLL;
	case 0xA0:
	case VK_SHIFT: return DIK_LSHIFT;
	case 0xA1: return DIK_RSHIFT;
	case 0xA2:
	case VK_CONTROL: return DIK_LCONTROL;
	case 0xA3: return DIK_RCONTROL;
	case 0xA4:
	case VK_MENU: return DIK_LMENU;
	case 0xA5: return DIK_RMENU;
	case 0xBD: return DIK_MINUS;
	case 0xBB: return DIK_EQUALS;
	case 0xDB: return DIK_LBRACKET;
	case 0xDD: return DIK_RBRACKET;
	case 0xDC: return DIK_BACKSLASH;
	case 0xBA: return DIK_SEMICOLON;
	case 0xDE: return DIK_APOSTROPHE;
	case 0xC0: return DIK_GRAVE;
	case 0xBC: return DIK_COMMA;
	case 0xBE: return DIK_PERIOD;
	case 0xBF: return DIK_SLASH;
	case 0x6F: return DIK_DIVIDE;
	case 0x6A: return DIK_MULTIPLY;
	case 0x6D: return DIK_SUBTRACT;
	case 0x6B: return DIK_ADD;
	case 0x6E: return DIK_DECIMAL;
	default:
		break;
	}

	if (vk >= 0x60 && vk <= 0x69) {
		return (UnsignedByte)(DIK_NUMPAD0 + (vk - 0x60));
	}

	return 0;
}

void DirectInputKeyboard::getKey(KeyboardIO *key)
{
	int vk = 0;
	bool down = false;
	UnsignedByte dik = 0;

	if (key == NULL) {
		return;
	}

	key->key = KEY_NONE;

	if (!Platform_Pop_Key_Change(&vk, &down)) {
		return;
	}

	dik = vk_to_dik(vk);
	if (dik == 0) {
		return;
	}

	key->key = dik;
	key->state = down ? (UnsignedByte)KEY_STATE_DOWN : (UnsignedByte)KEY_STATE_UP;
	key->status = KeyboardIO::STATUS_UNUSED;
}

void DirectInputKeyboard::openKeyboard() {}
void DirectInputKeyboard::closeKeyboard() {}
