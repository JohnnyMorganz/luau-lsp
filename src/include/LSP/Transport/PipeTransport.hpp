#pragma once

#include "Transport.hpp"

class PipeTransport : public Transport {
public:
    PipeTransport(const std::string& socketPath);
    ~PipeTransport() override;

    void send(const std::string& data) override;
    void read(char *buffer, unsigned int length) override;
    bool readLine(std::string& output) override;

private:
    std::string socketPath;
    int socketFd = -1;

    void connect();
};