#include <atomic>
#include <cctype>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

#include <godark/godark.hpp>
#include <nlohmann/json.hpp>

namespace {

std::atomic<bool> g_running{true};

void handle_signal(int) {
    g_running = false;
}

std::string edge_base_url() {
    if (const char* v = std::getenv("GODARK_EDGE_URL")) return v;
    if (const char* v = std::getenv("GDX_EDGE_URL")) return v;
    return "wss://api.godark-dex.com";
}

bool tls_skip_verify() {
    auto truthy = [](const char* v) {
        if (!v || !*v) return false;
        std::string s(v);
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s == "1" || s == "true" || s == "yes";
    };
    if (truthy(std::getenv("GDX_TLS_SKIP_VERIFY"))) return true;
    if (truthy(std::getenv("GODARK_TLS_SKIP_VERIFY"))) return true;
    return false;
}

} // namespace

int main() {
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    godark::TransportConfig transport;
    transport.tls_skip_verify = tls_skip_verify();
    godark::MarketDataClient client(edge_base_url(), std::move(transport));

    try {
        client.connect();
        client.subscribe_orderbook("BTC-USDC-PERP", [](const nlohmann::json& msg) {
            std::cout << "[orderbook] " << msg.dump() << std::endl;
        });
        client.subscribe_trades("BTC-USDC-PERP", [](const nlohmann::json& msg) {
            std::cout << "[trades] " << msg.dump() << std::endl;
        });

        std::cout << "Connected to GoMarket feed. Press Ctrl+C to exit." << std::endl;
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        client.disconnect();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "market_data_example failed: " << e.what() << std::endl;
        return 1;
    }
}
