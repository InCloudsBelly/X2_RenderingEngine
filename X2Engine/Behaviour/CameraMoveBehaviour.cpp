#include "CameraMoveBehaviour.h"
#include "Core/Logic/Component/Base/CameraBase.h"
#include "Camera/PerspectiveCamera.h"

#include "Core/Logic/Object/GameObject.h"
#include "Core/Logic/CoreObject/LogicInstance.h"
#include "Core/Logic/Manager/InputManager.h"

#include <iostream>
#include <glfw/glfw3.h>
#include <algorithm>

RTTR_REGISTRATION
{
	rttr::registration::class_<CameraMoveBehaviour>("CameraMoveBehaviour");
}

CameraMoveBehaviour::CameraMoveBehaviour()
	: m_moveSpeed(0.005),m_sensitivity(0.2),m_isEnableCursor(true)
{
}

CameraMoveBehaviour::~CameraMoveBehaviour()
{
}

void CameraMoveBehaviour::onAwake()
{
}

void CameraMoveBehaviour::onStart()
{
	InputManager* pInputManager = LogicInstance::getInputManager();
	pInputManager->registerScrollCallbackFunc(std::bind(&CameraMoveBehaviour::processScroll4ScrollCallback, this, std::placeholders::_1, std::placeholders::_2));
	pInputManager->registerMouseButtonCallbackFunc(std::bind(&CameraMoveBehaviour::processMouseButton4MouseButtonCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void CameraMoveBehaviour::onUpdate()
{
	auto deltaTime = LogicInstance::m_time.getDeltaDuration();

	auto camera = static_cast<CameraBase*>(getGameObject()->getComponent("CameraBase"));
	CameraBase::CameraData* cameraData = camera->getCameraData();

	glm::vec3 forward = glm::normalize(cameraData->forward);
	glm::vec3 right = glm::normalize(cameraData->right);
	glm::vec3 up = glm::cross(right, forward);

	{
		glm::vec3 posMovement{ 0 };

		float moveDistance = deltaTime * 10;

		if (LogicInstance::getInputManager()->getKeyStatus()[GLFW_KEY_W])
			posMovement += moveDistance * forward;
		if (LogicInstance::getInputManager()->getKeyStatus()[GLFW_KEY_S])
			posMovement -= moveDistance * forward;
		if (LogicInstance::getInputManager()->getKeyStatus()[GLFW_KEY_D])
			posMovement += moveDistance * right;
		if (LogicInstance::getInputManager()->getKeyStatus()[GLFW_KEY_A])
			posMovement -= moveDistance * right;
		if (LogicInstance::getInputManager()->getKeyStatus()[GLFW_KEY_Q])
			posMovement -= moveDistance * 0.8f * up;
		if (LogicInstance::getInputManager()->getKeyStatus()[GLFW_KEY_E])
			posMovement += moveDistance * 0.8f * up;

		getGameObject()->transform.setTranslation(posMovement);
	}


	static double pitch = asin(forward.y);
	static double yaw = asin(forward.z / cos(pitch));

	{
		static std::array<double, 2> preCursorPos = LogicInstance::getInputManager()->getCursorPos();
		std::array<double, 2> curCursorPos = LogicInstance::getInputManager()->getCursorPos();
		std::array<double, 2> CursorOffset = { curCursorPos[0] - preCursorPos[0] ,  preCursorPos[1] - curCursorPos[1] };

		preCursorPos = curCursorPos;

		if (m_isEnableCursor) {
			yaw += glm::radians(CursorOffset[0] * m_sensitivity);
			pitch += glm::radians(CursorOffset[1] * m_sensitivity);

			if (pitch > glm::radians(89.0))
				pitch = glm::radians(89.0);
			else if (pitch < glm::radians(-89.0))
				pitch = glm::radians(-89.0);

			forward.x = cos(pitch) * cos(yaw);
			forward.y = sin(pitch);
			forward.z = cos(pitch) * sin(yaw);
			forward = glm::normalize(forward);
			right = glm::normalize(glm::cross({ 0,1,0 }, -forward));

			camera->setForwardDir(forward);
			camera->setRightDir(right);
		}
	}

}

void CameraMoveBehaviour::onDestroy()
{
	
}


void CameraMoveBehaviour::processScroll4ScrollCallback(double vOffsetX, double vOffsetY)
{
	auto camera = static_cast<CameraBase*>(getGameObject()->getComponent("CameraBase"));
	if (camera->cameraType == CameraBase::CameraType::PERSPECTIVE)
	{
		auto perspectiveCamera = static_cast<PerspectiveCamera*>(camera);
		float& fov = perspectiveCamera->fovAngle;

		if (fov >= 1.0 && fov <= 60.0)
			fov -= vOffsetY;
		if (fov < 1.0)
			fov = 1.0;
		else if (fov > 60.0)
			fov = 60.0;
	}
	
}

//************************************************************************************
//Function:
void CameraMoveBehaviour::processMouseButton4MouseButtonCallback(int vButton, int vAction, int vMods)
{
	if (vButton == GLFW_MOUSE_BUTTON_RIGHT && vAction == GLFW_PRESS)
	{
		m_isEnableCursor = !m_isEnableCursor;
	}
}