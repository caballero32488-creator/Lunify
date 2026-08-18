#pragma once

#include "lunify/ast.hpp"
#include "lunify/lexer.hpp"

#include <memory>
#include <string>
#include <vector>

namespace lunify {

struct ParseResult {
    std::vector<StmtPtr> statements;
    bool ok = false;
    std::string error;
};

ParseResult parse(const std::vector<Token>& tokens);

}