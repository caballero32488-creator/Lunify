#pragma once

#include "lunify/options.hpp"

#include <string>

namespace lunify {

struct MinifyResult {
    std::string output;
    bool ok = true;
    std::string error;
    std::size_t originalSize = 0;
    std::size_t minifiedSize = 0;
};

MinifyResult minify(const std::string& source, const Options& opts);

}