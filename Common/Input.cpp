#include "Input.h"
InputData Input::inputData[2];
bool Input::inputDataSwitcher(false);
bool Input::IsKeyDown(KeyCode keycode)
{
	return inputData[inputDataSwitcher].keyDownArray[(size_t)keycode];
}
bool Input::IsKeyUp(KeyCode keycode)
{
	return inputData[inputDataSwitcher].keyUpArray[(size_t)keycode];
}
bool Input::IsKeyPressed(KeyCode keycode)
{
	return GetAsyncKeyState((int)keycode);
}
int2 Input::MouseMovement()
{
	auto& a = inputData[inputDataSwitcher];
	return {
		a.currentMousePosition.x - a.lastMousePosition.x,
		a.currentMousePosition.y - a.lastMousePosition.y
	};
}

void Input::OnMoveMouse(int2 num)
{
	auto& a = inputData[!inputDataSwitcher];
	a.currentMousePosition.x = num.x;
	a.currentMousePosition.y = num.y;
}
void Input::UpdateFrame(int2 cursorPos)
{
	auto& a = inputData[inputDataSwitcher];
	if (cursorPos.x < 0 || cursorPos.y < 0)
		a.lastMousePosition = a.currentMousePosition;
	else
		a.lastMousePosition = { cursorPos.x, cursorPos.y };
	a.lMouseDown = false;
	a.rMouseDown = false;
	a.mMouseDown = false;
	a.lMouseUp = false;
	a.rMouseUp = false;
	a.mMouseUp = false;
	a.mouseState = 0;
	a.keyDownArray.Reset(false);
	a.keyUpArray.Reset(false);
	inputDataSwitcher = !inputDataSwitcher;
}