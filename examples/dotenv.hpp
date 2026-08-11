#pragma once

#include <cstdlib>
#include <fstream>
#include <initializer_list>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>

namespace godark_examples {

inline std::unordered_set<std::string>& os_keys() {
    static std::unordered_set<std::string> keys;
    return keys;
}

inline std::unordered_map<std::string, std::string>& file_vals() {
    static std::unordered_map<std::string, std::string> vals;
    return vals;
}

inline bool& os_snapshotted() {
    static bool snap = false;
    return snap;
}

inline void snapshot_os_env() {
    os_keys().clear();
    file_vals().clear();
    if (environ != nullptr) {
        for (char** e = environ; *e != nullptr; ++e) {
            std::string kv(*e);
            const auto eq = kv.find('=');
            if (eq == std::string::npos || eq == 0) continue;
            const std::string key = kv.substr(0, eq);
            const std::string val = kv.substr(eq + 1);
            if (!val.empty()) os_keys().insert(key);
        }
    }
    os_snapshotted() = true;
}

inline void load_dotenv(const std::string& path = ".env") {
    if (os_snapshotted()) return;
    snapshot_os_env();
    std::ifstream in(path);
    if (!in.is_open()) return;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;

        const std::size_t eq = line.find('=');
        if (eq == std::string::npos || eq == 0) continue;

        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        while (!key.empty() && (key.front() == ' ' || key.front() == '\t')) key.erase(key.begin());
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(value.begin());
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r')) value.pop_back();
        if (value.size() >= 2) {
            const char a = value.front();
            const char b = value.back();
            if ((a == '"' && b == '"') || (a == '\'' && b == '\'')) {
                value = value.substr(1, value.size() - 2);
            }
        }
        if (key.empty()) continue;

        file_vals()[key] = value;
        // Keep externally provided env vars as highest priority.
        if (std::getenv(key.c_str()) != nullptr) continue;
        setenv(key.c_str(), value.c_str(), 0);
    }
}

/// OS GODARK then OS GDX, then the same names from `.env`.
inline std::string env_first(std::initializer_list<const char*> names) {
    if (os_snapshotted()) {
        for (const char* name : names) {
            if (os_keys().count(name) == 0) continue;
            const char* val = std::getenv(name);
            if (val && val[0] != '\0') return val;
        }
        for (const char* name : names) {
            auto it = file_vals().find(name);
            if (it != file_vals().end() && !it->second.empty()) return it->second;
        }
        return "";
    }
    for (const char* name : names) {
        const char* val = std::getenv(name);
        if (val && val[0] != '\0') return val;
    }
    return "";
}

} // namespace godark_examples
