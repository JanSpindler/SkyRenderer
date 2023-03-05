#pragma once

#include <string>

namespace en
{
	class Log
	{
	public:
		static void Info(const std::string& msg);
		static void Warn(const std::string& msg);
		static void Error(const std::string& msg, bool exit);
	};
}
