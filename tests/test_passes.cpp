#include <cstdio>
#include <string>
#include <vector>

#include "lunify/lexer.hpp"
#include "lunify/parser.hpp"
#include "lunify/passes.hpp"
#include "lunify/scope.hpp"

static int failures = 0;

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
            ++failures;                                                   \
        }                                                                 \
    } while (0)

static lunify::ParseResult parseText(const std::string& text) {
    return lunify::parse(lunify::lex(text));
}

int main() {
    lunify::ParseResult p = parseText("local x = 1 + 2\nprint(x)");
    CHECK(p.ok);
    lunify::constantFold(p.statements);
    auto* l = static_cast<lunify::LocalStmt*>(p.statements[0].get());
    CHECK(l->values[0]->kind == lunify::ExprKind::Number);
    CHECK(static_cast<lunify::NumberExpr*>(l->values[0].get())->text == "3");

    p = parseText("local x = 'a' .. 'b'");
    lunify::constantFold(p.statements);
    l = static_cast<lunify::LocalStmt*>(p.statements[0].get());
    CHECK(l->values[0]->kind == lunify::ExprKind::String);
    CHECK(static_cast<lunify::StringExpr*>(l->values[0].get())->text == "\"ab\"");

    p = parseText("local x = true and false");
    lunify::constantFold(p.statements);
    l = static_cast<lunify::LocalStmt*>(p.statements[0].get());
    CHECK(l->values[0]->kind == lunify::ExprKind::Bool);
    CHECK(static_cast<lunify::BoolExpr*>(l->values[0].get())->value == false);

    p = parseText("return 1\nprint('dead')");
    CHECK(p.ok);
    lunify::deadCodeElim(p.statements);
    CHECK(p.statements.size() == 2);
    CHECK(p.statements[0]->dead == false);
    CHECK(p.statements[1]->dead == true);

    p = parseText("if true then a() else b() end");
    lunify::deadCodeElim(p.statements);
    CHECK(p.statements.size() == 1);
    CHECK(p.statements[0]->kind == lunify::StmtKind::Do);
    auto* d = static_cast<lunify::DoStmt*>(p.statements[0].get());
    CHECK(d->body.size() == 1);
    CHECK(d->body[0]->kind == lunify::StmtKind::Call);

    p = parseText("local unused = 5\nlocal used = 6\nprint(used)");
    CHECK(p.ok);
    lunify::analyzeScopes(p.statements);
    lunify::deadCodeElim(p.statements);
    CHECK(p.statements.size() == 3);
    CHECK(p.statements[0]->dead == true);
    CHECK(p.statements[1]->dead == false);

    p = parseText("local x = 5\nprint(x)");
    CHECK(p.ok);
    lunify::ScopeResult scopes = lunify::analyzeScopes(p.statements);
    CHECK(scopes.symbols.size() == 1);
    lunify::Options opts;
    lunify::inlineLocals(p.statements, scopes, opts);
    auto* l2 = static_cast<lunify::LocalStmt*>(p.statements[0].get());
    CHECK(l2->dead == true);

    p = parseText("local function unusedFn() end\nused()");
    lunify::analyzeScopes(p.statements);
    lunify::deadCodeElim(p.statements);
    CHECK(p.statements[0]->dead == true);

    p = parseText("function f(a, b) return a end");
    lunify::analyzeScopes(p.statements);
    lunify::deadParams(p.statements);
    auto* f = static_cast<lunify::FuncDeclStmt*>(p.statements[0].get());
    CHECK(f->params.size() == 1);
    CHECK(f->params[0] == "a");

    if (failures == 0) {
        std::printf("test_passes: all ok\n");
        return 0;
    }
    std::printf("test_passes: %d failure(s)\n", failures);
    return 1;
}
