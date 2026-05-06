/// Minimal REST-only demo: auth → ECDH session/setup → encrypted place + cancel (needs healthy stack).
#include <cstdlib>
#include <iostream>
#include <optional>

#include <godark/enums.hpp>
#include <godark/rest_client.hpp>

int main() {
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

        auto ack = client.place_order(
            "BTC-USDC-PERP",
            godark::Side::BUY,
            godark::OrderType::LIMIT,
            0.01,
            std::optional<double>{10000.0},
            godark::TimeInForce::GTC,
            false,
            std::nullopt,
            std::nullopt,
            std::nullopt);

        std::cout << "place ok order_id=" << ack.order_id << " seq=" << ack.sequence << "\n";

        auto cx = client.cancel_order(ack.order_id, "BTC-USDC-PERP");
        std::cout << "cancel ok seq=" << cx.sequence << "\n";

        client.disconnect();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
