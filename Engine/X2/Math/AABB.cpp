#include "Precompiled.h"
#include "AABB.h"

namespace X2 {
    namespace Volume {
        AABB::AABB(glm::vec3 Min, glm::vec3 Max) : Min(Min), Max(Max) {

        }

        bool AABB::Intersects(AABB aabb) {

            return aabb.Min.x <= Max.x && aabb.Max.x >= Min.x &&
                aabb.Min.y <= Max.y && aabb.Max.y >= Min.y &&
                aabb.Min.z <= Max.z && aabb.Max.z >= Min.z;

        }

        bool AABB::IsInside(glm::vec3 point) {

            return point.x >= Min.x && point.x <= Max.x &&
                point.y >= Min.y && point.y <= Max.y &&
                point.z >= Min.z && point.z <= Max.z;

        }

        bool AABB::IsInside(AABB aabb) {

            return IsInside(aabb.Min) && IsInside(aabb.Max);

        }

        AABB AABB::Transform(glm::mat4 matrix) {

            glm::vec3 cube[] = { glm::vec3(Min.x, Min.y, Min.z), glm::vec3(Min.x, Min.y, Max.z),
                glm::vec3(Max.x, Min.y, Min.z), glm::vec3(Max.x, Min.y, Max.z),
                glm::vec3(Min.x, Max.y, Min.z), glm::vec3(Min.x, Max.y, Max.z),
                glm::vec3(Max.x, Max.y, Min.z), glm::vec3(Max.x, Max.y, Max.z) };

            for (uint8_t i = 0; i < 8; i++) {
                auto homogeneous = matrix * glm::vec4(cube[i], 1.0f);
                cube[i] = glm::vec3(homogeneous) / homogeneous.w;
            }

            glm::vec3 Min = cube[0], Max = cube[0];

            for (uint8_t i = 1; i < 8; i++) {
                Min.x = glm::min(Min.x, cube[i].x);
                Min.y = glm::min(Min.y, cube[i].y);
                Min.z = glm::min(Min.z, cube[i].z);

                Max.x = glm::max(Max.x, cube[i].x);
                Max.y = glm::max(Max.y, cube[i].y);
                Max.z = glm::max(Max.z, cube[i].z);
            }

            return AABB(Min, Max);

        }

        AABB AABB::Translate(glm::vec3 translation) {

            return AABB(Min + translation, Max + translation);

        }

        AABB AABB::Scale(float scale) {

            auto center = 0.5f * (Min + Max);
            auto scaledMin = center + scale * (Min - center);
            auto scaledMax = center + scale * (Max - center);

            return AABB(scaledMin, scaledMax);

        }

        void AABB::Grow(AABB aabb) {

            Max = glm::max(Max, aabb.Max);
            Min = glm::min(Min, aabb.Min);

        }

        void AABB::Grow(glm::vec3 vector) {

            Max = glm::max(vector, Max);
            Min = glm::min(vector, Min);

        }

        void AABB::Intersect(AABB aabb) {

            Min = glm::max(Min, aabb.Min);
            Max = glm::min(Max, aabb.Max);

        }

        float AABB::GetSurfaceArea() const {
            auto dimension = Max - Min;

            return 2.0f * (dimension.x * dimension.y +
                dimension.y * dimension.z +
                dimension.z * dimension.x);
        }

        std::vector<glm::vec3> AABB::GetCorners() {

            std::vector<glm::vec3> corners;

            glm::vec3 cube[] = { glm::vec3(Min.x, Min.y, Max.z), glm::vec3(Max.x, Min.y, Max.z),
                glm::vec3(Max.x, Max.y, Max.z), glm::vec3(Min.x, Max.y, Max.z),
                glm::vec3(Min.x, Min.y, Min.z), glm::vec3(Max.x, Min.y, Min.z),
                glm::vec3(Max.x, Max.y, Min.z), glm::vec3(Min.x, Max.y, Min.z) };

            for (uint8_t i = 0; i < 8; i++)
                corners.push_back(cube[i]);

            return corners;

        }
    }

}

