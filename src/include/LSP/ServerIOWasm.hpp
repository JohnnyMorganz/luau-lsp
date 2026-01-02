#include "LSP/ServerIO.hpp"
#include <stdexcept>
#include <string>
#include <exception>
#include <emscripten/bind.h>
#include <emscripten/console.h>
#include <emscripten/val.h>

using std::runtime_error;
using emscripten::val;

class ServerIOWasm : public ServerIO {
    protected:
    const val output;

    public:
    ServerIOWasm(const val & output_) : output(output_) {}

    const void sendRawMessage(const json& message) const override
    {
        output(message.dump());
    }
    const bool readRawMessage(std::string& output) const override
    {
        emscripten_console_error("ServerIOWeb::readRawMessage should never be called");
        return false;
    }
};