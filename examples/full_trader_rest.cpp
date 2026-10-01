/// REST-only trader demo — auth + encrypted place/modify/cancel + snapshots.
/// Prices and sizes are decimal strings only (never double/float).
#include <cstdlib>
#include <iostream>
#include <string>

#include <godark/rest_client.hpp>
#include "dotenv.hpp"

namespace {

const char* getenv_first(std::initializer_list<const char*> names) {
    for (const char* n : names) {
        if (const char* v = std::getenv(n); v && v[0] != '\0') return v;
    }
    return nullptr;
}

/// Limit price as a decimal string. Override with GDX_LIVE_PRICE / GODARK_E2E_PRICE.
const char* rest_limit_price() {
    if (const char* p = getenv_first(
            {"GDX_LIVE_PRICE", "GODARK_LIVE_PRICE", "GODARK_E2E_PRICE", "GDX_E2E_PRICE"})) {
        return p;
    }
    return "74000";
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
        if (const char* legacy = getenv_first({"GODARK_API_KEY", "GDX_API_KEY"})) {
            cfg.legacy_api_key = legacy;
        } else if (kid && sec && pass) {
            cfg.api_key_id = kid;
            cfg.api_secret = sec;
            cfg.passphrase = pass;
        } else {
            cfg.legacy_api_key = "test-key-1";
        }

        godark::GodarkRestClient client{cfg};
        client.connect();

        if (auto uid = client.account()) {
            std::cout << "identity account=" << *uid
                      << " scope=" << client.token_scope().value_or("") << "\n";
        }

        const auto open_orders = client.get_open_orders();
        std::cout << "open_orders " << open_orders.rows.size() << "\n";
        const auto positions = client.get_positions();
        std::cout << "positions " << positions.rows.size() << "\n";
        const auto account = client.get_account();
        if (account.summary) {
            std::cout << "account total_collateral=" << account.summary->total_collateral << "\n";
        }

        const std::string price = rest_limit_price();
        // REST place forwards client_order_id on the body and does not register it.
        auto ack = client.place_order("BTC-USDC-PERP", godark::Side::BUY, godark::OrderType::LIMIT,
            "0.01", price, godark::TimeInForce::GTC, false, std::nullopt, std::nullopt,
            std::string("sdk-cpp-rest-demo"));
        std::cout << "placed order_id=" << ack.order_id << " success=" << std::boolalpha << ack.success
                  << " @ " << price << "\n";

        auto modify = client.modify_order(ack.order_id, "BTC-USDC-PERP", "73936", std::nullopt);
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
