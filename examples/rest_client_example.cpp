// GoDark C++ SDK — minimal GodarkRestClient demo.
//
// Auth + account reads + public market-data GETs. Encrypted place/cancel/
// modify/update_leverage require GodarkClient (WebSocket / Noise XK); see
// quickstart / full_trader_example.
//
//   ./rest_client_example
//
// Environment:
//   GODARK_API_KEY_ID, GODARK_API_SECRET, GODARK_PASSPHRASE
//   GODARK_REST_URL (optional; default https://api.godark-dex.com)

#include <cstdlib>
#include <iostream>
#include <string>

#include <godark/godark.hpp>
#include "dotenv.hpp"

int main() {
    godark_examples::load_dotenv();

    const char* key_id = std::getenv("GODARK_API_KEY_ID");
    const char* secret = std::getenv("GODARK_API_SECRET");
    const char* passphrase = std::getenv("GODARK_PASSPHRASE");
    if (!key_id || !secret || !passphrase) {
        std::cerr << "Set GODARK_API_KEY_ID, GODARK_API_SECRET and GODARK_PASSPHRASE\n";
        return 1;
    }

    godark::GodarkRestClient::Config cfg;
    cfg.api_key_id = key_id;
    cfg.api_secret = secret;
    cfg.passphrase = passphrase;
    if (const char* rest = std::getenv("GODARK_REST_URL"); rest && rest[0] != '\0') {
        cfg.rest_base_url = rest;
    }

    try {
        godark::GodarkRestClient client{cfg};

        // Public market-data GETs — no connect() required.
        auto rates = client.get_funding_rates();
        auto oi = client.get_open_interest();
        auto vol = client.get_volume();
        std::cout << "funding_rates: " << rates.size() << " symbols"
                  << " (first=" << (rates.empty() ? nlohmann::json(nullptr) : rates[0]) << ")\n";
        std::cout << "open_interest: " << oi.size() << " symbols"
                  << " (first=" << (oi.empty() ? nlohmann::json(nullptr) : oi[0]) << ")\n";
        std::cout << "volume: total_24h=" << vol.value("total_volume_24h", "")
                  << " symbols=" << vol.value("symbols", nlohmann::json::array()).size() << "\n";

        std::cout << "connecting (REST auth/token)...\n";
        client.connect();

        auto me = client.get_me();
        std::cout << "me: id=" << me.id << " wallet=" << me.wallet_address << " tier=" << me.tier
                  << "\n";

        auto lev = client.get_leverage();
        std::cout << "leverage settings: " << lev.settings.size() << " entries\n";
        for (std::size_t i = 0; i < lev.settings.size() && i < 5; ++i) {
            const auto& row = lev.settings[i];
            std::cout << "  symbol_id=" << row.symbol_id << " leverage=" << row.leverage << "\n";
        }

        try {
            auto bal = client.get_my_balance();
            std::cout << "balance: shielded_raw=" << bal.shielded_balance_raw
                      << " wallet_ui=" << bal.wallet_usdt_ui << "\n";
        } catch (const std::exception& e) {
            std::cout << "get_my_balance skipped: " << e.what() << "\n";
        }

        std::cout << "REST reads succeeded.\n";
        std::cout << "Encrypted trading requires GodarkClient over WebSocket (Noise XK).\n";
        client.disconnect();
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
    return 0;
}
