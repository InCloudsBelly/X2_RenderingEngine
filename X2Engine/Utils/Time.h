#pragma once
#include <chrono>

class Time
{
private:
	std::chrono::system_clock::time_point m_launchTime;
	std::chrono::system_clock::time_point m_preTime;
	std::chrono::system_clock::time_point m_curTime;
public:
	Time();
	~Time();
	void launch();
	void refresh();
	double getDeltaDuration();
	double getLaunchDuration();
};