// GoDark SDK -- Quickstart Example (C++)
//
// Place one post-only limit sell at least 500 above the live mark, wait,
// then cancel that order. Size is 0.001. Prices are decimal strings.
// A missing mark, or a failed place or cancel, exits non-zero and does not
// leave the order working.
//
// GODARK_API_KEY_ID=gdk_... GODARK_API_SECRET=... GODARK_PASSPHRASE=... ./quickstart
// WebSocket: GODARK_EDGE_URL=wss://... (https is rewritten to wss)

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include <godark/godark.hpp>
#include "dotenv.hpp"
#include "live_mark.hpp"

int main() {
    godark_examples::load_dotenv();

    const std::string key_id_env =
        godark_examples::env_first({"GODARK_API_KEY_ID", "GDX_API_KEY_ID"});
    const std::string secret_env =
        godark_examples::env_first({"GODARK_API_SECRET", "GDX_API_SECRET"});
    const std::string passphrase_env =
        godark_examples::env_first({"GODARK_PASSPHRASE", "GDX_PASSPHRASE"});
    const std::string url_env = godark_examples::resolve_edge_url();

    godark::ClientConfig config;
    const std::string legacy =
        godark_examples::env_first({"GODARK_API_KEY", "GDX_API_KEY"});
    if (!legacy.empty() && key_id_env.empty()) {
        config.api_key = legacy;
        if (auto uid = godark_examples::env_first({"GODARK_USER_UUID", "GDX_USER_UUID"});
            !uid.empty()) {
            config.user_uuid = uid;
        }
    } else if (key_id_env.empty() || secret_env.empty() || passphrase_env.empty()) {
        std::cerr << "Set GODARK_API_KEY_ID/GODARK_API_SECRET/GODARK_PASSPHRASE "
                     "(GDX_* aliases accepted)\n";
        return 1;
    } else {
        config.api_key_id = key_id_env;
        config.api_secret = secret_env;
        config.passphrase = passphrase_env;
    }
    config.environment = godark::Environment::Testnet;
    if (std::string account =
            godark_examples::env_first({"GODARK_ACCOUNT", "GDX_ACCOUNT"});
        !account.empty()) {
        config.account = std::move(account);
    }
    if (std::string pin = godark_examples::env_first(
            {"GODARK_HPKE_STATIC_PUBLIC_KEY", "GDX_HPKE_STATIC_PUBLIC_KEY",
             "GDX_HPKE_STATIC_PUBKEY"});
        !pin.empty()) {
        config.hpke_static_public_key_hex = std::move(pin);
    }
    if (!url_env.empty()) config.base_url = url_env;

    const std::string tls_skip =
        godark_examples::env_first({"GODARK_TLS_SKIP_VERIFY", "GDX_TLS_SKIP_VERIFY"});
    if (tls_skip == "1" || tls_skip == "true")
        config.transport.tls_skip_verify = true;

    const std::string rest_base = godark_examples::resolve_rest_base();
    const std::uint64_t symbol_id = godark_examples::btc_symbol_id(rest_base);
    if (symbol_id == 0) {
        std::cerr << "No BTC-USDC-PERP instrument from the edge; placing nothing\n";
        return 1;
    }

    std::optional<godark_examples::SafeQuotes> quotes;
    try {
        godark::GodarkRestClient::Config probe_cfg;
        if (!key_id_env.empty()) {
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
        std::cerr << "No live mark (open interest notional/size or position mark); placing nothing\n";
        return 1;
    }

    try {
        godark::GodarkClient client(config);
        client.connect();
        std::cout << "Connected as account "
                  << client.account().value_or("<unavailable>") << "\n";

        client.subscribe({"orders"});

        std::string order_id;
        try {
            auto ack = client.place_order(
                godark_examples::kSymbol,
                godark::Side::SELL,
                godark::OrderType::LIMIT,
                godark_examples::kQty,
                quotes->sell,
                godark::TimeInForce::GTC,
                godark::PlaceOrderConfirmation::Book,
                godark::PlaceOrderOptions{.post_only = true});
            if (!godark_examples::ack_ok(ack)) {
                std::cerr << "Place failed\n";
                client.disconnect();
                return 1;
            }
            order_id = ack.order_id;
            std::cout << "Place OK -- order_id=" << order_id
                      << " (post-only SELL @ " << quotes->sell << ")\n";
        } catch (const godark::OrderError& e) {
            std::cerr << "Order rejected: " << e.what();
            if (e.error_code) std::cerr << " [" << *e.error_code << "]";
            std::cerr << "\n";
            client.disconnect();
            return 1;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));

        try {
            auto cancel = client.cancel_order(order_id, godark_examples::kSymbol);
            if (!godark_examples::ack_ok(cancel)) {
                std::cerr << "Cancel failed for order_id=" << order_id << "\n";
                client.disconnect();
                return 1;
            }
            std::cout << "cancel OK -- order_id=" << cancel.order_id << "\n";
        } catch (const std::exception& e) {
            std::cerr << "Cancel failed: " << e.what() << "\n";
            try {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                client.cancel_order(order_id, godark_examples::kSymbol);
            } catch (...) {
            }
            client.disconnect();
            return 1;
        }

        client.disconnect();
        std::cout << "Disconnected\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
