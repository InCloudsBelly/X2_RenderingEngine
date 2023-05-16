#include "IntersectionChecker.h"

void IntersectionChecker::setIntersectPlanes(const glm::vec4* planes, size_t planeCount)
{
	m_intersectPlanes.clear();
	m_intersectPlanes.resize(planeCount);

	for (auto i = 0; i < planeCount; i++)
	{
		m_intersectPlanes[i] = planes[i];
	}
}

bool IntersectionChecker::check(const glm::vec3* vertexes, size_t vertexCount)
{
	int planeCount = m_intersectPlanes.size();
	for (int j = 0; j < planeCount; j++)
	{
		for (int i = 0; i < vertexCount; i++)
		{
			if (glm::dot(glm::vec4(vertexes[i], 1), m_intersectPlanes[j]) >= 0)
			{
				goto Out;
			}
		}
		return false;
	Out:
		continue;
	}
	return true;
}

bool IntersectionChecker::check(const glm::vec3* vertexes, size_t vertexCount, glm::mat4 matrix)
{
	std::vector<glm::vec4> wvBoundryVertexes = std::vector<glm::vec4>(vertexCount);
	for (size_t i = 0; i < vertexCount; i++)
	{
		wvBoundryVertexes[i] = matrix * glm::vec4(vertexes[i], 1.0);
	}
	int planeCount = m_intersectPlanes.size();
	for (int j = 0; j < planeCount; j++)
	{
		for (int i = 0; i < vertexCount; i++)
		{
			if (glm::dot(wvBoundryVertexes[i], m_intersectPlanes[j]) >= 0)
			{
				goto Out;
			}
		}
		return false;
	Out:
		continue;
	}
	return true;
}

IntersectionChecker::IntersectionChecker()
	: m_intersectPlanes()
{
}

IntersectionChecker::IntersectionChecker(std::vector<glm::vec4>& intersectPlanes)
	: m_intersectPlanes(intersectPlanes)
{
}

IntersectionChecker::~IntersectionChecker()
{
}
