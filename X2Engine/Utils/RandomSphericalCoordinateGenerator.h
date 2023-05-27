#pragma once
#include <random>
#include <glm/vec3.hpp>

class RandomSphericalCoordinateGenerator
{
public:
	RandomSphericalCoordinateGenerator(float minTheta, float maxTheta, float minPhi, float maxPhi, float radius);
	RandomSphericalCoordinateGenerator();
	~RandomSphericalCoordinateGenerator();
	RandomSphericalCoordinateGenerator(const RandomSphericalCoordinateGenerator&) = delete;
	RandomSphericalCoordinateGenerator& operator=(const RandomSphericalCoordinateGenerator&) = delete;
	RandomSphericalCoordinateGenerator(RandomSphericalCoordinateGenerator&&) = delete;
	RandomSphericalCoordinateGenerator& operator=(RandomSphericalCoordinateGenerator&&) = delete;
private:
	std::default_random_engine m_engine;
	std::uniform_real_distribution<float> m_thetaDistribution;
	std::uniform_real_distribution<float> m_phiDistribution;

public:
	float radius;
	static const float pi;
public:
	glm::vec3 get();
};

