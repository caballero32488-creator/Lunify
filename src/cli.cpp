#include "lunify/cli.hpp"

#include "lunify/config.hpp"
#include "lunify/minifier.hpp"
#include "lunify/options.hpp"
#include "lunify/version.hpp"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace lunify {

namespace {

void printUsage(std::ostream& out) {
    out << "usage: lunify [options] <input.lua>\n"
        << "\n"
        << "options:\n"
        << "  -c, --config <file>      load config JSON file\n"
        << "  -o, --output <file>      write output to file (default: stdout)\n"
        << "  -l, --level <1|2|3>      compression level (default: 2)\n"
        << "      --remove-comments    strip comments (default: on)\n"
        << "      --remove-whitespace  strip whitespace/newlines (default: on)\n"
        << "      --strip-types        remove Luau type annotations (default: on)\n"
        << "      --shorten-numbers    shorten number literals (default: on)\n"
        << "      --rename-locals      rename locals to short names (default: on)\n"
        << "      --constant-fold      fold constant expressions (default: on)\n"
        << "      --dead-code-elim     remove unreachable/unused code (default: on)\n"
        << "      --inline-locals      inline single-use literal locals (default: on)\n"
        << "      --dead-params        remove unused trailing params (default: on)\n"
        << "      --rename-globals     rename globals too (default: off)\n"
        << "      --encode-strings     XOR-encode string literals (default: off)\n"
        << "      --strip-debug        remove print/warn calls (default: off)\n"
        << "      --keep-comments      do not remove comments\n"
        << "      --no-shorten-numbers do not shorten numbers\n"
        << "      --no-final-newline   do not add trailing newline\n"
        << "      --rename-prefix <s>  prefix for renamed locals (default: \"\")\n"
        << "      --keep <name>        keep global name unrenamed (repeatable)\n"
        << "  -V, --version            print version\n"
        << "  -h, --help               print this help\n";
}

bool match(const std::string& arg, const char* a, const char* b) {
    return arg == a || arg == b;
}

std::string needValue(int& i, int argc, char** argv, const char* flag) {
    if (i + 1 >= argc) {
        std::cerr << "error: missing value for " << flag << "\n";
        std::exit(2);
    }
    return argv[++i];
}

}

int run(int argc, char** argv) {
    if (argc < 2) {
        printUsage(std::cerr);
        return 1;
    }

    Options opts;
    std::string input;
    std::string output;
    bool haveConfig = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(std::cout);
            return 0;
        } else if (arg == "-V" || arg == "--version") {
            std::cout << "lunify " << VERSION_STRING << "\n";
            return 0;
        } else if (match(arg, "-c", "--config")) {
            std::string path = needValue(i, argc, argv, arg.c_str());
            std::string err;
            opts = loadConfigFile(path, &err);
            if (!err.empty()) {
                std::cerr << "error: " << err << "\n";
                return 1;
            }
            haveConfig = true;
        } else if (match(arg, "-o", "--output")) {
            output = needValue(i, argc, argv, arg.c_str());
        } else if (match(arg, "-l", "--level")) {
            std::string v = needValue(i, argc, argv, arg.c_str());
            opts.level = std::atoi(v.c_str());
            if (opts.level < 1) opts.level = 1;
            if (opts.level > 3) opts.level = 3;
            applyLevel(opts);
        } else if (arg == "--remove-comments") {
            opts.removeComments = true;
        } else if (arg == "--remove-whitespace") {
            opts.removeWhitespace = true;
        } else if (arg == "--strip-types") {
            opts.stripTypes = true;
        } else if (arg == "--shorten-numbers") {
            opts.shortenNumbers = true;
        } else if (arg == "--rename-locals") {
            opts.renameLocals = true;
        } else if (arg == "--constant-fold") {
            opts.constantFold = true;
        } else if (arg == "--dead-code-elim") {
            opts.deadCodeElim = true;
        } else if (arg == "--inline-locals") {
            opts.inlineLocals = true;
        } else if (arg == "--dead-params") {
            opts.deadParams = true;
        } else if (arg == "--rename-globals") {
            opts.renameGlobals = true;
        } else if (arg == "--encode-strings") {
            opts.encodeStrings = true;
        } else if (arg == "--strip-debug") {
            opts.stripDebug = true;
        } else if (arg == "--keep-comments") {
            opts.removeComments = false;
        } else if (arg == "--no-shorten-numbers") {
            opts.shortenNumbers = false;
        } else if (arg == "--no-final-newline") {
            opts.finalNewline = false;
        } else if (arg == "--rename-prefix") {
            opts.renamePrefix = needValue(i, argc, argv, arg.c_str());
        } else if (arg == "--keep") {
            opts.keep.push_back(needValue(i, argc, argv, arg.c_str()));
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "error: unknown option " << arg << "\n";
            return 2;
        } else {
            if (!input.empty()) {
                std::cerr << "error: multiple input files given\n";
                return 2;
            }
            input = arg;
        }
    }

    (void)haveConfig;

    if (input.empty()) {
        std::cerr << "error: no input file\n";
        return 1;
    }

    std::ifstream in(input, std::ios::binary);
    if (!in) {
        std::cerr << "error: cannot open " << input << "\n";
        return 1;
    }
    std::string src((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    MinifyResult result = minify(src, opts);
    if (!result.ok) {
        std::cerr << "error: " << result.error << "\n";
        return 1;
    }

    if (!output.empty()) {
        std::ofstream f(output, std::ios::binary);
        if (!f) {
            std::cerr << "error: cannot write " << output << "\n";
            return 1;
        }
        f << result.output;
    } else {
        std::cout << result.output;
    }
    return 0;
}

}
