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

static std::string env_first(std::initializer_list<const char*> names) {
    for (const char* name : names) {
        const char* val = std::getenv(name);
        if (val && val[0] != '\0') return val;
    }
    return "";
}

int main() {
    godark_examples::load_dotenv();

    const char* key_id_env = std::getenv("GODARK_API_KEY_ID");
    const char* secret_env = std::getenv("GODARK_API_SECRET");
    const char* passphrase_env = std::getenv("GODARK_PASSPHRASE");
    const char* url_env    = std::getenv("GODARK_EDGE_URL");

    if (!key_id_env || !secret_env || !passphrase_env) {
        std::cerr << "Set GODARK_API_KEY_ID, GODARK_API_SECRET and GODARK_PASSPHRASE\n";
        return 1;
    }

    godark::ClientConfig config;
    config.api_key_id = key_id_env;
    config.api_secret = secret_env;
    config.passphrase = passphrase_env;
    config.environment = godark::Environment::Testnet;
    if (std::string pin = env_first(
            {"GDX_NOISE_STATIC_PUBLIC_KEY", "GDX_NOISE_STATIC_PUBKEY",
             "GODARK_NOISE_STATIC_PUBLIC_KEY"});
        !pin.empty()) {
        config.noise_static_public_key_hex = std::move(pin);
    }
    if (url_env && url_env[0] != '\0') config.base_url = url_env;

    const char* tls_skip = std::getenv("GODARK_TLS_SKIP_VERIFY");
    if (tls_skip && (std::string(tls_skip) == "1" || std::string(tls_skip) == "true"))
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
                999999.0);
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
