#include "Utils/RandomSphericalCoordinateGenerator.h"
#include <algorithm>
#include <glm/glm.hpp>

const float RandomSphericalCoordinateGenerator::pi = std::acos(-1.0);

RandomSphericalCoordinateGenerator::RandomSphericalCoordinateGenerator(float minTheta, float maxTheta, float minPhi, float maxPhi, float radius)
	: m_engine()
	, m_thetaDistribution(-std::clamp(std::cos(minTheta / 180.0f * pi), -1.0f, 1.0f) * 0.5f + 0.5f, -std::clamp(std::cos(maxTheta / 180.0f * pi), -1.0f, 1.0f) * 0.5f + 0.5f)
	, m_phiDistribution(std::clamp(minPhi / 360.0f, 0.0f, 1.0f), std::clamp(maxPhi / 360.0f, 0.0f, 1.0f))
	, radius(radius)
{
}

RandomSphericalCoordinateGenerator::RandomSphericalCoordinateGenerator()
	: RandomSphericalCoordinateGenerator(0, 180, 0, 360, 1)
{
}

RandomSphericalCoordinateGenerator::~RandomSphericalCoordinateGenerator()
{
}

glm::vec3 RandomSphericalCoordinateGenerator::get()
{
	float t = std::acos(1.0f - 2 * m_thetaDistribution(m_engine));
	float p = 2 * pi * m_phiDistribution(m_engine);
	float scale = std::sqrt(1 - std::cos(t));
	return glm::normalize(glm::vec3(std::cos(p) * scale, std::sin(p) * scale, std::cos(t))) * radius;
}
