#include "Precompiled.h"
#include "Ray.h"

#include <glm/glm.hpp>

namespace X2 {

    namespace Volume {

        Ray::Ray(glm::vec3 origin, glm::vec3 direction) : origin(origin),
            direction(direction), inverseDirection(1.0f / direction) {



        }

        glm::vec3 Ray::Get(float distance) const {

            return origin + distance * direction;

        }

        bool Ray::Intersects(AABB aabb, float tmin, float tmax) {

            auto t = 0.0f;

            return Intersects(aabb, tmin, tmax, t);

        }

        bool Ray::Intersects(AABB aabb, float tmin, float tmax, float& t) {

            auto t0 = (aabb.Min - origin) * inverseDirection;
            auto t1 = (aabb.Max - origin) * inverseDirection;

            auto tsmall = glm::min(t0, t1);
            auto tbig = glm::max(t0, t1);

            auto tminf = glm::max(tmin, glm::max(tsmall.x, glm::max(tsmall.y, tsmall.z)));
            auto tmaxf = glm::min(tmax, glm::min(tbig.x, glm::min(tbig.y, tbig.z)));

            t = tminf;

            return (tminf <= tmaxf);

        }

        bool Ray::Intersects(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2) {

            glm::vec3 intersection;

            return Intersects(v0, v1, v2, intersection);

        }

        bool Ray::Intersects(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3& intersection) {

            auto e0 = v1 - v0;
            auto e1 = v2 - v0;
            auto s = origin - v0;

            auto p = cross(s, e0);
            auto q = cross(direction, e1);

            auto sol = glm::vec3(glm::dot(p, e1), glm::dot(q, s),
                glm::dot(p, direction)) / (glm::dot(q, e0));

            if (sol.x >= 0.0f && sol.y >= 0.0f &&
                sol.z >= 0.0f && sol.y + sol.z <= 1.0f) {

                intersection = sol;

                return true;

            }

            return false;

        }

        bool Ray::Intersects(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, float& distance) {

            auto e0 = v1 - v0;
            auto e1 = v2 - v0;
            auto s = origin - v0;

            auto p = cross(s, e0);
            auto q = cross(direction, e1);

            auto sol = glm::vec3(glm::dot(p, e1), glm::dot(q, s),
                glm::dot(p, direction)) / (glm::dot(q, e0));

            if (sol.x >= 0.0f && sol.y >= 0.0f &&
                sol.z >= 0.0f && sol.y + sol.z <= 1.0f) {

                distance = glm::length(sol - origin);

                return true;

            }

            return false;

        }

        glm::vec3 Ray::Distance(Ray ray, float& distance) {

            constexpr float minFloat = std::numeric_limits<float>::min();

            auto cross = glm::cross(direction, ray.direction);
            auto denom = glm::max(glm::pow(glm::length(cross), 2.0f),
                minFloat);

            auto t = ray.origin - origin;
            auto det0 = glm::determinant(glm::mat3(t, ray.direction, cross));
            auto det1 = glm::determinant(glm::mat3(t, direction, cross));

            auto t1 = det1 / denom;

            distance = t1;

            return ray.Get(t1);

        }

    }

}