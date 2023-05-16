#include "InputManager.h"
#include <crtdbg.h>
#include "Core/Graphic/CoreObject/Window.h"

int InputManager::m_CursorStatus = 1;
std::array<GLboolean, 1024>  InputManager::m_KeysStatus = { GL_FALSE };
std::array<GLboolean, 3>     InputManager::m_MouseButtonStatus = { GL_FALSE };
std::array<GLdouble, 2>      InputManager::m_CursorPos = { 0.0 };
std::array<GLdouble, 2>      InputManager::m_CursorOffset = { 0.0 };
std::array<GLdouble, 2>      InputManager::m_CursorPosLastFrame = { 0.0 };
std::array<GLdouble, 2>      InputManager::m_ScrollJourney = { 0.0 };
std::vector<std::function<void(int, int, int)>>          InputManager::m_MouseButtonCallbackResponseFuncSet;
std::vector<std::function<void(GLdouble, GLdouble)>>     InputManager::m_CursorCallbackResponseFuncSet;
std::vector<std::function<void(GLdouble, GLdouble)>>     InputManager::m_ScrollCallbackResponseFuncSet;
std::vector<std::function<void(int, int, int, int)>>	 InputManager::m_KeyCallbackResponseFuncSet;

InputManager::InputManager()
{
}

InputManager::~InputManager()
{
}

//***********************************************************************************
//Function:
void InputManager::init(GLFWwindow* window)
{
	registerCallbackFunc(window);
}

//************************************************************************************
//Function:
void InputManager::registerCallbackFunc(GLFWwindow* window)
{
	glfwSetKeyCallback(window, keyCallbackFunc);
	glfwSetMouseButtonCallback(window, mouseButtonCallbackFunc);
	glfwSetCursorPosCallback(window, cursorCallbackFunc);
	glfwSetScrollCallback(window, scrollCallbackFunc);
}

//************************************************************************************
//Function:
void InputManager::keyCallbackFunc(GLFWwindow* vWindow, int vKey, int vScancode, int vAction, int vMode)
{
	if (vKey == GLFW_KEY_ESCAPE && vAction == GLFW_PRESS)
		glfwSetWindowShouldClose(vWindow, GL_TRUE);

	m_KeysStatus[vKey] = vAction;

	/*if (m_KeysStatus[GLFW_KEY_LEFT_ALT] == GLFW_PRESS && m_KeysStatus[GLFW_KEY_C] == GLFW_PRESS)
	{
		m_CursorStatus = !m_CursorStatus;
		glfwSetInputMode(ResourceManager::getOrCreateInstance()->fetchOrCreateGLFWWindow()->fetchWindow(), GLFW_CURSOR, m_CursorStatus ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
		if (m_CursorStatus == 1)
			m_CursorStatus = 2;
	}*/

	if (!m_KeyCallbackResponseFuncSet.empty())
	{
		for (auto vIt : m_KeyCallbackResponseFuncSet)
			vIt(vKey, vScancode, vAction, vMode);
	}
}

//************************************************************************************
//Function:
void InputManager::mouseButtonCallbackFunc(GLFWwindow* vWindow, int vButton, int vAction, int vMods)
{
	m_MouseButtonStatus[vButton - GLFW_MOUSE_BUTTON_LEFT] = vAction;   //GLFWÖÐdefine GLFW_MOUSE_BUTTON_LEFT 0£¬define GLFW_PRESS 1

	if (!m_MouseButtonCallbackResponseFuncSet.empty())
	{
		for (auto vIt : m_MouseButtonCallbackResponseFuncSet)
			vIt(vButton, vAction, vMods);
	}
}

//************************************************************************************
//Function:
void InputManager::cursorCallbackFunc(GLFWwindow* vWindow, GLdouble vPosX, GLdouble vPosY)
{
	m_CursorPos[0] = vPosX;
	m_CursorPos[1] = vPosY;
	if (m_CursorStatus == 2)
	{
		m_CursorOffset[1] = m_CursorOffset[0] = 0;
		m_CursorStatus = 1;
	}
	else
	{
		m_CursorOffset[0] = m_CursorPos[0] - m_CursorPosLastFrame[0];
		m_CursorOffset[1] = m_CursorPosLastFrame[1] - m_CursorPos[1];
	}
	m_CursorPosLastFrame = m_CursorPos;

	if (!m_CursorCallbackResponseFuncSet.empty())
	{
		for (auto vIt : m_CursorCallbackResponseFuncSet)
			vIt(vPosX, vPosY);
	}
}

//************************************************************************************
//Function:
void InputManager::scrollCallbackFunc(GLFWwindow* vWindow, GLdouble vOffsetX, GLdouble vOffsetY)
{
	m_ScrollJourney[0] += vOffsetX;
	m_ScrollJourney[1] += vOffsetY;

	if (!m_ScrollCallbackResponseFuncSet.empty())
	{
		for (auto vIt : m_ScrollCallbackResponseFuncSet)
			vIt(vOffsetX, vOffsetY);
	}
}

//************************************************************************************
//Function:
const std::array<GLboolean, 1024>& InputManager::getKeyStatus() const
{
	return m_KeysStatus;
}

//************************************************************************************
//Function:
const std::array<GLboolean, 3>& InputManager::getMouseButtonStatus() const
{
	return m_MouseButtonStatus;
}

//************************************************************************************
//Function:
const std::array<GLdouble, 2>& InputManager::getCursorPos() const
{
	return m_CursorPos;
}

const std::array<GLdouble, 2>& InputManager::getCursorOffset() const
{
	return m_CursorOffset;
}

//************************************************************************************
//Function:
const std::array<GLdouble, 2>& InputManager::getScrollJourney() const
{
	return m_ScrollJourney;
}

//************************************************************************************
//Function:
int InputManager::registerCursorCallbackFunc(std::function<void(GLdouble, GLdouble)> vCursorCallbackFunc)
{
	m_CursorCallbackResponseFuncSet.push_back(vCursorCallbackFunc);
	return static_cast<int>(m_CursorCallbackResponseFuncSet.size()) - 1;
}

//************************************************************************************
//Function:
int InputManager::registerKeyCallbackFunc(std::function<void(int, int, int, int)> vKeyCallbackFunc)
{
	m_KeyCallbackResponseFuncSet.push_back(vKeyCallbackFunc);
	return static_cast<int>(m_KeyCallbackResponseFuncSet.size()) - 1;
}

//************************************************************************************
//Function:
int InputManager::registerMouseButtonCallbackFunc(std::function<void(int, int, int)> vMouseButtonCallbackFunc)
{
	m_MouseButtonCallbackResponseFuncSet.push_back(vMouseButtonCallbackFunc);
	return static_cast<int>(m_MouseButtonCallbackResponseFuncSet.size()) - 1;
}

//************************************************************************************
//Function:
int InputManager::registerScrollCallbackFunc(std::function<void(GLdouble, GLdouble)> vScrollCallbackFunc)
{
	m_ScrollCallbackResponseFuncSet.push_back(vScrollCallbackFunc);
	return static_cast<int>(m_ScrollCallbackResponseFuncSet.size()) - 1;
}

//************************************************************************************
//Function:  no test...
void InputManager::unregisterKeyCallbackFunc(int vEraseIndex)
{
	m_ScrollCallbackResponseFuncSet.erase(m_ScrollCallbackResponseFuncSet.begin() + vEraseIndex);
}

//************************************************************************************
//Function:
void InputManager::unregisterCursorCallbackFunc(int vEraseIndex)
{
	m_CursorCallbackResponseFuncSet.erase(m_CursorCallbackResponseFuncSet.begin() + vEraseIndex);
}

//************************************************************************************
//Function:
void InputManager::unregisterScrollCallbackFunc(int vEraseIndex)
{
	m_ScrollCallbackResponseFuncSet.erase(m_ScrollCallbackResponseFuncSet.begin() + vEraseIndex);
}

//************************************************************************************
//Function:
void InputManager::unregisterMouseButtonCallbackFunc(int vEraseIndex)
{
	m_MouseButtonCallbackResponseFuncSet.erase(m_MouseButtonCallbackResponseFuncSet.begin() + vEraseIndex);
}

//************************************************************************************
//Function:
void InputManager::unregisterKeyCallbackFunc(std::function<void(int, int, int, int)> vKeyCallbackFunc)
{
	//To do...
}

//************************************************************************************
//Function:
void InputManager::unregisterCursorCallbackFunc(std::function<void(GLdouble, GLdouble)> vCursorCallbackFunc)
{
	//To do...
}

//************************************************************************************
//Function:
void InputManager::unregisterScrollCallbackFunc(std::function<void(GLdouble, GLdouble)> vScrollCallbackFunc)
{
	//To do...
}

//************************************************************************************
//Function:
void InputManager::unregisterMouseButtonCallbackFunc(std::function<void(int, int, int)> vMouseButtonCallbackFunc)
{
	//To do...
}