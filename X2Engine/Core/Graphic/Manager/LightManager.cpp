#include "LightManager.h"
#include "Core/Graphic/Instance/Buffer.h"
#include "Core/Graphic/Command/CommandBuffer.h"
#include "Core/Logic/Component/Base/LightBase.h"
#include <map>
#include <vma/vk_mem_alloc.h>
#include "Light/AmbientLight.h"

LightManager::LightManager()
	: m_forwardLightInfosBuffer(nullptr)
	, m_tileBasedForwardLightInfosBuffer(nullptr)
	, m_irradianceCubeImage(nullptr)
	, m_prefilteredCubeImage(nullptr)
	, m_lutImage(nullptr)
	, m_ambientLightInfo()
	, m_mainLightInfo()
	, m_ortherLightInfos()
	, m_ortherLightCount()
	, m_ortherLightBoundingBoxInfos()
{
	//m_forwardLightInfosBuffer = new Buffer(sizeof(ForwardLightInfos), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,VMA_MEMORY_USAGE_CPU_ONLY);
	//m_tileBasedForwardLightInfosBuffer = new Buffer(sizeof(TileBasedForwardLightInfos), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
	//m_tileBasedForwardLightBoundingBoxInfosBuffer = new Buffer(sizeof(TileBasedForwardLightBoundingBoxInfos), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
}

LightManager::~LightManager()
{
}

void LightManager::setLightInfo(std::vector<Component*> lights)
{
	std::multimap<LightBase::LightType, LightBase*> unqiueLights = std::multimap<LightBase::LightType, LightBase*>();
	std::vector< LightBase*> otherLights = std::vector< LightBase*>();
	for (const auto& lightComponent : lights)
	{
		LightBase* light = static_cast<LightBase*>(lightComponent);
		switch (light->lightType)
		{
		case LightBase::LightType::DIRECTIONAL:
		{
			unqiueLights.emplace(std::make_pair(light->lightType, light));
			m_mainLight = light;
			break;
		}
		case LightBase::LightType::POINT:
		{
			otherLights.emplace_back(light);
			break;
		}
		case LightBase::LightType::AMBIENT:
		{
			unqiueLights.emplace(std::make_pair(light->lightType, light));
			break;
		}
		case LightBase::LightType::SPOT:
		{
			otherLights.emplace_back(light);
			break;
		}
		default:
			break;
		}
	}

	//ambient
	auto skyBoxIterator = unqiueLights.find(LightBase::LightType::AMBIENT);
	if (skyBoxIterator != std::end(unqiueLights))
	{
		auto data = skyBoxIterator->second->getLightInfo();
		m_ambientLightInfo = *reinterpret_cast<const LightInfo*>(data);
		m_irradianceCubeImage = static_cast<AmbientLight*>(skyBoxIterator->second)->m_irradianceCubeImage;
		m_prefilteredCubeImage = static_cast<AmbientLight*>(skyBoxIterator->second)->m_prefilteredCubeImage;
		m_lutImage = static_cast<AmbientLight*>(skyBoxIterator->second)->m_lutImage;
	}
	else
	{
		m_ambientLightInfo = {};
	}

	//main
	auto directionalIterator = unqiueLights.find(LightBase::LightType::DIRECTIONAL);
	if (directionalIterator != std::end(unqiueLights))
	{
		auto data = directionalIterator->second->getLightInfo();
		m_mainLightInfo = *reinterpret_cast<const LightInfo*>(data);
	}
	else
	{
		m_mainLightInfo = {};
	}

	m_ortherLightInfos = {};
	m_ortherLightBoundingBoxInfos = {};
	m_ortherLightCount = std::min(MAX_ORTHER_LIGHT_COUNT, static_cast<int>(otherLights.size()));
	for (int i = 0; i < m_ortherLightCount; i++)
	{
		auto data = otherLights[i]->getLightInfo();
		auto boxData = otherLights[i]->getBoundingBox();
		m_ortherLightInfos[i] = *reinterpret_cast<const LightInfo*>(data);
		m_ortherLightBoundingBoxInfos[i] = *reinterpret_cast<const LightBoundingBox*>(boxData->data());
	}


	m_forwardLightInfosBuffer->WriteData(
		[this](void* pointer) -> void
		{
			int ortherLightCount = m_ortherLightCount;
			int importantLightCount = std::min(ortherLightCount, MAX_FORWARD_ORTHER_LIGHT_COUNT);
			int unimportantLightCount = std::min(ortherLightCount - importantLightCount, MAX_FORWARD_ORTHER_LIGHT_COUNT);

			auto offset = reinterpret_cast<char*>(pointer);
			memcpy(offset + offsetof(ForwardLightInfos, importantLightCount), &importantLightCount, sizeof(int));
			memcpy(offset + offsetof(ForwardLightInfos, unimportantLightCount), &unimportantLightCount, sizeof(int));
			memcpy(offset + offsetof(ForwardLightInfos, ambientLightInfo), &m_ambientLightInfo, sizeof(LightInfo));
			memcpy(offset + offsetof(ForwardLightInfos, mainLightInfo), &m_mainLightInfo, sizeof(LightInfo));
			memcpy(offset + offsetof(ForwardLightInfos, importantLightInfos), m_ortherLightInfos.data(), sizeof(LightInfo) * importantLightCount);
			memcpy(offset + offsetof(ForwardLightInfos, unimportantLightInfos), m_ortherLightInfos.data() + sizeof(LightInfo) * importantLightCount, sizeof(LightInfo) * unimportantLightCount);
		}
	);

	m_tileBasedForwardLightInfosBuffer->WriteData(
		[this](void* pointer) -> void
		{
			int ortherLightCount = std::min(m_ortherLightCount, MAX_TILE_BASED_FORWARD_ORTHER_LIGHT_COUNT);

			auto offset = reinterpret_cast<char*>(pointer);
			memcpy(offset + offsetof(TileBasedForwardLightInfos, ortherLightCount), &ortherLightCount, sizeof(int));
			memcpy(offset + offsetof(TileBasedForwardLightInfos, ambientLightInfo), &m_ambientLightInfo, sizeof(LightInfo));
			memcpy(offset + offsetof(TileBasedForwardLightInfos, mainLightInfo), &m_mainLightInfo, sizeof(LightInfo));
			memcpy(offset + offsetof(TileBasedForwardLightInfos, ortherLightInfos), m_ortherLightInfos.data(), sizeof(LightInfo) * ortherLightCount);
		}
	);

	m_tileBasedForwardLightBoundingBoxInfosBuffer->WriteData(
		[this](void* pointer) -> void
		{
			int ortherLightCount = std::min(m_ortherLightCount, MAX_TILE_BASED_FORWARD_ORTHER_LIGHT_COUNT);

			auto offset = reinterpret_cast<char*>(pointer);
			memcpy(offset + offsetof(TileBasedForwardLightBoundingBoxInfos, lightCount), &ortherLightCount, sizeof(int));
			memcpy(offset + offsetof(TileBasedForwardLightBoundingBoxInfos, lightBoundingBoxInfos), m_ortherLightBoundingBoxInfos.data(), sizeof(LightBoundingBox) * ortherLightCount);
		}
	);
}

Image* LightManager::getAmbientTextureCube()
{
	return m_irradianceCubeImage;
}

Buffer* LightManager::getForwardLightInfosBuffer()
{
	return m_forwardLightInfosBuffer;
}

Buffer* LightManager::getTileBasedForwardLightInfosBuffer()
{
	return m_tileBasedForwardLightInfosBuffer;
}

Buffer* LightManager::getTileBasedForwardLightBoundindBoxInfosBuffer()
{
	return m_tileBasedForwardLightBoundingBoxInfosBuffer;
}

LightManager::LightInfo LightManager::getMainLightInfo()
{
	return m_mainLightInfo;
}

LightBase* LightManager::getMainLight()
{
	return m_mainLight;
}

void LightManager::setAmbientLightParameters(Material* material, ImageSampler* sampler) const
{
	material->setSampledImageCube("irradianceCubeImage", m_irradianceCubeImage, sampler);
	material->setSampledImageCube("prefilteredCubeImage", m_prefilteredCubeImage, sampler);
	material->setSampledImage2D("lutImage", m_lutImage, sampler);
}
