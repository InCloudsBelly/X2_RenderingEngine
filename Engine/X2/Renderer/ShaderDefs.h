#pragma once
#include "X2/Core/Base.h"

namespace X2::ShaderDef {

	enum class AOMethod
	{
		None = 0, GTAO = BIT(1), HBAO = BIT(2), All = GTAO | HBAO,
	};

	constexpr static std::underlying_type_t<AOMethod> GetMethodIndex(const AOMethod method)
	{
		switch (method)
		{
		case AOMethod::None: return 0;
		case AOMethod::GTAO: return 1;
		case AOMethod::HBAO: return 2;
		case AOMethod::All: return 3;
		}
		return 0;
	}
	constexpr static ShaderDef::AOMethod ROMETHODS[4] = { AOMethod::None, AOMethod::GTAO, AOMethod::HBAO, AOMethod::All };

	constexpr static AOMethod GetAOMethod(const bool gtaoEnabled, const bool hbaEnabled)
	{
		if (gtaoEnabled && hbaEnabled)
			return AOMethod::All;
		else if (gtaoEnabled)
			return AOMethod::GTAO;
		else if (hbaEnabled)
			return AOMethod::HBAO;
		else return AOMethod::None;
	}


	enum class AAMethod
	{
		SMAA = 0
	};

	enum class SMAAEdgeMethod
	{
		Color = 0, Lumen = 1, Depth = 2
	};

	enum class SMAAQuality
	{
		Ultra = 0, High = 1, Medium = 2, Low = 3
	};

}
