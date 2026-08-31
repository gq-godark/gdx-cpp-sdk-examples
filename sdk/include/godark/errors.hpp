#pragma once

#include <optional>
#include <stdexcept>
#include <string>

namespace godark {

class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class AuthenticationError : public Error {
public:
    using Error::Error;
};

class SessionError : public Error {
public:
    using Error::Error;
};

class OrderError : public Error {
public:
    std::optional<std::string> error_code;

    explicit OrderError(const std::string& message,
                        std::optional<std::string> code = std::nullopt)
        : Error(message), error_code(std::move(code)) {}
};

class ConnectionError : public Error {
public:
    using Error::Error;
};

class EncryptionError : public Error {
public:
    using Error::Error;
};

class TimeoutError : public Error {
public:
    using Error::Error;
};

} // namespace godark
