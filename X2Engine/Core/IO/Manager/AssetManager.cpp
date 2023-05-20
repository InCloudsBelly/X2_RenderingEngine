#include "AssetManager.h"
#include <iostream>


AssetManager::AssetManager()
	: m_wrappers()
	//, _mutex()
{
}

AssetManager::~AssetManager()
{
	for (auto iterator = m_wrappers.begin(); iterator != m_wrappers.end(); iterator++)
	{
		if (iterator->second->referenceCount != 0)
			std::cout << "Warn! " << iterator->first << " hasn't been delete!  ReferenceCount is " << iterator->second->referenceCount << std::endl;
		delete iterator->second->asset;
		delete iterator->second;
	}
}

void AssetManager::unload(AssetBase* asset)
{
	if (asset != nullptr)
	{
		//std::unique_lock<std::mutex> wrapperLock(asset->m_wrapper->mutex);
		if(asset->m_wrapper != nullptr)
			asset->m_wrapper->referenceCount--;
	}
}

void AssetManager::unload(std::string path)
{
	//std::unique_lock<std::mutex> managerLock(_mutex);

	//Get target pool
	AssetWrapper* targetWrapper = m_wrappers[path];

	{
		//std::unique_lock<std::mutex> wrapperLock(targetWrapper->mutex);

		targetWrapper->referenceCount--;
	}
}

void AssetManager::collect()
{
	std::vector<AssetWrapper*> collectedWrappers = std::vector<AssetWrapper*>();
	{
		//std::unique_lock<std::mutex> managerLock(_mutex);
		for (auto iter = m_wrappers.begin(); iter != m_wrappers.end(); )
		{
			//std::unique_lock<std::mutex> wrapperLock(iter->second->mutex);
			if (iter->second->referenceCount == 0)
			{
				if (iter->second->isLoading)
				{
					iter++;
				}
				else
				{
					delete iter->second->asset;
					collectedWrappers.emplace_back(iter->second);
					iter = m_wrappers.erase(iter);
				}
			}
			else
			{
				iter++;
			}
		}
	}
	for (const auto& wrapper : collectedWrappers)
	{
		delete wrapper;
	}
}