#pragma once

#include <string>
#include <functional>
#include <cstdint>

#ifndef _WIN32

// Minimal WebSocket server using POSIX sockets.
// Accepts a single client connection, performs the HTTP upgrade handshake,
// then exposes send/receive of text frames.
class WebSocketServer
{
public:
    explicit WebSocketServer(int port);
    ~WebSocketServer();

    // Block until a client connects and the WS handshake completes.
    // Returns false if the server socket could not be created/bound.
    bool listen();

    // Read one complete WebSocket text frame. Blocks until a frame arrives.
    // Returns false on connection close or error.
    bool receive(std::string& message);

    // Send a WebSocket text frame.
    void send(const std::string& message);

    // Close the client connection (does not destroy the server socket).
    void closeClient();

    // Close everything.
    void close();

    bool isConnected() const;

private:
    int port;
    int serverFd = -1;
    int clientFd = -1;

    bool performHandshake();
    bool readClientFrame(std::string& payload, uint8_t& opcode);
    void sendFrame(const std::string& payload, uint8_t opcode = 0x01 /* text */);

    static std::string computeAcceptKey(const std::string& clientKey);
    static std::string base64Encode(const uint8_t* data, size_t len);
    static void sha1(const uint8_t* data, size_t len, uint8_t out[20]);
};

#endif // _WIN32
