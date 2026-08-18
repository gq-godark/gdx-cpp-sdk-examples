// GoDark SDK -- Quickstart Example (C++)
//
// Place a limit sell, then cancel it.
// This MM distribution supports MARKET and LIMIT order placement only.
//
// GODARK_API_KEY_ID=gdk_... GODARK_API_SECRET=... GODARK_PASSPHRASE=... ./quickstart
// Optional: GODARK_EDGE_URL / GDX_NOISE_STATIC_PUBLIC_KEY (Testnet defaults are baked in)

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include <godark/godark.hpp>
#include "dotenv.hpp"

int main() {
    godark_examples::load_dotenv();

    const std::string key_id_env =
        godark_examples::env_first({"GODARK_API_KEY_ID", "GDX_API_KEY_ID"});
    const std::string secret_env =
        godark_examples::env_first({"GODARK_API_SECRET", "GDX_API_SECRET"});
    const std::string passphrase_env =
        godark_examples::env_first({"GODARK_PASSPHRASE", "GDX_PASSPHRASE"});
    const std::string url_env =
        godark_examples::env_first({"GODARK_EDGE_URL", "GDX_EDGE_URL"});

    if (key_id_env.empty() || secret_env.empty() || passphrase_env.empty()) {
        std::cerr << "Set GODARK_API_KEY_ID, GODARK_API_SECRET and GODARK_PASSPHRASE\n";
        return 1;
    }

    godark::ClientConfig config;
    config.api_key_id = key_id_env;
    config.api_secret = secret_env;
    config.passphrase = passphrase_env;
    config.environment = godark::Environment::Testnet;
    if (std::string pin = godark_examples::env_first(
            {"GODARK_NOISE_STATIC_PUBLIC_KEY", "GDX_NOISE_STATIC_PUBLIC_KEY",
             "GDX_NOISE_STATIC_PUBKEY"});
        !pin.empty()) {
        config.noise_static_public_key_hex = std::move(pin);
    }
    if (!url_env.empty()) config.base_url = url_env;

    const std::string tls_skip =
        godark_examples::env_first({"GODARK_TLS_SKIP_VERIFY", "GDX_TLS_SKIP_VERIFY"});
    if (tls_skip == "1" || tls_skip == "true")
        config.transport.tls_skip_verify = true;

    try {
        godark::GodarkClient client(config);
        client.connect();
        std::cout << "Connected as user " << *client.user_uuid() << "\n";

        // Book confirmation waits on private order updates; subscribe first.
        client.subscribe({"orders"});

        const std::string symbol = "BTC-USDC-PERP";
        try {
            auto ack = client.place_order(
                symbol,
                godark::Side::SELL,
                godark::OrderType::LIMIT,
                0.01,
                69515.2);
            std::cout << "Place OK -- order_id=" << ack.order_id << "\n";

            // Allow the resting order to settle before cancel (avoids CANCEL_TOO_SOON).
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            auto cancel = client.cancel_order(ack.order_id, symbol);
            std::cout << "Cancel OK -- order_id=" << cancel.order_id << "\n";
        } catch (const godark::OrderError& e) {
            std::cerr << "Order rejected: " << e.what();
            if (e.error_code) std::cerr << " [" << *e.error_code << "]";
            std::cerr << "\n";
        }

        client.disconnect();
        std::cout << "Disconnected\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
