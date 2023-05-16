#pragma once
#include "Core/Logic/Component/Base/Behaviour.h"
#include <string>
#include <future>

class Mesh;
class Shader;

class PresentRenderer_Behaviour : public Behaviour
{
public:
	CONSTRUCTOR(PresentRenderer_Behaviour)
		void onAwake()override;
	void onStart()override;
	void onUpdate()override;
	void onDestroy()override;

private:
	Mesh* mesh;
	Shader* shader;

	RTTR_ENABLE(Behaviour)
};
