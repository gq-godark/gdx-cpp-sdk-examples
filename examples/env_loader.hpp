// Minimal `.env` loader for the GoDark C++ examples.
//
// Header-only, no third-party dependencies. Searches the current working
// directory and a few parent directories for a `.env` file and merges its
// `KEY=VALUE` pairs into the process environment. The OS environment always
// wins over `.env` (matching python-dotenv, dotenv-rs, and the JS dotenv
// package), so `KEY=value ./quickstart` still overrides a `.env` entry.
//
// Usage:
//   #include "env_loader.hpp"
//   int main() {
//       godark::examples::load_dotenv();   // before any std::getenv()
//       ...
//   }

#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

#if defined(_WIN32)
#  include <stdlib.h>
#else
#  include <cstdlib>
#endif

namespace godark::examples {

namespace dotenv_detail {

inline int set_env(const char* key, const char* value) {
#if defined(_WIN32)
    // Do not overwrite an existing variable.
    size_t sz = 0;
    if (getenv_s(&sz, nullptr, 0, key) == 0 && sz > 0) return 0;
    return _putenv_s(key, value);
#else
    return ::setenv(key, value, /*overwrite=*/0);
#endif
}

inline std::string trim(std::string_view s) {
    const auto not_space = [](unsigned char c) { return !std::isspace(c); };
    auto begin = std::find_if(s.begin(), s.end(), not_space);
    auto end = std::find_if(s.rbegin(), s.rend(), not_space).base();
    return (begin < end) ? std::string(begin, end) : std::string{};
}

inline std::string strip_quotes(std::string s) {
    if (s.size() >= 2) {
        const char first = s.front();
        const char last = s.back();
        if ((first == '"' || first == '\'') && first == last) {
            s = s.substr(1, s.size() - 2);
        }
    }
    return s;
}

inline std::filesystem::path find_dotenv(const std::filesystem::path& start, int max_levels) {
    std::error_code ec;
    auto dir = std::filesystem::absolute(start, ec);
    if (ec) return {};
    for (int i = 0; i <= max_levels; ++i) {
        auto candidate = dir / ".env";
        if (std::filesystem::is_regular_file(candidate, ec)) return candidate;
        if (!dir.has_parent_path() || dir.parent_path() == dir) break;
        dir = dir.parent_path();
    }
    return {};
}

}  // namespace dotenv_detail

/// Load `.env` from the current working directory or a parent directory.
/// Returns the path that was loaded, or an empty path when no file was found.
/// Existing process environment variables are preserved.
inline std::filesystem::path load_dotenv(
    const std::filesystem::path& start = std::filesystem::current_path(),
    int max_levels = 5) {
    const auto path = dotenv_detail::find_dotenv(start, max_levels);
    if (path.empty()) return {};

    std::ifstream in(path);
    if (!in) return {};

    std::string line;
    while (std::getline(in, line)) {
        std::string trimmed = dotenv_detail::trim(line);
        if (trimmed.empty() || trimmed.front() == '#') continue;

        // Allow `export KEY=VALUE` so the same file can be `source`d from a shell.
        constexpr std::string_view export_prefix = "export ";
        if (trimmed.compare(0, export_prefix.size(), export_prefix) == 0) {
            trimmed = dotenv_detail::trim(trimmed.substr(export_prefix.size()));
        }

        const auto eq = trimmed.find('=');
        if (eq == std::string::npos) continue;

        std::string key = dotenv_detail::trim(trimmed.substr(0, eq));
        if (key.empty()) continue;

        std::string value = dotenv_detail::trim(trimmed.substr(eq + 1));
        // Strip an inline `# comment` when the value is unquoted.
        if (!value.empty() && value.front() != '"' && value.front() != '\'') {
            if (const auto hash = value.find(" #"); hash != std::string::npos) {
                value = dotenv_detail::trim(value.substr(0, hash));
            }
        }
        value = dotenv_detail::strip_quotes(std::move(value));

        dotenv_detail::set_env(key.c_str(), value.c_str());
    }
    return path;
}

}  // namespace godark::examples
