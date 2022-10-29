#pragma once

#include <ostream>

#include "Protocol/Structures.hpp"

namespace lsp
{
std::ostream& operator<<(std::ostream& lhs, const lsp::Position& position);
std::ostream& operator<<(std::ostream& lhs, const lsp::Range& location);
} // namespace lsp