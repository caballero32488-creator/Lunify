#include <cstdio>
#include <string>

#include "lunify/config.hpp"
#include "lunify/options.hpp"

static int failures = 0;

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
            ++failures;                                                   \
        }                                                                 \
    } while (0)

int main() {
    std::string err;
    lunify::Options opts = lunify::loadConfigText(R"({"level": 1})", &err);
    CHECK(err.empty());
    CHECK(opts.level == 1);
    CHECK(opts.renameLocals == false);
    CHECK(opts.removeComments == true);

    opts = lunify::loadConfigText(
        R"({"level": 3, "stripDebug": true, "keep": ["foo"], "renamePrefix": "p"})", &err);
    CHECK(err.empty());
    CHECK(opts.level == 3);
    CHECK(opts.encodeStrings == true);
    CHECK(opts.stripDebug == true);
    CHECK(opts.keep.size() == 1);
    CHECK(opts.keep[0] == "foo");
    CHECK(opts.renamePrefix == "p");

    opts = lunify::loadConfigText(R"({"renameLocals": false})", &err);
    CHECK(err.empty());
    CHECK(opts.renameLocals == false);
    CHECK(opts.level == 2);

    opts = lunify::loadConfigText("{ bad json", &err);
    CHECK(!err.empty());

    opts = lunify::loadConfigText(R"({"level": 2, "encodeStrings": true, "renameGlobals": true, "finalNewline": false})", &err);
    CHECK(err.empty());
    CHECK(opts.encodeStrings == true);
    CHECK(opts.renameGlobals == true);
    CHECK(opts.finalNewline == false);

    lunify::Options base;
    lunify::applyLevel(base);
    CHECK(base.level == 2);
    CHECK(base.constantFold == true);
    CHECK(base.encodeStrings == false);

    base.level = 3;
    lunify::applyLevel(base);
    CHECK(base.encodeStrings == true);

    base.level = 1;
    lunify::applyLevel(base);
    CHECK(base.constantFold == false);
    CHECK(base.removeComments == true);

    if (failures == 0) {
        std::printf("test_config: all ok\n");
        return 0;
    }
    std::printf("test_config: %d failure(s)\n", failures);
    return 1;
}
