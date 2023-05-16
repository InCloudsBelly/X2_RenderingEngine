#include "SphericalHarmonicsUtils.h"

#define _USE_MATH_DEFINES
#include <cmath>
#include <random>

#define M_PI       3.14159265358979323846

const float PI = (float)M_PI;

inline float NormalRandom(float mu = 0.f, float sigma = 1.f)
{
	static std::default_random_engine generator;
	static std::normal_distribution<float> distribution(mu, sigma);
	return distribution(generator);
}

std::vector<float> SphericalHarmonicsUtils::Basis(const glm::vec3& pos)
{
	int n = (5) * (5);
	std::vector<float> Y(n);
	glm::vec3 normal = glm::normalize(pos);
	float x = normal.x;
	float y = normal.y;
	float z = normal.z;

	Y[0] = 1.f / 2.f * sqrt(1.f / PI);

	Y[1] = sqrt(3.f / (4.f * PI)) * (-x);
	Y[2] = sqrt(3.f / (4.f * PI)) * y;
	Y[3] = sqrt(3.f / (4.f * PI)) * (-z);

	Y[4] = 1.f / 2.f * sqrt(15.f / PI) * x * z;
	Y[5] = 1.f / 2.f * sqrt(15.f / PI) * (-x * y);
	Y[6] = 1.f / 4.f * sqrt(5.f / PI) * (3 * y * y - 1);
	Y[7] = 1.f / 2.f * sqrt(15.f / PI) * (-y * z);
	Y[8] = 1.f / 4.f * sqrt(15.f / PI) * (z * z - x * x);

	Y[9] = -1.f / 8.f * sqrt(2.f * 35.f / PI) * x * (3 * z * z - x * x);
	Y[10] = 1.f / 2.f * sqrt(105.f / PI) * (x * z * y);
	Y[11] = -1.f / 8.f * sqrt(2.f * 21.f / PI) * x * (-1 + 5 * y * y);
	Y[12] = 1.f / 4.f * sqrt(7.f / PI) * y * (5 * y * y - 3);
	Y[13] = -1.f / 8.f * sqrt(2.f * 21.f / PI) * z * (-1 + 5 * y * y);
	Y[14] = 1.f / 4.f * sqrt(105.f / PI) * (z * z - x * x) * y;
	Y[15] = -1.f / 8.f * sqrt(2.f * 35.f / PI) * z * (z * z - 3 * x * x);

	Y[16] = 2.503343 * z * x * (z * z - x * x);
	Y[17] = -1.770131 * x * y * (3.0 * z * z - x * x);
	Y[18] = -0.946175 * z * x * (7.0 * y * y - 1.0);;
	Y[19] = -0.669047 * x * y * (7.0 * y * y - 3.0);
	Y[20] = 0.105786 * (35.0 * y * y * y * y - 30.0 * y * y + 3.0);
	Y[21] = -0.669047 * z * y * (7.0 * y * y - 3.0);
	Y[22] = 0.473087 * (z * z - x * x) * (7.0 * y * y - 1.0);
	Y[23] = -1.770131 * z * y * (z * z - 3.0 * x * x);
	Y[24] = 0.625836 * (z * z * (z * z - 3.0 * x * x) - x * x * (3.0 * z * z - x * x));

	return Y;
}

std::vector<glm::vec3> SphericalHarmonicsUtils::computeSkyboxSH(Bitmap& cubemap)
{
	int pixelSize = (
		cubemap.comp_ * Bitmap::getBytesPerComponent(cubemap.fmt_)
		);

	int samplenum = 1000000;
	std::vector<glm::vec3> pos(samplenum);
	std::vector<glm::vec3> color(samplenum);

	for (int i = 0; i < samplenum; i++)
	{
		// Position

		float x, y, z;
		do {
			x = NormalRandom();
			y = NormalRandom();
			z = NormalRandom();
		} while (x == 0 && y == 0 && z == 0);
		pos[i] = glm::vec3(x, y, z);


		// Sample Color
		int index;
		glm::vec2 uv;

		float ax = std::abs(x);
		float ay = std::abs(y);
		float az = std::abs(z);

		if (ax >= ay && ax >= az)	// x face
		{
			index = x >= 0 ? 0 : 1;
			uv = { z / x,  -y / ax };
		}
		else if (ay >= az)	// y face
		{
			index = y >= 0 ? 2 : 3;
			uv = { -x / ay, z / y };
		}
		else // z face
		{
			index = z >= 0 ? 4 : 5;
			uv = { -x / z,  -y / az };
		}

		uv = uv * 0.5f + 0.5f;


		int w = (int)(uv[0] * (cubemap.w_ - 1));
		int h = (int)(uv[1] * (cubemap.h_ - 1));

		glm::vec4 pixData = cubemap.getPixel(w, h, index);

		color[i] = glm::vec3(pixData.r, pixData.g, pixData.b);
	}


	//Evaluate
	int n = (5) * (5);
	std::vector<glm::vec3> coefs = std::vector<glm::vec3>(n, glm::vec3());

	for (int i = 0; i < samplenum; i++)
	{
		std::vector<float> Y = Basis(pos[i]);
		for (int j = 0; j < n; j++)
		{
			coefs[j] = coefs[j] + Y[j] * color[i];
		}
	}
	for (glm::vec3& coef : coefs)
	{
		coef = 4 * PI * coef / (float)samplenum;
	}

	return coefs;
}
