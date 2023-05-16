#include "Core/IO/Asset/AssetBase.h"
#include <rttr/registration>

RTTR_REGISTRATION
{
	rttr::registration::class_<AssetBase>("AssetBase");
}

AssetBase::AssetBase(bool isLoaded)
	: m_isHeldByManager(isLoaded)
	, m_wrapper(nullptr)
	, m_path()
{
}

AssetBase::~AssetBase()
{
}

bool AssetBase::isHeldByManager()
{
	return m_isHeldByManager;
}

std::string AssetBase::getPath()
{
	return m_path;
}
