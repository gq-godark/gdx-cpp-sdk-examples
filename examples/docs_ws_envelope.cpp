/**
 * Raw WebSocket docs-envelope example using Boost.Beast (plaintext ws:// only).
 *
 * Canonical wss:// endpoints require TLS in production; this sample does not implement TLS.
 * Set GODARK_WS_URL / GDX_WS_URL to the ws:// base your deployment exposes (path /ws/v1 when required).
 *
 * Auth:
 *   Prefer GODARK_API_KEY_ID + GODARK_API_SECRET -> login.args.token "<key_id>:<secret>"
 *   Or GODARK_AUTH_TOKEN / GDX_AUTH_TOKEN when your environment uses a single-token login.
 */

#include <cstdlib>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <nlohmann/json.hpp>

namespace beast = boost::beast;
namespace net = boost::asio;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;
using json = nlohmann::json;

namespace {

const char* env_first(const std::vector<const char*>& names) {
    for (const char* name : names) {
        const char* value = std::getenv(name);
        if (value && value[0] != '\0') return value;
    }
    return nullptr;
}

std::string auth_token() {
    if (const char* token = env_first({"GODARK_AUTH_TOKEN", "GDX_AUTH_TOKEN"})) {
        return token;
    }

    const char* key_id = env_first({
        "GODARK_API_KEY_ID",
        "GDX_API_KEY_ID",
        "GODARK_API_KEY",
        "GDX_API_KEY",
    });
    const char* secret = env_first({"GODARK_API_SECRET", "GDX_API_SECRET"});
    if (!key_id || !secret) {
        throw std::runtime_error(
            "Set GODARK_API_KEY_ID + GODARK_API_SECRET, or set GODARK_AUTH_TOKEN / "
            "GDX_AUTH_TOKEN if your deployment uses single-token login.");
    }
    return std::string(key_id) + ":" + secret;
}

std::string new_id() {
    std::random_device rd;
    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 4; ++i) ss << rd();
    return ss.str();
}

json frame(const std::string& op, json args = json::object()) {
    return json{{"id", new_id()}, {"op", op}, {"args", std::move(args)}};
}

std::string printable(json payload) {
    if (payload.value("op", "") == "login" && payload.contains("args") && payload["args"].is_object()) {
        payload["args"]["token"] = "<redacted>";
    }
    return payload.dump();
}

struct ParsedWsUrl {
    std::string host;
    std::string port;
    std::string target;
};

ParsedWsUrl parse_ws_url(const std::string& url) {
    const std::string prefix = "ws://";
    if (url.rfind(prefix, 0) != 0) {
        throw std::runtime_error(
            "This sample only supports plain ws:// URLs (no TLS). Point GODARK_WS_URL / GDX_WS_URL at a "
            "ws:// host and path your operator provides (often ending in /ws/v1).");
    }
    std::string rest = url.substr(prefix.size());
    std::string host_port;
    std::string target = "/";
    if (const auto slash = rest.find('/'); slash != std::string::npos) {
        host_port = rest.substr(0, slash);
        target = rest.substr(slash);
    } else {
        host_port = rest;
    }
    std::string host = host_port;
    std::string port = "80";
    if (const auto colon = host_port.rfind(':'); colon != std::string::npos) {
        host = host_port.substr(0, colon);
        port = host_port.substr(colon + 1);
    }
    return {host, port, target};
}

json recv_json(websocket::stream<tcp::socket>& ws) {
    beast::flat_buffer buffer;
    ws.read(buffer);
    return json::parse(beast::buffers_to_string(buffer.data()));
}

json recv_and_expect(websocket::stream<tcp::socket>& ws, const std::string& expected_id, const std::string& expected_op);

json send_and_expect(websocket::stream<tcp::socket>& ws, const json& payload, const std::string& expected_op) {
    const std::string wire = payload.dump();
    std::cout << "SEND " << printable(payload) << "\n";
    ws.write(net::buffer(wire));
    return recv_and_expect(ws, payload.value("id", ""), expected_op);
}

json recv_and_expect(websocket::stream<tcp::socket>& ws, const std::string& expected_id, const std::string& expected_op) {
    json msg = recv_json(ws);
    std::cout << "RECV " << msg.dump() << "\n";

    if (msg.value("id", "") != expected_id) {
        throw std::runtime_error("response id did not match request id");
    }
    if (msg.value("op", "") != expected_op) {
        throw std::runtime_error("response op did not match expected op");
    }
    if (msg.value("code", -1) != 0) {
        throw std::runtime_error("response code was not 0: " + msg.value("message", ""));
    }
    return msg;
}

} // namespace

int main() {
    try {
        const char* url_env = env_first({"GODARK_WS_URL", "GDX_WS_URL"});
        // Placeholder default when unset; always override for real deployments.
        const std::string ws_url = url_env ? url_env : "ws://127.0.0.1:4000/ws/v1";
        const std::string token = auth_token();

        const auto parsed = parse_ws_url(ws_url);
        net::io_context ioc;
        tcp::resolver resolver{ioc};
        websocket::stream<tcp::socket> ws{ioc};
        auto const results = resolver.resolve(parsed.host, parsed.port);
        auto ep = net::connect(ws.next_layer(), results);
        ws.handshake(parsed.host + ":" + std::to_string(ep.port()), parsed.target);

        std::cout << "Connected to " << ws_url << "\n";

        send_and_expect(ws, frame("login", json{{"token", token}}), "login");
        send_and_expect(ws, frame("ping"), "pong");

        const json sub = frame("subscribe", json::array({json{{"channel", "orders"}}, json{{"channel", "positions"}}}));
        send_and_expect(ws, sub, "subscribe");
        recv_and_expect(ws, sub.value("id", ""), "subscribe");

        send_and_expect(ws, frame("logout"), "logout");

        beast::error_code ec;
        ws.close(websocket::close_code::normal, ec);
        if (ec && ec != net::error::eof && ec != websocket::error::closed) {
            throw beast::system_error{ec};
        }
        std::cout << "OK: docs-envelope login, ping, subscribe, and logout succeeded\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "docs_ws_envelope failed: " << e.what() << "\n";
        return 1;
    }
}
