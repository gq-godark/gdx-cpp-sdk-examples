// Docs-wire encrypted trading smoke: login, subscribe, trade hooks, logout.
// Configure GODARK_EDGE_URL / GDX_EDGE_URL, GODARK_AUTH_TOKEN / GDX_AUTH_TOKEN (or API key env vars); see source.

#include <godark/godark.hpp>

#include <cstdlib>
#include <iostream>
#include <string>

static std::string env_or(const char* name, const char* fallback) {
    const char* value = std::getenv(name);
    return value && value[0] != '\0' ? value : fallback;
}

template <typename Fn>
static void report(const char* name, Fn&& fn) {
    try {
        auto ack = fn();
        std::cout << name << " OK order_id=" << ack.order_id
                  << " sequence=" << ack.sequence << "\n";
    } catch (const godark::OrderError& e) {
        std::cout << name << " ORDER_ERROR " << e.what() << "\n";
    } catch (const std::exception& e) {
        std::cout << name << " ERROR " << e.what() << "\n";
    }
}

int main() {
    godark::ClientConfig cfg;
    cfg.api_key = env_or("GODARK_AUTH_TOKEN", env_or("GDX_AUTH_TOKEN", "test-key-1").c_str());
    cfg.base_url = env_or("GODARK_EDGE_URL", env_or("GDX_EDGE_URL", "wss://api.godark-dex.com").c_str());
    cfg.user_uuid = env_or(
        "GODARK_USER_UUID",
        env_or("GDX_USER_UUID", "00000000-0000-4000-8000-000000000001").c_str());
    cfg.auto_reconnect = false;

    godark::GodarkClient client(cfg);
    client.connect();
    std::cout << "connected " << *client.user_uuid() << "\n";
    client.subscribe({"orders", "positions"});
    std::cout << "subscribed\n";

    report("order.place", [&] {
        return client.place_order(
            "BTC-USDC-PERP",
            godark::Side::BUY,
            godark::OrderType::LIMIT,
            0.001,
            1.0,
            godark::TimeInForce::GTC);
    });
    report("order.modify", [&] {
        return client.modify_order("999999999", "BTC-USDC-PERP", 2.0);
    });
    report("order.cancel", [&] {
        return client.cancel_order("999999999", "BTC-USDC-PERP");
    });
    client.logout();
    std::cout << "logout OK\n";
    return 0;
}
