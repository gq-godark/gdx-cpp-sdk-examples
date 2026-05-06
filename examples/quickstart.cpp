// GoDark SDK -- Quickstart Example (C++)
//
// Place a limit sell, then cancel it.
//
// GODARK_API_KEY_ID=gdk_... GODARK_API_SECRET=... GODARK_EDGE_URL=wss://api.godark-dex.com ./quickstart

#include <cstdlib>
#include <iostream>
#include <string>

#include <godark/godark.hpp>

int main() {
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

    try {
        godark::GodarkClient client(config);
        client.connect();
        std::cout << "Connected as user " << *client.user_uuid() << "\n";

        const std::string symbol = "BTC-USDC-PERP";
        auto ack = client.place_order(
            symbol,
            godark::Side::SELL,
            godark::OrderType::LIMIT,
            0.01,
            999999.0);
        std::cout << "Place OK -- order_id=" << ack.order_id << "\n";

        auto cancel = client.cancel_order(ack.order_id, symbol);
        std::cout << "Cancel OK -- order_id=" << cancel.order_id << "\n";

        client.disconnect();
        std::cout << "Disconnected\n";
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
