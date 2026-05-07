/// Minimal REST-only demo: auth → ECDH session/setup → encrypted place + cancel (needs healthy stack).
#include <cstdlib>
#include <iostream>
#include <optional>

#include <godark/enums.hpp>
#include <godark/rest_client.hpp>

#include "env_loader.hpp"
#include "error_helpers.hpp"

int main() {
    godark::examples::load_dotenv();

    try {
        const char* base = std::getenv("GDX_REST_URL");
        const char* kid = std::getenv("GDX_API_KEY_ID");
        const char* sec = std::getenv("GDX_API_SECRET");

        godark::GodarkRestClient::Config cfg;
        if (base && base[0] != '\0') cfg.rest_base_url = base;
        else cfg.rest_base_url = "https://api.godark-dex.com";

        if (kid && sec && kid[0] != '\0' && sec[0] != '\0') {
            cfg.api_key_id = kid;
            cfg.api_secret = sec;
        } else if (const char* legacy = std::getenv("GDX_API_KEY"); legacy && legacy[0] != '\0') {
            cfg.legacy_api_key = legacy;
        } else {
            cfg.legacy_api_key = "test-key-1";
        }

        godark::GodarkRestClient client{cfg};
        client.connect();

        double limit_price = 10000.0;
        if (const char* p = std::getenv("GODARK_TEST_LIMIT_PRICE")) {
            try { limit_price = std::stod(p); } catch (...) {}
        }

        auto ack = client.place_order(
            "BTC-USDC-PERP",
            godark::Side::BUY,
            godark::OrderType::LIMIT,
            0.01,
            std::optional<double>{limit_price},
            godark::TimeInForce::GTC,
            false,
            std::nullopt,
            std::nullopt,
            std::nullopt);
        if (!ack.success) {
            godark::examples::log_order_ack_failure(std::cerr, "[rest]", "place_order", ack);
            return 1;
        }
        std::cout << "place ok order_id=" << ack.order_id << " seq=" << ack.sequence << "\n";

        auto cx = client.cancel_order(ack.order_id, "BTC-USDC-PERP");
        if (!cx.success) {
            godark::examples::log_order_ack_failure(std::cerr, "[rest]", "cancel_order", cx);
            return 1;
        }
        std::cout << "cancel ok seq=" << cx.sequence << "\n";

        client.disconnect();
        return 0;
    } catch (const godark::OrderError& e) {
        godark::examples::log_order_exception(std::cerr, "[rest]", "place/cancel", e);
        return 1;
    } catch (const godark::Error& e) {
        godark::examples::log_sdk_exception(std::cerr, "[rest]", "main", "godark::Error", e);
        return 1;
    } catch (const std::exception& e) {
        godark::examples::log_sdk_exception(std::cerr, "[rest]", "main", "std::exception", e);
        return 1;
    }
}
