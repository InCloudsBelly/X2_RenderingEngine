#pragma once

#include "X2/Core/Log.h"

#include <assimp/DefaultLogger.hpp>
#include <assimp/LogStream.hpp>

namespace X2 {

	struct AssimpLogStream : public Assimp::LogStream
	{
		static void Initialize()
		{
			if (Assimp::DefaultLogger::isNullLogger())
			{
				Assimp::DefaultLogger::create("", Assimp::Logger::VERBOSE);
				Assimp::DefaultLogger::get()->attachStream(new AssimpLogStream, Assimp::Logger::Debugging | Assimp::Logger::Info | Assimp::Logger::Warn | Assimp::Logger::Err);
			}
		}

		virtual void write(const char* message) override
		{
			std::string msg(message);
			if (!msg.empty() && msg[msg.length() - 1] == '\n')
			{
				msg.erase(msg.length() - 1);
			}
			if (strncmp(message, "Debug", 5) == 0)
			{
				X2_CORE_TRACE_TAG("Assimp", msg);
			}
			else if (strncmp(message, "Info", 4) == 0)
			{
				X2_CORE_INFO_TAG("Assimp", msg);
			}
			else if (strncmp(message, "Warn", 4) == 0)
			{
				X2_CORE_WARN_TAG("Assimp", msg);
			}
			else
			{
				X2_CORE_ERROR_TAG("Assimp", msg);
			}
		}
	};

}
