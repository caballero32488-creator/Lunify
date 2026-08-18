#pragma once

#include <string>
#include <vector>

namespace lunify {

struct Options {
    int level = 2;
    bool removeComments = true;
    bool removeWhitespace = true;
    bool stripTypes = true;
    bool shortenNumbers = true;
    bool renameLocals = true;
    bool constantFold = true;
    bool deadCodeElim = true;
    bool inlineLocals = true;
    bool deadParams = true;
    bool renameGlobals = false;
    bool encodeStrings = false;
    bool stripDebug = false;
    bool finalNewline = true;
    std::string renamePrefix;
    std::vector<std::string> keep;
};

void applyLevel(Options& opts);

}