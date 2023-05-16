#pragma once
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <vector>
#include <algorithm>
#include <array>

class OrientedBoundingBox
{
private:
    static glm::mat3 ComputeCovarianceMatrix(std::vector<glm::vec3>& positions);
    static void JacobiSolver(glm::mat3& matrix, float* eValue, glm::vec3* eVectors);
    static void SchmidtOrthogonal(glm::vec3& v0, glm::vec3& v1, glm::vec3& v2);
    glm::vec3 m_center;
    std::array<glm::vec3, 3> m_directions;
    glm::vec3 m_halfEdgeLength;
    std::array<glm::vec3, 8> m_boundryVertexes;
public:
    const glm::vec3& Center();
    const std::array<glm::vec3, 3>& Directions();
    const glm::vec3& HalfEdgeLength();
    const std::array<glm::vec3, 8>& BoundryVertexes();

    OrientedBoundingBox(std::vector<glm::vec3>& positions);
};