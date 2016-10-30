
#include <string>
#include <vector>

namespace sixfive {

struct MonParser {

	struct Impl;

	struct Command {
		bool valid;
		std::string name;
		std::vector<int> args;
		std::string strarg;
		operator bool() {
			return valid;
		}

	};

	~MonParser();
	MonParser(MonParser &&op) noexcept;
	MonParser &operator=(MonParser &&op) noexcept;
	MonParser();

	// Defines are handled internally
	// Commands are of the form <string> <arg>... where <arg> is int or string.
	Command parseLine(const std::string &line);

	std::unique_ptr<Impl> impl;

};

} // namespace
