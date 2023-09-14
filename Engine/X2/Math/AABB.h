#ifndef X2_AABB_H
#define X2_AABB_H

#include <vector>
#include <glm/glm.hpp>

namespace X2 {

    namespace Volume {

        /**
         * Axis aligned bounding box for collision detection and space partitioning.
         * This axis aligned bounding box class describes the bound by min and max vectors.
         */
        class AABB {

        public:
            /**
             * Constructs an AABB object.
             */
            AABB() = default;

            /**
             * Constructs an AABB object.
             * @param min The minimum vector of the bounding box
             * @param max The maximum vector of the bounding box
             */
            AABB(glm::vec3 min, glm::vec3 max);

            /**
             * Checks whether there is an intersection.
             * @param aabb An AABB that is tested for intersection.
             * @return True if it intersects, false otherwise.
             */
            bool Intersects(AABB aabb);

            /**
             * Checks whether the AABB encloses an object.
             * @param point A point that is tested if it is inside the AABB.
             * @return True if the point is inside, false otherwise.
             */
            bool IsInside(glm::vec3 point);

            /**
             * Checks whether the AABB encloses an object.
             * @param aabb An AABB that is tested if it is inside the AABB.
             * @return True if the AABB is inside, false otherwise.
             */
            bool IsInside(AABB aabb);

            /**
             * Transforms the AABB.
             * @param matrix A transformation matrix.
             * @return The transformed AABB.
             * @note The AABB where this method was called on won't be transformed.
             */
            AABB Transform(glm::mat4 matrix);

            /**
             * Translates the AABB.
             * @param translation A translation vector.
             * @return The translated AABB.
             * @note The AABB where this method was called on won't be translated.
             */
            AABB Translate(glm::vec3 translation);

            /**
             * Uniformly scales the AABB on all axis.
             * @param scale A scale factor.
             * @return The scaled AABB.
             * @note The AABB where this method was called on won't be scaled.
             */
            AABB Scale(float scale);

            void Grow(AABB aabb);

            void Grow(glm::vec3 vector);

            void Intersect(AABB aabb);

            float GetSurfaceArea() const;

            /**
             * Returns the eight corners of the AABB.
             * @return The eight corners of the AABB.
             * @note The corners have the following order:
             * glm::vec3(min.x, min.y, max.z), glm::vec3(max.x, min.y, max.z),
             * glm::vec3(max.x, max.y, max.z), glm::vec3(min.x, max.y, max.z),
             * glm::vec3(min.x, min.y, min.z), glm::vec3(max.x, min.y, min.z),
             * glm::vec3(max.x, max.y, min.z), glm::vec3(min.x, max.y, min.z)
             */
            std::vector<glm::vec3> GetCorners();

            glm::vec3 Min = glm::vec3(0.0f);
            glm::vec3 Max = glm::vec3(0.0f);

        };

    }

}

#endif