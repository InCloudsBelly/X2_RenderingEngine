#pragma once

#include "X2/Core/Application.h"

#ifdef X2_PLATFORM_WINDOWS

extern X2::Application* X2::CreateApplication(int argc, char** argv);
bool g_ApplicationRunning = true;

namespace X2 {

	int Main(int argc, char** argv)
	{
		while (g_ApplicationRunning)
		{
			InitializeCore();
			Application* app = CreateApplication(argc, argv);
			X2_CORE_ASSERT(app, "Client Application is null!");
			app->Run();
			delete app;
			ShutdownCore();
		}
		return 0;
	}

}

#ifdef X2_DIST

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
	return X2::Main(__argc, __argv);
}

#else

int main(int argc, char** argv)
{
	return X2::Main(argc, argv);
}

#endif // X2_DIST

#endif // X2_PLATFORM_WINDOWS
