#pragma once

#include "X2/Renderer/Mesh.h"

#include <filesystem>

namespace X2 {

#ifdef X2_DIST
	class AssimpMeshImporter
	{
	public:
		AssimpMeshImporter(const std::filesystem::path& path) {}

		Ref<MeshSource> ImportToMeshSource() { return nullptr; }
		bool ImportSkeleton(Scope<Skeleton>& skeleton) { return false; }
		bool ImportAnimations(const uint32_t animationIndex, const Skeleton& skeleton, std::vector<Scope<Animation>>& animations) { return false; }
		bool IsCompatibleSkeleton(const uint32_t animationIndex, const Skeleton& skeleton) { return false; }
		uint32_t GetAnimationCount() { return 0; }
	private:
		void TraverseNodes(Ref<MeshSource> meshSource, void* assimpNode, uint32_t nodeIndex, const glm::mat4& parentTransform = glm::mat4(1.0f), uint32_t level = 0) {}
	private:
		const std::filesystem::path m_Path;
	};
#else
	class AssimpMeshImporter
	{
	public:
		AssimpMeshImporter(const std::filesystem::path& path);

		Ref<MeshSource> ImportToMeshSource();
		/*bool ImportSkeleton(Scope<Skeleton>& skeleton);
		bool ImportAnimations(const uint32_t animationIndex, const Skeleton& skeleton, std::vector<Scope<Animation>>& animations);
		bool IsCompatibleSkeleton(const uint32_t animationIndex, const Skeleton& skeleton);*/
		uint32_t GetAnimationCount();
	private:
		void TraverseNodes(Ref<MeshSource> meshSource, void* assimpNode, uint32_t nodeIndex, const glm::mat4& parentTransform = glm::mat4(1.0f), uint32_t level = 0);
	private:
		const std::filesystem::path m_Path;
	};
#endif
}
