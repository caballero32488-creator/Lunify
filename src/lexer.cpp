#include "lunify/lexer.hpp"

#include <cstring>
#include <vector>

namespace lunify {

namespace {

bool isIdentStart(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool isIdentPart(char c) {
    return isIdentStart(c) || (c >= '0' && c <= '9');
}

bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

bool isHexDigit(char c) {
    return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

std::string closeDelimiter(size_t eq) {
    std::string close = "]";
    close.append(eq, '=');
    close.push_back(']');
    return close;
}

size_t longStringLength(const std::string& s, size_t i) {
    size_t eq = 0;
    size_t j = i + 1;
    while (j < s.size() && s[j] == '=') {
        ++eq;
        ++j;
    }
    if (j >= s.size() || s[j] != '[') {
        return 1;
    }
    std::string close = closeDelimiter(eq);
    size_t found = s.find(close, j + 1);
    if (found == std::string::npos) {
        return s.size() - i;
    }
    return found + close.size() - i;
}

size_t numberLength(const std::string& s, size_t i) {
    size_t n = s.size();
    if (s[i] == '0' && i + 1 < n && (s[i + 1] == 'x' || s[i + 1] == 'X')) {
        size_t j = i + 2;
        while (j < n && (isHexDigit(s[j]) || s[j] == '.')) {
            ++j;
        }
        if (j < n && (s[j] == 'p' || s[j] == 'P')) {
            ++j;
            if (j < n && (s[j] == '+' || s[j] == '-')) {
                ++j;
            }
            while (j < n && isDigit(s[j])) {
                ++j;
            }
        }
        return j - i;
    }
    if (s[i] == '0' && i + 1 < n && (s[i + 1] == 'b' || s[i + 1] == 'B')) {
        size_t j = i + 2;
        while (j < n && (s[j] == '0' || s[j] == '1')) {
            ++j;
        }
        return j - i;
    }
    size_t j = i;
    while (j < n && isDigit(s[j])) {
        ++j;
    }
    if (j < n && s[j] == '.' && (j + 1 >= n || s[j + 1] != '.')) {
        ++j;
        while (j < n && isDigit(s[j])) {
            ++j;
        }
    }
    if (j < n && (s[j] == 'e' || s[j] == 'E')) {
        size_t k = j + 1;
        if (k < n && (s[k] == '+' || s[k] == '-')) {
            ++k;
        }
        if (k < n && isDigit(s[k])) {
            j = k;
            while (j < n && isDigit(s[j])) {
                ++j;
            }
        }
    }
    return j - i;
}

}

bool isKeyword(const std::string& text) {
    static const char* const words[] = {
        "and", "break", "continue", "do", "else", "elseif", "end", "false",
        "for", "function", "goto", "if", "in", "local", "nil", "not", "or",
        "repeat", "return", "then", "true", "until", "while",
    };
    for (const char* w : words) {
        if (text == w) return true;
    }
    return false;
}

std::vector<Token> lex(const std::string& src) {
    std::vector<Token> tokens;
    const char* ops[] = {"...", "..", "::", "==", "~=", "<=", ">=", "<<", ">>", "//"};
    size_t i = 0;
    size_t n = src.size();
    unsigned line = 1;
    unsigned column = 1;

    auto advance = [&](size_t count) {
        for (size_t k = 0; k < count && i < n; ++k) {
            if (src[i] == '\n') {
                ++line;
                column = 1;
            } else {
                ++column;
            }
            ++i;
        }
    };

    while (i < n) {
        char c = src[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance(1);
            continue;
        }
        unsigned startLine = line;
        unsigned startCol = column;
        if (c == '-' && i + 1 < n && src[i + 1] == '-') {
            size_t start = i;
            advance(2);
            if (i < n && src[i] == '[') {
                size_t eq = 0;
                size_t j = i + 1;
                while (j < n && src[j] == '=') {
                    ++eq;
                    ++j;
                }
                if (j < n && src[j] == '[') {
                    std::string close = closeDelimiter(eq);
                    size_t found = src.find(close, j + 1);
                    size_t end = (found == std::string::npos) ? n : found + close.size();
                    advance(end - i);
                    tokens.push_back({TokenKind::Comment, src.substr(start, i - start), startLine, startCol});
                    continue;
                }
            }
            while (i < n && src[i] != '\n') {
                advance(1);
            }
            tokens.push_back({TokenKind::Comment, src.substr(start, i - start), startLine, startCol});
            continue;
        }
        if (c == '"' || c == '\'') {
            size_t start = i;
            char q = c;
            advance(1);
            while (i < n) {
                if (src[i] == '\\') {
                    advance(2);
                    continue;
                }
                if (src[i] == q) {
                    advance(1);
                    break;
                }
                advance(1);
            }
            tokens.push_back({TokenKind::String, src.substr(start, i - start), startLine, startCol});
            continue;
        }
        if (c == '[' && i + 1 < n) {
            size_t eq = 0;
            size_t j = i + 1;
            while (j < n && src[j] == '=') {
                ++eq;
                ++j;
            }
            if (j < n && src[j] == '[') {
                size_t len = longStringLength(src, i);
                tokens.push_back({TokenKind::LongString, src.substr(i, len), startLine, startCol});
                advance(len);
                continue;
            }
        }
        if (isDigit(c) || (c == '.' && i + 1 < n && isDigit(src[i + 1]))) {
            size_t len = numberLength(src, i);
            tokens.push_back({TokenKind::Number, src.substr(i, len), startLine, startCol});
            advance(len);
            continue;
        }
        if (isIdentStart(c)) {
            size_t start = i;
            while (i < n && isIdentPart(src[i])) {
                advance(1);
            }
            tokens.push_back({TokenKind::Word, src.substr(start, i - start), startLine, startCol});
            continue;
        }
        size_t len = 0;
        for (const char* op : ops) {
            size_t l = std::strlen(op);
            if (n - i >= l && src.compare(i, l, op) == 0) {
                len = l;
                break;
            }
        }
        if (len == 0) {
            len = 1;
        }
        tokens.push_back({TokenKind::Symbol, src.substr(i, len), startLine, startCol});
        advance(len);
    }
    tokens.push_back({TokenKind::Eof, "", line, column});
    return tokens;
}

}
