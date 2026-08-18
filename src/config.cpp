#include "lunify/config.hpp"

#include <string>

namespace lunify {

namespace {

bool readBool(const json::Value* v, bool fallback) {
    if (!v || !v->isBool()) return fallback;
    return v->boolean;
}

void applyFlag(Options& opts, const json::Value* v, bool& flag) {
    if (v && v->isBool()) flag = v->boolean;
}

}

void applyConfig(Options& opts, const json::Value& root) {
    if (!root.isObject()) return;

    const json::Value* level = root.find("level");
    if (level && level->isNumber()) {
        int l = static_cast<int>(level->number);
        if (l < 1) l = 1;
        if (l > 3) l = 3;
        opts.level = l;
        applyLevel(opts);
    }

    applyFlag(opts, root.find("removeComments"), opts.removeComments);
    applyFlag(opts, root.find("removeWhitespace"), opts.removeWhitespace);
    applyFlag(opts, root.find("stripTypes"), opts.stripTypes);
    applyFlag(opts, root.find("shortenNumbers"), opts.shortenNumbers);
    applyFlag(opts, root.find("renameLocals"), opts.renameLocals);
    applyFlag(opts, root.find("constantFold"), opts.constantFold);
    applyFlag(opts, root.find("deadCodeElim"), opts.deadCodeElim);
    applyFlag(opts, root.find("inlineLocals"), opts.inlineLocals);
    applyFlag(opts, root.find("deadParams"), opts.deadParams);
    applyFlag(opts, root.find("renameGlobals"), opts.renameGlobals);
    applyFlag(opts, root.find("encodeStrings"), opts.encodeStrings);
    applyFlag(opts, root.find("stripDebug"), opts.stripDebug);
    applyFlag(opts, root.find("finalNewline"), opts.finalNewline);

    const json::Value* prefix = root.find("renamePrefix");
    if (prefix && prefix->isString()) {
        opts.renamePrefix = prefix->string;
    }

    const json::Value* keep = root.find("keep");
    if (keep && keep->isArray()) {
        opts.keep.clear();
        for (const auto& item : keep->array) {
            if (item.isString()) opts.keep.push_back(item.string);
        }
    }
}

Options loadConfigText(const std::string& text, std::string* error) {
    if (error) error->clear();
    Options opts;
    json::Value root = json::parse(text, error);
    if (error && !error->empty()) return opts;
    applyConfig(opts, root);
    return opts;
}

Options loadConfigFile(const std::string& path, std::string* error) {
    if (error) error->clear();
    Options opts;
    json::Value root = json::parseFile(path, error);
    if (error && !error->empty()) return opts;
    applyConfig(opts, root);
    return opts;
}

}
