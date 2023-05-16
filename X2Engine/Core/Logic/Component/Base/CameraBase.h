#pragma once
#include "Core/Logic/Component/Component.h"
#include <glm/mat4x4.hpp>
#include <array>
#include <glm/vec4.hpp>
#include <map>
#include <string>
#include "Utils/IntersectionChecker.h"
#include <array>

class Mesh;

class Buffer;
class Image;
class ImageSampler;

class CommandBuffer;

class RendererDataBase;

class CameraBase : public Component
{
	friend class Engine;
public:
	enum class CameraType
	{
		ORTHOGRAPHIC = 1,
		PERSPECTIVE = 2
	};
	struct CameraData
	{
		alignas(4)	int type;
		alignas(4)	float nearFlat;
		alignas(4)	float farFlat;
		alignas(4)	float aspectRatio;
		alignas(16)	glm::vec3 position;
		alignas(8)	glm::vec2 halfSize;
		alignas(16)	glm::vec4 parameter;
		alignas(16)	glm::vec3 forward = {0, 0, 1};
		alignas(16)	glm::vec3 right = {-1, 0, 0};
		alignas(16)	glm::vec4 clipPlanes[6];
		alignas(16) glm::mat4 projection;
		alignas(16) glm::mat4 view;
	};
public:
	static CameraBase* mainCamera;
	const CameraType cameraType;
	std::map<std::string,Image*> attachments;
	float nearFlat;
	float farFlat;
	float aspectRatio;
protected:
private:
	std::string m_rendererName;
	Buffer* m_buffer;
	CameraData m_cameraInfo;
	glm::mat4 m_projectionMatrix;
	IntersectionChecker m_intersectionChecker;

public:
	glm::mat4 getViewMatrix();
	glm::mat4 getProjectionMatrix();
	const glm::vec4* getClipPlanes();
	void refreshCameraInfo();
	Buffer* getCameraInfoBuffer();
	bool checkInFrustum(std::array<glm::vec3, 8>& vertexes, glm::mat4& matrix);
	void getAngularPointVPosition(glm::vec3(&points)[8]);

	void setRendererName(std::string rendererName);
	std::string getRendererName();
	void refreshRenderer();
	CameraData* getCameraData();
	RendererDataBase* getRendererData();

	void setForwardDir(glm::vec3& newDir);
	void setRightDir(glm::vec3& newDir);

	void onAwake()override;
	void onStart()override;
	void onUpdate()override;
	void onDestroy()override;

protected:
	CameraBase(CameraType cameraType, std::string rendererName, std::map<std::string,Image*> attachments);
	virtual ~CameraBase();
	virtual void onSetParameter(glm::vec4& parameter);

	virtual void onSetSize(glm::vec2& parameter) = 0;
	virtual void onSetClipPlanes(glm::vec4* clipPlanes) = 0;
	virtual void onSetProjectionMatrix(glm::mat4& matrix) = 0;
private:

	RTTR_ENABLE(Component)
};