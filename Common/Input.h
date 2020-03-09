#pragma once
#include "d3dUtil.h"
#include "BitArray.h"
enum class KeyCode : uint
{
	Backspace = 8,
	Tab = 9,
	Enter = 13,
	Shift = 16,
	Ctrl = 17,
	Alt = 18,
	Pause = 19,
	CapsLock = 20,
	ESC = 27,
	Space = 32,
	PageUp = 33,
	PageDown = 34,
	End = 35,
	Home = 36,
	LeftArrow = 37,
	UpArrow = 38,
	RightArrow = 39,
	DownArrow = 40,
	SnapShot = 44,
	Insert = 45,
	Delete = 46,
	Keyboard0 = 48,
	Keyboard1 = 49,
	Keyboard2 = 50,
	Keyboard3 = 51,
	Keyboard4 = 52,
	Keyboard5 = 53,
	Keyboard6 = 54,
	Keyboard7 = 55,
	Keyboard8 = 56,
	Keyboard9 = 57,
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
	LWindows = 91,
	RWindows = 92,
	Application = 93,
	Numpad0 = 96,
	Numpad1 = 97,
	Numpad2 = 98,
	Numpad3 = 99,
	Numpad4 = 100,
	Numpad5 = 101,
	Numpad6 = 102,
	Numpad7 = 103,
	Numpad8 = 104,
	Numpad9 = 105
};
class D3DApp;
class VEngine;
struct InputData
{
	BitArray keyDownArray;
	BitArray keyUpArray;
	int2 lastMousePosition;
	int2 currentMousePosition;
	WPARAM mouseState;
	bool lMouseDown;
	bool rMouseDown;
	bool mMouseDown;
	bool lMouseUp;
	bool rMouseUp;
	bool mMouseUp;
	constexpr InputData() : 
		keyDownArray(106),
		keyUpArray(106),
		lastMousePosition(0,0),
		currentMousePosition(0,0),
		mouseState(0),
		lMouseDown(false),
		rMouseDown(false),
		mMouseDown(false),
		lMouseUp(false),
		rMouseUp(false),
		mMouseUp(false)
	{

	}
};
class Input
{
	friend class D3DApp;
	friend class VEngine;
private:
	static InputData inputData[2];
	static bool inputDataSwitcher;
	static void OnMoveMouse(int2 num);
	static void UpdateFrame(int2 cursorPos);
public:
	Input() = delete;
	~Input() = delete;
	static bool IsKeyDown(KeyCode keycode);
	static bool IsKeyUp(KeyCode keycode);
	static bool IsKeyPressed(KeyCode keycode);
	static bool IsLeftMouseDown() {
		return inputData[inputDataSwitcher].lMouseDown;
	}
	static bool IsRightMouseDown()
	{
		return inputData[inputDataSwitcher].rMouseDown;
	}
	static bool IsMiddleMouseDown()
	{
		return inputData[inputDataSwitcher].mMouseDown;
	}
	static bool IsLeftMouseUp() {
		return inputData[inputDataSwitcher].lMouseUp;
	}
	static bool IsRightMouseUp()
	{
		return inputData[inputDataSwitcher].rMouseUp;
	}
	static bool IsMiddleMouseUp()
	{
		return inputData[inputDataSwitcher].mMouseUp;
	}
	static int2 MouseMovement();
	static bool isLeftMousePressing()
	{
		return (inputData[inputDataSwitcher].mouseState & MK_LBUTTON) != 0;
	}
	static bool isRightMousePressing()
	{
		return (inputData[inputDataSwitcher].mouseState & MK_RBUTTON) != 0;
	}
	static bool isMiddleMousePressing()
	{
		return (inputData[inputDataSwitcher].mouseState & MK_MBUTTON) != 0;
	}
};