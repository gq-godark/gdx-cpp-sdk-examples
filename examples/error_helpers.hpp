// Shared error-printing helpers for the GoDark C++ SDK examples.
//
// The SDK surfaces order failures in two places:
//   1. `godark::OrderError` thrown from `place_order` / `cancel_order` /
//      `modify_order` — carries `e.what()` (reason) and an optional
//      `e.error_code` (e.g. "PriceDeviationTooLarge").
//   2. `godark::OrderAck` returned from those calls when `success == false` —
//      carries `error_code` and `error` (reason) as optional strings.
//
// Each example funnels both surfaces through these helpers so the operator
// always sees both the symbolic code and the human reason.

#pragma once

#include <iostream>
#include <string>
#include <string_view>

#include <godark/errors.hpp>
#include <godark/types.hpp>

namespace godark::examples {

inline std::string_view value_or(const std::optional<std::string>& s,
                                 std::string_view fallback = "<none>") {
    return s ? std::string_view{*s} : fallback;
}

// Use when the SDK returns an OrderAck with success == false.
inline void log_order_ack_failure(std::ostream& out,
                                  std::string_view tag,
                                  std::string_view operation,
                                  const OrderAck& ack) {
    out << tag << " " << operation << " rejected"
        << " order_id=" << (ack.order_id.empty() ? "<none>" : ack.order_id)
        << " code="     << value_or(ack.error_code)
        << " reason="   << value_or(ack.error)
        << "\n";
}

// Use inside `catch (const godark::OrderError& e)` blocks.
inline void log_order_exception(std::ostream& out,
                                std::string_view tag,
                                std::string_view operation,
                                const OrderError& e) {
    out << tag << " " << operation << " threw OrderError"
        << " code="   << value_or(e.error_code)
        << " reason=" << e.what()
        << "\n";
}

// Generic SDK error (auth / connection / session / encryption / timeout).
inline void log_sdk_exception(std::ostream& out,
                              std::string_view tag,
                              std::string_view operation,
                              std::string_view kind,
                              const std::exception& e) {
    out << tag << " " << operation << " threw " << kind
        << " reason=" << e.what()
        << "\n";
}

} // namespace godark::examples
