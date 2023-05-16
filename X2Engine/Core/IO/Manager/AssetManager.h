#pragma once
#include <rttr/type.h>
#include <mutex>
#include <map>
#include <string>
//#include "Core/IO/CoreObject/Thread.h"
#include "Core/IO/Asset/AssetBase.h"
#include <functional>
#include <string>
#include <vector>
#include <fstream>


struct AssetWrapper
{
	std::string path;
	AssetBase* asset;
	uint32_t referenceCount;
	//std::mutex mutex;
	bool isLoading;
};

class AssetManager final
{
private:
	std::unordered_map<std::string, AssetWrapper*> m_wrappers;
	//std::mutex _mutex;
public:
	AssetManager();
	~AssetManager();
	AssetManager(const AssetManager&) = delete;
	AssetManager& operator=(const AssetManager&) = delete;
	AssetManager(AssetManager&&) = delete;
	AssetManager& operator=(AssetManager&&) = delete;

	/*template<typename TAsset>
	std::future<TAsset*> loadAsync(std::string path);*/
	
	template<typename TAsset>
	TAsset* load(std::string path);
	
	void unload(AssetBase* asset);
	void unload(std::string path);
	void collect();
};
//
//template<typename TAsset>
//std::future< TAsset*> AssetManager::loadAsync(std::string path)
//{
//	{
//		std::ifstream f(path);
//		Utils::Log::Exception("Can not find file: " + path + " .", !f.good());
//	}
//	{
//		//std::unique_lock<std::mutex> managerLock(_mutex);
//		
//		//Get target pool
//		AssetWrapper* targetWrapper = nullptr;
//		auto wrapperIter = m_wrappers.find(path);
//		if (wrapperIter == std::end(m_wrappers))
//		{
//			targetWrapper = new AssetWrapper();
//			targetWrapper->path = path;
//			targetWrapper->referenceCount = 0;
//			targetWrapper->asset = nullptr;
//			targetWrapper->isLoading = true;
//
//			m_wrappers.emplace(path, targetWrapper);
//		}
//		else
//		{
//			targetWrapper = wrapperIter->second;
//		}
//
//
//		{
//			//std::unique_lock<std::mutex> wrapperLock(targetWrapper->mutex);
//
//			//Check
//			targetWrapper->referenceCount++;
//			if (targetWrapper->asset)
//			{
//				return std::async([targetWrapper]()
//					{
//						while (targetWrapper->isLoading)
//						{
//							std::this_thread::yield();
//						}
//						return dynamic_cast<TAsset*>(targetWrapper->asset);
//					});
//			}
//			else
//			{
//				AssetBase* asset = dynamic_cast<AssetBase*>(new TAsset());
//				targetWrapper->asset = asset;
//				asset->m_wrapper = targetWrapper;
//				asset->m_path = path;
//
//				return CoreObject::Thread::AddTask
//				(
//					[targetWrapper](Core::Graphic::Command::CommandBuffer* transferCommandBuffer)->TAsset*
//					{
//						targetWrapper->asset->onLoad(transferCommandBuffer);
//						targetWrapper->isLoading = false;
//						return dynamic_cast<TAsset*>(targetWrapper->asset);
//					}
//				);
//			}
//		}
//	}
//
//}
template<typename TAsset>
TAsset* AssetManager::load(std::string path)
{
	{
		std::ifstream f(path);
		if (!f.good())
			throw(std::runtime_error("Can not find file: " + path + " ."));
	}

	{
		//std::unique_lock<std::mutex> managerLock(_mutex);

		//Get target pool
		AssetWrapper* targetWrapper = nullptr;
		auto wrapperIter = m_wrappers.find(path);

		if (wrapperIter == std::end(m_wrappers))
		{
			targetWrapper = new AssetWrapper();
			targetWrapper->path = path;
			targetWrapper->referenceCount = 0;
			targetWrapper->asset = nullptr;
			targetWrapper->isLoading = true;

			m_wrappers.emplace(path, targetWrapper);
		}
		else
		{
			targetWrapper = wrapperIter->second;
		}


		{
			targetWrapper->referenceCount++;
			if (targetWrapper->asset == nullptr)
			{
				AssetBase* asset = dynamic_cast<AssetBase*>(new TAsset());
				targetWrapper->asset = asset;
				asset->m_wrapper = targetWrapper;
				asset->m_path = path;


				targetWrapper->asset->onLoad(Instance::getTransferCommandBuffer());
				targetWrapper->isLoading = false;	
			}
			
			return dynamic_cast<TAsset*>(targetWrapper->asset);
		}
	}
}