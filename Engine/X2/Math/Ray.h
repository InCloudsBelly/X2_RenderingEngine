#ifndef X2_RAY_H
#define X2_RAY_H

#include "AABB.h"

namespace X2 {

    namespace Volume {

        class Ray {

        public:
            Ray() = default;

            Ray(glm::vec3 origin, glm::vec3 direction);

            glm::vec3 Get(float distance) const;

            bool Intersects(AABB aabb, float tmin, float tmax);

            bool Intersects(AABB aabb, float tmin, float tmax, float& t);

            bool Intersects(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2);

            bool Intersects(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3& intersection);
            bool Intersects(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, float& distance);


            glm::vec3 Distance(Ray ray, float& distance);

            glm::vec3 origin = glm::vec3(0.0f);
            glm::vec3 direction = glm::vec3(0.0f, 1.0f, 0.0f);

        private:
            glm::vec3 inverseDirection = glm::vec3(0.0f, 1.0f, 0.0f);

        };

    }

}


#endif