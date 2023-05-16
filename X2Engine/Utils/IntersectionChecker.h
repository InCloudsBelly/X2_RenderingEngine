#pragma once
#include <glm/glm.hpp>
#include <vector>


class IntersectionChecker final
{
private:
	std::vector<glm::vec4> m_intersectPlanes;
public:
	void setIntersectPlanes(const glm::vec4* planes, size_t planeCount);
	bool check(const glm::vec3* vertexes, size_t vertexCount);
	bool check(const glm::vec3* vertexes, size_t vertexCount, glm::mat4 matrix);

	IntersectionChecker();
	IntersectionChecker(std::vector<glm::vec4>& intersectPlanes);
	~IntersectionChecker();
};