/// GoDark C++ SDK — Trader Reference Example
///
/// Demonstrates:
///   1. Load credentials from .env / environment
///   2. Connect and authenticate (HPKE WebSocket session)
///   3. Register callbacks for order + position updates
///   4. Subscribe to private streams
///   5. Place, modify, and cancel MARKET/LIMIT orders
///   6. Mass-quote / batch-cancel ladder demo
///   7. Drain queued updates with try_recv_order()
///   8. Clean disconnect

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <godark/godark.hpp>

#include "dotenv.hpp"

static const char* SYMBOL = "BTC-USDC-PERP";

static std::string env_or(const char* name, const char* fallback) {
    const char* val = std::getenv(name);
    if (val && val[0] != '\0') return val;
    return fallback;
}

static double live_mark_price() {
    if (const char* raw = std::getenv("GDX_LIVE_PRICE"); raw && raw[0]) {
        return std::stod(raw);
    }
    if (const char* raw = std::getenv("GODARK_E2E_PRICE"); raw && raw[0]) {
        return std::stod(raw);
    }
    return 79000.0;
}

int main() {
    godark_examples::load_dotenv();

    const std::string sep(60, '=');
    std::cout << sep << "\n  GoDark SDK — Trader Reference Example\n" << sep << "\n";
    std::cout << "Order-type support in this distribution: MARKET, LIMIT\n";

    godark::ClientConfig cfg;
    const std::string legacy =
        godark_examples::env_first({"GODARK_API_KEY", "GDX_API_KEY"});
    if (!legacy.empty()) {
        cfg.api_key = legacy;
        if (auto uid = godark_examples::env_first({"GODARK_USER_UUID", "GDX_USER_UUID"});
            !uid.empty()) {
            cfg.user_uuid = uid;
        }
    } else {
        cfg.api_key_id = godark_examples::env_first({"GODARK_API_KEY_ID", "GDX_API_KEY_ID"});
        cfg.api_secret = godark_examples::env_first({"GODARK_API_SECRET", "GDX_API_SECRET"});
        cfg.passphrase = godark_examples::env_first({"GODARK_PASSPHRASE", "GDX_PASSPHRASE"});
        if (cfg.api_key_id.empty() || cfg.api_secret.empty() || cfg.passphrase.empty()) {
            std::cerr << "Missing credentials. Set GODARK_API_KEY_ID, GODARK_API_SECRET and "
                         "GODARK_PASSPHRASE or legacy GODARK_API_KEY for localnet.\n";
            return 1;
        }
    }
    cfg.environment = godark::Environment::Testnet;
    if (std::string edge = godark_examples::env_first({"GODARK_EDGE_URL", "GDX_EDGE_URL"});
        !edge.empty()) {
        cfg.base_url = std::move(edge);
    }
    if (std::string pin = godark_examples::env_first(
            {"GODARK_HPKE_STATIC_PUBLIC_KEY", "GDX_HPKE_STATIC_PUBLIC_KEY",
             "GDX_HPKE_STATIC_PUBKEY", "GODARK_NOISE_STATIC_PUBLIC_KEY",
             "GDX_NOISE_STATIC_PUBLIC_KEY", "GDX_NOISE_STATIC_PUBKEY"});
        !pin.empty()) {
        cfg.noise_static_public_key_hex = std::move(pin);
    }
    cfg.auto_reconnect = true;
    cfg.stream_buffer_size = 256;
    cfg.transport.command_timeout_sec = 10;
    cfg.transport.heartbeat_interval_sec = 30;
    cfg.transport.stale_timeout_sec = 60;

    const std::string tls_skip =
        godark_examples::env_first({"GODARK_TLS_SKIP_VERIFY", "GDX_TLS_SKIP_VERIFY"});
    if (tls_skip == "1" || tls_skip == "true")
        cfg.transport.tls_skip_verify = true;

    if (cfg.api_key.empty() && (cfg.api_key_id.empty() || cfg.api_secret.empty() || cfg.passphrase.empty())) {
        std::cerr << "Missing credentials. Set GODARK_API_KEY_ID, GODARK_API_SECRET and "
                     "GODARK_PASSPHRASE or legacy GODARK_API_KEY for localnet.\n";
        return 1;
    }

    std::cout << "Endpoint: "
              << (cfg.base_url.empty() ? godark::edge_base_url(godark::Environment::Testnet)
                                       : cfg.base_url)
              << "\n";

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

    // BTC-USDC-PERP is symbol_id 1; capture its live mark from snapshots so the
    // mass-quote ladder/cross prices below can anchor to the real touch instead
    // of a fixed constant.
    std::optional<double> last_mark_btc;

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
            if (row.symbol_id == 1 && row.mark_price) {
                try {
                    last_mark_btc = std::stod(*row.mark_price);
                } catch (...) {
                }
            }
        }
    };

    client.on_system_health = [&](const godark::SystemHealthUpdate& h) {
        ++health_count;
        std::cout << "HEALTH component=" << h.component_id
                  << "  state=" << h.state
                  << "  serving=" << (h.serving ? "yes" : "no")
                  << "  cause=" << h.cause << "\n";
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
              << "  (HPKE session)\n";

    client.subscribe({"orders", "positions"});
    std::cout << "Subscribed to order + position updates\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto fmt_err = [](const godark::OrderError& e) {
        std::string out = e.what();
        if (e.error_code) out += " [" + *e.error_code + "]";
        return out;
    };

    std::cout << "Setting leverage to 1 via GodarkRestClient.update_leverage...\n";
    if (!cfg.api_key.empty()) {
        std::cout << "Skipping REST leverage (legacy GODARK_API_KEY; C++ REST client requires key triple)\n";
    } else try {
        godark::GodarkRestClient::Config rest_cfg;
        rest_cfg.api_key_id = cfg.api_key_id;
        rest_cfg.api_secret = cfg.api_secret;
        rest_cfg.passphrase = cfg.passphrase;
        if (!cfg.base_url.empty()) {
            std::string rest = cfg.base_url;
            if (rest.rfind("wss://", 0) == 0) rest.replace(0, 6, "https://");
            else if (rest.rfind("ws://", 0) == 0) rest.replace(0, 5, "http://");
            const auto pos = rest.find("/ws/v1");
            if (pos != std::string::npos) rest.erase(pos);
            rest_cfg.rest_base_url = rest;
        }
        godark::GodarkRestClient rest{rest_cfg};
        rest.connect();
        auto lev_ack = rest.update_leverage(SYMBOL, 1);
        std::cout << "update_leverage: success=" << (lev_ack.success ? "true" : "false")
                  << "  order_id=" << lev_ack.order_id << "\n";
        rest.disconnect();
    } catch (const godark::OrderError& e) {
        std::cerr << "update_leverage rejected: " << fmt_err(e) << "\n";
    } catch (const godark::Error& e) {
        std::cerr << "update_leverage failed: " << e.what() << "\n";
    }

    const double mark = live_mark_price();
    const double buy_px = std::round(mark * 0.997 * 10.0) / 10.0;
    std::cout << "Placing limit BUY @ " << buy_px << " (mark=" << mark << ")...\n";
    godark::OrderAck buy_ack;
    try {
        buy_ack = client.place_order(
            SYMBOL, godark::Side::BUY, godark::OrderType::LIMIT,
            0.1, buy_px, godark::TimeInForce::GTC);
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

    const double modify_px = std::round(mark * 0.996 * 10.0) / 10.0;
    std::cout << "Modifying order price to " << modify_px << "...\n";
    try {
        auto mod_ack = client.modify_order(buy_ack.order_id, SYMBOL, modify_px);
        std::cout << "Modified: order_id=" << mod_ack.order_id << "\n";
    } catch (const godark::OrderError& e) {
        std::cerr << "Modify rejected: " << fmt_err(e) << "\n";
    } catch (const godark::Error& e) {
        std::cerr << "Modify rejected: " << e.what() << "\n";
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    const double sell_px = std::round(mark * 1.03 * 10.0) / 10.0;
    std::cout << "Placing limit SELL @ " << sell_px << "...\n";
    try {
        auto sell_ack = client.place_order(
            SYMBOL, godark::Side::SELL, godark::OrderType::LIMIT,
            0.05, sell_px);
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

    // --- Bulk quote (mass quote) ---
    // Place a whole ladder of resting quotes in one batched request. Passing
    // std::nullopt (or true) for post_only keeps post-only behaviour: a leg
    // that would cross is rejected as "failed" so the batch fuses into a single
    // MPC round. Pass std::optional<bool>{false} for the relaxed path, where a
    // crossing leg takes liquidity up to its limit and rests the remainder (the
    // number of taker fills is reported per leg as fill_count).
    // Anchor to live BTC mark from the snapshot; fall back to GDX_BASE.
    double base = last_mark_btc.value_or(std::stod(env_or("GDX_BASE", "64000")));
    auto round1 = [](double x) { return std::round(x * 10.0) / 10.0; };
    std::cout << "Mass-quoting a 3-level BUY ladder (post-only), base=" << base << "...\n";
    std::vector<uint64_t> resting_ids;
    try {
        std::vector<godark::MassQuoteLegInput> ladder = {
            {"BUY", round1(base * (1 - 0.003)), 0.02},
            {"BUY", round1(base * (1 - 0.006)), 0.02},
            {"BUY", round1(base * (1 - 0.009)), 0.02},
        };
        auto mq = client.mass_quote(SYMBOL, ladder, 1, std::nullopt);
        std::cout << "Mass quote: success=" << (mq.success ? "true" : "false")
                  << "  sequence=" << mq.sequence
                  << "  legs=" << mq.results.size() << "\n";
        for (const auto& r : mq.results) {
            std::cout << "  leg " << r.leg_index << ": status=" << r.status
                      << "  new_order_id=" << (r.new_order_id ? *r.new_order_id : "-")
                      << "  fills=" << r.fill_count
                      << "  err=" << (r.error_code ? std::to_string(*r.error_code) : "-") << "\n";
            if (r.status == "open" && r.new_order_id) {
                try {
                    resting_ids.push_back(std::stoull(*r.new_order_id));
                } catch (...) {
                }
            }
        }
    } catch (const godark::Error& e) {
        std::cerr << "Mass quote rejected: " << e.what() << "\n";
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    if (!resting_ids.empty()) {
        std::cout << "Batch-cancelling " << resting_ids.size()
                  << " ladder orders (cleanup)...\n";
        try {
            auto bc = client.batch_cancel(SYMBOL, resting_ids);
            for (const auto& r : bc.results) {
                std::cout << "  cancel id=" << r.order_id
                          << ": cancelled=" << (r.cancelled ? "true" : "false")
                          << "  err=" << (r.error_code ? std::to_string(*r.error_code) : "-")
                          << "\n";
            }
        } catch (const godark::Error& e) {
            std::cerr << "Batch cancel rejected: " << e.what() << "\n";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // Demonstrate the batch-level post_only flag on a crossing leg.
    // Price a BUY ~5% above the live mark (within the ~10% oracle band).
    double cross_px = round1(base * 1.05);
    std::cout << "Mass-quoting a crossing BUY with post_only=true (expect rejected/2018)...\n";
    try {
        auto mq = client.mass_quote(
            SYMBOL, {{"BUY", cross_px, 0.001}}, 1, std::optional<bool>{true});
        for (const auto& r : mq.results) {
            std::cout << "  leg " << r.leg_index << ": status=" << r.status
                      << "  err=" << (r.error_code ? std::to_string(*r.error_code) : "-")
                      << "  fills=" << r.fill_count << "\n";
        }
    } catch (const godark::Error& e) {
        std::cerr << "post_only=true mass quote rejected: " << e.what() << "\n";
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    std::cout << "Mass-quoting a crossing BUY with post_only=false (expect filled, fills>0)...\n";
    try {
        auto mq = client.mass_quote(
            SYMBOL, {{"BUY", cross_px, 0.003}}, 1, std::optional<bool>{false});
        std::vector<std::uint64_t> stray_ids;
        for (const auto& r : mq.results) {
            std::cout << "  leg " << r.leg_index << ": status=" << r.status
                      << "  new_order_id=" << (r.new_order_id ? *r.new_order_id : "-")
                      << "  err=" << (r.error_code ? std::to_string(*r.error_code) : "-")
                      << "  fills=" << r.fill_count << "\n";
            if (r.status == "open" && r.new_order_id) {
                try {
                    stray_ids.push_back(std::stoull(*r.new_order_id));
                } catch (...) {
                    // non-numeric id; skip
                }
            }
        }
        if (!stray_ids.empty()) {
            std::cout << "Batch-cancelling " << stray_ids.size()
                      << " post_only=false remainder(s)...\n";
            try {
                auto bc = client.batch_cancel(SYMBOL, stray_ids);
                for (const auto& r : bc.results) {
                    std::cout << "  cancel id=" << r.order_id
                              << ": cancelled=" << (r.cancelled ? "true" : "false")
                              << "  err=" << (r.error_code ? std::to_string(*r.error_code) : "-")
                              << "\n";
                }
            } catch (const godark::Error& e) {
                std::cerr << "post_only=false remainder cancel rejected: " << e.what() << "\n";
            }
        }
    } catch (const godark::Error& e) {
        std::cerr << "post_only=false mass quote rejected: " << e.what() << "\n";
    }
    std::this_thread::sleep_for(std::chrono::seconds(1));

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
