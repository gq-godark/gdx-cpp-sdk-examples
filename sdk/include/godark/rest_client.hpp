#pragma once

#include <godark/client.hpp>
#include <godark/enums.hpp>
#include <godark/errors.hpp>
#include <godark/types.hpp>
#include <godark/visibility.hpp>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace godark {

class RestTransport;

/// Infer [`Environment`] from a REST base URL host (for HPKE pin baking).
/// localhost/127.0.0.1 → Localnet; "devnet" → Devnet;
/// godark-dex.com (and unknown hosts) → Testnet.
[[nodiscard]] GODARK_API Environment infer_environment_from_rest_url(std::string_view rest_base);

/// REST-only trading client (AES-GCM encrypted orders via `/api/v1/*`).
/// Mirrors the Phase-B Python/Rust/JS SDKs: encrypt/decrypt locally; edge forwards ciphertext only.
class GODARK_API GodarkRestClient {
public:
    struct Config {
        /// Either `legacy_api_key` OR (`api_key_id` + `api_secret` + `passphrase`).
        std::optional<std::string> legacy_api_key;
        std::optional<std::string> api_key_id;
        std::optional<std::string> api_secret;
        std::optional<std::string> passphrase;
        std::optional<std::string> rest_base_url;
        /// Fallback when JWT omits `user_uuid` (local edge).
        std::optional<std::string> user_uuid;
        /// Pinned sequencer HPKE public key (hex). Resolves as: this field >
        /// `GDX_HPKE_STATIC_PUBLIC_KEY` (and aliases) > baked pin inferred from
        /// [`rest_base_url`] (Testnet/Devnet); Localnet has no baked pin.
        std::optional<std::string> hpke_static_public_key_hex;
        /// Overrides applied after edge fetch (or sole map when [`explicit_symbol_map`]).
        std::unordered_map<std::string, uint64_t> symbol_overrides;
        /// When true, use [`symbol_overrides`] only and skip edge instruments fetch.
        bool explicit_symbol_map = false;
    };

    explicit GodarkRestClient(Config cfg);
    ~GodarkRestClient();

    GodarkRestClient(const GodarkRestClient&) = delete;
    GodarkRestClient& operator=(const GodarkRestClient&) = delete;
    GodarkRestClient(GodarkRestClient&&) noexcept;
    GodarkRestClient& operator=(GodarkRestClient&&) noexcept;

    [[nodiscard]] bool is_session_established() const;
    [[nodiscard]] std::optional<std::string> user_uuid() const;
    [[nodiscard]] std::optional<std::string> token_scope() const;

    void connect();
    void disconnect();

    OrderAck place_order(const std::string& symbol, Side side, OrderType order_type, double quantity,
        std::optional<double> price, TimeInForce time_in_force, bool aon,
        std::optional<double> min_fill_size, std::optional<uint64_t> expiry_time,
        std::optional<std::string> client_order_id);

    OrderAck cancel_order(const std::string& order_id, const std::string& symbol);

    OrderAck cancel_order_by_client_id(const std::string& client_order_id, const std::string& symbol);

    OrderAck modify_order(const std::string& order_id, const std::string& symbol,
        std::optional<double> new_price, std::optional<double> new_quantity,
        std::optional<double> new_trigger_price = std::nullopt);

    LeverageSettings get_leverage();

    OrderAck update_leverage(const std::string& symbol, uint32_t leverage);

    /// Live open orders via encrypted `POST /api/v1/openOrders`.
    OpenOrdersSnapshot get_open_orders();

    /// Live positions via encrypted `POST /api/v1/positions`.
    PositionsSnapshot get_positions();

    /// Live account margin via encrypted `POST /api/v1/account`.
    AccountMarginUpdate get_account();

    /// Plaintext docs envelope `data` object from `GET /api/v1/orders/{id}`.
    nlohmann::json get_order(const std::string& order_id);

    /// Plaintext docs envelope `data` object from `GET /api/v1/orders?client_order_id=`.
    nlohmann::json get_order_by_client_id(const std::string& client_order_id);

    /// Poll `get_order` until status ∈ {FILLED,CANCELLED,REJECTED}.
    nlohmann::json await_terminal_status(const std::string& order_id, std::chrono::milliseconds timeout);

    /// Fetch the authenticated user's profile from `GET /api/v1/auth/me`.
    MeProfile get_me();

    /// Fetch on-chain balance snapshot for `owner` (Solana base58 wallet pubkey).
    Balance get_balance(const std::string& owner);

    /// Convenience: resolves wallet_address via `get_me()` (cached), then calls `get_balance`.
    Balance get_my_balance();

    /// Public `GET /api/v1/market-data/funding-rates` (no connect required).
    nlohmann::json get_funding_rates();

    /// Public `GET /api/v1/market-data/open-interest` (no connect required).
    nlohmann::json get_open_interest();

    /// Public `GET /api/v1/market-data/volume` (no connect required).
    nlohmann::json get_volume();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace godark
