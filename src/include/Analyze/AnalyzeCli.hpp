#pragma once
#include <unordered_map>
#include <string>
#include "argparse/argparse.hpp"

void registerFastFlags(std::unordered_map<std::string, std::string>& fastFlags);
int startAnalyze(const argparse::ArgumentParser& program);
