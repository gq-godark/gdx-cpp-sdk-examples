#pragma once

#include <godark/visibility.hpp>

#include <cstdint>
#include <map>
#include <string>

#include <nlohmann/json.hpp>

namespace godark {

/// Parse edge ``GET /api/v1/instruments`` data into symbol → symbol_id.
GODARK_API std::map<std::string, uint64_t> parse_symbol_map_from_instruments(
    const nlohmann::json& data);

/// Map WS or REST base URL to HTTP origin for public ``/api/v1/instruments``.
GODARK_API std::string http_origin_for_instruments(std::string base_url);

/// Fetch symbol map from edge; fall back to offline map when unreachable.
GODARK_API std::map<std::string, uint64_t> load_symbol_map_from_edge(const std::string& base_url);

} // namespace godark
