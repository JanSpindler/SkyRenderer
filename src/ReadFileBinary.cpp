#include <engine/util/ReadFileBinary.hpp>
#include <engine/util/Log.hpp>
#include <fstream>

namespace en
{
	std::vector<char> ReadFileBinary(const std::string& filename)
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (!file.is_open())
			Log::Error("Failed to open file " + filename, true);

		size_t fileSize = static_cast<size_t>(file.tellg());
		std::vector<char> buffer(fileSize);
		file.seekg(0);
		file.read(buffer.data(), fileSize);

		file.close();
		return buffer;
	}
}
