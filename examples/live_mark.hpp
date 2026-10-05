#pragma once

// Live-mark helpers for the trading samples.
// Order prices are derived here. GDX_LIVE_PRICE / GODARK_E2E_PRICE are ignored
// so a stale override cannot cross the book.

#include "dotenv.hpp"

#include <godark/godark.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace godark_examples {

inline constexpr const char* kSymbol = "BTC-USDC-PERP";
inline constexpr const char* kQty = "0.001";

struct SafeQuotes {
    std::string sell;
    std::string buy;
    std::string buy_modify;
    std::string ladder[3];
};

inline std::string rest_base_from_edge(std::string edge) {
    const auto query = edge.find('?');
    if (query != std::string::npos) edge.resize(query);
    while (!edge.empty() && (edge.back() == '/' || edge.back() == ' ')) edge.pop_back();
    for (const char* suffix : {"/ws/v1", "/ws"}) {
        const std::string suf(suffix);
        if (edge.size() >= suf.size() && edge.compare(edge.size() - suf.size(), suf.size(), suf) == 0) {
            edge.resize(edge.size() - suf.size());
        }
    }
    if (edge.rfind("wss://", 0) == 0) return "https://" + edge.substr(6);
    if (edge.rfind("ws://", 0) == 0) return "http://" + edge.substr(5);
    return edge;
}

inline std::string websocket_edge_url(std::string edge) {
    if (edge.rfind("https://", 0) == 0) return "wss://" + edge.substr(8);
    if (edge.rfind("http://", 0) == 0) return "ws://" + edge.substr(7);
    return edge;
}

/// GODARK_REST_URL / GDX_REST_URL, otherwise the HTTP origin of the edge URL.
inline std::string resolve_rest_base() {
    if (std::string rest = env_first({"GODARK_REST_URL", "GDX_REST_URL"}); !rest.empty()) {
        return rest;
    }
    if (std::string edge = env_first({"GODARK_EDGE_URL", "GDX_EDGE_URL"}); !edge.empty()) {
        return rest_base_from_edge(std::move(edge));
    }
    return {};
}

inline std::string resolve_edge_url() {
    return websocket_edge_url(env_first({"GODARK_EDGE_URL", "GDX_EDGE_URL"}));
}

inline bool live_creds_present() {
    return !env_first({"GODARK_API_KEY_ID", "GDX_API_KEY_ID"}).empty()
        && !env_first({"GODARK_API_SECRET", "GDX_API_SECRET"}).empty()
        && !env_first({"GODARK_PASSPHRASE", "GDX_PASSPHRASE"}).empty();
}

inline void apply_keypair(godark::GodarkRestClient::Config& cfg) {
    cfg.api_key_id = env_first({"GODARK_API_KEY_ID", "GDX_API_KEY_ID"});
    cfg.api_secret = env_first({"GODARK_API_SECRET", "GDX_API_SECRET"});
    cfg.passphrase = env_first({"GODARK_PASSPHRASE", "GDX_PASSPHRASE"});
    if (std::string account = env_first({"GODARK_ACCOUNT", "GDX_ACCOUNT"}); !account.empty()) {
        cfg.account = std::move(account);
    }
    if (std::string pin = env_first({"GODARK_HPKE_STATIC_PUBLIC_KEY", "GDX_HPKE_STATIC_PUBLIC_KEY",
                                     "GDX_HPKE_STATIC_PUBKEY"});
        !pin.empty()) {
        cfg.hpke_static_public_key_hex = std::move(pin);
    }
    if (std::string rest = resolve_rest_base(); !rest.empty()) {
        cfg.rest_base_url = std::move(rest);
    }
}

inline bool parse_positive_decimal(std::string_view in, __int128& mant, int& scale) {
    while (!in.empty() && std::isspace(static_cast<unsigned char>(in.front()))) in.remove_prefix(1);
    while (!in.empty() && std::isspace(static_cast<unsigned char>(in.back()))) in.remove_suffix(1);
    if (in.empty() || in.front() == '-' || in.front() == '+') return false;
    std::string digits;
    int frac = -1;
    for (char c : in) {
        if (c == '.') {
            if (frac >= 0) return false;
            frac = 0;
            continue;
        }
        if (c < '0' || c > '9') return false;
        digits.push_back(c);
        if (frac >= 0) ++frac;
    }
    if (digits.empty() || frac > 18) return false;
    std::size_t start = 0;
    while (start + 1 < digits.size() && digits[start] == '0') ++start;
    mant = 0;
    for (std::size_t i = start; i < digits.size(); ++i) {
        mant = mant * 10 + (digits[i] - '0');
    }
    scale = frac < 0 ? 0 : frac;
    return mant > 0;
}

inline bool pow10_i128(int n, __int128& out) {
    if (n < 0 || n > 30) return false;
    out = 1;
    for (int i = 0; i < n; ++i) out *= 10;
    return true;
}

inline std::string format_half_ticks(__int128 half) {
    if (half < 0) return {};
    const auto whole = static_cast<unsigned long long>(half / 2);
    if ((half % 2) == 0) return std::to_string(whole);
    return std::to_string(whole) + ".5";
}

/// `numer/denom` is `2 * mark`. Sell rounds up to the 0.5 tick at or above mark+500.
/// Buy rounds down to the 0.5 tick at or below mark-500.
inline std::optional<SafeQuotes> quotes_from_twice_mark(__int128 numer, __int128 denom) {
    if (denom <= 0 || numer <= 0) return std::nullopt;
    const __int128 sell_num = numer + 1000 * denom;
    const __int128 sell_half = (sell_num + denom - 1) / denom;
    const __int128 buy_num = numer - 1000 * denom;
    if (buy_num <= 0) return std::nullopt;
    const __int128 buy_half = buy_num / denom;
    if (buy_half < 4 || sell_half <= 0) return std::nullopt;

    SafeQuotes q;
    q.sell = format_half_ticks(sell_half);
    q.buy = format_half_ticks(buy_half);
    q.buy_modify = format_half_ticks(buy_half - 2);
    q.ladder[0] = q.buy;
    q.ladder[1] = format_half_ticks(buy_half - 2);
    q.ladder[2] = format_half_ticks(buy_half - 4);
    if (q.sell.empty() || q.buy.empty() || q.buy_modify.empty() || q.ladder[2].empty()) {
        return std::nullopt;
    }
    return q;
}

inline std::optional<SafeQuotes> quotes_from_mark_string(std::string_view mark) {
    __int128 mant = 0;
    int scale = 0;
    if (!parse_positive_decimal(mark, mant, scale)) return std::nullopt;
    __int128 denom = 0;
    if (!pow10_i128(scale, denom)) return std::nullopt;
    return quotes_from_twice_mark(2 * mant, denom);
}

inline std::optional<SafeQuotes> quotes_from_notional_over_size(std::string_view notional,
                                                                std::string_view size) {
    __int128 n_mant = 0;
    __int128 s_mant = 0;
    int n_scale = 0;
    int s_scale = 0;
    if (!parse_positive_decimal(notional, n_mant, n_scale)) return std::nullopt;
    if (!parse_positive_decimal(size, s_mant, s_scale)) return std::nullopt;
    __int128 n_pow = 0;
    __int128 s_pow = 0;
    if (!pow10_i128(n_scale, n_pow) || !pow10_i128(s_scale, s_pow)) return std::nullopt;
    // 2 * (notional/size) = 2 * n_mant * 10^s_scale / (s_mant * 10^n_scale)
    return quotes_from_twice_mark(2 * n_mant * s_pow, s_mant * n_pow);
}

inline std::uint64_t json_u64(const nlohmann::json& row, const char* key) {
    const auto it = row.find(key);
    if (it == row.end() || it->is_null()) return 0;
    if (it->is_number_unsigned()) return it->get<std::uint64_t>();
    if (it->is_number_integer()) {
        const auto v = it->get<std::int64_t>();
        return v < 0 ? 0 : static_cast<std::uint64_t>(v);
    }
    if (it->is_string()) {
        try {
            return static_cast<std::uint64_t>(std::stoull(it->get<std::string>()));
        } catch (...) {
            return 0;
        }
    }
    return 0;
}

inline std::optional<std::string> json_decimal(const nlohmann::json& row, const char* key) {
    const auto it = row.find(key);
    if (it == row.end() || it->is_null()) return std::nullopt;
    if (it->is_string()) {
        if (it->get<std::string>().empty()) return std::nullopt;
        return it->get<std::string>();
    }
    if (it->is_number()) return it->dump();
    return std::nullopt;
}

inline std::uint64_t btc_symbol_id(const std::string& rest_base) {
    if (rest_base.empty()) return 0;
    const auto instruments = godark::load_instruments_from_edge(rest_base);
    const auto it = instruments.find(kSymbol);
    if (it == instruments.end() || it->second.symbol_id == 0) return 0;
    return it->second.symbol_id;
}

inline std::optional<SafeQuotes> quotes_from_open_interest(const nlohmann::json& oi,
                                                           std::uint64_t symbol_id) {
    if (!oi.is_array() || symbol_id == 0) return std::nullopt;
    for (const auto& row : oi) {
        if (!row.is_object() || json_u64(row, "symbol_id") != symbol_id) continue;
        const auto notional = json_decimal(row, "oi_ccy");
        const auto size = json_decimal(row, "open_interest");
        if (!notional || !size) continue;
        if (auto q = quotes_from_notional_over_size(*notional, *size)) return q;
    }
    return std::nullopt;
}

inline std::optional<SafeQuotes> quotes_from_positions(const godark::PositionsSnapshot& snap,
                                                       std::uint64_t symbol_id) {
    for (const auto& row : snap.rows) {
        if (row.symbol_id != symbol_id || !row.mark_price || row.mark_price->empty()) continue;
        if (auto q = quotes_from_mark_string(*row.mark_price)) return q;
    }
    return std::nullopt;
}

inline bool ack_ok(const godark::OrderAck& ack) {
    return ack.success && !ack.order_id.empty();
}

inline bool decimal_is_zero(std::string_view raw) {
    bool saw_digit = false;
    for (char c : raw) {
        if (c == '.' || c == '+' || std::isspace(static_cast<unsigned char>(c))) continue;
        if (c < '0' || c > '9') return false;
        saw_digit = true;
        if (c != '0') return false;
    }
    return saw_digit;
}

/// Truncate toward zero to at most 4 decimal places. Empty when the result is zero.
inline std::optional<std::string> truncate_qty_4(std::string_view raw) {
    std::string s(raw);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    if (!s.empty() && s.front() == '+') s.erase(s.begin());
    if (s.empty() || s.front() == '-') return std::nullopt;
    const auto dot = s.find('.');
    std::string whole = dot == std::string::npos ? s : s.substr(0, dot);
    std::string frac = dot == std::string::npos ? std::string{} : s.substr(dot + 1);
    if (whole.empty()) whole = "0";
    if (!std::all_of(whole.begin(), whole.end(), ::isdigit)) return std::nullopt;
    if (!std::all_of(frac.begin(), frac.end(), ::isdigit)) return std::nullopt;
    if (frac.size() > 4) frac.resize(4);
    while (!frac.empty() && frac.back() == '0') frac.pop_back();
    const bool nonzero = whole.find_first_not_of('0') != std::string::npos
        || frac.find_first_not_of('0') != std::string::npos;
    if (!nonzero) return std::nullopt;
    if (frac.empty()) return whole;
    return whole + "." + frac;
}

inline void print_quotes(const SafeQuotes& q, const char* source) {
    std::cout << "live mark source=" << source
              << " post-only SELL @" << q.sell
              << " post-only BUY @" << q.buy
              << " qty=" << kQty << "\n";
}

}  // namespace godark_examples
