#pragma once
#include <mutex>
#include <string>
#include <rttr/registration>
#include <rttr/type>
#include "Core/Logic/Object/Object.h"

class CommandBuffer;

class AssetManager;
//class AssetWrapper;

class AssetBase;
class AssetWrapper;

class AssetBase : public Object
{
	friend class AssetManager;
private:
	bool m_isHeldByManager;
	std::string m_path;
	AssetWrapper* m_wrapper;
protected:
	virtual void onLoad(CommandBuffer* transferCommandBuffer) = 0;
	AssetBase(bool isHeldByManager = false);
	virtual ~AssetBase();
public:
	bool isHeldByManager();
	std::string getPath();

	RTTR_ENABLE()
};