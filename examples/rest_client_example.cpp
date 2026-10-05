// GoDark C++ SDK — minimal GodarkRestClient demo.
//
// Auth + account reads. For encrypted place/modify/cancel over REST (one-shot HPKE),
// see full_trader_rest.
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

        std::cout << "connecting (REST auth/token)...\n";
        client.connect();

        auto positions = client.get_positions();
        auto orders = client.get_open_orders();
        auto account = client.get_account();
        auto funding = client.get_funding_rates();
        auto interest = client.get_open_interest();
        auto volume = client.get_volume();
        std::cout << "positions: " << positions.rows.size() << " rows\n";
        std::cout << "open_orders: " << orders.rows.size() << " rows\n";
        std::cout << "account total_collateral="
                  << (account.summary ? account.summary->total_collateral : "?") << "\n";
        std::cout << "funding_rates: " << funding.size() << " rows\n";
        std::cout << "open_interest: " << interest.size() << " rows\n";
        std::cout << "volume: " << volume.dump() << "\n";

        std::cout << "REST reads succeeded.\n";
        std::cout << "For REST trading (place/modify/cancel), see full_trader_rest.\n";
        client.disconnect();
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
    return 0;
}
