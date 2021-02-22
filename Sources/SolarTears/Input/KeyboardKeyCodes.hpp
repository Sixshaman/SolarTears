#pragma once

#include <cstdint>

enum class KeyCode: uint8_t
{
	Unknown = 0,

	Esc,           F1,  F2,  F3,  F4,           F5,  F6,  F7,  F8,          F9,  F10,  F11,  F12,                                                        PrintScreen, ScrollLock, PauseBreak,

	Tilde,            One, Two, Three, Four, Five, Six, Seven, Eight, Nine, Zero, Minus, Plus,                                          Backspace,       Insert,      Home,       PageUp,         NumLock,   NumSlash,  NumAsterisk,  NumMinus,
	Tab,                          Q,   W,   E,   R,   T,   Y,   U,   I,   O,   P,                 OpenBrace, ClosedBrace, VerticalBar,                   Delete,      End,        PageDown,       NumSeven,  NumEight,  NumNine,      NumPlus,
	CapsLock,                      A,   S,   D,   F,   G,   H,   J,   K,   L,                     Colon,     Quote,                     Enter,                                                    NumFour,   NumFive,   NumSix,      
	LeftShift,                      Z,   X,   C,   V,   B,   N,   M,                              LessThan,  MoreThan,    QuestionMark, RightShift,                   Up,                         NumOne,    NumTwo,    NumThree,     NumEnter,
	LeftCtrl, LeftSys, LeftAlt,                  Spacebar,                                        RightAlt,  RightSys,    Menu,         RightCtrl,       Left,        Down,       Right,          NumZero,              NumDot
};