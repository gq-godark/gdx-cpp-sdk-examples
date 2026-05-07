/// Minimal REST-only demo: auth → ECDH session/setup → encrypted place + cancel (needs healthy stack).
#include <cstdlib>
#include <initializer_list>
#include <iostream>
#include <optional>

#include <godark/enums.hpp>
#include <godark/rest_client.hpp>

#include <godark/env_loader.hpp>
#include "error_helpers.hpp"

namespace {
// Pull the first non-empty value from a list of env var names. Lets us prefer
// the canonical GODARK_* names while still honoring the legacy GDX_* aliases.
const char* getenv_first(std::initializer_list<const char*> names) {
    for (const char* n : names) {
        if (const char* v = std::getenv(n); v && v[0] != '\0') return v;
    }
    return nullptr;
}
}  // namespace

int main() {
    godark::examples::load_dotenv();

    try {
        // Leaving cfg.rest_base_url empty lets the SDK's resolver pick it up
        // from GODARK_REST_URL / GDX_REST_URL or derive it from
        // GODARK_EDGE_URL (so a single env var configures both protocols).
        godark::GodarkRestClient::Config cfg;
        if (const char* base = getenv_first({"GODARK_REST_URL", "GDX_REST_URL"})) {
            cfg.rest_base_url = base;
        }

        const char* kid = getenv_first({"GODARK_API_KEY_ID", "GDX_API_KEY_ID"});
        const char* sec = getenv_first({"GODARK_API_SECRET",  "GDX_API_SECRET"});
        if (kid && sec) {
            cfg.api_key_id = kid;
            cfg.api_secret = sec;
        } else if (const char* legacy = getenv_first({"GODARK_API_KEY", "GDX_API_KEY"})) {
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
