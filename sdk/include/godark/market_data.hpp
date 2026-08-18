#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include <nlohmann/json_fwd.hpp>

#include <godark/client.hpp>
#include <godark/visibility.hpp>

namespace godark {

/// Strip edge `/ws` suffixes and append `/ws/gomarket` (WebSocket scheme).
GODARK_API std::string gomarket_ws_url(const std::string& base_url);

/// Resolve the market-data WebSocket URL.
/// Hosted edges default to `/ws/v1`. Override with `GODARK_MARKET_DATA_WS_URL`,
/// or set `GODARK_MARKET_DATA_USE_GOMARKET=1` for `/ws/gomarket`.
GODARK_API std::string resolve_market_data_ws_url(const std::string& base_url);

/// Map a market-data JSON message to callback key `channel:symbol`.
GODARK_API std::optional<std::string> subscription_callback_key(const nlohmann::json& msg);

/// Public market-data WebSocket client (order book, trades, public edge feeds).
class GODARK_API MarketDataClient {
public:
    /// Construct with a base URL. Resolved via `resolve_market_data_ws_url`
    /// (default `/ws/v1`).
    explicit MarketDataClient(const std::string& base_url);
    /// Construct with a transport configuration. `transport.tls_skip_verify=true`
    /// disables peer-certificate verification for the `wss://` handshake (dev /
    /// testnet only -- never enable against production).
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
    /// Public edge channel on `/ws/v1`: `volume`, `open_interest`, `funding_rate`.
    void subscribe_public_channel(
        const std::string& channel,
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
