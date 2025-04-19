// TODO: PipeTransport is not supported on Windows, requires implementing named pipes support
#ifndef _WIN32

#include "LSP/Transport/PipeTransport.hpp"

#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <iostream>

PipeTransport::PipeTransport(const std::string& socketPath) : socketPath(socketPath), socketFd(-1)
{
    socketFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socketFd == -1)
        throw std::runtime_error("Failed to create socket");
    connect();
}

PipeTransport::~PipeTransport()
{
    if (socketFd != -1)
        close(socketFd);
}

void PipeTransport::connect()
{
    struct sockaddr_un serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));

    serverAddress.sun_family = AF_UNIX;
    strncpy(serverAddress.sun_path, socketPath.c_str(), sizeof(serverAddress.sun_path) - 1);
    serverAddress.sun_path[sizeof(serverAddress.sun_path) - 1] = '\0';

    if (::connect(socketFd, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) {
        close(socketFd);
        throw new std::runtime_error("Failed to connect to socket at " + socketPath);
    }
}

void PipeTransport::send(const std::string& data)
{
    ssize_t bytesSent = ::send(socketFd, data.c_str(), data.size(), 0);
    if (bytesSent == -1) {
        throw std::runtime_error("Failed to send data");
    }
}

void PipeTransport::read(char* buffer, unsigned int length)
{
    size_t totalBytesRead = 0;
    while (totalBytesRead < length)
    {
        ssize_t bytesRead = ::recv(socketFd, buffer + totalBytesRead, length - totalBytesRead, 0);
        if (bytesRead == -1) {
            throw std::runtime_error("Failed to read from socket");
        } else if (bytesRead == 0) {
            throw std::runtime_error("Server closed the connection");
        }
        totalBytesRead += bytesRead;
    }
}

bool PipeTransport::readLine(std::string& output)
{
    if (socketFd == -1)
        return false;

    output.clear();
    char buffer[1];

    while (true) {
        ssize_t bytesRead = ::recv(socketFd, buffer, 1, 0);
        if (bytesRead == -1) {
            if (errno == EINTR)
                continue;
            throw std::runtime_error("Failed to read from socket: " + std::string(strerror(errno)));
        } else if (bytesRead == 0) {
            throw std::runtime_error("Server closed the connection");
        }

        if (buffer[0] == '\n')
            break;
        output.push_back(buffer[0]);
    }
    return true;
}

#endif
