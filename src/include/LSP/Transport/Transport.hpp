#pragma once

#include <string>

class Transport {
public:
    virtual ~Transport() {}

    virtual void send(const std::string& data) = 0;
    virtual void read(char* buffer, unsigned int length) = 0;
    virtual bool readLine(std::string& output) = 0;
};