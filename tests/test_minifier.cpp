#include <cstdio>
#include <string>

#include "lunify/minifier.hpp"
#include "lunify/options.hpp"

static int failures = 0;

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
            ++failures;                                                   \
        }                                                                 \
    } while (0)

#define CHECK_EQ(a, b)                                                    \
    do {                                                                  \
        if ((a) != (b)) {                                                 \
            std::printf("FAIL %s:%d: got \"%s\", want \"%s\"\n",          \
                        __FILE__, __LINE__, (a).c_str(), (b).c_str());    \
            ++failures;                                                   \
        }                                                                 \
    } while (0)

int main() {
    lunify::Options opts;
    opts.level = 2;
    lunify::applyLevel(opts);

    lunify::MinifyResult r = lunify::minify("local x = 1 -- hi\nprint(x)\n", opts);
    CHECK(r.ok);
    CHECK(!r.output.empty());
    CHECK(r.output.find("--") == std::string::npos);
    CHECK(r.minifiedSize < r.originalSize);

    r = lunify::minify("local longName = 5\nprint(longName)\n", opts);
    CHECK(r.ok);
    CHECK(r.output.find("longName") == std::string::npos);

    lunify::Options keepOpts = opts;
    keepOpts.keep.push_back("longName");
    r = lunify::minify("local longName = 5\nprint(longName)\n", keepOpts);
    CHECK(r.ok);
    CHECK(r.output.find("longName") != std::string::npos);

    lunify::Options l1 = opts;
    l1.level = 1;
    lunify::applyLevel(l1);
    r = lunify::minify("local abc = 5\nprint(abc)\n", l1);
    CHECK(r.ok);
    CHECK(r.output.find("abc") != std::string::npos);

    lunify::Options enc = opts;
    enc.encodeStrings = true;
    r = lunify::minify("print('hello')\n", enc);
    CHECK(r.ok);
    CHECK(r.output.find("_D") != std::string::npos);

    r = lunify::minify("", opts);
    CHECK(r.ok);

    r = lunify::minify("local x = 'a' .. 'b' -- c\nreturn x\n", opts);
    CHECK(r.ok);
    CHECK(r.output.find("--") == std::string::npos);

    return failures == 0 ? 0 : 1;
}
