#include "LSP/IostreamHelpers.hpp"

namespace lsp
{
std::ostream& operator<<(std::ostream& stream, const lsp::Position& position)
{
    return stream << "{ line = " << position.line << ", col = " << position.character << " }";
}

std::ostream& operator<<(std::ostream& stream, const lsp::Range& range)
{
    return stream << "Location { " << range.start << ", " << range.end << " }";
}
} // namespace lsp
