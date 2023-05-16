#pragma once
#include "Core/Logic/Component/Base/Behaviour.h"
#include <string>
#include <future>

class ImageSampler;
class Mesh;
class Shader;

class BackgroundRenderer_Behaviour : public Behaviour
{
public:
	CONSTRUCTOR(BackgroundRenderer_Behaviour)
		void onAwake()override;
	void onStart()override;
	void onUpdate()override;
	void onDestroy()override;

private:
	ImageSampler* sampler;
	Mesh* mesh;
	Shader* shader;

	RTTR_ENABLE(Behaviour)
};
