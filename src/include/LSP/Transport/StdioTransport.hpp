#pragma once

#include "Transport.hpp"

class StdioTransport : public Transport
{
public:
    void send(const std::string &data) override;
    void read(char *buffer, unsigned int length) override;
    bool readLine(std::string &output) override;
};
