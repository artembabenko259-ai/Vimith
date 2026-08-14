#include "vimith/mcp/mcp_server.hpp"
#include "vimith/disasm/disassembler.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <map>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using nlohmann::json;

namespace vimith::mcp {

namespace {

// ── HTTP (minimal: POST /mcp only, one request per connection) ─────────────

struct HttpRequest {
    std::string                       method;
    std::string                       path;
    std::map<std::string, std::string> headers; // lower-cased keys
    std::string                       body;
};

std::optional<HttpRequest> readRequest(SOCKET sock) {
    std::string raw;
    char        buf[4096];
    std::size_t headerEnd = std::string::npos;

    while (headerEnd == std::string::npos) {
        const int n = recv(sock, buf, sizeof(buf), 0);
        if (n <= 0) return std::nullopt;
        raw.append(buf, static_cast<std::size_t>(n));
        headerEnd = raw.find("\r\n\r\n");
        if (raw.size() > 1'000'000) return std::nullopt; // runaway header guard
    }

    const std::string headerBlock = raw.substr(0, headerEnd);
    std::string        bodySoFar   = raw.substr(headerEnd + 4);

    std::istringstream hs(headerBlock);
    std::string        requestLine;
    std::getline(hs, requestLine);
    if (!requestLine.empty() && requestLine.back() == '\r') requestLine.pop_back();

    HttpRequest req;
    {
        std::istringstream rl(requestLine);
        std::string        httpVersion;
        rl >> req.method >> req.path >> httpVersion;
    }

    std::string line;
    while (std::getline(hs, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        const auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon);
        std::string val = line.substr(colon + 1);
        while (!val.empty() && val.front() == ' ') val.erase(val.begin());
        for (auto& ch : key) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        req.headers[key] = val;
    }

    std::size_t contentLength = 0;
    if (auto it = req.headers.find("content-length"); it != req.headers.end()) {
        try { contentLength = static_cast<std::size_t>(std::stoul(it->second)); } catch (...) {}
    }

    while (bodySoFar.size() < contentLength) {
        const int n = recv(sock, buf, sizeof(buf), 0);
        if (n <= 0) break;
        bodySoFar.append(buf, static_cast<std::size_t>(n));
    }
    req.body = bodySoFar.substr(0, std::min(bodySoFar.size(), contentLength));

    return req;
}

void sendResponse(SOCKET sock, int status, const std::string& body,
                   const std::vector<std::pair<std::string, std::string>>& extraHeaders = {}) {
    const char* statusText =
        status == 200 ? "OK"
      : status == 202 ? "Accepted"
      : status == 400 ? "Bad Request"
      : status == 403 ? "Forbidden"
      : status == 404 ? "Not Found"
      : status == 405 ? "Method Not Allowed"
      : "Error";

    std::ostringstream resp;
    resp << "HTTP/1.1 " << status << " " << statusText << "\r\n"
         << "Content-Type: application/json\r\n"
         << "Content-Length: " << body.size() << "\r\n"
         << "Connection: close\r\n";
    for (const auto& [k, v] : extraHeaders) resp << k << ": " << v << "\r\n";
    resp << "\r\n" << body;

    const std::string out = resp.str();
    std::size_t       sent = 0;
    while (sent < out.size()) {
        const int n = send(sock, out.data() + sent, static_cast<int>(out.size() - sent), 0);
        if (n <= 0) break;
        sent += static_cast<std::size_t>(n);
    }
}

// MCP spec recommends validating Origin on local servers to block
// browser-based DNS-rebinding attacks. Non-browser MCP clients (Claude
// Desktop, Claude Code, CLI agents) typically send no Origin at all, so
// only requests that DO send one and point somewhere non-local are rejected.
bool isLocalOrigin(const std::string& origin) {
    return origin.find("://127.0.0.1") != std::string::npos ||
           origin.find("://localhost") != std::string::npos ||
           origin.find("://[::1]")     != std::string::npos;
}

// ── Byte <-> hex helpers ─────────────────────────────────────────────────

std::vector<std::uint8_t> parseHexBytes(const std::string& hex) {
    std::string digits;
    digits.reserve(hex.size());
    for (char c : hex) {
        if (std::isxdigit(static_cast<unsigned char>(c))) digits += c;
    }
    std::vector<std::uint8_t> out;
    out.reserve(digits.size() / 2);
    for (std::size_t i = 0; i + 1 < digits.size(); i += 2) {
        out.push_back(static_cast<std::uint8_t>(std::stoul(digits.substr(i, 2), nullptr, 16)));
    }
    return out;
}

std::string bytesToHexStr(std::span<const std::uint8_t> bytes) {
    static constexpr char kDigits[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(bytes.size() * 3);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i) out += ' ';
        out += kDigits[bytes[i] >> 4];
        out += kDigits[bytes[i] & 0xF];
    }
    return out;
}

std::string toHexAddr(std::size_t v) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase << v;
    return oss.str();
}

std::string randomHex(std::size_t nBytes) {
    static std::mt19937_64            rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, 255);
    static constexpr char              kDigits[] = "0123456789abcdef";
    std::string                        out;
    out.reserve(nBytes * 2);
    for (std::size_t i = 0; i < nBytes; ++i) {
        const int v = dist(rng);
        out += kDigits[v >> 4];
        out += kDigits[v & 0xF];
    }
    return out;
}

// ── MCP tool schema builders (explicit json::object() — no brace-init magic) ─

json makeIntProp(int minimum, std::optional<int> maximum = std::nullopt) {
    json p       = json::object();
    p["type"]    = "integer";
    p["minimum"] = minimum;
    if (maximum) p["maximum"] = *maximum;
    return p;
}

json makeStringProp(const std::string& description = {}) {
    json p    = json::object();
    p["type"] = "string";
    if (!description.empty()) p["description"] = description;
    return p;
}

json makeTool(const std::string& name, const std::string& description,
              std::vector<std::pair<std::string, json>> properties,
              std::vector<std::string>                  required) {
    json props = json::object();
    for (auto& [k, v] : properties) props[k] = v;

    json schema         = json::object();
    schema["type"]       = "object";
    schema["properties"] = props;
    if (!required.empty()) schema["required"] = required;

    json tool          = json::object();
    tool["name"]        = name;
    tool["description"] = description;
    tool["inputSchema"] = schema;
    return tool;
}

json toolsListJson() {
    json tools = json::array();

    tools.push_back(makeTool("get_status",
        "Get the currently open file's path, size, live cursor offset, and dirty flag.",
        {}, {}));

    tools.push_back(makeTool("read_bytes",
        "Read raw bytes from the open file at a given offset (returns hex + ASCII).",
        {{"offset", makeIntProp(0)}, {"length", makeIntProp(1, 4096)}},
        {"offset", "length"}));

    tools.push_back(makeTool("disassemble",
        "Decode x86-64 instructions starting at a byte offset.",
        {{"offset", makeIntProp(0)}, {"count", makeIntProp(1, 200)}},
        {"offset"}));

    tools.push_back(makeTool("write_patch",
        "Overwrite bytes at an offset with the given hex bytes, writing straight "
        "through to the mapped file. No confirmation, no undo \xe2\x80\x94 the change "
        "is immediate and permanent the moment this tool returns.",
        {{"offset", makeIntProp(0)},
         {"bytes_hex", makeStringProp(R"(Hex bytes, spaces optional, e.g. "90 90 C3")")}},
        {"offset", "bytes_hex"}));

    tools.push_back(makeTool("search_bytes",
        "Find occurrences of a byte pattern (hex) in the open file.",
        {{"pattern_hex", makeStringProp()},
         {"start_offset", makeIntProp(0)},
         {"max_matches", makeIntProp(1, 200)}},
        {"pattern_hex"}));

    tools.push_back(makeTool("set_cursor",
        "Move the visible hex/disassembly cursor to a byte offset (visual feedback in the editor).",
        {{"offset", makeIntProp(0)}},
        {"offset"}));

    return tools;
}

} // namespace

// ── McpServer ────────────────────────────────────────────────────────────

McpServer::McpServer(EditorState& state, core::BufferManager& buf, std::mutex& stateMutex)
    : m_state(state), m_buf(buf), m_mutex(stateMutex)
{}

McpServer::~McpServer() { stop(); }

void McpServer::setRedrawCallback(std::function<void()> cb) { m_redraw = std::move(cb); }

bool McpServer::start(std::uint16_t port) {
    if (m_running.load()) return true;

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;

    const SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) { WSACleanup(); return false; }

    BOOL reuse = TRUE;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr); // loopback only — never 0.0.0.0

    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        closesocket(sock); WSACleanup(); return false;
    }
    if (listen(sock, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(sock); WSACleanup(); return false;
    }

    m_listenSocket = reinterpret_cast<void*>(static_cast<std::uintptr_t>(sock));
    m_port         = port;
    m_running      = true;
    m_thread       = std::thread([this] { acceptLoop(); });
    return true;
}

void McpServer::stop() {
    if (!m_running.exchange(false)) return;
    if (m_listenSocket) {
        closesocket(static_cast<SOCKET>(reinterpret_cast<std::uintptr_t>(m_listenSocket)));
        m_listenSocket = nullptr;
    }
    if (m_thread.joinable()) m_thread.join();
    WSACleanup();
}

void McpServer::acceptLoop() {
    const SOCKET listenSock = static_cast<SOCKET>(reinterpret_cast<std::uintptr_t>(m_listenSocket));

    while (m_running.load()) {
        const SOCKET client = accept(listenSock, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            if (!m_running.load()) break;
            continue;
        }

        auto reqOpt = readRequest(client);
        if (!reqOpt) { closesocket(client); continue; }
        const HttpRequest& req = *reqOpt;

        if (auto it = req.headers.find("origin");
            it != req.headers.end() && !isLocalOrigin(it->second)) {
            sendResponse(client, 403, R"({"error":"origin not allowed"})");
            closesocket(client);
            continue;
        }

        if (req.method != "POST" || req.path != "/mcp") {
            const int status = (req.method == "GET" && req.path == "/mcp") ? 405 : 404;
            sendResponse(client, status, R"({"error":"Vimith MCP server: POST JSON-RPC 2.0 to /mcp"})");
            closesocket(client);
            continue;
        }

        json rpc;
        try {
            rpc = json::parse(req.body);
        } catch (...) {
            sendResponse(client, 400,
                R"({"jsonrpc":"2.0","id":null,"error":{"code":-32700,"message":"Parse error"}})");
            closesocket(client);
            continue;
        }

        const bool isNotification = rpc.is_object() && !rpc.contains("id");

        auto makeError = [](int code, const std::string& msg) {
            json err       = json::object();
            err["code"]    = code;
            err["message"] = msg;
            json out       = json::object();
            out["jsonrpc"] = "2.0";
            out["id"]      = nullptr;
            out["error"]   = err;
            return out;
        };

        json response;
        try {
            response = rpc.is_object()
                ? handleRpcInternal(rpc)
                : makeError(-32600, "Invalid Request: expected a JSON object");
        } catch (const std::exception& ex) {
            // Never let a malformed/unexpected request take the whole editor
            // down — this thread has no caller to propagate an exception to.
            response = makeError(-32603, std::string{"Internal error: "} + ex.what());
        }

        if (isNotification) {
            sendResponse(client, 202, "");
        } else {
            static const std::string sessionId = randomHex(16);
            sendResponse(client, 200, response.dump(), {{"Mcp-Session-Id", sessionId}});
        }

        closesocket(client);
    }
}

json McpServer::handleRpcInternal(const json& req) {
    const json        id     = req.value("id", json(nullptr));
    const std::string method = req.value("method", std::string{});

    auto errorResponse = [&](int code, const std::string& msg) {
        json err          = json::object();
        err["code"]       = code;
        err["message"]    = msg;
        json out          = json::object();
        out["jsonrpc"]    = "2.0";
        out["id"]         = id;
        out["error"]      = err;
        return out;
    };
    auto okResponse = [&](json result) {
        json out       = json::object();
        out["jsonrpc"] = "2.0";
        out["id"]      = id;
        out["result"]  = std::move(result);
        return out;
    };

    if (method == "initialize") {
        json result           = json::object();
        result["protocolVersion"] = "2025-06-18";
        json caps              = json::object();
        caps["tools"]           = json::object();
        result["capabilities"]  = caps;
        json info               = json::object();
        info["name"]            = "vimith";
        info["version"]         = "0.1.0";
        result["serverInfo"]    = info;
        result["instructions"]  =
            "Vimith is a live hex editor/disassembler. These tools read and patch "
            "the exact file the user has open on screen. write_patch applies "
            "immediately with no confirmation and no undo.";
        return okResponse(result);
    }

    if (method == "notifications/initialized" || method == "ping") {
        return okResponse(json::object());
    }

    if (method == "tools/list") {
        json result     = json::object();
        result["tools"] = toolsListJson();
        return okResponse(result);
    }

    if (method == "tools/call") {
        const json params   = req.value("params", json::object());
        const std::string toolName = params.value("name", std::string{});
        const json args     = params.value("arguments", json::object());
        return okResponse(callTool(toolName, args));
    }

    return errorResponse(-32601, "Method not found: " + method);
}

json McpServer::callTool(const std::string& name, const json& args) {
    auto textResult = [](std::string text, bool isError = false) {
        json content     = json::array();
        json block        = json::object();
        block["type"]      = "text";
        block["text"]       = std::move(text);
        content.push_back(block);
        json out          = json::object();
        out["content"]     = content;
        out["isError"]     = isError;
        return out;
    };

    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_buf.fileSize() == 0 && name != "get_status") {
        return textResult("No file is open in Vimith.", true);
    }

    if (name != "get_status" && m_buf.getMode() != core::BufferMode::Hex) {
        m_buf.switchMode(core::BufferMode::Hex);
    }

    const std::size_t fileSize = m_buf.fileSize();

    if (name == "get_status") {
        json j = json::object();
        j["file"]          = m_state.filename;
        j["size"]           = fileSize;
        j["cursor_offset"]  = m_state.hexOffset;
        j["dirty"]          = m_buf.isDirty();
        j["mode"]           = m_buf.getMode() == core::BufferMode::Hex ? "hex" : "text";
        return textResult(j.dump());
    }

    if (name == "read_bytes") {
        const std::size_t offset = args.value("offset", std::size_t{0});
        const std::size_t length = std::min<std::size_t>(args.value("length", std::size_t{16}), 4096);
        if (offset >= fileSize) {
            return textResult("Offset is past end of file (size=" + std::to_string(fileSize) + ").", true);
        }
        const auto  bytes = m_buf.readBytes(offset, length);
        std::string ascii;
        ascii.reserve(bytes.size());
        for (auto b : bytes) ascii += (b >= 0x20 && b < 0x7F) ? static_cast<char>(b) : '.';

        json j = json::object();
        j["offset"] = offset;
        j["hex"]    = bytesToHexStr(bytes);
        j["ascii"]  = ascii;
        return textResult(j.dump());
    }

    if (name == "disassemble") {
        const std::size_t offset = args.value("offset", std::size_t{0});
        const std::size_t count  = std::min<std::size_t>(args.value("count", std::size_t{16}), 200);
        if (offset >= fileSize) {
            return textResult("Offset is past end of file (size=" + std::to_string(fileSize) + ").", true);
        }
        constexpr std::size_t kMaxInsnLen = 15;
        const std::size_t     window      = std::min(fileSize - offset, count * kMaxInsnLen);
        const auto             bytes       = m_buf.readBytes(offset, window);

        static const disasm::Disassembler decoder;
        const auto insns = decoder.decode(bytes, offset, count);

        json arr = json::array();
        for (const auto& insn : insns) {
            json j = json::object();
            j["offset"] = insn.offset;
            j["bytes"]  = insn.bytesHex;
            j["text"]   = insn.text;
            j["valid"]  = insn.valid;
            arr.push_back(j);
        }
        json result = json::object();
        result["instructions"] = arr;
        return textResult(result.dump());
    }

    if (name == "write_patch") {
        const std::size_t offset = args.value("offset", std::size_t{0});
        const std::string hexStr = args.value("bytes_hex", std::string{});
        const auto         newBytes = parseHexBytes(hexStr);
        if (newBytes.empty()) {
            return textResult("bytes_hex did not contain any valid hex byte pairs.", true);
        }
        if (offset + newBytes.size() > fileSize) {
            return textResult("Patch would write past end of file (offset+len=" +
                std::to_string(offset + newBytes.size()) + ", size=" +
                std::to_string(fileSize) + ").", true);
        }

        const std::string oldHex = bytesToHexStr(m_buf.readBytes(offset, newBytes.size()));

        if (!m_buf.writeBytes(offset, newBytes)) {
            return textResult("Write failed (file may not be writable).", true);
        }

        m_state.hexOffset     = fileSize > 0 ? std::min(offset, fileSize - 1) : 0;
        m_state.statusMessage = "[MCP] patched " + std::to_string(newBytes.size()) +
                                 " byte(s) @ 0x" + toHexAddr(offset);
        if (m_redraw) m_redraw();

        json j = json::object();
        j["offset"]  = offset;
        j["old_hex"] = oldHex;
        j["new_hex"] = bytesToHexStr(newBytes);
        return textResult(j.dump());
    }

    if (name == "search_bytes") {
        const std::string patHex = args.value("pattern_hex", std::string{});
        const auto         pattern = parseHexBytes(patHex);
        if (pattern.empty()) {
            return textResult("pattern_hex did not contain any valid hex byte pairs.", true);
        }
        const std::size_t start      = args.value("start_offset", std::size_t{0});
        const std::size_t maxMatches = std::min<std::size_t>(args.value("max_matches", std::size_t{50}), 200);

        // Cap total scan so one call stays bounded on huge files.
        constexpr std::size_t kMaxScan = 64ull * 1024 * 1024;
        const std::size_t     scanEnd  = start < fileSize ? std::min(fileSize, start + kMaxScan) : start;
        const auto             hay      = m_buf.readBytes(start, scanEnd - start);

        json matches = json::array();
        for (std::size_t i = 0;
             matches.size() < maxMatches && i + pattern.size() <= hay.size();
             ++i) {
            if (std::equal(pattern.begin(), pattern.end(), hay.begin() + static_cast<std::ptrdiff_t>(i))) {
                matches.push_back(start + i);
            }
        }

        json j = json::object();
        j["matches"]       = matches;
        j["scanned_bytes"] = hay.size();
        j["truncated"]     = scanEnd < fileSize;
        return textResult(j.dump());
    }

    if (name == "set_cursor") {
        const std::size_t offset = args.value("offset", std::size_t{0});
        if (fileSize == 0) return textResult("No file open.", true);
        m_state.hexOffset = std::min(offset, fileSize - 1);
        if (m_redraw) m_redraw();
        json j = json::object();
        j["cursor_offset"] = m_state.hexOffset;
        return textResult(j.dump());
    }

    return textResult("Unknown tool: " + name, true);
}

} // namespace vimith::mcp
