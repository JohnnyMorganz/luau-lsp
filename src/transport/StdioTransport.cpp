#include "LSP/Transport/StdioTransport.hpp"

#include <iostream>

void StdioTransport::send(const std::string& data)
{
    std::cout << data;
    std::cout.flush();
}

void StdioTransport::read(char* buffer, unsigned int length)
{
    std::cin.read(buffer, length);
}

bool StdioTransport::readLine(std::string& output)
{
    if (!std::cin)
        return false;
    std::getline(std::cin, output);
    return true;
}