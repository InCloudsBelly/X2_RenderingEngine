#pragma once
#include <vulkan/vulkan_core.h>
#include <string>
#include <map>
#include <set>
#include <vector>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <rttr/type.h>

class Instance;


class RenderPassBase;

class FrameBuffer;
class Image;

class RenderPassManager final
{
	//friend class CoreObject::Thread;
	friend class Instance;
private:
	struct RenderPassWrapper
	{
		int refrenceCount = 0;
		RenderPassBase* renderPass = nullptr;
	};
	//std::mutex _managerMutex;
	std::map<std::string, RenderPassWrapper> m_renderPassWrappers;
	RenderPassManager();
	~RenderPassManager();
	RenderPassManager(const RenderPassManager&) = delete;
	RenderPassManager& operator=(const RenderPassManager&) = delete;
	RenderPassManager(RenderPassManager&&) = delete;
	RenderPassManager& operator=(RenderPassManager&&) = delete;

	void createRenderPass(RenderPassBase* renderPass);
public:
	RenderPassBase* loadRenderPass(std::string renderPassName);
	template<typename TRenderPass>
	RenderPassBase* loadRenderPass();

	void unloadRenderPass(std::string renderPassName);
	template<typename TRenderPass>
	void unloadRenderPass();
	void unloadRenderPass(RenderPassBase* renderPass);

	void collect();
};

template<typename TRenderPass>
RenderPassBase* RenderPassManager::loadRenderPass()
{
	return loadRenderPass(rttr::type::get<TRenderPass>().get_name().to_string());
}

template<typename TRenderPass>
void RenderPassManager::unloadRenderPass()
{
	unloadRenderPass(rttr::type::get<TRenderPass>().get_name().to_string());
}
