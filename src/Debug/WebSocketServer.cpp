#ifndef _WIN32

#include "Debug/WebSocketServer.hpp"

#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <stdexcept>
#include <iostream>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

// ── SHA-1 (RFC 3174) ─────────────────────────────────────────────────────────
// Used for the WebSocket handshake accept key computation.

static inline uint32_t rotl32(uint32_t v, int n)
{
    return (v << n) | (v >> (32 - n));
}

void WebSocketServer::sha1(const uint8_t* data, size_t len, uint8_t out[20])
{
    uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};

    // Pre-process: pad message
    size_t totalLen = len + 1 + 8;
    size_t paddedLen = ((totalLen + 63) / 64) * 64;
    std::vector<uint8_t> msg(paddedLen, 0);
    std::memcpy(msg.data(), data, len);
    msg[len] = 0x80;
    uint64_t bitLen = (uint64_t)len * 8;
    for (int i = 0; i < 8; ++i)
        msg[paddedLen - 8 + i] = (uint8_t)(bitLen >> (56 - i * 8));

    for (size_t block = 0; block < paddedLen / 64; ++block)
    {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i)
        {
            const uint8_t* p = msg.data() + block * 64 + i * 4;
            w[i] = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
        }
        for (int i = 16; i < 80; ++i)
            w[i] = rotl32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; ++i)
        {
            uint32_t f, k;
            if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999u; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1u; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
            else { f = b ^ c ^ d; k = 0xCA62C1D6u; }

            uint32_t tmp = rotl32(a, 5) + f + e + k + w[i];
            e = d; d = c; c = rotl32(b, 30); b = a; a = tmp;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }

    for (int i = 0; i < 5; ++i)
        for (int j = 0; j < 4; ++j)
            out[i * 4 + j] = (uint8_t)(h[i] >> (24 - j * 8));
}

// ── Base64 encode ─────────────────────────────────────────────────────────────

std::string WebSocketServer::base64Encode(const uint8_t* data, size_t len)
{
    static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);

    for (size_t i = 0; i < len; i += 3)
    {
        uint32_t n = (uint32_t)data[i] << 16;
        if (i + 1 < len) n |= (uint32_t)data[i + 1] << 8;
        if (i + 2 < len) n |= data[i + 2];

        out += table[(n >> 18) & 0x3F];
        out += table[(n >> 12) & 0x3F];
        out += (i + 1 < len) ? table[(n >> 6) & 0x3F] : '=';
        out += (i + 2 < len) ? table[n & 0x3F] : '=';
    }
    return out;
}

// ── WS handshake key ──────────────────────────────────────────────────────────

std::string WebSocketServer::computeAcceptKey(const std::string& clientKey)
{
    static const char* magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    std::string combined = clientKey + magic;
    uint8_t digest[20];
    sha1(reinterpret_cast<const uint8_t*>(combined.data()), combined.size(), digest);
    return base64Encode(digest, 20);
}

// ── WebSocketServer ───────────────────────────────────────────────────────────

WebSocketServer::WebSocketServer(int aPort)
    : port(aPort)
{
}

WebSocketServer::~WebSocketServer()
{
    close();
}

bool WebSocketServer::listen()
{
    serverFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0)
    {
        std::cerr << "DAP WebSocket: socket() failed\n";
        return false;
    }

    int opt = 1;
    ::setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);

    if (::bind(serverFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        std::cerr << "DAP WebSocket: bind() failed on port " << port << "\n";
        ::close(serverFd);
        serverFd = -1;
        return false;
    }

    if (::listen(serverFd, 1) < 0)
    {
        std::cerr << "DAP WebSocket: listen() failed\n";
        ::close(serverFd);
        serverFd = -1;
        return false;
    }

    // Block until one client connects
    clientFd = ::accept(serverFd, nullptr, nullptr);
    if (clientFd < 0)
    {
        std::cerr << "DAP WebSocket: accept() failed\n";
        return false;
    }

    return performHandshake();
}

bool WebSocketServer::performHandshake()
{
    // Read the HTTP upgrade request
    std::string request;
    char buf[1];
    while (true)
    {
        ssize_t n = ::recv(clientFd, buf, 1, 0);
        if (n <= 0)
            return false;
        request += buf[0];
        if (request.size() >= 4 && request.substr(request.size() - 4) == "\r\n\r\n")
            break;
    }

    // Extract Sec-WebSocket-Key
    std::string key;
    const std::string keyHeader = "Sec-WebSocket-Key: ";
    auto pos = request.find(keyHeader);
    if (pos == std::string::npos)
        return false;
    pos += keyHeader.size();
    auto end = request.find("\r\n", pos);
    if (end == std::string::npos)
        return false;
    key = request.substr(pos, end - pos);

    // Send HTTP 101 Switching Protocols
    std::string acceptKey = computeAcceptKey(key);
    std::string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + acceptKey + "\r\n\r\n";

    ssize_t sent = ::send(clientFd, response.data(), response.size(), 0);
    return sent == (ssize_t)response.size();
}

bool WebSocketServer::receive(std::string& message)
{
    while (true)
    {
        std::string payload;
        uint8_t opcode = 0;
        if (!readClientFrame(payload, opcode))
            return false;

        if (opcode == 0x08) // Close frame
        {
            closeClient();
            return false;
        }
        if (opcode == 0x09) // Ping — send pong
        {
            sendFrame(payload, 0x0A);
            continue;
        }
        if (opcode == 0x01 || opcode == 0x02) // Text or binary
        {
            message = std::move(payload);
            return true;
        }
        // Unknown frame: ignore
    }
}

bool WebSocketServer::readClientFrame(std::string& payload, uint8_t& opcode)
{
    auto readByte = [this](uint8_t& b) -> bool {
        ssize_t n = ::recv(clientFd, &b, 1, MSG_WAITALL);
        return n == 1;
    };

    uint8_t b0, b1;
    if (!readByte(b0) || !readByte(b1))
        return false;

    opcode = b0 & 0x0F;
    bool masked = (b1 & 0x80) != 0;
    uint64_t payloadLen = b1 & 0x7F;

    if (payloadLen == 126)
    {
        uint8_t ext[2];
        if (::recv(clientFd, ext, 2, MSG_WAITALL) != 2)
            return false;
        payloadLen = ((uint64_t)ext[0] << 8) | ext[1];
    }
    else if (payloadLen == 127)
    {
        uint8_t ext[8];
        if (::recv(clientFd, ext, 8, MSG_WAITALL) != 8)
            return false;
        payloadLen = 0;
        for (int i = 0; i < 8; ++i)
            payloadLen = (payloadLen << 8) | ext[i];
    }

    uint8_t mask[4] = {};
    if (masked)
    {
        if (::recv(clientFd, mask, 4, MSG_WAITALL) != 4)
            return false;
    }

    payload.resize(payloadLen);
    if (payloadLen > 0)
    {
        ssize_t n = ::recv(clientFd, &payload[0], (size_t)payloadLen, MSG_WAITALL);
        if (n != (ssize_t)payloadLen)
            return false;
    }

    if (masked)
    {
        for (size_t i = 0; i < payload.size(); ++i)
            payload[i] ^= mask[i % 4];
    }

    return true;
}

void WebSocketServer::send(const std::string& message)
{
    sendFrame(message, 0x01);
}

void WebSocketServer::sendFrame(const std::string& payload, uint8_t opcode)
{
    if (clientFd < 0)
        return;

    std::vector<uint8_t> frame;
    frame.push_back(0x80 | (opcode & 0x0F)); // FIN + opcode

    size_t len = payload.size();
    if (len < 126)
    {
        frame.push_back((uint8_t)len);
    }
    else if (len < 65536)
    {
        frame.push_back(126);
        frame.push_back((uint8_t)(len >> 8));
        frame.push_back((uint8_t)(len & 0xFF));
    }
    else
    {
        frame.push_back(127);
        for (int i = 7; i >= 0; --i)
            frame.push_back((uint8_t)(len >> (i * 8)));
    }

    frame.insert(frame.end(), payload.begin(), payload.end());
    ::send(clientFd, frame.data(), frame.size(), 0);
}

void WebSocketServer::closeClient()
{
    if (clientFd >= 0)
    {
        // Send close frame
        uint8_t closeFrame[2] = {0x88, 0x00};
        ::send(clientFd, closeFrame, 2, 0);
        ::close(clientFd);
        clientFd = -1;
    }
}

void WebSocketServer::close()
{
    closeClient();
    if (serverFd >= 0)
    {
        ::close(serverFd);
        serverFd = -1;
    }
}

bool WebSocketServer::isConnected() const
{
    return clientFd >= 0;
}

#endif // _WIN32
