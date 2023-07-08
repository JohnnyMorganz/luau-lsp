#pragma once
#include <unordered_map>
#include <string>

void registerFastFlags(std::unordered_map<std::string, std::string>& fastFlags);
int startAnalyze(int argc, char** argv);