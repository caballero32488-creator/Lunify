#pragma once

#include "lunify/ast.hpp"
#include "lunify/options.hpp"

#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace lunify {

struct Symbol {
    std::string name;
    std::string renamed;
    bool renamed_ = false;
    bool noRename = false;
    bool used = false;
    bool usedAsWrite = false;
    bool isParam = false;
    bool isSelf = false;
    std::size_t refCount = 0;
    Stmt* declStmt = nullptr;
    std::unique_ptr<Expr> inlineValue;
};

struct GlobalInfo {
    bool read = false;
    bool written = false;
};

struct ScopeResult {
    std::deque<std::unique_ptr<Symbol>> symbols;
    std::unordered_map<std::string, GlobalInfo> globals;
    std::unordered_map<std::string, std::string> globalRenames;
    std::vector<std::pair<std::string*, Symbol*>> declSites;
};

ScopeResult analyzeScopes(std::vector<StmtPtr>& block);
void renameVariables(std::vector<StmtPtr>& block, ScopeResult& scopes, const Options& opts);

}