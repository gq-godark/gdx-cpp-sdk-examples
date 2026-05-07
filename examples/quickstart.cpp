// GoDark SDK -- Quickstart Example (C++)
//
// Place a limit sell, then cancel it.
//
// GODARK_API_KEY_ID=gdk_... GODARK_API_SECRET=... GODARK_EDGE_URL=wss://api.godark-dex.com ./quickstart

#include <cstdlib>
#include <iostream>
#include <string>

#include <godark/godark.hpp>

#include <godark/env_loader.hpp>
#include "error_helpers.hpp"

int main() {
    godark::examples::load_dotenv();

    const char* key_id_env = std::getenv("GODARK_API_KEY_ID");
    const char* secret_env = std::getenv("GODARK_API_SECRET");
    const char* url_env    = std::getenv("GODARK_EDGE_URL");
    if (!url_env) url_env  = std::getenv("GODARK_BASE_URL");

    if (!key_id_env || !secret_env) {
        std::cerr << "Set GODARK_API_KEY_ID and GODARK_API_SECRET\n";
        return 1;
    }

    godark::ClientConfig config;
    config.api_key_id = key_id_env;
    config.api_secret = secret_env;
    if (url_env) config.base_url = url_env;

    double limit_price = 999999.0;
    if (const char* p = std::getenv("GODARK_TEST_LIMIT_PRICE")) {
        try { limit_price = std::stod(p); } catch (...) {}
    }

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
            limit_price);
        if (!ack.success) {
            godark::examples::log_order_ack_failure(std::cerr, "[quickstart]", "place_order", ack);
            return 1;
        }
        std::cout << "Place OK -- order_id=" << ack.order_id << " price=" << limit_price << "\n";

        const char* skip_cancel = std::getenv("GODARK_SKIP_CANCEL");
        if (skip_cancel && skip_cancel[0]) {
            std::cout << "Skipping cancel (GODARK_SKIP_CANCEL set) -- order remains in book\n";
        } else {
            auto cancel = client.cancel_order(ack.order_id, symbol);
            if (!cancel.success) {
                godark::examples::log_order_ack_failure(std::cerr, "[quickstart]", "cancel_order", cancel);
                return 1;
            }
            std::cout << "Cancel OK -- order_id=" << cancel.order_id << "\n";
        }

        client.disconnect();
        std::cout << "Disconnected\n";
    } catch (const godark::OrderError& e) {
        godark::examples::log_order_exception(std::cerr, "[quickstart]", "place/cancel", e);
        return 1;
    } catch (const godark::AuthenticationError& e) {
        godark::examples::log_sdk_exception(std::cerr, "[quickstart]", "connect", "AuthenticationError", e);
        return 1;
    } catch (const godark::Error& e) {
        godark::examples::log_sdk_exception(std::cerr, "[quickstart]", "session", "godark::Error", e);
        return 1;
    } catch (const std::exception& e) {
        godark::examples::log_sdk_exception(std::cerr, "[quickstart]", "main", "std::exception", e);
        return 1;
    }

    return 0;
}
