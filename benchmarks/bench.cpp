#include <chrono>
#include <cstdio>
#include <string>

#include "lunify/minifier.hpp"
#include "lunify/options.hpp"

int main() {
    std::string source;
    for (int i = 0; i < 2000; ++i) {
        source += "local var" + std::to_string(i) + " = " + std::to_string(i) + " + 1\n";
        source += "print(var" + std::to_string(i) + ")\n";
    }

    lunify::Options opts;
    opts.level = 2;
    lunify::applyLevel(opts);

    auto t0 = std::chrono::steady_clock::now();
    lunify::MinifyResult r = lunify::minify(source, opts);
    auto t1 = std::chrono::steady_clock::now();

    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::printf("input:  %zu bytes\n", source.size());
    std::printf("output: %zu bytes\n", r.output.size());
    std::printf("ratio:  %.1f%%\n", 100.0 * (1.0 - (double)r.output.size() / (double)source.size()));
    std::printf("time:   %.2f ms\n", ms);
    return r.ok ? 0 : 1;
}
