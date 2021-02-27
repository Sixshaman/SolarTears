#include "Win32KeyboardKeyMap.hpp"
#include <Windows.h>

KeyboardKeyMap::KeyboardKeyMap()
{
	InitializeKeyMap();
	memset(mNativeKeyMap, (uint8_t)(ControlCode::Nothing), 256 * sizeof(uint8_t));
}

KeyboardKeyMap::~KeyboardKeyMap()
{
}

void KeyboardKeyMap::MapKey(KeyCode keyCode, ControlCode controlCode)
{
	uint8_t nativeKeyCode = mAgnosticKeyMap[(uint8_t)keyCode];
	mNativeKeyMap[nativeKeyCode] = controlCode;
}

ControlCode KeyboardKeyMap::GetControlCode(uint8_t nativeKeyCode) const
{
	return mNativeKeyMap[nativeKeyCode];
}

void KeyboardKeyMap::InitializeKeyMap()
{
	memset(mAgnosticKeyMap, 0, sizeof(KeyCode) * 256);

	mAgnosticKeyMap[(uint8_t)KeyCode::Esc] = VK_ESCAPE;

	mAgnosticKeyMap[(uint8_t)KeyCode::F1]  = VK_F1;
	mAgnosticKeyMap[(uint8_t)KeyCode::F2]  = VK_F2;
	mAgnosticKeyMap[(uint8_t)KeyCode::F3]  = VK_F3;
	mAgnosticKeyMap[(uint8_t)KeyCode::F4]  = VK_F4;
	mAgnosticKeyMap[(uint8_t)KeyCode::F5]  = VK_F5;
	mAgnosticKeyMap[(uint8_t)KeyCode::F6]  = VK_F6;
	mAgnosticKeyMap[(uint8_t)KeyCode::F7]  = VK_F7;
	mAgnosticKeyMap[(uint8_t)KeyCode::F8]  = VK_F8;
	mAgnosticKeyMap[(uint8_t)KeyCode::F9]  = VK_F9;
	mAgnosticKeyMap[(uint8_t)KeyCode::F10] = VK_F10;
	mAgnosticKeyMap[(uint8_t)KeyCode::F11] = VK_F11;
	mAgnosticKeyMap[(uint8_t)KeyCode::F12] = VK_F12;

	mAgnosticKeyMap[(uint8_t)KeyCode::Tilde]     = VK_OEM_3;
	mAgnosticKeyMap[(uint8_t)KeyCode::One]       = '1';
	mAgnosticKeyMap[(uint8_t)KeyCode::Two]       = '2';
	mAgnosticKeyMap[(uint8_t)KeyCode::Three]     = '3';
	mAgnosticKeyMap[(uint8_t)KeyCode::Four]      = '4';
	mAgnosticKeyMap[(uint8_t)KeyCode::Five]      = '5';
	mAgnosticKeyMap[(uint8_t)KeyCode::Six]       = '6';
	mAgnosticKeyMap[(uint8_t)KeyCode::Seven]     = '7';
	mAgnosticKeyMap[(uint8_t)KeyCode::Eight]     = '8';
	mAgnosticKeyMap[(uint8_t)KeyCode::Nine]      = '9';
	mAgnosticKeyMap[(uint8_t)KeyCode::Zero]      = '0';
	mAgnosticKeyMap[(uint8_t)KeyCode::Minus]     = VK_OEM_MINUS;
	mAgnosticKeyMap[(uint8_t)KeyCode::Plus]      = VK_OEM_PLUS;
	mAgnosticKeyMap[(uint8_t)KeyCode::Backspace] = VK_BACK;

	mAgnosticKeyMap[(uint8_t)KeyCode::Tab]         = VK_TAB;
	mAgnosticKeyMap[(uint8_t)KeyCode::Q]           = 'Q';
	mAgnosticKeyMap[(uint8_t)KeyCode::W]           = 'W';
	mAgnosticKeyMap[(uint8_t)KeyCode::E]           = 'E';
	mAgnosticKeyMap[(uint8_t)KeyCode::R]           = 'R';
	mAgnosticKeyMap[(uint8_t)KeyCode::T]           = 'T';
	mAgnosticKeyMap[(uint8_t)KeyCode::Y]           = 'Y';
	mAgnosticKeyMap[(uint8_t)KeyCode::U]           = 'U';
	mAgnosticKeyMap[(uint8_t)KeyCode::I]           = 'I';
	mAgnosticKeyMap[(uint8_t)KeyCode::O]           = 'O';
	mAgnosticKeyMap[(uint8_t)KeyCode::P]           = 'P';
	mAgnosticKeyMap[(uint8_t)KeyCode::OpenBrace]   = VK_OEM_4;
	mAgnosticKeyMap[(uint8_t)KeyCode::ClosedBrace] = VK_OEM_6;
	mAgnosticKeyMap[(uint8_t)KeyCode::VerticalBar] = VK_OEM_5;

	mAgnosticKeyMap[(uint8_t)KeyCode::CapsLock] = VK_CAPITAL; //Karl Marx. Das Kapital.
	mAgnosticKeyMap[(uint8_t)KeyCode::A]        = 'A';
	mAgnosticKeyMap[(uint8_t)KeyCode::S]        = 'S';
	mAgnosticKeyMap[(uint8_t)KeyCode::D]        = 'D';
	mAgnosticKeyMap[(uint8_t)KeyCode::F]        = 'F';
	mAgnosticKeyMap[(uint8_t)KeyCode::G]        = 'G';
	mAgnosticKeyMap[(uint8_t)KeyCode::H]        = 'H';
	mAgnosticKeyMap[(uint8_t)KeyCode::J]        = 'J';
	mAgnosticKeyMap[(uint8_t)KeyCode::K]        = 'K';
	mAgnosticKeyMap[(uint8_t)KeyCode::L]        = 'L';
	mAgnosticKeyMap[(uint8_t)KeyCode::Colon]    = VK_OEM_1;
	mAgnosticKeyMap[(uint8_t)KeyCode::Quote]    = VK_OEM_7;
	mAgnosticKeyMap[(uint8_t)KeyCode::Enter]    = VK_RETURN;

	mAgnosticKeyMap[(uint8_t)KeyCode::LeftShift]    = VK_LSHIFT;
	mAgnosticKeyMap[(uint8_t)KeyCode::Z]            = 'Z';
	mAgnosticKeyMap[(uint8_t)KeyCode::X]            = 'X';
	mAgnosticKeyMap[(uint8_t)KeyCode::C]            = 'C';
	mAgnosticKeyMap[(uint8_t)KeyCode::V]            = 'V';
	mAgnosticKeyMap[(uint8_t)KeyCode::B]            = 'B';
	mAgnosticKeyMap[(uint8_t)KeyCode::N]            = 'N';
	mAgnosticKeyMap[(uint8_t)KeyCode::M]            = 'M';
	mAgnosticKeyMap[(uint8_t)KeyCode::LessThan]     = VK_OEM_COMMA;
	mAgnosticKeyMap[(uint8_t)KeyCode::MoreThan]     = VK_OEM_PERIOD;
	mAgnosticKeyMap[(uint8_t)KeyCode::QuestionMark] = VK_OEM_2;
	mAgnosticKeyMap[(uint8_t)KeyCode::RightShift]   = VK_RSHIFT;

	mAgnosticKeyMap[(uint8_t)KeyCode::LeftCtrl]  = VK_LCONTROL;
	mAgnosticKeyMap[(uint8_t)KeyCode::LeftSys]   = VK_LWIN;
	mAgnosticKeyMap[(uint8_t)KeyCode::LeftAlt]   = VK_LMENU;
	mAgnosticKeyMap[(uint8_t)KeyCode::Spacebar]  = VK_SPACE;
	mAgnosticKeyMap[(uint8_t)KeyCode::RightAlt]  = VK_RMENU;
	mAgnosticKeyMap[(uint8_t)KeyCode::RightSys]  = VK_RWIN; //This one doesn't normally work
	mAgnosticKeyMap[(uint8_t)KeyCode::Menu]      = VK_APPS;
	mAgnosticKeyMap[(uint8_t)KeyCode::RightCtrl] = VK_RCONTROL;

	mAgnosticKeyMap[(uint8_t)KeyCode::PrintScreen] = VK_SNAPSHOT; //This one doesn't normally work
	mAgnosticKeyMap[(uint8_t)KeyCode::ScrollLock]  = VK_SCROLL;
	mAgnosticKeyMap[(uint8_t)KeyCode::PauseBreak]  = VK_PAUSE;

	mAgnosticKeyMap[(uint8_t)KeyCode::Insert]   = VK_INSERT;
	mAgnosticKeyMap[(uint8_t)KeyCode::Delete]   = VK_DELETE;
	mAgnosticKeyMap[(uint8_t)KeyCode::Home]     = VK_HOME;
	mAgnosticKeyMap[(uint8_t)KeyCode::End]      = VK_END;
	mAgnosticKeyMap[(uint8_t)KeyCode::PageUp]   = VK_PRIOR;
	mAgnosticKeyMap[(uint8_t)KeyCode::PageDown] = VK_NEXT;

	mAgnosticKeyMap[(uint8_t)KeyCode::Up]    = VK_UP;
	mAgnosticKeyMap[(uint8_t)KeyCode::Left]  = VK_LEFT;
	mAgnosticKeyMap[(uint8_t)KeyCode::Down]  = VK_DOWN;
	mAgnosticKeyMap[(uint8_t)KeyCode::Right] = VK_RIGHT;

	mAgnosticKeyMap[(uint8_t)KeyCode::NumLock]     = VK_NUMLOCK;
	mAgnosticKeyMap[(uint8_t)KeyCode::NumSlash]    = VK_DIVIDE;
	mAgnosticKeyMap[(uint8_t)KeyCode::NumAsterisk] = VK_MULTIPLY;
	mAgnosticKeyMap[(uint8_t)KeyCode::NumMinus]    = VK_SUBTRACT;
	mAgnosticKeyMap[(uint8_t)KeyCode::NumSeven]    = VK_NUMPAD7;
	mAgnosticKeyMap[(uint8_t)KeyCode::NumEight]    = VK_NUMPAD8;
	mAgnosticKeyMap[(uint8_t)KeyCode::NumNine]     = VK_NUMPAD9;
	mAgnosticKeyMap[(uint8_t)KeyCode::NumPlus]     = VK_ADD;
	mAgnosticKeyMap[(uint8_t)KeyCode::NumFour]     = VK_NUMPAD4;
	mAgnosticKeyMap[(uint8_t)KeyCode::NumFive]     = VK_NUMPAD5;
	mAgnosticKeyMap[(uint8_t)KeyCode::NumSix]      = VK_NUMPAD6;
	mAgnosticKeyMap[(uint8_t)KeyCode::NumOne]      = VK_NUMPAD1;
	mAgnosticKeyMap[(uint8_t)KeyCode::NumTwo]      = VK_NUMPAD2;
	mAgnosticKeyMap[(uint8_t)KeyCode::NumThree]    = VK_NUMPAD3;
	mAgnosticKeyMap[(uint8_t)KeyCode::NumEnter]    = VK_SEPARATOR;
	mAgnosticKeyMap[(uint8_t)KeyCode::NumZero]     = VK_NUMPAD0;
	mAgnosticKeyMap[(uint8_t)KeyCode::NumDot]      = VK_DECIMAL;
}
