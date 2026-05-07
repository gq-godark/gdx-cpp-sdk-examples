/// GoDark C++ SDK — Complete Trader Example
///
/// Demonstrates a full client workflow:
///
///   1. Configure transport (TLS, timeouts, headers)
///   2. Authenticate with API key pair
///   3. Register callbacks (order, position, error, reconnect)
///   4. Subscribe to private streams
///   5. Stream public market data
///   6. Place, modify, and cancel orders
///   7. Drain queued updates via try_recv_order()
///   8. Clean disconnect
///
/// Build this target from your SDK build directory, then run:
///
///   cmake --build . --target full_trader_example
///   ./full_trader_example
///
/// Defaults to wss://api.godark-dex.com (testnet). Override with GDX_EDGE_URL.

#include <godark/godark.hpp>
#include <nlohmann/json.hpp>

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <godark/env_loader.hpp>
#include "error_helpers.hpp"

static const char* DEFAULT_API_KEY_ID = "YOUR_API_KEY_ID";
static const char* DEFAULT_API_SECRET = "YOUR_API_SECRET";
static const char* SYMBOL = "BTC-USDC-PERP";

static std::string env_or(const char* name, const char* fallback) {
    const char* val = std::getenv(name);
    if (val && val[0] != '\0') return val;
    return fallback;
}

/// First non-empty env var from the list; `fallback` if none are set.
static std::string env_first(std::initializer_list<const char*> names, const char* fallback) {
    for (const char* n : names) {
        const char* v = std::getenv(n);
        if (v && v[0] != '\0') return v;
    }
    return fallback;
}

/// Truthy if env var is "1" / "true" / "yes" (case-insensitive).
static bool env_truthy(std::initializer_list<const char*> names) {
    for (const char* n : names) {
        const char* v = std::getenv(n);
        if (!v || v[0] == '\0') continue;
        std::string s(v);
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (s == "1" || s == "true" || s == "yes" || s == "on") return true;
    }
    return false;
}

int main() {
    godark::examples::load_dotenv();

    const std::string sep(60, '=');
    std::cout << sep << "\n  GoDark SDK — Complete Trader Example (C++)\n" << sep << "\n";

    // ── 1. Configuration ────────────────────────────────────────────
    godark::ClientConfig cfg;
    cfg.api_key_id = env_first({"GDX_API_KEY_ID", "GODARK_API_KEY_ID"}, DEFAULT_API_KEY_ID);
    cfg.api_secret = env_first({"GDX_API_SECRET", "GODARK_API_SECRET"}, DEFAULT_API_SECRET);
    cfg.base_url   = env_first({"GDX_EDGE_URL", "GODARK_EDGE_URL"}, "wss://api.godark-dex.com");
    cfg.auto_reconnect = true;
    cfg.stream_buffer_size = 256;

    cfg.transport.extra_headers = {{"X-Trader-Tag", "cpp-full-trader-demo"}};
    cfg.transport.command_timeout_sec = 10;
    cfg.transport.heartbeat_interval_sec = 30;
    cfg.transport.stale_timeout_sec = 60;
    cfg.transport.tls_skip_verify =
        env_truthy({"GDX_TLS_SKIP_VERIFY", "GODARK_TLS_SKIP_VERIFY"});

    std::cout << "Endpoint: " << cfg.base_url
              << "  (TLS skip verify=" << (cfg.transport.tls_skip_verify ? "true" : "false")
              << ")\n";

    godark::GodarkClient client(cfg);

    // ── 2. Register callbacks before connecting ─────────────────────
    int order_count = 0;
    int position_count = 0;
    int snapshot_count = 0;
    int health_count = 0;
    int balance_count = 0;
    int margin_count = 0;
    int funding_count = 0;
    int settle_count = 0;
    int error_count = 0;

    client.on_order_update = [&](const godark::OrderUpdate& u) {
        ++order_count;
        std::cout << "ORDER  " << godark::to_string(u.update_type)
                  << "  id=" << u.order_id
                  << "  status=" << godark::to_string(u.status)
                  << "  filled=" << u.filled_qty
                  << "  remaining=" << u.remaining_qty << "\n";
    };

    client.on_position_update = [&](const godark::PositionUpdate& u) {
        ++position_count;
        std::cout << "POS    side=" << godark::to_string(u.side)
                  << "  size=" << u.size
                  << "  entry=" << u.entry_price << "\n";
    };

    client.on_positions_snapshot = [&](const godark::PositionsSnapshot& s) {
        ++snapshot_count;
        std::cout << "SNAP   source=" << static_cast<int>(s.source)
                  << "  rows=" << s.rows.size()
                  << "  ts=" << s.server_timestamp << "\n";
        for (const auto& row : s.rows) {
            std::cout << "  -> symbol=" << row.symbol_id
                      << "  side=" << godark::to_string(row.side)
                      << "  size=" << row.size
                      << "  entry=" << row.entry_price
                      << "  mark=" << (row.mark_price ? *row.mark_price : "—")
                      << "\n";
        }
    };

    client.on_system_health = [&](const godark::SystemHealthUpdate& h) {
        ++health_count;
        std::cout << "HEALTH nodes=" << h.total_nodes
                  << "  accepting=" << (h.accepting_orders ? "true" : "false")
                  << "  ready=" << h.ready
                  << "  degraded=" << h.degraded << "\n";
    };

    client.on_balance_update = [&](const godark::BalanceUpdate& b) {
        ++balance_count;
        std::cout << "BAL    user=" << b.user_uuid
                  << "  shielded_raw=" << b.shielded_balance_raw
                  << "  ts=" << b.timestamp << "\n";
    };

    client.on_margin_alert = [&](const godark::MarginAlert& a) {
        ++margin_count;
        std::cout << "MARGIN owner=" << a.owner
                  << "  symbol=" << a.symbol_id
                  << "  tier=" << a.tier
                  << "  ratio_bps=" << a.margin_ratio_bps
                  << "  recovered=" << (a.recovered ? "true" : "false") << "\n";
    };

    client.on_funding_rate_update = [&](const godark::FundingRateUpdate& f) {
        ++funding_count;
        std::cout << "FUND   symbol=" << f.symbol_id
                  << "  current=" << f.current_rate
                  << "  predicted=" << f.predicted_rate << "\n";
    };

    client.on_settlement_update = [&](const godark::SettlementUpdate& s) {
        ++settle_count;
        std::cout << "SETTLE batch=" << s.batch_id
                  << "  status=" << static_cast<int>(s.status)
                  << "  tx=" << s.tx_signature << "\n";
    };

    client.on_reconnect = []() {
        std::cout << "RECONNECTED -- channels restored automatically\n";
    };

    client.on_error = [&](const godark::Error& e) {
        ++error_count;
        if (auto* oe = dynamic_cast<const godark::OrderError*>(&e)) {
            godark::examples::log_order_exception(std::cerr, "[full]", "non-fatal", *oe);
        } else {
            godark::examples::log_sdk_exception(std::cerr, "[full]", "non-fatal", "godark::Error", e);
        }
    };

    // ── 3. Connect & authenticate ──────────────────────────────────
    std::cout << "Connecting...\n";
    try {
        client.connect();
    } catch (const godark::Error& e) {
        godark::examples::log_sdk_exception(std::cerr, "[full]", "connect", "godark::Error", e);
        return 1;
    }

    auto uid = client.user_uuid();
    std::cout << "Authenticated as user_uuid=" << (uid ? *uid : "?")
              << "  (session encrypted, buffer=" << cfg.stream_buffer_size << ")\n";

    // ── 4. Subscribe to private channels ───────────────────────────
    client.subscribe({"orders", "positions"});
    std::cout << "Subscribed to order + position updates\n";

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // ── 5. Market data (public, no auth) ───────────────────────────
    godark::MarketDataClient md(cfg.base_url, cfg.transport);
    try {
        md.connect();
        md.subscribe_orderbook(SYMBOL, [](const nlohmann::json& msg) {
            auto asks = msg.find("asks");
            if (asks != msg.end() && asks->is_array() && !asks->empty()) {
                std::cout << "ORDERBOOK  best_ask=" << asks->front().dump() << "\n";
            }
        });
        md.subscribe_trades(SYMBOL, [](const nlohmann::json& msg) {
            std::cout << "TRADE  price=" << msg.value("price", "?")
                      << "  size=" << msg.value("size", "?")
                      << "  side=" << msg.value("side", "?") << "\n";
        });
        std::cout << "Market data streaming for " << SYMBOL << "\n";
    } catch (const std::exception& e) {
        std::cerr << "Market data unavailable (continuing without): " << e.what() << "\n";
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    // ── 6. Place a limit BUY ───────────────────────────────────────
    // Prices below assume a BTC mark near $80k (testnet/localnet). On
    // environments that cap mark deviation (e.g. localnet at 1000 bps),
    // override via GODARK_TEST_LIMIT_PRICE — that becomes the BUY price;
    // the modify and SELL legs are derived as small offsets from it.
    double buy_price    = 67500.0;
    if (const char* p = std::getenv("GODARK_TEST_LIMIT_PRICE")) {
        try { buy_price = std::stod(p); } catch (...) {}
    }
    const double modify_price = buy_price + 500.0;
    const double sell_price   = buy_price + 5000.0;
    std::cout << "Placing limit BUY...\n";
    godark::OrderAck buy_ack;
    try {
        buy_ack = client.place_order(
            SYMBOL, godark::Side::BUY, godark::OrderType::LIMIT,
            0.1, buy_price, godark::TimeInForce::GTC);
        if (!buy_ack.success) {
            godark::examples::log_order_ack_failure(std::cerr, "[full]", "BUY place_order", buy_ack);
            md.disconnect();
            client.disconnect();
            return 1;
        }
        std::cout << "BUY placed: order_id=" << buy_ack.order_id
                  << "  sequence=" << buy_ack.sequence << "\n";
    } catch (const godark::OrderError& e) {
        godark::examples::log_order_exception(std::cerr, "[full]", "BUY place_order", e);
        md.disconnect();
        client.disconnect();
        return 1;
    } catch (const godark::Error& e) {
        godark::examples::log_sdk_exception(std::cerr, "[full]", "BUY place_order", "godark::Error", e);
        md.disconnect();
        client.disconnect();
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // ── 7. Modify the order ────────────────────────────────────────
    std::cout << "Modifying order price to " << modify_price << "...\n";
    try {
        auto mod_ack = client.modify_order(buy_ack.order_id, SYMBOL, modify_price);
        if (!mod_ack.success) {
            godark::examples::log_order_ack_failure(std::cerr, "[full]", "modify_order", mod_ack);
        } else {
            std::cout << "Modified: order_id=" << mod_ack.order_id << "\n";
        }
    } catch (const godark::OrderError& e) {
        godark::examples::log_order_exception(std::cerr, "[full]", "modify_order", e);
    } catch (const godark::Error& e) {
        godark::examples::log_sdk_exception(std::cerr, "[full]", "modify_order", "godark::Error", e);
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // ── 8. Place a SELL and cancel it ──────────────────────────────
    std::cout << "Placing limit SELL...\n";
    try {
        auto sell_ack = client.place_order(
            SYMBOL, godark::Side::SELL, godark::OrderType::LIMIT,
            0.05, sell_price);
        if (!sell_ack.success) {
            godark::examples::log_order_ack_failure(std::cerr, "[full]", "SELL place_order", sell_ack);
        } else {
            std::cout << "SELL placed: order_id=" << sell_ack.order_id << "\n";

            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            auto cancel_ack = client.cancel_order(sell_ack.order_id, SYMBOL);
            if (!cancel_ack.success) {
                godark::examples::log_order_ack_failure(std::cerr, "[full]", "SELL cancel_order", cancel_ack);
            } else {
                std::cout << "SELL cancelled: order_id=" << cancel_ack.order_id << "\n";
            }
        }
    } catch (const godark::OrderError& e) {
        godark::examples::log_order_exception(std::cerr, "[full]", "SELL flow", e);
    } catch (const godark::Error& e) {
        godark::examples::log_sdk_exception(std::cerr, "[full]", "SELL flow", "godark::Error", e);
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // ── 9. Drain queued order updates via pull-based API ───────────
    std::cout << "Draining queued order updates...\n";
    int drained = 0;
    while (auto u = client.try_recv_order()) {
        ++drained;
        std::cout << "  (queued) order_id=" << u->order_id
                  << " status=" << godark::to_string(u->status) << "\n";
    }
    std::cout << "Drained " << drained << " queued order update(s)\n";

    // ── 10. Cancel original BUY (cleanup) ──────────────────────────
    std::cout << "Cancelling original BUY (cleanup)...\n";
    try {
        client.cancel_order(buy_ack.order_id, SYMBOL);
        std::cout << "Original BUY cancelled\n";
    } catch (...) {
        std::cout << "Original BUY already filled or cancelled\n";
    }

    // ── 11. Summary ────────────────────────────────────────────────
    std::cout << sep << "\n  Session complete\n"
              << "  Order updates received (via callback): " << order_count << "\n"
              << "  Position updates received:             " << position_count << "\n"
              << "  Positions snapshots received:          " << snapshot_count << "\n"
              << "  System health pulses received:         " << health_count << "\n"
              << "  Balance updates received:              " << balance_count << "\n"
              << "  Margin alerts received:                " << margin_count << "\n"
              << "  Funding rate updates received:         " << funding_count << "\n"
              << "  Settlement updates received:           " << settle_count << "\n"
              << "  Non-fatal errors received:             " << error_count << "\n"
              << sep << "\n";

    // ── 12. Disconnect ─────────────────────────────────────────────
    md.disconnect();
    client.disconnect();
    std::cout << "Disconnected cleanly\n";

    return 0;
}
