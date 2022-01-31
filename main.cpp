#include <iostream>
#include "json.hpp"

int main() {
	const json data = json::parse(R"({"test": { "number": 45.54545, "string": "hi there!" }, "boolean": true })");
	std::cout << data << std::endl;
	json toll = data["test"];

	const std::string s = data["test"]["string"];
	const double d = data["test"]["number"];
	const bool b = data["boolean"];

    std::cout << s << " " << d << " " << b << std::endl;
}
