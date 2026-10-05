/// REST trader demo — auth, snapshots, and one post-only limit buy.
/// The limit is at least 500 below the live mark, size 0.001, tick 0.5.
/// This process cancels only the order it places. Place, modify, or cancel
/// failure exits non-zero. Missing live credentials exit non-zero.
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include <godark/rest_client.hpp>
#include "dotenv.hpp"
#include "live_mark.hpp"

int main() {
    godark_examples::load_dotenv();

    if (!godark_examples::live_creds_present()) {
        std::cerr << "Missing live credentials. Set GODARK_API_KEY_ID, GODARK_API_SECRET and "
                     "GODARK_PASSPHRASE (GDX_* aliases accepted).\n";
        return 1;
    }
    const std::string rest_base = godark_examples::resolve_rest_base();
    if (rest_base.empty()) {
        std::cerr << "Set GODARK_REST_URL or GODARK_EDGE_URL / GDX_EDGE_URL\n";
        return 1;
    }

    try {
        godark::GodarkRestClient::Config cfg;
        godark_examples::apply_keypair(cfg);
        godark::GodarkRestClient client{cfg};

        const std::uint64_t symbol_id = godark_examples::btc_symbol_id(rest_base);
        if (symbol_id == 0) {
            std::cerr << "No BTC-USDC-PERP instrument; placing nothing\n";
            return 1;
        }

        std::optional<godark_examples::SafeQuotes> quotes =
            godark_examples::quotes_from_open_interest(client.get_open_interest(), symbol_id);
        const char* source = "open_interest";

        client.connect();

        if (!quotes) {
            quotes = godark_examples::quotes_from_positions(client.get_positions(), symbol_id);
            source = "positions_snapshot";
        }
        if (!quotes) {
            std::cerr << "No live mark; placing nothing\n";
            client.disconnect();
            return 1;
        }
        godark_examples::print_quotes(*quotes, source);

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

        const auto client_order_id = std::string("sdk-cpp-rest-")
            + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::system_clock::now().time_since_epoch())
                                 .count());

        godark::PlaceOrderOptions opts;
        opts.post_only = true;
        auto ack = client.place_order(
            godark_examples::kSymbol, godark::Side::BUY, godark::OrderType::LIMIT,
            std::optional<std::string>{godark_examples::kQty},
            std::optional<std::string>{quotes->buy},
            godark::TimeInForce::GTC, false, std::nullopt, std::nullopt,
            client_order_id, opts);
        if (!godark_examples::ack_ok(ack)) {
            std::cerr << "place failed\n";
            client.disconnect();
            return 1;
        }
        std::cout << "placed order_id=" << ack.order_id << " success=" << std::boolalpha
                  << ack.success << " @ " << quotes->buy << "\n";

        std::this_thread::sleep_for(std::chrono::seconds(1));

        auto modify = client.modify_order(ack.order_id, godark_examples::kSymbol,
                                           quotes->buy_modify, std::nullopt);
        if (!modify.success) {
            std::cerr << "modify failed; cancelling " << ack.order_id << "\n";
            try {
                client.cancel_order(ack.order_id, godark_examples::kSymbol);
            } catch (const std::exception& cancel_err) {
                std::cerr << "cancel after modify failure: " << cancel_err.what() << "\n";
            }
            client.disconnect();
            return 1;
        }
        std::cout << "modified success=" << modify.success << " @ " << quotes->buy_modify << "\n";

        std::this_thread::sleep_for(std::chrono::seconds(1));

        auto cancel = client.cancel_order(ack.order_id, godark_examples::kSymbol);
        if (!godark_examples::ack_ok(cancel)) {
            std::cerr << "cancel failed for order_id=" << ack.order_id << "\n";
            client.disconnect();
            return 1;
        }
        std::cout << "cancelled success=" << cancel.success << " order_id=" << cancel.order_id
                  << "\n";

        bool flat = true;
        for (const auto& row : client.get_positions().rows) {
            if (row.symbol_id == symbol_id && !godark_examples::decimal_is_zero(row.size)) {
                flat = false;
                std::cerr << "position opened symbol_id=" << row.symbol_id
                          << " size=" << row.size << "; reduce-only flatten\n";
                auto qty = godark_examples::truncate_qty_4(row.size);
                if (!qty) {
                    std::cerr << "position size is below 4 decimal places\n";
                    client.disconnect();
                    return 1;
                }
                const bool long_pos = row.side != godark::Side::SELL;
                godark::PlaceOrderOptions reduce;
                reduce.reduce_only = true;
                auto flat_ack = client.place_order(
                    godark_examples::kSymbol,
                    long_pos ? godark::Side::SELL : godark::Side::BUY,
                    godark::OrderType::LIMIT,
                    qty,
                    long_pos ? quotes->buy : quotes->sell,
                    godark::TimeInForce::IOC, false, std::nullopt, std::nullopt,
                    std::nullopt, reduce);
                if (!flat_ack.success) {
                    std::cerr << "reduce-only flatten failed\n";
                    client.disconnect();
                    return 1;
                }
            }
        }
        if (!flat) {
            for (const auto& row : client.get_positions().rows) {
                if (row.symbol_id == symbol_id && !godark_examples::decimal_is_zero(row.size)) {
                    std::cerr << "position still open size=" << row.size << "\n";
                    client.disconnect();
                    return 1;
                }
            }
            client.disconnect();
            return 1;
        }

        client.disconnect();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
