#include <cstdio>
#include <string>
#include <vector>

#include "lunify/lexer.hpp"

static int failures = 0;

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
            ++failures;                                                   \
        }                                                                 \
    } while (0)

int main() {
    std::vector<lunify::Token> t = lunify::lex(
        "local x = 42 -- comment\n"
        "local s = 'hi'\n"
        "local l = [[long]]\n"
        "x = x + 1\n");

    bool sawComment = false;
    bool sawNumber = false;
    bool sawString = false;
    bool sawLong = false;
    bool sawWord = false;
    for (const auto& tok : t) {
        if (tok.kind == lunify::TokenKind::Comment) sawComment = true;
        if (tok.kind == lunify::TokenKind::Number && tok.text == "42") sawNumber = true;
        if (tok.kind == lunify::TokenKind::String && tok.text == "'hi'") sawString = true;
        if (tok.kind == lunify::TokenKind::LongString && tok.text == "[[long]]") sawLong = true;
        if (tok.kind == lunify::TokenKind::Word && tok.text == "local") sawWord = true;
    }
    CHECK(sawComment);
    CHECK(sawNumber);
    CHECK(sawString);
    CHECK(sawLong);
    CHECK(sawWord);
    CHECK(t.back().kind == lunify::TokenKind::Eof);

    CHECK(lunify::isKeyword("local"));
    CHECK(lunify::isKeyword("function"));
    CHECK(!lunify::isKeyword("localx"));

    std::vector<lunify::Token> ops = lunify::lex("a..b a::b a==b a~=b a<=b a>=b a<<b a>>b a//b ...");
    std::vector<std::string> texts;
    for (const auto& tok : ops) {
        if (tok.kind != lunify::TokenKind::Eof) texts.push_back(tok.text);
    }
    CHECK(texts.size() == 28);

    std::vector<lunify::Token> num = lunify::lex("0x1F 0b1010 3.14 1e10");
    int nums = 0;
    for (const auto& tok : num) {
        if (tok.kind == lunify::TokenKind::Number) ++nums;
    }
    CHECK(nums == 4);

    if (failures == 0) {
        std::printf("test_lexer: all ok\n");
        return 0;
    }
    std::printf("test_lexer: %d failure(s)\n", failures);
    return 1;
}
