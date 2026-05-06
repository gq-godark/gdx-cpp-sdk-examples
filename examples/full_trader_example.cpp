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

    client.on_reconnect = []() {
        std::cout << "RECONNECTED -- channels restored automatically\n";
    };

    client.on_error = [&](const godark::Error& e) {
        ++error_count;
        std::cerr << "SDK ERROR (non-fatal): " << e.what() << "\n";
    };

    // ── 3. Connect & authenticate ──────────────────────────────────
    std::cout << "Connecting...\n";
    try {
        client.connect();
    } catch (const godark::Error& e) {
        std::cerr << "Failed to connect: " << e.what() << "\n";
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
    std::cout << "Placing limit BUY...\n";
    godark::OrderAck buy_ack;
    try {
        buy_ack = client.place_order(
            SYMBOL, godark::Side::BUY, godark::OrderType::LIMIT,
            0.1, 67500.0, godark::TimeInForce::GTC);
        std::cout << "BUY placed: order_id=" << buy_ack.order_id
                  << "  sequence=" << buy_ack.sequence << "\n";
    } catch (const godark::Error& e) {
        std::cerr << "BUY failed: " << e.what() << "\n";
        md.disconnect();
        client.disconnect();
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // ── 7. Modify the order ────────────────────────────────────────
    std::cout << "Modifying order price to $68,000...\n";
    try {
        auto mod_ack = client.modify_order(buy_ack.order_id, SYMBOL, 68000.0);
        std::cout << "Modified: order_id=" << mod_ack.order_id << "\n";
    } catch (const godark::Error& e) {
        std::cerr << "Modify rejected: " << e.what() << "\n";
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    // ── 8. Place a SELL and cancel it ──────────────────────────────
    std::cout << "Placing limit SELL...\n";
    try {
        auto sell_ack = client.place_order(
            SYMBOL, godark::Side::SELL, godark::OrderType::LIMIT,
            0.05, 95000.0);
        std::cout << "SELL placed: order_id=" << sell_ack.order_id << "\n";

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        auto cancel_ack = client.cancel_order(sell_ack.order_id, SYMBOL);
        std::cout << "SELL cancelled: order_id=" << cancel_ack.order_id << "\n";
    } catch (const godark::Error& e) {
        std::cerr << "Sell/cancel flow: " << e.what() << "\n";
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
              << "  Non-fatal errors received:             " << error_count << "\n"
              << sep << "\n";

    // ── 12. Disconnect ─────────────────────────────────────────────
    md.disconnect();
    client.disconnect();
    std::cout << "Disconnected cleanly\n";

    return 0;
}
