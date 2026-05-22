// GoDark SDK -- Quickstart Example (C++)
//
// Place a limit sell, then cancel it.
// This MM distribution supports MARKET and LIMIT order placement only.
//
// GODARK_API_KEY_ID=gdk_... GODARK_API_SECRET=... GODARK_EDGE_URL=wss://api.godark-dex.com ./quickstart

#include <cstdlib>
#include <iostream>
#include <string>

#include <godark/godark.hpp>
#include "dotenv.hpp"

int main() {
    godark_examples::load_dotenv();

    const char* key_id_env = std::getenv("GODARK_API_KEY_ID");
    const char* secret_env = std::getenv("GODARK_API_SECRET");
    const char* url_env    = std::getenv("GODARK_EDGE_URL");

    if (!key_id_env || !secret_env) {
        std::cerr << "Set GODARK_API_KEY_ID and GODARK_API_SECRET\n";
        return 1;
    }

    godark::ClientConfig config;
    config.api_key_id = key_id_env;
    config.api_secret = secret_env;
    if (url_env) config.base_url = url_env;

    const char* tls_skip = std::getenv("GODARK_TLS_SKIP_VERIFY");
    if (tls_skip && (std::string(tls_skip) == "1" || std::string(tls_skip) == "true"))
        config.transport.tls_skip_verify = true;

    try {
        godark::GodarkClient client(config);
        client.connect();
        std::cout << "Connected as user " << *client.user_uuid() << "\n";

        const std::string symbol = "BTC-USDC-PERP";
        try {
            auto ack = client.place_order(
                symbol,
                godark::Side::SELL,
                godark::OrderType::LIMIT,
                0.01,
                999999.0);
            std::cout << "Place OK -- order_id=" << ack.order_id << "\n";

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
