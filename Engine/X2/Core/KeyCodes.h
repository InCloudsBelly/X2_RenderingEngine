#pragma once

#include <ostream>

namespace X2
{
	typedef enum class KeyCode : uint16_t
	{
		// From glfw3.h
		Space = 32,
		Apostrophe = 39, /* ' */
		Comma = 44, /* , */
		Minus = 45, /* - */
		Period = 46, /* . */
		Slash = 47, /* / */

		D0 = 48, /* 0 */
		D1 = 49, /* 1 */
		D2 = 50, /* 2 */
		D3 = 51, /* 3 */
		D4 = 52, /* 4 */
		D5 = 53, /* 5 */
		D6 = 54, /* 6 */
		D7 = 55, /* 7 */
		D8 = 56, /* 8 */
		D9 = 57, /* 9 */

		Semicolon = 59, /* ; */
		Equal = 61, /* = */

		A = 65,
		B = 66,
		C = 67,
		D = 68,
		E = 69,
		F = 70,
		G = 71,
		H = 72,
		I = 73,
		J = 74,
		K = 75,
		L = 76,
		M = 77,
		N = 78,
		O = 79,
		P = 80,
		Q = 81,
		R = 82,
		S = 83,
		T = 84,
		U = 85,
		V = 86,
		W = 87,
		X = 88,
		Y = 89,
		Z = 90,

		LeftBracket = 91,  /* [ */
		Backslash = 92,  /* \ */
		RightBracket = 93,  /* ] */
		GraveAccent = 96,  /* ` */

		World1 = 161, /* non-US #1 */
		World2 = 162, /* non-US #2 */

		/* Function keys */
		Escape = 256,
		Enter = 257,
		Tab = 258,
		Backspace = 259,
		Insert = 260,
		Delete = 261,
		Right = 262,
		Left = 263,
		Down = 264,
		Up = 265,
		PageUp = 266,
		PageDown = 267,
		Home = 268,
		End = 269,
		CapsLock = 280,
		ScrollLock = 281,
		NumLock = 282,
		PrintScreen = 283,
		Pause = 284,
		F1 = 290,
		F2 = 291,
		F3 = 292,
		F4 = 293,
		F5 = 294,
		F6 = 295,
		F7 = 296,
		F8 = 297,
		F9 = 298,
		F10 = 299,
		F11 = 300,
		F12 = 301,
		F13 = 302,
		F14 = 303,
		F15 = 304,
		F16 = 305,
		F17 = 306,
		F18 = 307,
		F19 = 308,
		F20 = 309,
		F21 = 310,
		F22 = 311,
		F23 = 312,
		F24 = 313,
		F25 = 314,

		/* Keypad */
		KP0 = 320,
		KP1 = 321,
		KP2 = 322,
		KP3 = 323,
		KP4 = 324,
		KP5 = 325,
		KP6 = 326,
		KP7 = 327,
		KP8 = 328,
		KP9 = 329,
		KPDecimal = 330,
		KPDivide = 331,
		KPMultiply = 332,
		KPSubtract = 333,
		KPAdd = 334,
		KPEnter = 335,
		KPEqual = 336,

		LeftShift = 340,
		LeftControl = 341,
		LeftAlt = 342,
		LeftSuper = 343,
		RightShift = 344,
		RightControl = 345,
		RightAlt = 346,
		RightSuper = 347,
		Menu = 348
	} Key;

	enum class KeyState
	{
		None = -1,
		Pressed,
		Held,
		Released
	};

	enum class CursorMode
	{
		Normal = 0,
		Hidden = 1,
		Locked = 2
	};

	typedef enum class MouseButton : uint16_t
	{
		Button0 = 0,
		Button1 = 1,
		Button2 = 2,
		Button3 = 3,
		Button4 = 4,
		Button5 = 5,
		Left = Button0,
		Right = Button1,
		Middle = Button2
	} Button;


	inline std::ostream& operator<<(std::ostream& os, KeyCode keyCode)
	{
		os << static_cast<int32_t>(keyCode);
		return os;
	}

	inline std::ostream& operator<<(std::ostream& os, MouseButton button)
	{
		os << static_cast<int32_t>(button);
		return os;
	}
}

// From glfw3.h
#define X2_KEY_SPACE           ::X2::Key::Space
#define X2_KEY_APOSTROPHE      ::X2::Key::Apostrophe    /* ' */
#define X2_KEY_COMMA           ::X2::Key::Comma         /* , */
#define X2_KEY_MINUS           ::X2::Key::Minus         /* - */
#define X2_KEY_PERIOD          ::X2::Key::Period        /* . */
#define X2_KEY_SLASH           ::X2::Key::Slash         /* / */
#define X2_KEY_0               ::X2::Key::D0
#define X2_KEY_1               ::X2::Key::D1
#define X2_KEY_2               ::X2::Key::D2
#define X2_KEY_3               ::X2::Key::D3
#define X2_KEY_4               ::X2::Key::D4
#define X2_KEY_5               ::X2::Key::D5
#define X2_KEY_6               ::X2::Key::D6
#define X2_KEY_7               ::X2::Key::D7
#define X2_KEY_8               ::X2::Key::D8
#define X2_KEY_9               ::X2::Key::D9
#define X2_KEY_SEMICOLON       ::X2::Key::Semicolon     /* ; */
#define X2_KEY_EQUAL           ::X2::Key::Equal         /* = */
#define X2_KEY_A               ::X2::Key::A
#define X2_KEY_B               ::X2::Key::B
#define X2_KEY_C               ::X2::Key::C
#define X2_KEY_D               ::X2::Key::D
#define X2_KEY_E               ::X2::Key::E
#define X2_KEY_F               ::X2::Key::F
#define X2_KEY_G               ::X2::Key::G
#define X2_KEY_H               ::X2::Key::H
#define X2_KEY_I               ::X2::Key::I
#define X2_KEY_J               ::X2::Key::J
#define X2_KEY_K               ::X2::Key::K
#define X2_KEY_L               ::X2::Key::L
#define X2_KEY_M               ::X2::Key::M
#define X2_KEY_N               ::X2::Key::N
#define X2_KEY_O               ::X2::Key::O
#define X2_KEY_P               ::X2::Key::P
#define X2_KEY_Q               ::X2::Key::Q
#define X2_KEY_R               ::X2::Key::R
#define X2_KEY_S               ::X2::Key::S
#define X2_KEY_T               ::X2::Key::T
#define X2_KEY_U               ::X2::Key::U
#define X2_KEY_V               ::X2::Key::V
#define X2_KEY_W               ::X2::Key::W
#define X2_KEY_X               ::X2::Key::X
#define X2_KEY_Y               ::X2::Key::Y
#define X2_KEY_Z               ::X2::Key::Z
#define X2_KEY_LEFT_BRACKET    ::X2::Key::LeftBracket   /* [ */
#define X2_KEY_BACKSLASH       ::X2::Key::Backslash     /* \ */
#define X2_KEY_RIGHT_BRACKET   ::X2::Key::RightBracket  /* ] */
#define X2_KEY_GRAVE_ACCENT    ::X2::Key::GraveAccent   /* ` */
#define X2_KEY_WORLD_1         ::X2::Key::World1        /* non-US #1 */
#define X2_KEY_WORLD_2         ::X2::Key::World2        /* non-US #2 */

/* Function keys */
#define X2_KEY_ESCAPE          ::X2::Key::Escape
#define X2_KEY_ENTER           ::X2::Key::Enter
#define X2_KEY_TAB             ::X2::Key::Tab
#define X2_KEY_BACKSPACE       ::X2::Key::Backspace
#define X2_KEY_INSERT          ::X2::Key::Insert
#define X2_KEY_DELETE          ::X2::Key::Delete
#define X2_KEY_RIGHT           ::X2::Key::Right
#define X2_KEY_LEFT            ::X2::Key::Left
#define X2_KEY_DOWN            ::X2::Key::Down
#define X2_KEY_UP              ::X2::Key::Up
#define X2_KEY_PAGE_UP         ::X2::Key::PageUp
#define X2_KEY_PAGE_DOWN       ::X2::Key::PageDown
#define X2_KEY_HOME            ::X2::Key::Home
#define X2_KEY_END             ::X2::Key::End
#define X2_KEY_CAPS_LOCK       ::X2::Key::CapsLock
#define X2_KEY_SCROLL_LOCK     ::X2::Key::ScrollLock
#define X2_KEY_NUM_LOCK        ::X2::Key::NumLock
#define X2_KEY_PRINT_SCREEN    ::X2::Key::PrintScreen
#define X2_KEY_PAUSE           ::X2::Key::Pause
#define X2_KEY_F1              ::X2::Key::F1
#define X2_KEY_F2              ::X2::Key::F2
#define X2_KEY_F3              ::X2::Key::F3
#define X2_KEY_F4              ::X2::Key::F4
#define X2_KEY_F5              ::X2::Key::F5
#define X2_KEY_F6              ::X2::Key::F6
#define X2_KEY_F7              ::X2::Key::F7
#define X2_KEY_F8              ::X2::Key::F8
#define X2_KEY_F9              ::X2::Key::F9
#define X2_KEY_F10             ::X2::Key::F10
#define X2_KEY_F11             ::X2::Key::F11
#define X2_KEY_F12             ::X2::Key::F12
#define X2_KEY_F13             ::X2::Key::F13
#define X2_KEY_F14             ::X2::Key::F14
#define X2_KEY_F15             ::X2::Key::F15
#define X2_KEY_F16             ::X2::Key::F16
#define X2_KEY_F17             ::X2::Key::F17
#define X2_KEY_F18             ::X2::Key::F18
#define X2_KEY_F19             ::X2::Key::F19
#define X2_KEY_F20             ::X2::Key::F20
#define X2_KEY_F21             ::X2::Key::F21
#define X2_KEY_F22             ::X2::Key::F22
#define X2_KEY_F23             ::X2::Key::F23
#define X2_KEY_F24             ::X2::Key::F24
#define X2_KEY_F25             ::X2::Key::F25

/* Keypad */
#define X2_KEY_KP_0            ::X2::Key::KP0
#define X2_KEY_KP_1            ::X2::Key::KP1
#define X2_KEY_KP_2            ::X2::Key::KP2
#define X2_KEY_KP_3            ::X2::Key::KP3
#define X2_KEY_KP_4            ::X2::Key::KP4
#define X2_KEY_KP_5            ::X2::Key::KP5
#define X2_KEY_KP_6            ::X2::Key::KP6
#define X2_KEY_KP_7            ::X2::Key::KP7
#define X2_KEY_KP_8            ::X2::Key::KP8
#define X2_KEY_KP_9            ::X2::Key::KP9
#define X2_KEY_KP_DECIMAL      ::X2::Key::KPDecimal
#define X2_KEY_KP_DIVIDE       ::X2::Key::KPDivide
#define X2_KEY_KP_MULTIPLY     ::X2::Key::KPMultiply
#define X2_KEY_KP_SUBTRACT     ::X2::Key::KPSubtract
#define X2_KEY_KP_ADD          ::X2::Key::KPAdd
#define X2_KEY_KP_ENTER        ::X2::Key::KPEnter
#define X2_KEY_KP_EQUAL        ::X2::Key::KPEqual

#define X2_KEY_LEFT_SHIFT      ::X2::Key::LeftShift
#define X2_KEY_LEFT_CONTROL    ::X2::Key::LeftControl
#define X2_KEY_LEFT_ALT        ::X2::Key::LeftAlt
#define X2_KEY_LEFT_SUPER      ::X2::Key::LeftSuper
#define X2_KEY_RIGHT_SHIFT     ::X2::Key::RightShift
#define X2_KEY_RIGHT_CONTROL   ::X2::Key::RightControl
#define X2_KEY_RIGHT_ALT       ::X2::Key::RightAlt
#define X2_KEY_RIGHT_SUPER     ::X2::Key::RightSuper
#define X2_KEY_MENU            ::X2::Key::Menu

// Mouse
#define X2_MOUSE_BUTTON_LEFT    ::X2::Button::Left
#define X2_MOUSE_BUTTON_RIGHT   ::X2::Button::Right
#define X2_MOUSE_BUTTON_MIDDLE  ::X2::Button::Middle
