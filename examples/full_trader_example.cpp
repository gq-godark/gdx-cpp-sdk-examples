/// GoDark C++ SDK — Trader Reference Example
///
/// Demonstrates:
///   1. Load credentials from .env / environment
///   2. Connect and authenticate (HPKE WebSocket session)
///   3. Register callbacks for order + position updates
///   4. Subscribe to private streams
///   5. Place, modify, and cancel post-only LIMIT orders priced from a live mark
///   6. Mass-quote / batch-cancel a post-only ladder
///   7. Drain queued updates with try_recv_order()
///   8. Clean disconnect
///
/// Every order this process places is post-only, size 0.001, and at least 500
/// away from the live mark (tick 0.5). Place or cancel failure exits non-zero.
/// This process cancels only those orders.

#include <chrono>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <godark/godark.hpp>

#include "dotenv.hpp"
#include "live_mark.hpp"

static const char* SYMBOL = godark_examples::kSymbol;

int main() {
    godark_examples::load_dotenv();

    const std::string sep(60, '=');
    std::cout << sep << "\n  GoDark SDK — Trader Reference Example\n" << sep << "\n";
    std::cout << "Sample orders: post-only LIMIT, priced from the live mark\n";

    godark::ClientConfig cfg;
    const std::string legacy =
        godark_examples::env_first({"GODARK_API_KEY", "GDX_API_KEY"});
    const bool live = godark_examples::live_creds_present();
    if (!live && !legacy.empty()) {
        cfg.api_key = legacy;
        if (auto account = godark_examples::env_first({"GODARK_ACCOUNT", "GDX_ACCOUNT"});
            !account.empty()) {
            cfg.account = account;
        }
    } else if (!live) {
        std::cerr << "Missing credentials. Set GODARK_API_KEY_ID, GODARK_API_SECRET and "
                     "GODARK_PASSPHRASE (GDX_* aliases accepted).\n";
        return 1;
    } else {
        cfg.api_key_id = godark_examples::env_first({"GODARK_API_KEY_ID", "GDX_API_KEY_ID"});
        cfg.api_secret = godark_examples::env_first({"GODARK_API_SECRET", "GDX_API_SECRET"});
        cfg.passphrase = godark_examples::env_first({"GODARK_PASSPHRASE", "GDX_PASSPHRASE"});
    }
    cfg.environment = godark::Environment::Testnet;
    if (std::string edge = godark_examples::resolve_edge_url(); !edge.empty()) {
        cfg.base_url = std::move(edge);
    }
    if (std::string pin = godark_examples::env_first(
            {"GODARK_HPKE_STATIC_PUBLIC_KEY", "GDX_HPKE_STATIC_PUBLIC_KEY",
             "GDX_HPKE_STATIC_PUBKEY"});
        !pin.empty()) {
        cfg.hpke_static_public_key_hex = std::move(pin);
    }
    if (std::string account = godark_examples::env_first({"GODARK_ACCOUNT", "GDX_ACCOUNT"});
        !account.empty()) {
        cfg.account = std::move(account);
    }
    cfg.auto_reconnect = true;
    cfg.stream_buffer_size = 256;
    cfg.transport.command_timeout_sec = 10;
    cfg.transport.heartbeat_interval_sec = 30;
    cfg.transport.stale_timeout_sec = 120;
    cfg.transport.missed_heartbeat_limit = 2;

    const std::string tls_skip =
        godark_examples::env_first({"GODARK_TLS_SKIP_VERIFY", "GDX_TLS_SKIP_VERIFY"});
    if (tls_skip == "1" || tls_skip == "true")
        cfg.transport.tls_skip_verify = true;

    const std::string rest_base = godark_examples::resolve_rest_base();
    const std::uint64_t symbol_id = godark_examples::btc_symbol_id(rest_base);
    if (symbol_id == 0) {
        std::cerr << "No BTC-USDC-PERP instrument; placing nothing\n";
        return 1;
    }

    std::optional<godark_examples::SafeQuotes> quotes;
    try {
        godark::GodarkRestClient::Config probe_cfg;
        if (live) {
            godark_examples::apply_keypair(probe_cfg);
        } else {
            probe_cfg.legacy_api_key = legacy;
            if (!rest_base.empty()) probe_cfg.rest_base_url = rest_base;
        }
        godark::GodarkRestClient probe{probe_cfg};
        if (auto from_oi = godark_examples::quotes_from_open_interest(
                probe.get_open_interest(), symbol_id)) {
            quotes = std::move(from_oi);
            godark_examples::print_quotes(*quotes, "open_interest");
        } else {
            probe.connect();
            if (auto from_pos = godark_examples::quotes_from_positions(
                    probe.get_positions(), symbol_id)) {
                quotes = std::move(from_pos);
                godark_examples::print_quotes(*quotes, "positions_snapshot");
            }
            probe.disconnect();
        }
    } catch (const std::exception& e) {
        std::cerr << "Live mark lookup failed: " << e.what() << "\n";
        return 1;
    }
    if (!quotes) {
        std::cerr << "No live mark; placing nothing\n";
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
    int leverage_count = 0;
    int error_count = 0;

    client.on_order_update = [&](const godark::OrderUpdate& u) {
        ++order_count;
        std::cout << "ORDER  " << godark::to_string(u.update_type)
                  << "  id=" << u.order_id
                  << "  status=" << godark::to_string(u.status)
                  << "  filled=" << u.filled_qty
                  << "  remaining=" << u.remaining_qty;
        if (u.cancel_reason.has_value()) {
            std::cout << "  cancel_reason="
                      << godark::to_string(*u.cancel_reason);
        }
        if (u.reduce_only) {
            std::cout << "  reduce_only=true";
        }
        if (u.post_only) {
            std::cout << "  post_only=true";
        }
        std::cout << "\n";
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
                  << "  rate=" << f.funding_rate
                  << "  last=" << f.last_funding_rate << "\n";
    };

    client.on_settlement_update = [&](const godark::SettlementUpdate& s) {
        ++settle_count;
        std::cout << "SETTLE batch=" << s.batch_id
                  << "  status=" << static_cast<int>(s.status)
                  << "  users=" << s.affected_user_uuids.size() << "\n";
    };

    client.on_leverage_settings = [&](const godark::LeverageSettings& ls) {
        ++leverage_count;
        std::cout << "LEVERAGE settings=" << ls.settings.size() << "\n";
        for (std::size_t i = 0; i < ls.settings.size() && i < 5; ++i) {
            const auto& row = ls.settings[i];
            std::cout << "  symbol_id=" << row.symbol_id << " leverage=" << row.leverage << "\n";
        }
    };

    client.on_reconnect = []() {
        std::cout << "RECONNECTED -- channels restored automatically\n";
    };

    client.on_error = [&](const godark::Error& e) {
        ++error_count;
        if (dynamic_cast<const godark::ConnectionError*>(&e) != nullptr
            && std::string_view(e.what()).find("stale heartbeat") != std::string_view::npos) {
            std::cerr << "STALE HEARTBEAT (non-fatal, auto-reconnect expected): "
                      << e.what() << "\n";
            return;
        }
        std::cerr << "SDK ERROR (non-fatal): " << e.what() << "\n";
    };

    std::vector<std::string> own_orders;
    auto cancel_own = [&](const std::vector<std::string>& ids) -> bool {
        bool ok = true;
        for (const auto& id : ids) {
            try {
                auto ca = client.cancel_order(id, SYMBOL);
                std::cout << "  cancel order_id=" << ca.order_id
                          << " success=" << (ca.success ? "true" : "false") << "\n";
                if (!godark_examples::ack_ok(ca)) ok = false;
            } catch (const std::exception& e) {
                std::cerr << "cancel " << id << " failed: " << e.what() << "\n";
                ok = false;
            }
        }
        return ok;
    };

    auto fail = [&](const std::string& why) -> int {
        std::cerr << why << "\n";
        if (!own_orders.empty()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            cancel_own(own_orders);
        }
        try {
            client.disconnect();
        } catch (...) {
        }
        return 1;
    };

    std::cout << "Connecting...\n";
    try {
        client.connect();
    } catch (const godark::Error& e) {
        std::cerr << "Failed to connect: " << e.what() << "\n";
        return 1;
    }

    auto account = client.account();
    std::cout << "Authenticated as account=" << (account ? *account : "?")
              << "  (HPKE session)\n";

    client.subscribe({"orders", "positions", "funding_rate"});
    std::cout << "Subscribed to order + position + funding updates\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    auto fmt_err = [](const godark::OrderError& e) {
        std::string out = e.what();
        if (e.error_code) out += " [" + *e.error_code + "]";
        return out;
    };

    std::cout << "Setting leverage to 1 via GodarkClient.update_leverage...\n";
    try {
        auto lev_ack = client.update_leverage(SYMBOL, 1);
        std::cout << "update_leverage: success=" << (lev_ack.success ? "true" : "false")
                  << "  order_id=" << lev_ack.order_id << "\n";
    } catch (const godark::OrderError& e) {
        std::cerr << "update_leverage rejected: " << fmt_err(e) << "\n";
    } catch (const godark::Error& e) {
        std::cerr << "update_leverage failed: " << e.what() << "\n";
    }

    godark::PlaceOrderOptions post_only;
    post_only.post_only = true;

    std::cout << "Placing post-only limit BUY @ " << quotes->buy << "...\n";
    std::string buy_id;
    try {
        auto buy_ack = client.place_order(
            SYMBOL, godark::Side::BUY, godark::OrderType::LIMIT,
            godark_examples::kQty, quotes->buy, godark::TimeInForce::GTC,
            godark::PlaceOrderConfirmation::Book, post_only);
        if (!godark_examples::ack_ok(buy_ack)) return fail("BUY place failed");
        buy_id = buy_ack.order_id;
        own_orders.push_back(buy_id);
        std::cout << "BUY placed: order_id=" << buy_id
                  << "  sequence=" << buy_ack.sequence << "\n";
    } catch (const godark::OrderError& e) {
        return fail("BUY rejected: " + fmt_err(e));
    } catch (const godark::Error& e) {
        return fail(std::string("BUY failed: ") + e.what());
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Modifying order price to " << quotes->buy_modify << "...\n";
    try {
        auto mod_ack = client.modify_order(buy_id, SYMBOL, quotes->buy_modify);
        if (!mod_ack.success) return fail("Modify failed");
        std::cout << "Modified: order_id=" << mod_ack.order_id << "\n";
    } catch (const godark::OrderError& e) {
        return fail("Modify rejected: " + fmt_err(e));
    } catch (const godark::Error& e) {
        return fail(std::string("Modify rejected: ") + e.what());
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Cancelling BUY " << buy_id << "...\n";
    if (!cancel_own({buy_id})) return fail("BUY cancel failed");
    own_orders.clear();

    std::cout << "Placing post-only limit SELL @ " << quotes->sell << "...\n";
    std::string sell_id;
    try {
        auto sell_ack = client.place_order(
            SYMBOL, godark::Side::SELL, godark::OrderType::LIMIT,
            godark_examples::kQty, quotes->sell, godark::TimeInForce::GTC,
            godark::PlaceOrderConfirmation::Book, post_only);
        if (!godark_examples::ack_ok(sell_ack)) return fail("SELL place failed");
        sell_id = sell_ack.order_id;
        own_orders.push_back(sell_id);
        std::cout << "SELL placed: order_id=" << sell_id << "\n";
    } catch (const godark::OrderError& e) {
        return fail("SELL rejected: " + fmt_err(e));
    } catch (const godark::Error& e) {
        return fail(std::string("SELL failed: ") + e.what());
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Cancelling SELL " << sell_id << "...\n";
    if (!cancel_own({sell_id})) return fail("SELL cancel failed");
    own_orders.clear();

    std::cout << "Draining queued order updates...\n";
    int drained = 0;
    while (auto u = client.try_recv_order()) {
        ++drained;
        std::cout << "  (queued) order_id=" << u->order_id
                  << " status=" << godark::to_string(u->status) << "\n";
    }
    std::cout << "Drained " << drained << " queued order update(s)\n";

    std::cout << "Mass-quoting a 3-level post-only BUY ladder"
              << " @" << quotes->ladder[0] << " / " << quotes->ladder[1]
              << " / " << quotes->ladder[2] << "...\n";
    std::vector<std::uint64_t> ladder_ids;
    try {
        std::vector<godark::MassQuoteLegInput> ladder = {
            {"BUY", quotes->ladder[0], godark_examples::kQty},
            {"BUY", quotes->ladder[1], godark_examples::kQty},
            {"BUY", quotes->ladder[2], godark_examples::kQty},
        };
        auto mq = client.mass_quote(SYMBOL, ladder, std::optional<bool>{true});
        std::cout << "Mass quote: success=" << (mq.success ? "true" : "false")
                  << "  sequence=" << mq.sequence
                  << "  legs=" << mq.results.size() << "\n";
        bool legs_ok = mq.success && mq.results.size() == ladder.size();
        for (const auto& r : mq.results) {
            std::cout << "  leg " << r.leg_index << ": status=" << r.status
                      << "  new_order_id=" << (r.new_order_id ? *r.new_order_id : "-")
                      << "  fills=" << r.fill_count
                      << "  err=" << (r.error_code ? std::to_string(*r.error_code) : "-") << "\n";
            if (r.status != "open" || r.fill_count != 0 || !r.new_order_id) legs_ok = false;
            if (r.new_order_id) {
                own_orders.push_back(*r.new_order_id);
                try {
                    ladder_ids.push_back(std::stoull(*r.new_order_id));
                } catch (...) {
                    legs_ok = false;
                }
            }
        }
        if (!legs_ok) return fail("Mass quote did not rest every post-only leg");
    } catch (const godark::Error& e) {
        return fail(std::string("Mass quote rejected: ") + e.what());
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Batch-cancelling " << ladder_ids.size() << " ladder order(s)...\n";
    try {
        auto bc = client.batch_cancel(SYMBOL, ladder_ids);
        bool all_cancelled = bc.results.size() == ladder_ids.size();
        for (const auto& r : bc.results) {
            std::cout << "  cancel id=" << r.order_id
                      << ": cancelled=" << (r.cancelled ? "true" : "false")
                      << "  err=" << (r.error_code ? std::to_string(*r.error_code) : "-")
                      << "\n";
            if (!r.cancelled) all_cancelled = false;
        }
        if (!all_cancelled) return fail("Batch cancel did not cancel every ladder order");
        own_orders.clear();
    } catch (const godark::Error& e) {
        return fail(std::string("Batch cancel rejected: ") + e.what());
    }

    bool opened = false;
    try {
        godark::GodarkRestClient::Config rest_cfg;
        if (live) godark_examples::apply_keypair(rest_cfg);
        else {
            rest_cfg.legacy_api_key = legacy;
            if (!rest_base.empty()) rest_cfg.rest_base_url = rest_base;
        }
        godark::GodarkRestClient rest{rest_cfg};
        rest.connect();
        for (const auto& row : rest.get_positions().rows) {
            if (row.symbol_id != symbol_id || godark_examples::decimal_is_zero(row.size)) continue;
            opened = true;
            std::cerr << "position opened size=" << row.size << "; reduce-only flatten\n";
            auto qty = godark_examples::truncate_qty_4(row.size);
            if (!qty) return fail("position size is below 4 decimal places");
            const bool long_pos = row.side == godark::Side::BUY;
            godark::PlaceOrderOptions reduce;
            reduce.reduce_only = true;
            auto flat_ack = rest.place_order(
                SYMBOL, long_pos ? godark::Side::SELL : godark::Side::BUY,
                godark::OrderType::LIMIT, qty,
                long_pos ? std::optional<std::string>{quotes->buy}
                         : std::optional<std::string>{quotes->sell},
                godark::TimeInForce::IOC, false, std::nullopt, std::nullopt,
                std::nullopt, reduce);
            if (!flat_ack.success) return fail("reduce-only flatten failed");
        }
        if (opened) {
            for (const auto& row : rest.get_positions().rows) {
                if (row.symbol_id == symbol_id && !godark_examples::decimal_is_zero(row.size)) {
                    return fail("position still open after reduce-only flatten");
                }
            }
        }
        rest.disconnect();
    } catch (const std::exception& e) {
        return fail(std::string("post-trade position check failed: ") + e.what());
    }
    if (opened) return fail("sample opened a position");

    std::cout << sep << "\n  Session complete\n"
              << "  Order updates received (via callback): " << order_count << "\n"
              << "  Position updates received:             " << position_count << "\n"
              << "  Positions snapshots received:          " << snapshot_count << "\n"
              << "  System health pulses received:         " << health_count << "\n"
              << "  Balance updates received:              " << balance_count << "\n"
              << "  Margin alerts received:                " << margin_count << "\n"
              << "  Funding rate updates received:         " << funding_count << "\n"
              << "  Settlement updates received:           " << settle_count << "\n"
              << "  Leverage settings pushes received:     " << leverage_count << "\n"
              << "  Non-fatal errors received:             " << error_count << "\n"
              << sep << "\n";

    client.disconnect();
    std::cout << "Disconnected cleanly\n";
    return 0;
}
