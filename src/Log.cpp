#include <engine/util/Log.hpp>
#include <iostream>

namespace en
{
	void Log::Info(const std::string& msg)
	{
		std::cout << "INFO: \t" << msg << std::endl;
	}

	void Log::Warn(const std::string& msg)
	{
		std::cout << "WARN: \t" << msg << std::endl;
	}

	void Log::Error(const std::string& msg, bool exit)
	{
		std::cout << "ERROR:\t" << msg << std::endl;
		if (exit)
			throw std::runtime_error("SkyRenderer ERROR: " + msg);
	}
}
