/**
 * End-to-end trading smoke test for the GoDark C++ SDK.
 *
 * Conventions:
 *   - Credentials from environment (never hard-coded)
 *   - Optional flags for auth-only or full place+cancel
 *
 * Environment (GODARK_* preferred; GDX_* aliases supported):
 *   GODARK_API_KEY_ID / GDX_API_KEY_ID   — API key id (e.g. gdk_...)
 *   GODARK_API_SECRET / GDX_API_SECRET   — API secret
 *   GODARK_EDGE_URL / GDX_EDGE_URL       — WebSocket base (default wss://api.godark-dex.com, testnet)
 *
 * Usage:
 *   ./e2e_trading_smoke                    # full flow: connect, place, cancel
 *   ./e2e_trading_smoke --auth-only       # connect + ECDH only (no orders)
 *   ./e2e_trading_smoke --help
 *
 * Exit codes:
 *   0  Success
 *   1  Configuration / usage error
 *   2  Connection or authentication failure
 *   3  Place order failed
 *   4  Cancel order failed
 */

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>

#include <godark/godark.hpp>

#include "env_loader.hpp"

namespace {

const char* env_first_non_empty(const char* primary, const char* fallback) {
    const char* a = std::getenv(primary);
    if (a && a[0] != '\0') return a;
    const char* b = std::getenv(fallback);
    if (b && b[0] != '\0') return b;
    return nullptr;
}

void print_usage(std::ostream& o) {
    o << "e2e_trading_smoke — GoDark C++ SDK end-to-end check\n\n"
         "Environment:\n"
         "  GODARK_API_KEY_ID / GDX_API_KEY_ID\n"
         "  GODARK_API_SECRET / GDX_API_SECRET\n"
         "  GODARK_EDGE_URL / GDX_EDGE_URL (optional)\n\n"
         "Options:\n"
         "  --auth-only     Only WebSocket auth + ECDH session (no trading)\n"
         "  --help          Show this message\n";
}

struct Options {
    bool auth_only = false;
};

bool parse_args(int argc, char** argv, Options& opt) {
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage(std::cout);
            std::exit(0);
        }
        if (arg == "--auth-only") {
            opt.auth_only = true;
            continue;
        }
        std::cerr << "Unknown argument: " << arg << "\n";
        print_usage(std::cerr);
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    godark::examples::load_dotenv();

    Options opt;
    if (!parse_args(argc, argv, opt)) return 1;

    const char* key_id = env_first_non_empty("GODARK_API_KEY_ID", "GDX_API_KEY_ID");
    const char* secret = env_first_non_empty("GODARK_API_SECRET", "GDX_API_SECRET");
    const char* url    = env_first_non_empty("GODARK_EDGE_URL", "GDX_EDGE_URL");

    if (!key_id || !secret) {
        std::cerr << "Missing credentials. Set GODARK_API_KEY_ID and GODARK_API_SECRET "
                     "(or GDX_* aliases).\n";
        return 1;
    }

    godark::ClientConfig config;
    config.api_key_id = key_id;
    config.api_secret = secret;
    if (url) config.base_url = url;

    const auto t0 = std::chrono::steady_clock::now();

    try {
        godark::GodarkClient client(config);

        std::cout << "[e2e] Connecting to " << config.base_url << " …\n";
        client.connect();

        const auto t_connect = std::chrono::steady_clock::now();
        const auto ms_connect = std::chrono::duration_cast<std::chrono::milliseconds>(
            t_connect - t0).count();

        if (!client.user_uuid()) {
            std::cerr << "[e2e] ERROR: user_uuid not set after connect\n";
            return 2;
        }

        std::cout << "[e2e] Auth + ECDH OK — user_uuid=" << *client.user_uuid()
                  << " (" << ms_connect << " ms)\n";

        if (opt.auth_only) {
            client.disconnect();
            std::cout << "[e2e] --auth-only: skipping orders. Done.\n";
            return 0;
        }

        // Far-from-market limit sell so the order rests without crossing
        const std::string symbol = "BTC-USDC-PERP";
        const double qty         = 0.01;
        const double price       = 999999.0;

        std::cout << "[e2e] Placing LIMIT SELL " << qty << " @ " << price << " …\n";
        auto place_ack = client.place_order(
            symbol,
            godark::Side::SELL,
            godark::OrderType::LIMIT,
            qty,
            price);

        if (!place_ack.success) {
            std::cerr << "[e2e] ERROR: place_order rejected\n";
            return 3;
        }
        std::cout << "[e2e] Place OK — order_id=" << place_ack.order_id
                  << " sequence=" << place_ack.sequence << "\n";

        std::cout << "[e2e] Cancelling order …\n";
        auto cancel_ack = client.cancel_order(place_ack.order_id, symbol);
        if (!cancel_ack.success) {
            std::cerr << "[e2e] ERROR: cancel_order rejected\n";
            return 4;
        }
        std::cout << "[e2e] Cancel OK — order_id=" << cancel_ack.order_id << "\n";

        client.disconnect();

        const auto t1 = std::chrono::steady_clock::now();
        const auto ms_total = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        std::cout << "[e2e] Full encrypted trading path validated (" << ms_total << " ms total).\n";
        return 0;

    } catch (const godark::AuthenticationError& e) {
        std::cerr << "[e2e] AuthenticationError: " << e.what() << "\n";
        return 2;
    } catch (const godark::ConnectionError& e) {
        std::cerr << "[e2e] ConnectionError: " << e.what() << "\n";
        return 2;
    } catch (const godark::SessionError& e) {
        std::cerr << "[e2e] SessionError: " << e.what() << "\n";
        return 2;
    } catch (const godark::OrderError& e) {
        std::cerr << "[e2e] OrderError: " << e.what() << "\n";
        return 3;
    } catch (const std::exception& e) {
        std::cerr << "[e2e] Error: " << e.what() << "\n";
        return 2;
    }
}
