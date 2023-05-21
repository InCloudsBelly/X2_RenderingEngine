#pragma once
#include <array>
#include <glm/glm.hpp>
#include <vector>

class LightBase;

class Component;

class Instance;

class Buffer;
class Image;

class CommandPool;
class CommandBuffer;

class Material;

class DirectionalLight;
class AmbientLight;


class LightManager final
{
public:
#define MAX_ORTHER_LIGHT_COUNT 256
#define MAX_FORWARD_ORTHER_LIGHT_COUNT 4
#define MAX_TILE_BASED_FORWARD_ORTHER_LIGHT_COUNT 256
	LightManager();
	~LightManager();
	LightManager(const LightManager&) = delete;
	LightManager& operator=(const LightManager&) = delete;
	LightManager(LightManager&&) = delete;
	LightManager& operator=(LightManager&&) = delete;
	struct LightInfo
	{
		alignas(4) int type;
		alignas(4) float intensity;
		alignas(4) float minRange;
		alignas(4) float maxRange;
		alignas(16) glm::vec4 extraParamter;
		alignas(16) glm::vec3 position;
		alignas(16) glm::vec3 direction;
		alignas(16) glm::vec4 color;
	};
	struct LightBoundingBox { glm::vec4 vertexes[8]; };
	struct ForwardLightInfos
	{
		LightInfo ambientLightInfo;
		LightInfo mainLightInfo;
		int importantLightCount;
		LightInfo importantLightInfos[MAX_FORWARD_ORTHER_LIGHT_COUNT];
		int unimportantLightCount;
		LightInfo unimportantLightInfos[MAX_FORWARD_ORTHER_LIGHT_COUNT];
	};
	struct TileBasedForwardLightInfos
	{
		LightInfo ambientLightInfo;
		LightInfo mainLightInfo;
		int ortherLightCount;
		LightInfo ortherLightInfos[MAX_TILE_BASED_FORWARD_ORTHER_LIGHT_COUNT];
	};
	struct TileBasedForwardLightBoundingBoxInfos
	{
		alignas(4) int lightCount;
		alignas(16) LightBoundingBox lightBoundingBoxInfos[MAX_TILE_BASED_FORWARD_ORTHER_LIGHT_COUNT];
	};
	void setLightInfo(std::vector<Component*> lights);
	AmbientLight* getAmbientLight();
	Buffer* getForwardLightInfosBuffer();
	Buffer* getTileBasedForwardLightInfosBuffer();
	Buffer* getTileBasedForwardLightBoundindBoxInfosBuffer();
	LightInfo getMainLightInfo();
	DirectionalLight* getMainLight();
	void setAmbientLightParameters(Material* material) const;
private:
	Buffer* m_forwardLightInfosBuffer;
	Buffer* m_tileBasedForwardLightInfosBuffer;
	Buffer* m_tileBasedForwardLightBoundingBoxInfosBuffer;

	LightInfo m_ambientLightInfo;
	//Image* m_irradianceCubeImage;
	//Image* m_prefilteredCubeImage;
	//Image* m_lutImage;
	LightBase* m_ambientLight;
	LightInfo m_mainLightInfo;
	LightBase* m_mainLight;
	int m_ortherLightCount;
	std::array<LightInfo, MAX_ORTHER_LIGHT_COUNT> m_ortherLightInfos;
	std::array<LightBoundingBox, MAX_ORTHER_LIGHT_COUNT> m_ortherLightBoundingBoxInfos;
};