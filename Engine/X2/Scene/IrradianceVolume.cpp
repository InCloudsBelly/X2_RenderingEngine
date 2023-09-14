#include "IrradianceVolume.h"
#include <glm/glm.hpp>

namespace X2 {

    namespace Lighting {

        IrradianceVolume::IrradianceVolume(Volume::AABB aabb, glm::ivec3 probeCount, bool lowerResMoments) :
            aabb(aabb), probeCount(probeCount), momRes(lowerResMoments ? 6 : 14), lowerResMoments(lowerResMoments) {

            auto irrRes = glm::ivec2(this->irrRes + 2);
            irrRes.x *= probeCount.x;
            irrRes.y *= probeCount.z;

            auto momRes = glm::ivec2(this->momRes + 2);
            momRes.x *= probeCount.x;
            momRes.y *= probeCount.z;

            size = aabb.Max - aabb.Min;
            cellSize = size / glm::vec3(probeCount - glm::ivec3(1));

            internal = InternalIrradianceVolume(irrRes, momRes, probeCount);
            internal.SetRayCount(rayCount, rayCountInactive);

        }

        glm::ivec3 IrradianceVolume::GetIrradianceArrayOffset(glm::ivec3 probeIndex) {

            auto irrRes = glm::ivec2(this->irrRes + 2);

            return glm::ivec3(probeIndex.x * irrRes.x + 1,
                probeIndex.z * irrRes.y + 1, probeIndex.y);

        }

        glm::ivec3 IrradianceVolume::GetMomentsArrayOffset(glm::ivec3 probeIndex) {

            auto momRes = glm::ivec2(this->momRes + 2);

            return glm::ivec3(probeIndex.x * momRes.x + 1,
                probeIndex.z * momRes.y + 1, probeIndex.y);

        }

        glm::vec3 IrradianceVolume::GetProbeLocation(glm::ivec3 probeIndex) {

            return aabb.Min + glm::vec3(probeIndex) * cellSize;

        }

        void IrradianceVolume::SetAABB(Volume::AABB aabb) {

            this->aabb = aabb;
            size = aabb.Max - aabb.Min;
            cellSize = size / glm::vec3(probeCount - glm::ivec3(1));

        }

        void IrradianceVolume::SetRayCount(uint32_t rayCount, uint32_t rayCountInactive) {

            this->rayCount = rayCount;
            this->rayCountInactive = rayCountInactive;

            internal.SetRayCount(rayCount, rayCountInactive);

        }

        void IrradianceVolume::SetProbeCount(glm::ivec3 probeCount) {

            this->probeCount = probeCount;

            auto irrRes = glm::ivec2(this->irrRes + 2);
            irrRes.x *= probeCount.x;
            irrRes.y *= probeCount.z;

            auto momRes = glm::ivec2(this->momRes + 2);
            momRes.x *= probeCount.x;
            momRes.y *= probeCount.z;

            internal = InternalIrradianceVolume(irrRes, momRes, probeCount);
            internal.SetRayCount(rayCount, rayCountInactive);

        }

        void IrradianceVolume::ClearProbes() {

            auto irrRes = glm::ivec2(this->irrRes + 2);
            irrRes.x *= probeCount.x;
            irrRes.y *= probeCount.z;

            auto momRes = glm::ivec2(this->momRes + 2);
            momRes.x *= probeCount.x;
            momRes.y *= probeCount.z;

            internal.ClearProbes(irrRes, momRes, probeCount);

        }

        void IrradianceVolume::ResetProbeOffsets() {

            internal.ResetProbeOffsets();

        }

        InternalIrradianceVolume::InternalIrradianceVolume(glm::ivec2 irrRes, glm::ivec2 momRes, glm::ivec3 probeCount) {

            m_probeCount = probeCount;

            ImageSpecification spec;
            spec.Format = ImageFormat::A2B10G10R10U;
            spec.Usage = ImageUsage::Attachment;
            spec.Width = irrRes.x;
            spec.Height = irrRes.y;
            spec.Layers = probeCount.y; // 4 cascades
            spec.DebugName = "irradianceArray0";
            irradianceArray0 = CreateRef<VulkanImage2D>(spec);
            irradianceArray0->Invalidate();

            spec.DebugName = "irradianceArray1";
            irradianceArray1 = CreateRef<VulkanImage2D>(spec);
            irradianceArray1->Invalidate();



            spec.Format = ImageFormat::RG16F;
            spec.Width = momRes.x;
            spec.Height = momRes.y;

            spec.DebugName = "momentsArray0";
            momentsArray0 = CreateRef<VulkanImage2D>(spec);
            momentsArray0->Invalidate();

            spec.DebugName = "momentsArray1";
            momentsArray1 = CreateRef<VulkanImage2D>(spec);
            momentsArray1->Invalidate();

            SwapTextures();
            ClearProbes(irrRes, momRes, probeCount);

        }

        void InternalIrradianceVolume::SetRayCount(uint32_t rayCount, uint32_t rayCountInactive) {

            m_rayCount = rayCount;
            m_rayCountInactive = rayCountInactive;

            rayDirBuffer.Allocate(m_rayCount * sizeof(glm::vec4));
            rayDirInactiveBuffer.Allocate(m_rayCountInactive * sizeof(glm::vec4));

            FillRayBuffers();

        }

        std::tuple<const VulkanImage2D&, const VulkanImage2D&>
            InternalIrradianceVolume::GetCurrentProbes() const {

            if (swapIdx == 0) {
                return std::tuple<const VulkanImage2D&, const VulkanImage2D&>(
                        *irradianceArray0, *momentsArray0
                    );
            }
            else {
                return std::tuple<const VulkanImage2D&, const VulkanImage2D&>(
                        *irradianceArray1, *momentsArray1
                    );
            }

        }

        std::tuple<const VulkanImage2D&, const VulkanImage2D&>
            InternalIrradianceVolume::GetLastProbes() const {

            if (swapIdx == 0) {
                return std::tuple<const VulkanImage2D&, const VulkanImage2D&>(
                        *irradianceArray1, *momentsArray1
                    );
            }
            else {
                return std::tuple<const VulkanImage2D&, const VulkanImage2D&>(
                        *irradianceArray0, *momentsArray0
                    );
            }

        }

        void InternalIrradianceVolume::SwapTextures() {

            swapIdx = (swapIdx + 1) % 2;

        }

        void InternalIrradianceVolume::FillRayBuffers() {
            // Low discrepency sequence for rays
            auto fibonacciSphere = [](float i, float n) -> glm::vec3 {
                const float PHI = 1.6180339887498948482045868343656f;

                float phi = 2.0f * 3.1415926 * i * PHI - floorf(i * PHI);
                float cosTheta = 1.0f - (2.0f * i + 1.0f) * (1.0f / n);
                float sinTheta = sqrtf(1.0f - cosTheta * cosTheta);

                return glm::vec3(cosf(phi) * sinTheta, sinf(phi) * sinTheta, cosTheta);
            };
            // Ray sorting function which sorts rays based on being oriented similarly
            auto sortRays = [](std::vector<glm::vec4>& rayDirs) {
                // Sort rays into 8 bins
                std::sort(rayDirs.begin(), rayDirs.end(), [=](glm::vec4 ele0, glm::vec4 ele1) -> bool {
                    uint8_t orient0 = 0, orient1 = 0;
                    if (ele0.y > 0.0f) orient0 |= (1 << 0);
                    if (ele0.x > 0.0f) orient0 |= (1 << 1);
                    if (ele0.z > 0.0f) orient0 |= (1 << 2);
                    if (ele1.y > 0.0f) orient1 |= (1 << 0);
                    if (ele1.x > 0.0f) orient1 |= (1 << 1);
                    if (ele1.z > 0.0f) orient1 |= (1 << 2);
                    return orient0 < orient1;
                    });
            };

            std::vector<glm::vec4> rayDirs;
            // Calculate and fill buffer for active probes
            auto rayCount = float(m_rayCount);
            for (float i = 0.0f; i < rayCount; i += 1.0f)
                rayDirs.push_back(glm::vec4(fibonacciSphere(i, rayCount), 0.0));
            //sortRays(rayDirs);
            rayDirBuffer.Copy(rayDirs.data(), rayDirs.size() * sizeof( glm::vec4));

            rayDirs.clear();

            // Calculate and fill buffer for inactive probes
            rayCount = float(m_rayCountInactive);
            for (float i = 0.0f; i < rayCount; i += 1.0f)
                rayDirs.push_back(glm::vec4(fibonacciSphere(i, rayCount), 0.0));
            //sortRays(rayDirs);
            rayDirInactiveBuffer.Copy(rayDirs.data(), rayDirs.size() * sizeof(glm::vec4));
        }

        void InternalIrradianceVolume::ClearProbes(glm::ivec2 irrRes, glm::ivec2 momRes, glm::ivec3 probeCount) {
            // Fill probe textures with initial values
            std::vector<float> irrVector(irrRes.x * irrRes.y * 4);
            std::vector<float> momVector(momRes.x * momRes.y * 2);

            std::fill(irrVector.begin(), irrVector.end(), 0.0f);
            std::fill(momVector.begin(), momVector.end(), 1000.0f);

            auto [irradianceArray, momentsArray] = GetCurrentProbes();

        /*    for (int32_t i = 0; i < probeCount.y; i++) {

                irradianceArray0.SetData(irrVector, i);
                momentsArray0.SetData(momVector, i);

                irradianceArray1.SetData(irrVector, i);
                momentsArray1.SetData(momVector, i);

            }*/

            // Fill probe state buffer with values of 0 (indicates a new probe)
            uint32_t zero = 0;
            std::vector<glm::vec4> probeStates(m_probeCount.x * m_probeCount.y * m_probeCount.z);
            std::fill(probeStates.begin(), probeStates.end(), glm::vec4(glm::vec3(0.0f), reinterpret_cast<float&>(zero)));
            probeStateBuffer.Copy(probeStates.data(), probeStates.size() * sizeof(glm::vec4));

            ResetProbeOffsets();
        }

        void InternalIrradianceVolume::ResetProbeOffsets() {

            std::vector<glm::vec4> probeOffsets(m_probeCount.x * m_probeCount.y * m_probeCount.z);
            std::fill(probeOffsets.begin(), probeOffsets.end(), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
            probeOffsetBuffer.Copy(probeOffsets.data(), probeOffsets.size() * sizeof(glm::vec4));

        }
    }

}