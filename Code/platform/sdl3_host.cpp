/*
 * SDL3: game window and event source.
 */
#include "sdl3_host.h"
#include "pe_resource_loader.h"
#include "linux/winuser.h"
#include "linux/winuser_extra.h"
#include "linux/shellapi.h"
#include "linux/linux_ani_cursor.h"

#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif

static SDL_Window *g_window = NULL;
static bool g_quit_posted = false;
static Platform_Window_Message_Handler g_wnd_msg_handler = NULL;
static Platform_Window_Resize_Handler g_resize_handler = NULL;

/* 256-byte keyboard state indexed by Win32 VK (subset used by WWKeyboard). */
static unsigned char g_key_down[256];
static unsigned char g_key_hit_pending[256];
static unsigned char g_key_prev[256];
static long g_wheel_accum = 0;
static int g_text_input_refs = 0;

#define PLATFORM_KEY_QUEUE_SIZE 128
struct PlatformKeyQueueEntry {
	int vk;
	unsigned char down;
};
static PlatformKeyQueueEntry g_key_queue[PLATFORM_KEY_QUEUE_SIZE];
static int g_key_queue_head = 0;
static int g_key_queue_tail = 0;

static void platform_key_queue_clear(void)
{
	g_key_queue_head = 0;
	g_key_queue_tail = 0;
}

static void platform_key_queue_push(int vk, bool down)
{
	const int next = (g_key_queue_tail + 1) % PLATFORM_KEY_QUEUE_SIZE;
	if (next == g_key_queue_head) {
		return;
	}
	g_key_queue[g_key_queue_tail].vk = vk;
	g_key_queue[g_key_queue_tail].down = down ? 1u : 0u;
	g_key_queue_tail = next;
}

static bool platform_key_queue_pop(int *vk, bool *down)
{
	if (g_key_queue_head == g_key_queue_tail) {
		return false;
	}
	*vk = g_key_queue[g_key_queue_head].vk;
	*down = g_key_queue[g_key_queue_head].down != 0;
	g_key_queue_head = (g_key_queue_head + 1) % PLATFORM_KEY_QUEUE_SIZE;
	return true;
}

static int sdl_scancode_to_vk(SDL_Scancode sc);

static void platform_dispatch_utf8_text(const char *utf8)
{
	const unsigned char *p;
	uint32_t cp;

	if (utf8 == NULL || g_wnd_msg_handler == NULL) {
		return;
	}

	p = (const unsigned char *)utf8;
	while (*p != '\0') {
		cp = 0;
		if ((*p & 0x80u) == 0u) {
			cp = *p++;
		} else if ((*p & 0xE0u) == 0xC0u && p[1] != '\0') {
			cp = ((uint32_t)(p[0] & 0x1Fu) << 6) | (uint32_t)(p[1] & 0x3Fu);
			p += 2;
		} else if ((*p & 0xF0u) == 0xE0u && p[1] != '\0' && p[2] != '\0') {
			cp = ((uint32_t)(p[0] & 0x0Fu) << 12) |
				((uint32_t)(p[1] & 0x3Fu) << 6) |
				(uint32_t)(p[2] & 0x3Fu);
			p += 3;
		} else if ((*p & 0xF8u) == 0xF0u && p[1] != '\0' && p[2] != '\0' && p[3] != '\0') {
			cp = ((uint32_t)(p[0] & 0x07u) << 18) |
				((uint32_t)(p[1] & 0x3Fu) << 12) |
				((uint32_t)(p[2] & 0x3Fu) << 6) |
				(uint32_t)(p[3] & 0x3Fu);
			p += 4;
		} else {
			++p;
			continue;
		}

		if (cp <= 0xFFFFu && g_window != NULL) {
			g_wnd_msg_handler((HWND)g_window, WM_CHAR, (WPARAM)cp, 0);
		}
	}
}

void Platform_Set_System_Cursor_Visible(bool visible)
{
	if (visible) {
		SDL_ShowCursor();
	} else {
		SDL_HideCursor();
	}
}

void Platform_Set_Text_Input_Enabled(bool enabled)
{
	if (g_window == NULL) {
		return;
	}

	if (enabled) {
		if (g_text_input_refs++ == 0) {
			SDL_StartTextInput(g_window);
		}
	} else if (g_text_input_refs > 0) {
		if (--g_text_input_refs == 0) {
			SDL_StopTextInput(g_window);
		}
	}
}

extern HWND MainWindow;
extern HINSTANCE ProgramInstance;
extern bool GameInFocus;

extern "C" void Renegade_Stop_Main_Loop(int exitCode);

void Platform_Init_Early(void)
{
	Renegade_Init_Embedded_Resources();
}

int Platform_Init_Video_Audio(void)
{
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD | SDL_INIT_EVENTS)) {
		SDL_Log("SDL_Init failed: %s", SDL_GetError());
		return 0;
	}

	uint32_t window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
#if defined(RENEGADE_VULKAN)
	window_flags |= SDL_WINDOW_VULKAN;
#endif
	g_window = SDL_CreateWindow(
		"Command and Conquer Generals",
		800,
		600,
		window_flags);
	if (!g_window) {
		SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
		SDL_Quit();
		return 0;
	}

	SDL_ShowWindow(g_window);
	SDL_RaiseWindow(g_window);
	for (int i = 0; i < 8; ++i) {
		SDL_PumpEvents();
	}

	MainWindow = (HWND)g_window;
	ProgramInstance = (HINSTANCE)1;
	GameInFocus = true;
	return 1;
}

SDL_Window *Platform_Get_SDL_Window(void)
{
	return g_window;
}

HWND Platform_Get_Main_HWnd(void)
{
	return (HWND)g_window;
}

void Platform_Set_Window_Message_Handler(Platform_Window_Message_Handler handler)
{
	g_wnd_msg_handler = handler;
	/*
	 * Window already has focus from Platform_Create_Window (GameInFocus=true)
	 * before WndProc is hooked. Synthesize WM_ACTIVATEAPP so Generals'
	 * isActive becomes true — required for campaign load movie drawing.
	 */
	if (handler != NULL && g_window != NULL && GameInFocus) {
		handler(MainWindow, WM_ACTIVATEAPP, (WPARAM)TRUE, 0);
		handler(MainWindow, WM_SETFOCUS, 0, 0);
	}
}

void Platform_Set_Async_Key(int vkey, bool down)
{
	if (vkey >= 0 && vkey < 256) {
		if (down && g_key_down[vkey] == 0) {
			g_key_hit_pending[vkey] = 1;
		}
		g_key_down[vkey] = down ? 1 : 0;
	}
}

bool Platform_Consume_Key_Hit(int vkey)
{
	if (vkey < 0 || vkey >= 256) {
		return false;
	}
	if (g_key_hit_pending[vkey] == 0) {
		return false;
	}
	g_key_hit_pending[vkey] = 0;
	return true;
}

bool Platform_Get_Async_Key(int vkey)
{
	if (vkey < 0 || vkey >= 256) {
		return false;
	}
	return g_key_down[vkey] != 0;
}

void Linux_Keyboard_ResetTransitionState(void)
{
	memset(g_key_prev, 0, sizeof(g_key_prev));
	memset(g_key_hit_pending, 0, sizeof(g_key_hit_pending));
	platform_key_queue_clear();
}

static bool platform_window_has_keyboard_focus(void)
{
	if (g_window == NULL) {
		return false;
	}
	return (SDL_GetWindowFlags(g_window) & SDL_WINDOW_INPUT_FOCUS) != 0;
}

static void platform_sync_keyboard_snapshot(void)
{
	unsigned char snapshot[256];

	memset(snapshot, 0, sizeof(snapshot));
	const bool has_focus = platform_window_has_keyboard_focus();
	GameInFocus = has_focus;
	if (!has_focus) {
		memset(g_key_down, 0, sizeof(g_key_down));
		Linux_Keyboard_ResetTransitionState();
		return;
	}

	int count = 0;
	const bool *keys = SDL_GetKeyboardState(&count);
	if (keys == NULL) {
		return;
	}

	const int limit = count < SDL_SCANCODE_COUNT ? count : SDL_SCANCODE_COUNT;
	for (int sc = 0; sc < limit; ++sc) {
		if (!keys[sc]) {
			continue;
		}
		const int vk = sdl_scancode_to_vk((SDL_Scancode)sc);
		if (vk > 0 && vk < 256) {
			snapshot[vk] = 1;
		}
	}

	for (int vk = 1; vk < 256; ++vk) {
		const bool down = snapshot[vk] != 0;
		if (down && g_key_down[vk] == 0) {
			g_key_hit_pending[vk] = 1;
		}
		g_key_down[vk] = down ? 1u : 0u;
	}
}

bool Platform_Pop_Key_Change(int *vk, bool *down)
{
	int next_vk = 0;
	bool next_down = false;

	if (vk == NULL || down == NULL) {
		return false;
	}

	platform_sync_keyboard_snapshot();

	if (platform_key_queue_pop(&next_vk, &next_down)) {
		*vk = next_vk;
		*down = next_down;
		g_key_prev[next_vk] = next_down ? 1u : 0u;
		return true;
	}

	for (int scan_vk = 1; scan_vk < 256; ++scan_vk) {
		const bool is_down = g_key_down[scan_vk] != 0;
		const bool was_down = g_key_prev[scan_vk] != 0;
		if (is_down == was_down) {
			continue;
		}

		g_key_prev[scan_vk] = is_down ? 1u : 0u;
		*vk = scan_vk;
		*down = is_down;
		return true;
	}

	return false;
}

void Platform_Accumulate_Mouse_Wheel(float delta_y)
{
	g_wheel_accum += (long)(delta_y * 120.0f);
}

long Platform_Consume_Mouse_Wheel(void)
{
	const long wheel = g_wheel_accum;
	g_wheel_accum = 0;
	return wheel;
}

static int sdl_key_to_vk(SDL_Keycode key)
{
	const SDL_Scancode sc = SDL_GetScancodeFromKey(key, NULL);
	if (sc != SDL_SCANCODE_UNKNOWN) {
		return sdl_scancode_to_vk(sc);
	}
	return 0;
}

static int sdl_scancode_to_vk(SDL_Scancode sc)
{
	/* SDL3 keyboard scancode → Win32 virtual key (US layout subset). */
	switch (sc) {
	case SDL_SCANCODE_ESCAPE: return VK_ESCAPE;
	case SDL_SCANCODE_RETURN: return VK_RETURN;
	case SDL_SCANCODE_SPACE: return VK_SPACE;
	case SDL_SCANCODE_LSHIFT: return 0xA0;
	case SDL_SCANCODE_RSHIFT: return 0xA1;
	case SDL_SCANCODE_LCTRL: return 0xA2;
	case SDL_SCANCODE_RCTRL: return 0xA3;
	case SDL_SCANCODE_LALT: return 0xA4;
	case SDL_SCANCODE_RALT: return 0xA5;
	case SDL_SCANCODE_CAPSLOCK: return VK_CAPITAL;
	case SDL_SCANCODE_TAB: return VK_TAB;
	case SDL_SCANCODE_BACKSPACE: return VK_BACK;
	case SDL_SCANCODE_DELETE: return VK_DELETE;
	case SDL_SCANCODE_HOME: return VK_HOME;
	case SDL_SCANCODE_END: return VK_END;
	case SDL_SCANCODE_PAGEUP: return VK_PRIOR;
	case SDL_SCANCODE_PAGEDOWN: return VK_NEXT;
	case SDL_SCANCODE_UP: return VK_UP;
	case SDL_SCANCODE_DOWN: return VK_DOWN;
	case SDL_SCANCODE_LEFT: return VK_LEFT;
	case SDL_SCANCODE_RIGHT: return VK_RIGHT;
	case SDL_SCANCODE_INSERT: return VK_INSERT;
	case SDL_SCANCODE_KP_ENTER: return VK_RETURN;
	case SDL_SCANCODE_MINUS: return 0xBD;
	case SDL_SCANCODE_EQUALS: return 0xBB;
	case SDL_SCANCODE_LEFTBRACKET: return 0xDB;
	case SDL_SCANCODE_RIGHTBRACKET: return 0xDD;
	case SDL_SCANCODE_BACKSLASH: return 0xDC;
	case SDL_SCANCODE_SEMICOLON: return 0xBA;
	case SDL_SCANCODE_APOSTROPHE: return 0xDE;
	case SDL_SCANCODE_GRAVE: return 0xC0;
	case SDL_SCANCODE_COMMA: return 0xBC;
	case SDL_SCANCODE_PERIOD: return 0xBE;
	case SDL_SCANCODE_SLASH: return 0xBF;
	default:
		if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z) {
			return 'A' + (sc - SDL_SCANCODE_A);
		}
		if (sc >= SDL_SCANCODE_0 && sc <= SDL_SCANCODE_9) {
			return '0' + (sc - SDL_SCANCODE_0);
		}
		if (sc >= SDL_SCANCODE_F1 && sc <= SDL_SCANCODE_F12) {
			return 0x70 + (sc - SDL_SCANCODE_F1);
		}
		return 0;
	}
}

extern void (*Win32_Key_Notify_Callback_Ptr)(unsigned int message, unsigned int wParam, long lParam);

static void Platform_Handle_Quit_Request(void)
{
	if (g_quit_posted) {
		return;
	}
	g_quit_posted = true;
	Renegade_Stop_Main_Loop(EXIT_SUCCESS);
}

BOOL Platform_Show_Window(HWND hwnd, int cmdShow)
{
	SDL_Window *window = (SDL_Window *)hwnd;
	if (window == NULL) {
		window = g_window;
	}
	if (window == NULL) {
		return FALSE;
	}

	switch (cmdShow) {
	case SW_HIDE:
	case SW_MINIMIZE:
		SDL_HideWindow(window);
		SDL_MinimizeWindow(window);
		break;
	default:
		SDL_ShowWindow(window);
		SDL_RaiseWindow(window);
		break;
	}

	for (int i = 0; i < 4; ++i) {
		SDL_PumpEvents();
	}
	return TRUE;
}

void Platform_Set_Window_Resize_Handler(Platform_Window_Resize_Handler handler)
{
	g_resize_handler = handler;
}

void Platform_Apply_Window_Display_Mode(bool windowed, int width, int height)
{
	if (g_window == NULL) {
		return;
	}

	if (windowed) {
		SDL_SetWindowFullscreen(g_window, false);
		SDL_SetWindowFullscreenMode(g_window, NULL);
		if (width > 0 && height > 0) {
			SDL_SetWindowSize(g_window, width, height);
		}
		SDL_SetWindowPosition(g_window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
	} else {
		SDL_DisplayID display = SDL_GetDisplayForWindow(g_window);
		SDL_DisplayMode mode;
		if (width > 0 && height > 0 &&
			SDL_GetClosestFullscreenDisplayMode(display, width, height, 0.0f, true, &mode)) {
			SDL_SetWindowFullscreenMode(g_window, &mode);
		} else {
			SDL_SetWindowFullscreenMode(g_window, NULL);
		}
		SDL_SetWindowFullscreen(g_window, true);
	}

	SDL_SyncWindow(g_window);
	Platform_Pump_Events();
}

void Platform_Pump_Events(void)
{
	SDL_Event ev;

	SDL_PumpEvents();
	while (SDL_PollEvent(&ev)) {
		switch (ev.type) {
		case SDL_EVENT_QUIT:
		case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
			Platform_Handle_Quit_Request();
			break;

		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			if (g_resize_handler != NULL &&
				ev.window.windowID == SDL_GetWindowID(g_window)) {
				g_resize_handler((int)ev.window.data1, (int)ev.window.data2);
			}
			break;

		case SDL_EVENT_WINDOW_FOCUS_GAINED:
			GameInFocus = true;
			Linux_Keyboard_ResetTransitionState();
			if (g_wnd_msg_handler != NULL) {
				/*
				 * Generals keys app focus off WM_ACTIVATEAPP (isActive). Without
				 * this, campaign load videos skip TheDisplay->draw() forever while
				 * Bink audio still plays — black/frozen screen until the movie ends.
				 */
				g_wnd_msg_handler(MainWindow, WM_ACTIVATEAPP, (WPARAM)TRUE, 0);
				g_wnd_msg_handler(MainWindow, WM_SETFOCUS, 0, 0);
			}
			if (g_text_input_refs > 0 && g_window != NULL) {
				SDL_StartTextInput(g_window);
			}
			break;
		case SDL_EVENT_WINDOW_FOCUS_LOST:
			GameInFocus = platform_window_has_keyboard_focus();
			if (!GameInFocus) {
				Linux_Keyboard_ResetTransitionState();
				memset(g_key_down, 0, sizeof(g_key_down));
			}
			if (g_wnd_msg_handler != NULL) {
				g_wnd_msg_handler(MainWindow, WM_KILLFOCUS, 0, 0);
				g_wnd_msg_handler(MainWindow, WM_ACTIVATEAPP, (WPARAM)FALSE, 0);
			}
			SDL_ShowCursor();
			break;

		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP: {
			const bool down = ev.key.down;
			if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.repeat) {
				break;
			}
			int vk = sdl_scancode_to_vk(ev.key.scancode);
			if (vk == 0) {
				vk = sdl_key_to_vk(ev.key.key);
			}
			if (vk) {
				Platform_Set_Async_Key(vk, down);
				platform_key_queue_push(vk, down);
				if (Win32_Key_Notify_Callback_Ptr) {
					const UINT wm = down ? WM_KEYDOWN : WM_KEYUP;
					const LPARAM lp = (ev.key.repeat ? (LPARAM)KF_REPEAT : 0);
					Win32_Key_Notify_Callback_Ptr(wm, (WPARAM)vk, lp);
				}
				if (g_wnd_msg_handler != NULL) {
					const UINT wm = down ? WM_KEYDOWN : WM_KEYUP;
					const LPARAM lp = (ev.key.repeat ? (LPARAM)KF_REPEAT : 0);
					g_wnd_msg_handler(MainWindow, wm, (WPARAM)vk, lp);
				}
			}
			break;
		}

		case SDL_EVENT_TEXT_INPUT:
			platform_dispatch_utf8_text(ev.text.text);
			break;

		case SDL_EVENT_MOUSE_WHEEL:
			if (g_wnd_msg_handler != NULL && g_window != NULL) {
				float delta = ev.wheel.y;
				if (ev.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
					delta = -delta;
				}
				const short wheel_delta = (short)(delta * 120.0f);
				if (wheel_delta != 0) {
					const float scale = SDL_GetWindowDisplayScale(g_window);
					const int cx = (int)(ev.wheel.mouse_x * (scale > 0.0f ? scale : 1.0f));
					const int cy = (int)(ev.wheel.mouse_y * (scale > 0.0f ? scale : 1.0f));
					POINT pt = { cx, cy };
					ClientToScreen((HWND)g_window, &pt);
					const WPARAM wp = (WPARAM)(((unsigned short)(SHORT)wheel_delta) << 16);
					const LPARAM lp = (LPARAM)(((unsigned long)(unsigned short)pt.y << 16) |
						((unsigned long)(unsigned short)pt.x & 0xFFFF));
					g_wnd_msg_handler(MainWindow, WM_MOUSEWHEEL, wp, lp);
				}
			}
			break;

		case SDL_EVENT_MOUSE_MOTION:
			if (g_wnd_msg_handler != NULL && g_window != NULL) {
				const float scale = SDL_GetWindowDisplayScale(g_window);
				const int x = (int)(ev.motion.x * (scale > 0.0f ? scale : 1.0f));
				const int y = (int)(ev.motion.y * (scale > 0.0f ? scale : 1.0f));
				const LPARAM lp = (LPARAM)((y << 16) | (x & 0xFFFF));
				g_wnd_msg_handler(MainWindow, WM_MOUSEMOVE, 0, lp);
				g_wnd_msg_handler(MainWindow, WM_SETCURSOR, (WPARAM)MainWindow, 0);
			}
			break;

		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP: {
			if (g_wnd_msg_handler != NULL && g_window != NULL) {
				UINT msg = 0;
				const bool down = (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
				/* SDL reports consecutive clicks; Win32 would emit *DBLCLK on
				 * the second press. Same-type unit selection depends on that. */
				const bool dbl = down && (ev.button.clicks >= 2);
				switch (ev.button.button) {
				case SDL_BUTTON_LEFT:
					msg = down ? (dbl ? WM_LBUTTONDBLCLK : WM_LBUTTONDOWN) : WM_LBUTTONUP;
					break;
				case SDL_BUTTON_RIGHT:
					msg = down ? (dbl ? WM_RBUTTONDBLCLK : WM_RBUTTONDOWN) : WM_RBUTTONUP;
					break;
				case SDL_BUTTON_MIDDLE:
					msg = down ? (dbl ? WM_MBUTTONDBLCLK : WM_MBUTTONDOWN) : WM_MBUTTONUP;
					break;
				default:
					break;
				}
				if (msg != 0) {
					const float scale = SDL_GetWindowDisplayScale(g_window);
					const int x = (int)(ev.button.x * (scale > 0.0f ? scale : 1.0f));
					const int y = (int)(ev.button.y * (scale > 0.0f ? scale : 1.0f));
					const WPARAM wp = down ? 1u : 0u;
					const LPARAM lp = (LPARAM)((y << 16) | (x & 0xFFFF));
					g_wnd_msg_handler(MainWindow, msg, wp, lp);
				}
			}
			break;
		}

		default:
			break;
		}
	}

	platform_sync_keyboard_snapshot();
}

void Platform_Pre_Shutdown(void)
{
	GameInFocus = false;
	if (g_window) {
		SDL_HideWindow(g_window);
	}
	for (int i = 0; i < 8; ++i) {
		SDL_PumpEvents();
	}
}

void Platform_Shutdown(void)
{
	Linux_Ani_Cursor_Shutdown();
	MainWindow = NULL;
	if (g_window) {
		SDL_DestroyWindow(g_window);
		g_window = NULL;
	}
	for (int i = 0; i < 4; ++i) {
		SDL_PumpEvents();
	}
	/* SDL_Quit can deadlock after graphics teardown on some drivers. */
}
