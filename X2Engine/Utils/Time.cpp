#include "Time.h"

Time::Time()
	: m_launchTime()
	, m_preTime()
	, m_curTime()
{
}

Time::~Time()
{
}

void Time::launch()
{
	m_launchTime = std::chrono::system_clock::now();
	m_preTime = m_launchTime;
	m_curTime = m_launchTime;
}

void Time::refresh()
{
	m_preTime = m_curTime;
	m_curTime = std::chrono::system_clock::now();
}

double Time::getDeltaDuration()
{
	auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(m_curTime - m_preTime);
	return double(duration.count()) / 1000000000;
}

double Time::getLaunchDuration()
{
	auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(m_curTime - m_launchTime);
	return double(duration.count()) / 1000000000;
}
