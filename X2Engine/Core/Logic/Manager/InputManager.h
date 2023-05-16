#pragma once
#include <GLFW/glfw3.h>
#include <functional>
#include <vector>
#include <array>

class InputManager
{
public:
	InputManager();
	~InputManager();

	GLvoid init(GLFWwindow* window);

	GLint registerKeyCallbackFunc(std::function<GLvoid(GLint, GLint, GLint, GLint)> vKeyCallbackFunc);
	GLint registerCursorCallbackFunc(std::function<GLvoid(GLdouble, GLdouble)> vCursorCallbackFunc);
	GLint registerScrollCallbackFunc(std::function<GLvoid(GLdouble, GLdouble)> vScrollCallbackFunc);
	GLint registerMouseButtonCallbackFunc(std::function<GLvoid(GLint, GLint, GLint)> vMouseButtonCallbackFunc);

	GLvoid unregisterKeyCallbackFunc(std::function<GLvoid(GLint, GLint, GLint, GLint)> vKeyCallbackFunc);
	GLvoid unregisterCursorCallbackFunc(std::function<GLvoid(GLdouble, GLdouble)> vCursorCallbackFunc);
	GLvoid unregisterScrollCallbackFunc(std::function<GLvoid(GLdouble, GLdouble)> vScrollCallbackFunc);
	GLvoid unregisterMouseButtonCallbackFunc(std::function<GLvoid(GLint, GLint, GLint)> vMouseButtonCallbackFunc);

	GLvoid unregisterKeyCallbackFunc(GLint vEraseIndex);
	GLvoid unregisterCursorCallbackFunc(GLint vEraseIndex);
	GLvoid unregisterScrollCallbackFunc(GLint vEraseIndex);
	GLvoid unregisterMouseButtonCallbackFunc(GLint vEraseIndex);

	const std::array<GLboolean, 1024>& getKeyStatus() const;
	const std::array<GLboolean, 3>& getMouseButtonStatus() const;
	const std::array<GLdouble, 2>& getCursorPos() const;
	const std::array<GLdouble, 2>& getCursorOffset() const;
	const std::array<GLdouble, 2>& getScrollJourney() const;
	GLboolean						   getCursorStatus() const { return m_CursorStatus; }

	void setCursorStatus(GLboolean vCursorStatus) { m_CursorStatus; }

private:
	static GLint m_CursorStatus;

	static std::array<GLboolean, 1024>     m_KeysStatus;
	static std::array<GLboolean, 3>        m_MouseButtonStatus;
	static std::array<GLdouble, 2>         m_CursorPos;
	static std::array<GLdouble, 2>         m_CursorPosLastFrame;
	static std::array<GLdouble, 2>         m_CursorOffset;
	static std::array<GLdouble, 2>         m_ScrollJourney;		//���ִ�һ��ʼ�ۼƻ�����·��

	static std::vector<std::function<GLvoid(GLint, GLint, GLint)>>        m_MouseButtonCallbackResponseFuncSet;
	static std::vector<std::function<GLvoid(GLdouble, GLdouble)>>         m_CursorCallbackResponseFuncSet;
	static std::vector<std::function<GLvoid(GLdouble, GLdouble)>>         m_ScrollCallbackResponseFuncSet;
	static std::vector<std::function<GLvoid(GLint, GLint, GLint, GLint)>> m_KeyCallbackResponseFuncSet;


	GLvoid registerCallbackFunc(GLFWwindow* window);
	static GLvoid keyCallbackFunc(GLFWwindow* vWindow, GLint vKey, GLint vScancode, GLint vAction, GLint vMode);
	static GLvoid cursorCallbackFunc(GLFWwindow* vWindow, GLdouble vPosX, GLdouble vPosY);
	static GLvoid scrollCallbackFunc(GLFWwindow* vWindow, GLdouble vOffsetX, GLdouble vOffsetY);
	static GLvoid mouseButtonCallbackFunc(GLFWwindow* vWindow, GLint vButton, GLint vAction, GLint vMods);
};