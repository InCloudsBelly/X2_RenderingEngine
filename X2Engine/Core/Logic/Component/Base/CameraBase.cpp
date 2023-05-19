#include "CameraBase.h"
#include "Core/Logic/Object/GameObject.h"
#include <glm/vec4.hpp>
//#include "Utils/Log.h"
#include <rttr/registration>
#include <iostream>
#include "Core/Graphic/Instance/Buffer.h"
#include "Core/Graphic/Command/CommandBuffer.h"
#include "Asset/Mesh.h"
#include "Core/Graphic/Instance/Image.h"
#include "Core/Graphic/CoreObject/GraphicInstance.h"
#include "Core/Graphic/Manager/RenderPassManager.h"
#include "Core/Graphic/Manager/RenderPipelineManager.h"

CameraBase* CameraBase::mainCamera = nullptr;

RTTR_REGISTRATION
{
	using namespace rttr;
	registration::class_<CameraBase>("CameraBase");
}

glm::mat4 CameraBase::getViewMatrix()
{
	return m_cameraInfo.view;
}

glm::mat4 CameraBase::getProjectionMatrix()
{
	return m_cameraInfo.projection;
}

const glm::vec4* CameraBase::getClipPlanes()
{
	onSetClipPlanes(m_cameraInfo.clipPlanes);
	return m_cameraInfo.clipPlanes;
}


void CameraBase::refreshCameraInfo()
{
	m_cameraInfo.type = static_cast<int>(cameraType);
	m_cameraInfo.nearFlat = nearFlat;
	m_cameraInfo.farFlat = farFlat;
	m_cameraInfo.aspectRatio = aspectRatio;
	m_cameraInfo.position = getGameObject()->transform.getTranslation();
	onSetParameter(m_cameraInfo.parameter);
	onSetSize(m_cameraInfo.halfSize);
	//m_cameraInfo.forward = glm::normalize(m_cameraInfo.forward);
	//m_cameraInfo.right = glm::normalize(m_cameraInfo.right);
	
	glm::vec3 rotation = getGameObject()->transform.getRotation();
	float yaw = rotation.y;
	float pitch = rotation.x;

	m_cameraInfo.forward = glm::normalize(glm::vec3{ glm::cos(pitch)* glm::cos(yaw), glm::sin(pitch), glm::cos(pitch)* glm::sin(yaw) });
	m_cameraInfo.right = glm::normalize(glm::cross(glm::vec3(0, 1, 0), -m_cameraInfo.forward));
	//m_cameraInfo.right = getGameObject()->transform.getModelMatrix() * glm::vec4(1, 0, 0, 0);
	////std::cout << "right : " << m_cameraInfo.right.x << ", " << m_cameraInfo.right.y << ", " << m_cameraInfo.right.z << std::endl;
	//m_cameraInfo.right = glm::cross(m_cameraInfo.forward, glm::vec3(0, 1, 0));
	

	onSetClipPlanes(m_cameraInfo.clipPlanes);
	onSetProjectionMatrix(m_cameraInfo.projection);

	glm::vec3 eyePos = m_cameraInfo.position;
	glm::vec3 targetPos = eyePos+ m_cameraInfo.forward;
	glm::vec3 up = glm::normalize(glm::cross(m_cameraInfo.right, m_cameraInfo.forward));
	m_cameraInfo.view = glm::lookAt(eyePos, targetPos, up);

	m_intersectionChecker.setIntersectPlanes(m_cameraInfo.clipPlanes, 6);

	m_buffer->WriteData(&m_cameraInfo, sizeof(CameraData));
}

Buffer* CameraBase::getCameraInfoBuffer()
{
	return m_buffer;
}

bool CameraBase::checkInFrustum(std::array<glm::vec3, 8>& vertexes, glm::mat4& matrix)
{
	return m_intersectionChecker.check(vertexes.data(), 8, matrix);
}

void CameraBase::getAngularPointVPosition(glm::vec3(&points)[8])
{
	const glm::vec3 nPositions[8] = {
		{-1, 1, 0},
		{-1, -1, 0},
		{1, -1, 0},
		{1, 1, 0},
		{-1, 1, 1},
		{-1, -1, 1},
		{1, -1, 1},
		{1, 1, 1}
	};
	if (cameraType == CameraType::ORTHOGRAPHIC)
	{
		for (int i = 0; i < 8; i++)
		{
			auto& ndcPosition = nPositions[i];
			glm::vec3 nearFlatPosition = glm::vec3(glm::vec2(ndcPosition.x, ndcPosition.y) * m_cameraInfo.halfSize, -m_cameraInfo.nearFlat);
			glm::vec3 farFlatPosition = glm::vec3(glm::vec2(ndcPosition.x, ndcPosition.y) * m_cameraInfo.halfSize, -m_cameraInfo.farFlat);
			float linearDepth = ndcPosition.z;
			points[i] = nearFlatPosition * (1 - linearDepth) + farFlatPosition * linearDepth;
		}
	}
	else if (cameraType == CameraType::PERSPECTIVE)
	{
		for (int i = 0; i < 8; i++)
		{
			auto& ndcPosition = nPositions[i];
			glm::vec3 nearFlatPosition = glm::vec3(glm::vec2(ndcPosition.x, ndcPosition.y) * m_cameraInfo.halfSize, -m_cameraInfo.nearFlat);
			glm::vec3 farFlatPosition = glm::vec3(glm::vec2(ndcPosition.x, ndcPosition.y) * m_cameraInfo.halfSize * m_cameraInfo.farFlat / m_cameraInfo.nearFlat, -m_cameraInfo.farFlat);
			float linearDepth = m_cameraInfo.nearFlat * ndcPosition.z / (ndcPosition.z * (m_cameraInfo.nearFlat - m_cameraInfo.farFlat) + m_cameraInfo.farFlat);
			points[i] = nearFlatPosition * (1 - linearDepth) + farFlatPosition * linearDepth;
		}
	}
}

void CameraBase::setRendererName(std::string rendererName)
{
	Instance::getRenderPipelineManager()->destroyRendererData(this);
	m_rendererName = rendererName;
	Instance::getRenderPipelineManager()->createRendererData(this);
}

std::string CameraBase::getRendererName()
{
	return m_rendererName;
}

void CameraBase::refreshRenderer()
{
	Instance::getRenderPipelineManager()->refreshRendererData(this);
}

CameraBase::CameraData* CameraBase::getCameraData()
{
	 return &m_cameraInfo; 
}

RendererDataBase* CameraBase::getRendererData()
{
	return Instance::getRenderPipelineManager()->getRendererData(this);
}

void CameraBase::onAwake()
{
}

void CameraBase::onStart()
{
}

void CameraBase::onUpdate()
{
}

void CameraBase::onDestroy()
{
	Instance::getRenderPipelineManager()->destroyRendererData(this);
	delete m_buffer;

	for (auto attachment : attachments)
		delete attachment.second;
}

CameraBase::CameraBase(CameraType cameraType, std::string rendererName, std::map<std::string,Image*> attachments)
	: Component(ComponentType::CAMERA)
	, attachments(attachments)
	, cameraType(cameraType)
	, nearFlat(3.0f)
	, farFlat(100.0f)
	, aspectRatio(16.0f / 9.0f)
	, m_cameraInfo()
	, m_projectionMatrix(glm::mat4(1.0f))
	, m_intersectionChecker()
	, m_buffer(new Buffer(sizeof(CameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_ONLY))
	, m_rendererName(rendererName)
{
	Instance::getRenderPipelineManager()->createRendererData(this);
}

CameraBase::~CameraBase()
{
	
}

void CameraBase::onSetParameter(glm::vec4& parameter)
{
}

void CameraBase::setForwardDir(glm::vec3& newDir)
{
	m_cameraInfo.forward = newDir;
}

void CameraBase::setRightDir(glm::vec3& newDir)
{
	m_cameraInfo.right = newDir;
}
