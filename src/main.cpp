#include <iostream>
#include <string>
#include <optional>
#include "Luau/StringUtils.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

/** Reads stdin for a JSON-RPC message into output */
bool readRawMessage(std::string& output)
{
    unsigned int contentLength = 0;
    std::string line;

    // Read the headers
    while (true)
    {
        if (!std::cin)
            return false;
        std::getline(std::cin, line);

        if (Luau::startsWith(line, "Content-Length: "))
        {
            if (contentLength != 0)
            {
                std::cerr << "Duplicate content-length header found. Discarding old value";
            }
            std::string len = line.substr(16);
            contentLength = std::stoi(len);
            continue;
        }

        // Trim line and check if its empty (i.e., we have ended the header block)
        line.erase(line.find_last_not_of(" \n\r\t") + 1);
        if (line.empty())
            break;
    }

    // Check if no Content-Length found
    if (contentLength == 0)
    {
        std::cerr << "Failed to read content length\n";
        return false;
    }

    // TODO: check if contentlength is too large?

    // Read the JSON message into output
    output.resize(contentLength);
    std::cin.read(&output[0], contentLength);
    return true;
}

/** Sends a raw JSON-RPC message to stdout */
void sendRawMessage(const json& message)
{
    std::string s = message.dump();
    std::cout << "Content-Length: " << s.length() << "\r\n";
    std::cout << "\r\n";
    std::cout << s << "\r\n";
}

// void sendRequest(int id, /* TODO: can be a string too? */ const std::string& method, std::optional<const json&> params) {}
// void sendNotification(const std::string& method, std::optional<const json&> params) {}

// void onRequest(int id, /* TODO: can be a string too? maybe just take a JSON? */ const std::string& method /*, optional json value params*/) {}
// // void onResponse(); // id = integer/string/null, result?: string | number | boolean | object | null, error?: ResponseError
// void onNotification(const std::string& method, std::optional<const json&> params) {}

int main()
{
    // Begin input loop
    std::string jsonString;
    while (std::cin)
    {
        if (readRawMessage(jsonString))
        {
            // Parse the input
            // TODO: handle invalid json
            auto j = json::parse(jsonString);

            if (!j.contains("jsonrpc"))
            {
                std::cerr << "Not a JSON-RPC v2 message";
                continue;
            }

            // TODO: dispatch to relevant handler
        }
    }
    return 0;
}