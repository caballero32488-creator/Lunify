#include "lunify/options.hpp"

namespace lunify {

void applyLevel(Options& opts) {
    bool base = true;
    bool advanced = false;
    bool encode = false;
    if (opts.level >= 3) {
        encode = true;
    }
    if (opts.level >= 2) {
        advanced = true;
    }
    if (opts.level < 1) {
        base = false;
    }
    opts.removeComments = base;
    opts.removeWhitespace = base;
    opts.stripTypes = base;
    opts.shortenNumbers = base;
    opts.renameLocals = advanced;
    opts.constantFold = advanced;
    opts.deadCodeElim = advanced;
    opts.inlineLocals = advanced;
    opts.deadParams = advanced;
    opts.renameGlobals = false;
    opts.encodeStrings = encode;
    opts.stripDebug = false;
}

}
