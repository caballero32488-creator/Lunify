#pragma once

#include <string>
#include <vector>

namespace lunify {

enum class TokenKind {
    Word,
    Number,
    String,
    LongString,
    Symbol,
    Comment,
    Eof,
};

struct Token {
    TokenKind kind = TokenKind::Eof;
    std::string text;
    unsigned line = 1;
    unsigned column = 1;
};

std::vector<Token> lex(const std::string& source);
bool isKeyword(const std::string& text);

}