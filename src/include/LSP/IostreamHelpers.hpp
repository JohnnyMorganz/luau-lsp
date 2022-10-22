#pragma once

#include "LSP/Protocol.hpp"
#include <ostream>

namespace lsp
{
std::ostream& operator<<(std::ostream& lhs, const lsp::Position& position);
std::ostream& operator<<(std::ostream& lhs, const lsp::Range& location);
} // namespace lsp