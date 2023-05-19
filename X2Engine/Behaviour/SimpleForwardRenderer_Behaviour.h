#pragma once
#include "Core/Logic/Component/Base/Behaviour.h"
#include <string>
#include <future>

class Mesh;
class Shader;
class Image;
class ImageSampler;

class SimpleForwardRenderer_Behaviour : public Behaviour
{
public:
	CONSTRUCTOR(SimpleForwardRenderer_Behaviour)
		void onAwake()override;
	void onStart()override;
	void onUpdate()override;
	void onDestroy()override;

private:
	Mesh* mesh;
	Image* albedoTexture;
	ImageSampler* sampler;
	Shader* shader;

	RTTR_ENABLE(Behaviour)
};
