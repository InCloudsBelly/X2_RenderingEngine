#include "Core/Logic/CoreObject/LogicInstance.h"
//#include "Utils/Condition.h"
#include "Core/Logic/Manager/InputManager.h"

LogicInstance::RootGameObject LogicInstance::rootObject = LogicInstance::RootGameObject();
//AirEngine::Utils::Condition* LogicInstance::g_exitCondition = nullptr;
std::unordered_set<GameObject*> LogicInstance::g_validGameObjectInIteration = std::unordered_set<GameObject*>();
std::unordered_set<Component*> LogicInstance::g_validComponentInIteration = std::unordered_set<Component*>();
Time LogicInstance::m_time = Time();
bool LogicInstance::g_needIterateRenderer = false;
InputManager* LogicInstance::g_inputManager = nullptr;

void LogicInstance::Init()
{
	g_inputManager = new InputManager();
	//_exitCondition = new AirEngine::Utils::Condition();
	g_needIterateRenderer = false;
	m_time.launch();
}

void LogicInstance::destroy()
{
	delete g_inputManager;
}

//void LogicInstance::exit()
//{
//	//_exitCondition->Awake();
//}
//
//void LogicInstance::WaitExit()
//{
//	_exitCondition->Wait();
//}

bool LogicInstance::needIterateRenderer()
{
	return g_needIterateRenderer;
}

void LogicInstance::setNeedIterateRenderer(bool needIterateRenderer)
{
	g_needIterateRenderer = needIterateRenderer;
}

InputManager* LogicInstance::getInputManager()
{
	return g_inputManager;
}

LogicInstance::LogicInstance()
{
}

LogicInstance::~LogicInstance()
{
}

LogicInstance::RootGameObject::RootGameObject()
	: m_gameObject("RootGameObject")
{
}

LogicInstance::RootGameObject::~RootGameObject()
{
}

