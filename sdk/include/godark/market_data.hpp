#pragma once

#include <functional>
#include <memory>
#include <string>

#include <nlohmann/json_fwd.hpp>

#include <godark/client.hpp>
#include <godark/visibility.hpp>

namespace godark {

/// Public market-data WebSocket client (order book, trades).
class GODARK_API MarketDataClient {
public:
    /// Construct with a base URL. The client appends `/ws/gomarket`
    /// automatically; trailing `/`, legacy `/ws`, and canonical `/ws/v1`
    /// suffixes are all stripped before appending.
    explicit MarketDataClient(const std::string& base_url);
    /// Construct with a transport configuration. `transport.tls_skip_verify=true`
    /// disables peer-certificate verification for the `wss://` handshake (dev /
    /// testnet only -- never enable against production). The `/ws/gomarket`
    /// suffix is appended automatically as in the single-arg constructor.
    MarketDataClient(const std::string& base_url, TransportConfig transport);
    ~MarketDataClient();

    // Non-copyable and non-movable. The PIMPL owns a live WebSocket
    // transport and a running heartbeat thread; the thread's worker
    // captures pointers into `Impl`, so moving `Impl` while the thread is
    // alive would dangle those captures. Aligned with `GodarkClient` for
    // a single ownership story across the public SDK surface.
    MarketDataClient(const MarketDataClient&) = delete;
    MarketDataClient& operator=(const MarketDataClient&) = delete;
    MarketDataClient(MarketDataClient&&) = delete;
    MarketDataClient& operator=(MarketDataClient&&) = delete;

    void connect();
    void disconnect();

    bool is_connected() const;

    void subscribe_orderbook(
        const std::string& symbol,
        std::function<void(const nlohmann::json&)> callback);
    void subscribe_trades(
        const std::string& symbol,
        std::function<void(const nlohmann::json&)> callback);
    void unsubscribe(const std::string& channel, const std::string& symbol);

private:
#if defined(GODARK_ENABLE_TEST_PEER)
    friend class MarketDataClientTestPeer;
#endif
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace godark
