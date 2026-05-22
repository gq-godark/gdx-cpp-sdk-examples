#pragma once

#include <cstdlib>
#include <fstream>
#include <string>

namespace godark_examples {

inline void load_dotenv(const std::string& path = ".env") {
    std::ifstream in(path);
    if (!in.is_open()) return;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;

        const std::size_t eq = line.find('=');
        if (eq == std::string::npos || eq == 0) continue;

        const std::string key = line.substr(0, eq);
        const std::string value = line.substr(eq + 1);
        if (key.empty()) continue;

        // Keep externally provided env vars as highest priority.
        if (std::getenv(key.c_str()) != nullptr) continue;
        setenv(key.c_str(), value.c_str(), 0);
    }
}

} // namespace godark_examples
