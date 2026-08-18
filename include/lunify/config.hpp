#pragma once

#include "lunify/json.hpp"
#include "lunify/options.hpp"

#include <string>

namespace lunify {

Options loadConfigText(const std::string& text, std::string* error);
Options loadConfigFile(const std::string& path, std::string* error);
void applyConfig(Options& opts, const json::Value& root);

}