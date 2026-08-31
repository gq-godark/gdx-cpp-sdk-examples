#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace godark {

// ----- Side -----

enum class Side : int {
    BUY  = 1,
    SELL = 2,
};

inline std::string to_string(Side v) {
    switch (v) {
        case Side::BUY:  return "BUY";
        case Side::SELL: return "SELL";
    }
    throw std::invalid_argument("Unknown Side value");
}

inline Side side_from_string(std::string_view s) {
    if (s == "BUY")  return Side::BUY;
    if (s == "SELL") return Side::SELL;
    throw std::invalid_argument("Unknown Side string: " + std::string(s));
}

inline Side side_from_proto(int v) {
    switch (v) {
        case 1: return Side::BUY;
        case 2: return Side::SELL;
        default: throw std::invalid_argument("Unknown Side proto value: " + std::to_string(v));
    }
}

inline int to_proto(Side v) { return static_cast<int>(v); }

// ----- OrderType -----

enum class OrderType : int {
    MARKET     = 1,
    LIMIT      = 2,
    PEG_TO_MID = 3,
    PEG_TO_BID = 4,
    PEG_TO_ASK = 5,
};

inline std::string to_string(OrderType v) {
    switch (v) {
        case OrderType::MARKET:     return "MARKET";
        case OrderType::LIMIT:      return "LIMIT";
        case OrderType::PEG_TO_MID: return "PEG_TO_MID";
        case OrderType::PEG_TO_BID: return "PEG_TO_BID";
        case OrderType::PEG_TO_ASK: return "PEG_TO_ASK";
    }
    throw std::invalid_argument("Unknown OrderType value");
}

inline OrderType order_type_from_string(std::string_view s) {
    if (s == "MARKET")     return OrderType::MARKET;
    if (s == "LIMIT")      return OrderType::LIMIT;
    if (s == "PEG_TO_MID") return OrderType::PEG_TO_MID;
    if (s == "PEG_TO_BID") return OrderType::PEG_TO_BID;
    if (s == "PEG_TO_ASK") return OrderType::PEG_TO_ASK;
    throw std::invalid_argument("Unknown OrderType string: " + std::string(s));
}

inline OrderType order_type_from_proto(int v) {
    switch (v) {
        case 1: return OrderType::MARKET;
        case 2: return OrderType::LIMIT;
        case 3: return OrderType::PEG_TO_MID;
        case 4: return OrderType::PEG_TO_BID;
        case 5: return OrderType::PEG_TO_ASK;
        default: throw std::invalid_argument("Unknown OrderType proto value: " + std::to_string(v));
    }
}

inline int to_proto(OrderType v) { return static_cast<int>(v); }

// ----- TimeInForce -----

enum class TimeInForce : int {
    GTC = 1,
    IOC = 2,
    FOK = 3,
    GTD = 4,
};

inline std::string to_string(TimeInForce v) {
    switch (v) {
        case TimeInForce::GTC: return "GTC";
        case TimeInForce::IOC: return "IOC";
        case TimeInForce::FOK: return "FOK";
        case TimeInForce::GTD: return "GTD";
    }
    throw std::invalid_argument("Unknown TimeInForce value");
}

inline TimeInForce time_in_force_from_string(std::string_view s) {
    if (s == "GTC") return TimeInForce::GTC;
    if (s == "IOC") return TimeInForce::IOC;
    if (s == "FOK") return TimeInForce::FOK;
    if (s == "GTD") return TimeInForce::GTD;
    throw std::invalid_argument("Unknown TimeInForce string: " + std::string(s));
}

inline TimeInForce time_in_force_from_proto(int v) {
    switch (v) {
        case 1: return TimeInForce::GTC;
        case 2: return TimeInForce::IOC;
        case 3: return TimeInForce::FOK;
        case 4: return TimeInForce::GTD;
        default: throw std::invalid_argument("Unknown TimeInForce proto value: " + std::to_string(v));
    }
}

inline int to_proto(TimeInForce v) { return static_cast<int>(v); }

/// Controls when a WebSocket place_order call returns.
enum class PlaceOrderConfirmation {
    /// Return as soon as the sequencer acknowledges the command.
    Ack,
    /// Wait for the first book outcome update (safe default).
    Book,
};

// ----- OrderStatus -----

enum class OrderStatus : int {
    NEW              = 1,
    PARTIALLY_FILLED = 2,
    FILLED           = 3,
    CANCELLED        = 4,
    REJECTED         = 5,
};

inline std::string to_string(OrderStatus v) {
    switch (v) {
        case OrderStatus::NEW:              return "NEW";
        case OrderStatus::PARTIALLY_FILLED: return "PARTIALLY_FILLED";
        case OrderStatus::FILLED:           return "FILLED";
        case OrderStatus::CANCELLED:        return "CANCELLED";
        case OrderStatus::REJECTED:         return "REJECTED";
    }
    throw std::invalid_argument("Unknown OrderStatus value");
}

inline OrderStatus order_status_from_string(std::string_view s) {
    if (s == "NEW")              return OrderStatus::NEW;
    if (s == "PARTIALLY_FILLED") return OrderStatus::PARTIALLY_FILLED;
    if (s == "FILLED")           return OrderStatus::FILLED;
    if (s == "CANCELLED")        return OrderStatus::CANCELLED;
    if (s == "REJECTED")         return OrderStatus::REJECTED;
    throw std::invalid_argument("Unknown OrderStatus string: " + std::string(s));
}

inline OrderStatus order_status_from_proto(int v) {
    switch (v) {
        case 1: return OrderStatus::NEW;
        case 2: return OrderStatus::PARTIALLY_FILLED;
        case 3: return OrderStatus::FILLED;
        case 4: return OrderStatus::CANCELLED;
        case 5: return OrderStatus::REJECTED;
        default: throw std::invalid_argument("Unknown OrderStatus proto value: " + std::to_string(v));
    }
}

inline int to_proto(OrderStatus v) { return static_cast<int>(v); }

// ----- OrderUpdateType -----

enum class OrderUpdateType : int {
    OPEN             = 1,
    FILLED           = 2,
    PARTIALLY_FILLED = 3,
    CANCELLED        = 4,
    REJECTED         = 5,
    MODIFIED         = 6,
    CANCEL_REJECTED  = 7,
    MODIFY_REJECTED  = 8,
};

inline std::string to_string(OrderUpdateType v) {
    switch (v) {
        case OrderUpdateType::OPEN:             return "OPEN";
        case OrderUpdateType::FILLED:           return "FILLED";
        case OrderUpdateType::PARTIALLY_FILLED: return "PARTIALLY_FILLED";
        case OrderUpdateType::CANCELLED:        return "CANCELLED";
        case OrderUpdateType::REJECTED:         return "REJECTED";
        case OrderUpdateType::MODIFIED:         return "MODIFIED";
        case OrderUpdateType::CANCEL_REJECTED:  return "CANCEL_REJECTED";
        case OrderUpdateType::MODIFY_REJECTED:  return "MODIFY_REJECTED";
    }
    throw std::invalid_argument("Unknown OrderUpdateType value");
}

inline OrderUpdateType order_update_type_from_string(std::string_view s) {
    if (s == "OPEN")             return OrderUpdateType::OPEN;
    if (s == "FILLED")           return OrderUpdateType::FILLED;
    if (s == "PARTIALLY_FILLED") return OrderUpdateType::PARTIALLY_FILLED;
    if (s == "CANCELLED")        return OrderUpdateType::CANCELLED;
    if (s == "REJECTED")         return OrderUpdateType::REJECTED;
    if (s == "MODIFIED")         return OrderUpdateType::MODIFIED;
    if (s == "CANCEL_REJECTED")  return OrderUpdateType::CANCEL_REJECTED;
    if (s == "MODIFY_REJECTED")  return OrderUpdateType::MODIFY_REJECTED;
    throw std::invalid_argument("Unknown OrderUpdateType string: " + std::string(s));
}

inline OrderUpdateType order_update_type_from_proto(int v) {
    switch (v) {
        case 1: return OrderUpdateType::OPEN;
        case 2: return OrderUpdateType::FILLED;
        case 3: return OrderUpdateType::PARTIALLY_FILLED;
        case 4: return OrderUpdateType::CANCELLED;
        case 5: return OrderUpdateType::REJECTED;
        case 6: return OrderUpdateType::MODIFIED;
        case 7: return OrderUpdateType::CANCEL_REJECTED;
        case 8: return OrderUpdateType::MODIFY_REJECTED;
        default: throw std::invalid_argument("Unknown OrderUpdateType proto value: " + std::to_string(v));
    }
}

inline int to_proto(OrderUpdateType v) { return static_cast<int>(v); }

// ----- PositionUpdateType -----

enum class PositionUpdateType : int {
    SNAPSHOT        = 1,
    OPEN            = 2,
    INCREASE        = 3,
    DECREASE        = 4,
    CLOSE           = 5,
    FUNDING_APPLIED = 6,
};

inline std::string to_string(PositionUpdateType v) {
    switch (v) {
        case PositionUpdateType::SNAPSHOT:        return "SNAPSHOT";
        case PositionUpdateType::OPEN:            return "OPEN";
        case PositionUpdateType::INCREASE:        return "INCREASE";
        case PositionUpdateType::DECREASE:        return "DECREASE";
        case PositionUpdateType::CLOSE:           return "CLOSE";
        case PositionUpdateType::FUNDING_APPLIED: return "FUNDING_APPLIED";
    }
    throw std::invalid_argument("Unknown PositionUpdateType value");
}

inline PositionUpdateType position_update_type_from_string(std::string_view s) {
    if (s == "SNAPSHOT")        return PositionUpdateType::SNAPSHOT;
    if (s == "OPEN")            return PositionUpdateType::OPEN;
    if (s == "INCREASE")        return PositionUpdateType::INCREASE;
    if (s == "DECREASE")        return PositionUpdateType::DECREASE;
    if (s == "CLOSE")           return PositionUpdateType::CLOSE;
    if (s == "FUNDING_APPLIED") return PositionUpdateType::FUNDING_APPLIED;
    throw std::invalid_argument("Unknown PositionUpdateType string: " + std::string(s));
}

inline PositionUpdateType position_update_type_from_proto(int v) {
    switch (v) {
        case 1: return PositionUpdateType::SNAPSHOT;
        case 2: return PositionUpdateType::OPEN;
        case 3: return PositionUpdateType::INCREASE;
        case 4: return PositionUpdateType::DECREASE;
        case 5: return PositionUpdateType::CLOSE;
        case 6: return PositionUpdateType::FUNDING_APPLIED;
        default: throw std::invalid_argument("Unknown PositionUpdateType proto value: " + std::to_string(v));
    }
}

inline int to_proto(PositionUpdateType v) { return static_cast<int>(v); }

// ----- CancelReason -----

enum class CancelReason : int {
    USER_REQUESTED = 1,
    IOC_REMAINDER  = 2,
    FOK_NOT_FILLED = 3,
    EXPIRED        = 4,
    SYSTEM         = 5,
    ADL            = 6,
    LIQUIDATED_CANCELED = 7,
    MARGIN_CANCELED = 8,
    REDUCE_ONLY    = 9,
    STP_EXPIRE_TAKER = 10,
    STP_CANCEL_RESTING = 11,
};

inline std::string to_string(CancelReason v) {
    switch (v) {
        case CancelReason::USER_REQUESTED: return "USER_REQUESTED";
        case CancelReason::IOC_REMAINDER:  return "IOC_REMAINDER";
        case CancelReason::FOK_NOT_FILLED: return "FOK_NOT_FILLED";
        case CancelReason::EXPIRED:        return "EXPIRED";
        case CancelReason::SYSTEM:         return "SYSTEM";
        case CancelReason::ADL:            return "ADL";
        case CancelReason::LIQUIDATED_CANCELED: return "LIQUIDATED_CANCELED";
        case CancelReason::MARGIN_CANCELED: return "MARGIN_CANCELED";
        case CancelReason::REDUCE_ONLY:    return "REDUCE_ONLY";
        case CancelReason::STP_EXPIRE_TAKER: return "STP_EXPIRE_TAKER";
        case CancelReason::STP_CANCEL_RESTING: return "STP_CANCEL_RESTING";
    }
    throw std::invalid_argument("Unknown CancelReason value");
}

inline CancelReason cancel_reason_from_string(std::string_view s) {
    if (s == "USER_REQUESTED") return CancelReason::USER_REQUESTED;
    if (s == "IOC_REMAINDER")  return CancelReason::IOC_REMAINDER;
    if (s == "FOK_NOT_FILLED") return CancelReason::FOK_NOT_FILLED;
    if (s == "EXPIRED")        return CancelReason::EXPIRED;
    if (s == "SYSTEM")         return CancelReason::SYSTEM;
    if (s == "ADL")            return CancelReason::ADL;
    if (s == "LIQUIDATED_CANCELED") return CancelReason::LIQUIDATED_CANCELED;
    if (s == "MARGIN_CANCELED") return CancelReason::MARGIN_CANCELED;
    if (s == "REDUCE_ONLY")    return CancelReason::REDUCE_ONLY;
    if (s == "STP_EXPIRE_TAKER") return CancelReason::STP_EXPIRE_TAKER;
    if (s == "STP_CANCEL_RESTING") return CancelReason::STP_CANCEL_RESTING;
    throw std::invalid_argument("Unknown CancelReason string: " + std::string(s));
}

inline CancelReason cancel_reason_from_proto(int v) {
    switch (v) {
        case 1: return CancelReason::USER_REQUESTED;
        case 2: return CancelReason::IOC_REMAINDER;
        case 3: return CancelReason::FOK_NOT_FILLED;
        case 4: return CancelReason::EXPIRED;
        case 5: return CancelReason::SYSTEM;
        case 6: return CancelReason::ADL;
        case 7: return CancelReason::LIQUIDATED_CANCELED;
        case 8: return CancelReason::MARGIN_CANCELED;
        case 9: return CancelReason::REDUCE_ONLY;
        case 10: return CancelReason::STP_EXPIRE_TAKER;
        case 11: return CancelReason::STP_CANCEL_RESTING;
        default: throw std::invalid_argument("Unknown CancelReason proto value: " + std::to_string(v));
    }
}

inline int to_proto(CancelReason v) { return static_cast<int>(v); }

} // namespace godark
