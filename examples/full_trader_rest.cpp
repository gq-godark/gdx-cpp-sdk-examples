/// REST-only trader demo — auth + encrypted place/modify/cancel + snapshots.
///
///   ./full_trader_rest
///
/// Environment:
///   GODARK_API_KEY_ID, GODARK_API_SECRET, GODARK_PASSPHRASE (or GODARK_API_KEY for localnet)
///   GODARK_REST_URL (optional)
///   GDX_LIVE_PRICE (optional; default 78000)

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include <godark/godark.hpp>
#include "dotenv.hpp"

namespace {
const char* getenv_first(std::initializer_list<const char*> names) {
    for (const char* n : names) {
        if (const char* v = std::getenv(n); v && v[0] != '\0') return v;
    }
    return nullptr;
}

double live_price() {
    if (const char* p = getenv_first({"GDX_LIVE_PRICE", "GODARK_LIVE_PRICE"})) {
        return std::stod(p);
    }
    return 78000.0;
}
double rest_limit_price() {
    return live_price() - 5000.0;
}
}  // namespace

int main() {
    godark_examples::load_dotenv();

    try {
        godark::GodarkRestClient::Config cfg;
        if (const char* base = getenv_first({"GODARK_REST_URL", "GDX_REST_URL"})) {
            cfg.rest_base_url = base;
        }

        const char* kid = getenv_first({"GODARK_API_KEY_ID", "GDX_API_KEY_ID"});
        const char* sec = getenv_first({"GODARK_API_SECRET", "GDX_API_SECRET"});
        const char* pass = getenv_first({"GODARK_PASSPHRASE", "GDX_PASSPHRASE"});
        if (kid && sec && pass) {
            cfg.api_key_id = kid;
            cfg.api_secret = sec;
            cfg.passphrase = pass;
        } else if (const char* legacy = getenv_first({"GODARK_API_KEY", "GDX_API_KEY"})) {
            cfg.legacy_api_key = legacy;
        } else {
            std::cerr << "Set GODARK_API_KEY_ID, GODARK_API_SECRET and GODARK_PASSPHRASE\n";
            return 1;
        }

        godark::GodarkRestClient client{cfg};
        client.connect();

        if (auto uid = client.user_uuid()) {
            std::cout << "identity user_uuid=" << *uid
                      << " scope=" << client.token_scope().value_or("") << "\n";
        }

        const auto open_orders = client.get_open_orders();
        std::cout << "open_orders " << open_orders.rows.size() << "\n";
        const auto positions = client.get_positions();
        std::cout << "positions " << positions.rows.size() << "\n";
        const auto account = client.get_account();
        if (account.account) {
            std::cout << "account total_collateral=" << account.account->total_collateral << "\n";
        }

        const double price = rest_limit_price();
        auto ack = client.place_order("BTC-USDC-PERP", godark::Side::BUY, godark::OrderType::LIMIT,
            0.01, price, godark::TimeInForce::GTC, false, std::nullopt, std::nullopt,
            std::string("sdk-cpp-rest-demo"));
        std::cout << "placed order_id=" << ack.order_id << " success=" << std::boolalpha << ack.success
                  << "\n";

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        auto modify = client.modify_order(ack.order_id, "BTC-USDC-PERP", price - 64.0, std::nullopt);
        std::cout << "modified success=" << modify.success << "\n";

        auto cancel = client.cancel_order(ack.order_id, "BTC-USDC-PERP");
        std::cout << "cancelled success=" << cancel.success << "\n";

        client.disconnect();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
