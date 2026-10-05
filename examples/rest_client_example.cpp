// GoDark C++ SDK — minimal GodarkRestClient demo.
//
// Read-only: auth, positions, open orders, account collateral, and public
// funding / open interest / volume. It does not place orders and does not
// call /auth/me, leverage, or balance.
//
//   ./rest_client_example
//
// Environment (GDX_* aliases accepted):
//   GODARK_API_KEY_ID, GODARK_API_SECRET, GODARK_PASSPHRASE
//   GODARK_REST_URL, or GODARK_EDGE_URL / GDX_EDGE_URL (wss:// becomes https://)

#include <iostream>
#include <string>

#include <godark/godark.hpp>
#include "dotenv.hpp"
#include "live_mark.hpp"

int main() {
    godark_examples::load_dotenv();

    if (!godark_examples::live_creds_present()) {
        std::cerr << "Set GODARK_API_KEY_ID, GODARK_API_SECRET and GODARK_PASSPHRASE"
                     " (GDX_* aliases accepted)\n";
        return 1;
    }

    const std::string rest_base = godark_examples::resolve_rest_base();
    if (rest_base.empty()) {
        std::cerr << "Set GODARK_REST_URL or GODARK_EDGE_URL / GDX_EDGE_URL\n";
        return 1;
    }

    godark::GodarkRestClient::Config cfg;
    godark_examples::apply_keypair(cfg);

    try {
        godark::GodarkRestClient client{cfg};

        std::cout << "connecting (REST auth/token) rest=" << rest_base << "\n";
        client.connect();

        auto positions = client.get_positions();
        auto orders = client.get_open_orders();
        auto account = client.get_account();
        auto funding = client.get_funding_rates();
        auto interest = client.get_open_interest();
        auto volume = client.get_volume();
        std::cout << "positions: " << positions.rows.size() << " rows\n";
        for (const auto& row : positions.rows) {
            std::cout << "  position symbol_id=" << row.symbol_id
                      << " size=" << row.size << "\n";
        }
        std::cout << "open_orders: " << orders.rows.size() << " rows\n";
        for (const auto& row : orders.rows) {
            std::cout << "  order id=" << row.order_id
                      << " symbol_id=" << row.symbol_id
                      << " qty=" << row.remaining_qty << "\n";
        }
        std::cout << "account total_collateral="
                  << (account.summary ? account.summary->total_collateral : "?") << "\n";
        std::cout << "funding_rates: " << funding.size() << " rows\n";
        std::cout << "open_interest: " << interest.size() << " rows\n";
        std::cout << "volume: " << volume.dump() << "\n";

        std::cout << "REST reads succeeded.\n";
        std::cout << "For REST trading (post-only place/modify/cancel), see full_trader_rest.\n";
        client.disconnect();
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
    return 0;
}
