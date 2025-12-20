#pragma once

#include "Protocol/Structures.hpp"

std::string applyEdit(const std::string& source, const std::vector<lsp::TextEdit>& edits);
