#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <godark/enums.hpp>

namespace godark {

struct OrderAck {
    std::string order_id;
    bool success;
    std::string sequence;
    std::optional<std::string> error_code = std::nullopt;
    std::optional<std::string> error = std::nullopt;
};

struct OrderUpdate {
    std::string order_id;
    std::string user_uuid;
    int64_t symbol_id;
    Side side;
    OrderStatus status;
    OrderUpdateType update_type;
    std::string price;
    std::string quantity;
    std::string filled_qty;
    std::string remaining_qty;
    std::string cum_fill;
    std::optional<CancelReason> cancel_reason = std::nullopt;
    std::optional<int64_t> reject_reason_code = std::nullopt;
    int64_t correlation_id = 0;
    int64_t timestamp = 0;
};

struct PositionUpdate {
    std::string user_uuid;
    int64_t symbol_id;
    Side side;
    PositionUpdateType update_type;
    std::string size;
    std::string entry_price;
    std::string previous_size = "0";
    std::string fill_price = "0";
    std::string fill_qty = "0";
    int64_t correlation_id = 0;
    int64_t timestamp = 0;
};

} // namespace godark
