#pragma once

#include "X2/Core/UUID.h"
#include "X2/Asset/AssetTypes.h"

namespace X2 {

	using AssetHandle = UUID;

	class Asset 
	{
	public:
		AssetHandle Handle = 0;
		uint16_t Flags = (uint16_t)AssetFlag::None;

		virtual ~Asset() {}

		static AssetType GetStaticType() { return AssetType::None; }
		virtual AssetType GetAssetType() const { return AssetType::None; }

		bool IsValid() const { return ((Flags & (uint16_t)AssetFlag::Missing) | (Flags & (uint16_t)AssetFlag::Invalid)) == 0; }

		virtual bool operator==(const Asset& other) const
		{
			return Handle == other.Handle;
		}

		virtual bool operator!=(const Asset& other) const
		{
			return !(*this == other);
		}

		bool IsFlagSet(AssetFlag flag) const { return (uint16_t)flag & Flags; }
		void SetFlag(AssetFlag flag, bool value = true)
		{
			if (value)
				Flags |= (uint16_t)flag;
			else
				Flags &= ~(uint16_t)flag;
		}
	};


}
