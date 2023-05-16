#pragma once

#include <vector>
#include "Core/Logic/Component/Component.h"

class Semaphore;
class Fence;

class FrameBuffer;

class Engine
{
public:

	void run();

private:

	void init();
	void prepareData();
	void mainLoop();
	void cleanup();

	void iterateByStaticBfs(std::vector<Component::ComponentType> targetComponentTypes, std::vector<std::vector<Component*>>& targetComponents);
	void iterateByDynamicBfs(Component::ComponentType targetComponentType);
	void destroyByStaticBfs(std::vector<Component::ComponentType> targetComponentTypes);

};