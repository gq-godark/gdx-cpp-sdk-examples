/// GoDark C++ SDK — Trader Reference Example
///
/// Demonstrates:
///   1. Load credentials from .env / environment
///   2. Connect and authenticate
///   3. Register callbacks for order + position updates
///   4. Subscribe to private streams
///   5. Place, modify, and cancel MARKET/LIMIT orders
///   6. Drain queued updates with try_recv_order()
///   7. Clean disconnect

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include <godark/godark.hpp>
#include <godark/rest_client.hpp>

#include "dotenv.hpp"

static const char* SYMBOL = "BTC-USDC-PERP";

static std::string env_or(const char* name, const char* fallback) {
    const char* val = std::getenv(name);
    if (val && val[0] != '\0') return val;
    return fallback;
}

int main() {
    godark_examples::load_dotenv();

    const std::string sep(60, '=');
    std::cout << sep << "\n  GoDark SDK — Trader Reference Example\n" << sep << "\n";
    std::cout << "Order-type support in this distribution: MARKET, LIMIT\n";

    godark::ClientConfig cfg;
    cfg.api_key_id = env_or("GODARK_API_KEY_ID", "");
    cfg.api_secret = env_or("GODARK_API_SECRET", "");
    cfg.passphrase = env_or("GODARK_PASSPHRASE", "");
    cfg.base_url = env_or("GODARK_EDGE_URL", "wss://api.godark-dex.com");
    cfg.auto_reconnect = true;
    cfg.stream_buffer_size = 256;
    cfg.transport.command_timeout_sec = 10;
    cfg.transport.heartbeat_interval_sec = 30;
    cfg.transport.stale_timeout_sec = 60;

    const char* tls_skip = std::getenv("GODARK_TLS_SKIP_VERIFY");
    if (tls_skip && (std::string(tls_skip) == "1" || std::string(tls_skip) == "true"))
        cfg.transport.tls_skip_verify = true;

    if (cfg.api_key_id.empty() || cfg.api_secret.empty() || cfg.passphrase.empty()) {
        std::cerr << "Missing credentials. Set GODARK_API_KEY_ID, GODARK_API_SECRET and "
                     "GODARK_PASSPHRASE (or provide them in .env).\n";
        return 1;
    }

    std::cout << "Endpoint: " << cfg.base_url << "\n";

    try {
        godark::GodarkRestClient::Config rcfg;
        rcfg.api_key_id = cfg.api_key_id;
        rcfg.api_secret = cfg.api_secret;
        rcfg.passphrase = cfg.passphrase;
        godark::GodarkRestClient rest{rcfg};
        rest.connect();
        std::cout << "Balance: shielded_raw=" << rest.get_my_balance().shielded_balance_raw << "\n";
        rest.disconnect();
    } catch (const std::exception& e) {
        std::cerr << "Balance fetch failed: " << e.what() << "\n";
    }

    godark::GodarkClient client(cfg);

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
    };

    client.on_system_health = [&](const godark::SystemHealthUpdate& h) {
        ++health_count;
        std::cout << "HEALTH total=" << h.total_nodes
                  << "  ready=" << h.ready
                  << "  accepting=" << (h.accepting_orders ? "yes" : "no") << "\n";
    };

    client.on_balance_update = [&](const godark::BalanceUpdate& b) {
        ++balance_count;
        std::cout << "BAL    shielded_raw=" << b.shielded_balance_raw << "\n";
    };

    client.on_margin_alert = [&](const godark::MarginAlert& a) {
        ++margin_count;
        std::cout << "MARGIN symbol=" << a.symbol_id
                  << "  tier=" << a.tier
                  << "  ratio_bps=" << a.margin_ratio_bps
                  << (a.recovered ? "  (recovered)" : "") << "\n";
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
                  << "  users=" << s.affected_user_uuids.size() << "\n";
    };

    client.on_reconnect = []() {
        std::cout << "RECONNECTED -- channels restored automatically\n";
    };

    client.on_error = [&](const godark::Error& e) {
        ++error_count;
        std::cerr << "SDK ERROR (non-fatal): " << e.what() << "\n";
    };

    std::cout << "Connecting...\n";
    try {
        client.connect();
    } catch (const godark::Error& e) {
        std::cerr << "Failed to connect: " << e.what() << "\n";
        return 1;
    }

    auto uid = client.user_uuid();
    std::cout << "Authenticated as user_uuid=" << (uid ? *uid : "?")
              << "  (session encrypted)\n";

    client.subscribe({"orders", "positions"});
    std::cout << "Subscribed to order + position updates\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto fmt_err = [](const godark::OrderError& e) {
        std::string out = e.what();
        if (e.error_code) out += " [" + *e.error_code + "]";
        return out;
    };

    std::cout << "Placing limit BUY...\n";
    godark::OrderAck buy_ack;
    try {
        buy_ack = client.place_order(
            SYMBOL, godark::Side::BUY, godark::OrderType::LIMIT,
            0.1, 67500.0, godark::TimeInForce::GTC);
        std::cout << "BUY placed: order_id=" << buy_ack.order_id
                  << "  sequence=" << buy_ack.sequence << "\n";
    } catch (const godark::OrderError& e) {
        std::cerr << "BUY rejected: " << fmt_err(e) << "\n";
        client.disconnect();
        return 1;
    } catch (const godark::Error& e) {
        std::cerr << "BUY failed: " << e.what() << "\n";
        client.disconnect();
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << "Modifying order price to 68000...\n";
    try {
        auto mod_ack = client.modify_order(buy_ack.order_id, SYMBOL, 68000.0);
        std::cout << "Modified: order_id=" << mod_ack.order_id << "\n";
    } catch (const godark::OrderError& e) {
        std::cerr << "Modify rejected: " << fmt_err(e) << "\n";
    } catch (const godark::Error& e) {
        std::cerr << "Modify rejected: " << e.what() << "\n";
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << "Placing limit SELL...\n";
    try {
        auto sell_ack = client.place_order(
            SYMBOL, godark::Side::SELL, godark::OrderType::LIMIT,
            0.05, 95000.0);
        std::cout << "SELL placed: order_id=" << sell_ack.order_id << "\n";

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        auto cancel_ack = client.cancel_order(sell_ack.order_id, SYMBOL);
        std::cout << "SELL cancelled: order_id=" << cancel_ack.order_id << "\n";
    } catch (const godark::OrderError& e) {
        std::cerr << "Sell/cancel flow: " << fmt_err(e) << "\n";
    } catch (const godark::Error& e) {
        std::cerr << "Sell/cancel flow: " << e.what() << "\n";
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << "Draining queued order updates...\n";
    int drained = 0;
    while (auto u = client.try_recv_order()) {
        ++drained;
        std::cout << "  (queued) order_id=" << u->order_id
                  << " status=" << godark::to_string(u->status) << "\n";
    }
    std::cout << "Drained " << drained << " queued order update(s)\n";

    std::cout << "Cancelling original BUY (cleanup)...\n";
    try {
        client.cancel_order(buy_ack.order_id, SYMBOL);
        std::cout << "Original BUY cancelled\n";
    } catch (...) {
        std::cout << "Original BUY already filled or cancelled\n";
    }

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

    client.disconnect();
    std::cout << "Disconnected cleanly\n";
    return 0;
}
