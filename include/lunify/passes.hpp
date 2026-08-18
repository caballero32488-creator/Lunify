#pragma once

#include "lunify/ast.hpp"
#include "lunify/scope.hpp"

namespace lunify {

void constantFold(std::vector<StmtPtr>& block);
void deadCodeElim(std::vector<StmtPtr>& block);
void inlineLocals(std::vector<StmtPtr>& block, ScopeResult& scopes, const Options& opts);
void deadParams(std::vector<StmtPtr>& block);

}