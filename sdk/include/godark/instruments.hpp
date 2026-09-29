#pragma once

#include <godark/visibility.hpp>

#include <cstdint>
#include <map>
#include <string>

#include <nlohmann/json.hpp>

namespace godark {

/// Per-instrument fixed-point scales from ``GET /api/v1/instruments``.
struct InstrumentDecimals {
    uint32_t price_decimals = 8;
    uint32_t quantity_decimals = 8;
};

/// One row from edge ``GET /api/v1/instruments``.
struct InstrumentInfo {
    uint64_t symbol_id = 0;
    uint32_t price_decimals = 8;
    uint32_t quantity_decimals = 8;

    InstrumentDecimals decimals() const {
        return InstrumentDecimals{price_decimals, quantity_decimals};
    }
};

/// Parse edge ``GET /api/v1/instruments`` data into symbol → metadata.
GODARK_API std::map<std::string, InstrumentInfo> parse_instruments_from_json(
    const nlohmann::json& data);

/// Parse edge ``GET /api/v1/instruments`` data into symbol → symbol_id.
GODARK_API std::map<std::string, uint64_t> parse_symbol_map_from_instruments(
    const nlohmann::json& data);

/// Map WS or REST base URL to HTTP origin for public ``/api/v1/instruments``.
GODARK_API std::string http_origin_for_instruments(std::string base_url);

/// Fetch symbol map from edge; fall back to offline map when unreachable.
GODARK_API std::map<std::string, uint64_t> load_symbol_map_from_edge(const std::string& base_url);

/// Fetch instrument metadata (ids + decimals) from edge; empty on failure.
GODARK_API std::map<std::string, InstrumentInfo> load_instruments_from_edge(
    const std::string& base_url);

} // namespace godark
