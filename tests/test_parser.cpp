#include <cstdio>
#include <string>
#include <vector>

#include "lunify/ast.hpp"
#include "lunify/lexer.hpp"
#include "lunify/parser.hpp"

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

static std::vector<lunify::StmtPtr>& stmts(lunify::ParseResult& pr) {
    return pr.statements;
}

int main() {
    lunify::ParseResult p = parseText("local x = 1 + 2 * 3");
    CHECK(p.ok);
    CHECK(stmts(p).size() == 1);
    CHECK(stmts(p)[0]->kind == lunify::StmtKind::Local);

    p = parseText("if a then b() elseif c then d() else e() end");
    CHECK(p.ok);
    CHECK(stmts(p).size() == 1);
    CHECK(stmts(p)[0]->kind == lunify::StmtKind::If);
    auto* iff = static_cast<lunify::IfStmt*>(stmts(p)[0].get());
    CHECK(iff->branches.size() == 2);
    CHECK(!iff->elseBody.empty());

    p = parseText("for i = 1, 10 do print(i) end");
    CHECK(p.ok);
    CHECK(stmts(p)[0]->kind == lunify::StmtKind::NumericFor);

    p = parseText("for k, v in pairs(t) do end");
    CHECK(p.ok);
    CHECK(stmts(p)[0]->kind == lunify::StmtKind::GenericFor);
    auto* g = static_cast<lunify::GenericForStmt*>(stmts(p)[0].get());
    CHECK(g->vars.size() == 2);

    p = parseText("function foo(a, b) return a + b end");
    CHECK(p.ok);
    CHECK(stmts(p)[0]->kind == lunify::StmtKind::FuncDecl);
    auto* fd = static_cast<lunify::FuncDeclStmt*>(stmts(p)[0].get());
    CHECK(fd->isLocal == false);
    CHECK(fd->params.size() == 2);

    p = parseText("local function bar() end");
    CHECK(p.ok);
    CHECK(stmts(p)[0]->kind == lunify::StmtKind::FuncDecl);
    CHECK(static_cast<lunify::FuncDeclStmt*>(stmts(p)[0].get())->isLocal);

    p = parseText("local t = {1, 2, k = 'v', [expr] = 3}");
    CHECK(p.ok);
    auto* tab = static_cast<lunify::LocalStmt*>(stmts(p)[0].get());
    auto* te = static_cast<lunify::TableExpr*>(tab->values[0].get());
    CHECK(te->fields.size() == 4);
    CHECK(te->fields[0].isList);
    CHECK(te->fields[2].keyName == "k");
    CHECK(te->fields[3].keyExpr != nullptr);

    p = parseText("local x: number = 5");
    CHECK(p.ok);

    p = parseText("type Foo = { x: number }");
    CHECK(p.ok);
    CHECK(stmts(p).size() == 1);
    CHECK(stmts(p)[0]->kind == lunify::StmtKind::Empty);

    p = parseText("function obj:m(x) return x end");
    CHECK(p.ok);
    auto* fm = static_cast<lunify::FuncDeclStmt*>(stmts(p)[0].get());
    CHECK(fm->params.size() == 2);
    CHECK(fm->params[0] == "self");
    CHECK(fm->params[1] == "x");

    p = parseText("local function f(...) return ... end");
    CHECK(p.ok);
    fm = static_cast<lunify::FuncDeclStmt*>(stmts(p)[0].get());
    CHECK(fm->params.empty());
    CHECK(fm->vararg);

    p = parseText("repeat x = x + 1 until x > 10");
    CHECK(p.ok);
    CHECK(stmts(p)[0]->kind == lunify::StmtKind::Repeat);

    p = parseText("goto done ::done::");
    CHECK(p.ok);
    CHECK(stmts(p).size() == 2);
    CHECK(stmts(p)[0]->kind == lunify::StmtKind::Goto);
    CHECK(stmts(p)[1]->kind == lunify::StmtKind::Label);

    p = parseText("while true do continue end");
    CHECK(p.ok);

    p = parseText("local x = }");
    CHECK(!p.ok);
    CHECK(!p.error.empty());

    p = parseText("obj:m('a', 'b')");
    CHECK(p.ok);
    CHECK(stmts(p)[0]->kind == lunify::StmtKind::Call);
    auto* call = static_cast<lunify::CallStmt*>(stmts(p)[0].get());
    CHECK(call->call->kind == lunify::ExprKind::MethodCall);

    if (failures == 0) {
        std::printf("test_parser: all ok\n");
        return 0;
    }
    std::printf("test_parser: %d failure(s)\n", failures);
    return 1;
}
