#include "lunify/parser.hpp"

#include <memory>
#include <string>
#include <vector>

namespace lunify {

namespace {

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens) : tokens_(tokens) {}

    ParseResult run() {
        ParseResult result;
        result.ok = parseBlock(result.statements);
        if (peek().kind != TokenKind::Eof) {
            result.ok = false;
            fail();
        }
        result.error = error_;
        return result;
    }

private:
    const std::vector<Token>& tokens_;
    std::size_t index_ = 0;
    std::string error_;
    Token eof_{TokenKind::Eof, ""};

    const Token& peek(std::size_t ahead = 0) const {
        std::size_t i = index_ + ahead;
        if (i >= tokens_.size()) return eof_;
        return tokens_[i];
    }

    const Token& next() {
        const Token& t = peek();
        if (index_ < tokens_.size() && tokens_[index_].kind != TokenKind::Eof) {
            ++index_;
        }
        return t;
    }

    bool atWord(const char* text) const {
        return peek().kind == TokenKind::Word && peek().text == text;
    }

    bool atSym(const char* text) const {
        return peek().kind == TokenKind::Symbol && peek().text == text;
    }

    bool eat(const char* text) {
        if (atWord(text) || atSym(text)) {
            next();
            return true;
        }
        return false;
    }

    void fail() {
        if (error_.empty()) {
            const Token& t = peek();
            error_ = "syntax error at line " + std::to_string(t.line) + ", column " +
                     std::to_string(t.column) + ": unexpected token '" + t.text + "'";
        }
    }

    bool parseBlock(std::vector<StmtPtr>& out) {
        while (true) {
            const Token& t = peek();
            if (t.kind == TokenKind::Eof) return true;
            if (t.kind == TokenKind::Comment) {
                next();
                continue;
            }
            if (t.kind == TokenKind::Word) {
                if (t.text == "end" || t.text == "else" || t.text == "elseif" || t.text == "until") {
                    return true;
                }
            }
            StmtPtr stmt;
            if (!parseStatement(stmt)) {
                fail();
                return false;
            }
            if (stmt) {
                out.push_back(std::move(stmt));
            }
        }
    }

    bool parseStatement(StmtPtr& out) {
        const Token& t = peek();
        if (t.kind == TokenKind::Symbol && t.text == ";") {
            next();
            out = std::make_unique<EmptyStmt>();
            return true;
        }
        if (t.kind == TokenKind::Symbol && t.text == "::") {
            next();
            if (peek().kind != TokenKind::Word) return false;
            auto label = std::make_unique<LabelStmt>();
            label->name = next().text;
            if (!atSym("::")) return false;
            next();
            out = std::move(label);
            return true;
        }
        if (t.kind != TokenKind::Word) {
            return parseExpressionStatement(out);
        }
        const std::string& w = t.text;
        if (w == "if") return parseIf(out);
        if (w == "while") return parseWhile(out);
        if (w == "do") return parseDo(out);
        if (w == "for") return parseFor(out);
        if (w == "repeat") return parseRepeat(out);
        if (w == "function") return parseFunctionDecl(out);
        if (w == "local") return parseLocal(out);
        if (w == "return") return parseReturn(out);
        if (w == "break") {
            next();
            out = std::make_unique<BreakStmt>();
            return true;
        }
        if (w == "continue") {
            next();
            out = std::make_unique<ContinueStmt>();
            return true;
        }
        if (w == "goto") {
            next();
            if (peek().kind != TokenKind::Word) return false;
            auto g = std::make_unique<GotoStmt>();
            g->label = next().text;
            out = std::move(g);
            return true;
        }
        if (w == "type" || (w == "export" && peek(1).kind == TokenKind::Word && peek(1).text == "type")) {
            next();
            if (w == "export") next();
            if (peek().kind == TokenKind::Word) next();
            if (eat("=")) {
                skipType();
            }
            out = std::make_unique<EmptyStmt>();
            return true;
        }
        return parseExpressionStatement(out);
    }

    bool skipType() {
        int depth = 0;
        while (true) {
            const Token& t = peek();
            if (t.kind == TokenKind::Eof) return true;
            if (t.kind == TokenKind::Symbol) {
                const std::string& s = t.text;
                if (s == "-" && peek(1).kind == TokenKind::Symbol && peek(1).text == ">") {
                    next();
                    next();
                    continue;
                }
                if (s == "(" || s == "{" || s == "[" || s == "<") {
                    ++depth;
                    next();
                    continue;
                }
                if (s == ")" || s == "}" || s == "]" || s == ">") {
                    if (depth == 0) return true;
                    --depth;
                    next();
                    continue;
                }
                if (s == "=" || s == ";" || s == ",") {
                    if (depth == 0) return true;
                    next();
                    continue;
                }
                if (s == "." || s == ":" || s == "|" || s == "&" || s == "...") {
                    next();
                    continue;
                }
                return false;
            }
            if (t.kind == TokenKind::Word) {
                if (depth == 0) {
                    const std::string& w = t.text;
                    if (w == "local" || w == "function" || w == "return" || w == "if" ||
                        w == "while" || w == "for" || w == "repeat" || w == "do" ||
                        w == "then" || w == "end" || w == "break" || w == "continue" ||
                        w == "goto" || w == "type" || w == "export" || w == "until" ||
                        w == "else" || w == "elseif" || w == "in") {
                        return true;
                    }
                }
                next();
                continue;
            }
            if (t.kind == TokenKind::String || t.kind == TokenKind::LongString) {
                next();
                continue;
            }
            return false;
        }
    }

    bool parseLocal(StmtPtr& out) {
        next();
        if (atWord("function")) {
            next();
            return parseLocalFunction(out);
        }
        auto stmt = std::make_unique<LocalStmt>();
        if (peek().kind != TokenKind::Word) return false;
        while (true) {
            stmt->names.push_back(next().text);
            if (atSym(":")) {
                next();
                skipType();
            }
            if (eat(",")) continue;
            break;
        }
        if (eat("=")) {
            if (!parseExprList(stmt->values)) return false;
        }
        out = std::move(stmt);
        return true;
    }

    bool parseLocalFunction(StmtPtr& out) {
        if (peek().kind != TokenKind::Word) return false;
        auto stmt = std::make_unique<FuncDeclStmt>();
        stmt->isLocal = true;
        stmt->target = std::make_unique<NameExpr>(next().text);
        if (!parseFuncSignature(stmt->params, stmt->vararg)) return false;
        if (!parseBlock(stmt->body)) return false;
        if (!eat("end")) return false;
        out = std::move(stmt);
        return true;
    }

    bool parseFunctionDecl(StmtPtr& out) {
        next();
        auto stmt = std::make_unique<FuncDeclStmt>();
        stmt->isLocal = false;
        if (peek().kind != TokenKind::Word) return false;
        ExprPtr target = std::make_unique<NameExpr>(next().text);
        while (atSym(".")) {
            next();
            if (peek().kind != TokenKind::Word) return false;
            auto idx = std::make_unique<IndexExpr>();
            idx->obj = std::move(target);
            idx->name = next().text;
            idx->isDot = true;
            target = std::move(idx);
        }
        if (atSym(":")) {
            next();
            if (peek().kind != TokenKind::Word) return false;
            auto idx = std::make_unique<IndexExpr>();
            idx->obj = std::move(target);
            idx->name = next().text;
            idx->isDot = false;
            target = std::move(idx);
            stmt->isMethod = true;
        }
        stmt->target = std::move(target);
        if (!parseFuncSignature(stmt->params, stmt->vararg)) return false;
        if (stmt->isMethod) {
            stmt->params.insert(stmt->params.begin(), "self");
        }
        if (!parseBlock(stmt->body)) return false;
        if (!eat("end")) return false;
        out = std::move(stmt);
        return true;
    }

    bool parseFuncSignature(std::vector<std::string>& params, bool& vararg) {
        if (atSym("<")) {
            next();
            skipType();
        }
        if (!atSym("(")) return false;
        next();
        if (atSym(")")) {
            next();
        } else {
            while (true) {
                if (atSym("...")) {
                    next();
                    vararg = true;
                    if (!atSym(")")) return false;
                    next();
                    break;
                }
                if (peek().kind != TokenKind::Word) return false;
                params.push_back(next().text);
                if (atSym(":")) {
                    next();
                    skipType();
                }
                if (eat(",")) continue;
                if (atSym(")")) {
                    next();
                    break;
                }
                return false;
            }
        }
        if (atSym(":")) {
            next();
            skipType();
        }
        return true;
    }

    bool parseIf(StmtPtr& out) {
        next();
        auto stmt = std::make_unique<IfStmt>();
        IfStmt::Branch branch;
        if (!parseExpr(branch.cond)) return false;
        if (!eat("then")) return false;
        if (!parseBlock(branch.body)) return false;
        stmt->branches.push_back(std::move(branch));
        while (atWord("elseif")) {
            next();
            IfStmt::Branch b;
            if (!parseExpr(b.cond)) return false;
            if (!eat("then")) return false;
            if (!parseBlock(b.body)) return false;
            stmt->branches.push_back(std::move(b));
        }
        if (atWord("else")) {
            next();
            if (!parseBlock(stmt->elseBody)) return false;
        }
        if (!eat("end")) return false;
        out = std::move(stmt);
        return true;
    }

    bool parseWhile(StmtPtr& out) {
        next();
        auto stmt = std::make_unique<WhileStmt>();
        if (!parseExpr(stmt->cond)) return false;
        if (!eat("do")) return false;
        if (!parseBlock(stmt->body)) return false;
        if (!eat("end")) return false;
        out = std::move(stmt);
        return true;
    }

    bool parseDo(StmtPtr& out) {
        next();
        auto stmt = std::make_unique<DoStmt>();
        if (!parseBlock(stmt->body)) return false;
        if (!eat("end")) return false;
        out = std::move(stmt);
        return true;
    }

    bool parseRepeat(StmtPtr& out) {
        next();
        auto stmt = std::make_unique<RepeatStmt>();
        if (!parseBlock(stmt->body)) return false;
        if (!eat("until")) return false;
        if (!parseExpr(stmt->cond)) return false;
        out = std::move(stmt);
        return true;
    }

    bool parseFor(StmtPtr& out) {
        next();
        if (peek().kind != TokenKind::Word) return false;
        std::string name = next().text;
        if (eat("=")) {
            auto stmt = std::make_unique<NumericForStmt>();
            stmt->var = name;
            if (!parseExpr(stmt->start)) return false;
            if (!eat(",")) return false;
            if (!parseExpr(stmt->stop)) return false;
            if (eat(",")) {
                if (!parseExpr(stmt->step)) return false;
            }
            if (!eat("do")) return false;
            if (!parseBlock(stmt->body)) return false;
            if (!eat("end")) return false;
            out = std::move(stmt);
            return true;
        }
        auto stmt = std::make_unique<GenericForStmt>();
        stmt->vars.push_back(name);
        while (eat(",")) {
            if (peek().kind != TokenKind::Word) return false;
            stmt->vars.push_back(next().text);
        }
        if (!eat("in")) return false;
        if (!parseExprList(stmt->exprs)) return false;
        if (!eat("do")) return false;
        if (!parseBlock(stmt->body)) return false;
        if (!eat("end")) return false;
        out = std::move(stmt);
        return true;
    }

    bool parseReturn(StmtPtr& out) {
        next();
        auto stmt = std::make_unique<ReturnStmt>();
        if (!atSym(";") && !atWord("end") && !atWord("else") && !atWord("elseif") &&
            !atWord("until") && peek().kind != TokenKind::Eof && peek().kind != TokenKind::Comment) {
            if (!parseExprList(stmt->values)) return false;
        }
        if (atSym(";")) next();
        out = std::move(stmt);
        return true;
    }

    bool parseExpressionStatement(StmtPtr& out) {
        ExprPtr first;
        if (!parseExpr(first)) return false;
        if (isCall(first)) {
            if (atSym(",") || atSym("=")) return false;
            auto stmt = std::make_unique<CallStmt>();
            stmt->call = std::move(first);
            out = std::move(stmt);
            return true;
        }
        std::vector<ExprPtr> targets;
        targets.push_back(std::move(first));
        while (eat(",")) {
            ExprPtr t;
            if (!parseExpr(t)) return false;
            if (isCall(t)) return false;
            targets.push_back(std::move(t));
        }
        if (!eat("=")) return false;
        std::vector<ExprPtr> values;
        if (!parseExprList(values)) return false;
        auto stmt = std::make_unique<AssignStmt>();
        stmt->targets = std::move(targets);
        stmt->values = std::move(values);
        out = std::move(stmt);
        return true;
    }

    static bool isCall(const ExprPtr& e) {
        return e->kind == ExprKind::Call || e->kind == ExprKind::MethodCall;
    }

    bool parseExpr(ExprPtr& out) {
        return parseSubExpr(out, 0);
    }

    static int binPrec(const std::string& op) {
        if (op == "or") return 1;
        if (op == "and") return 2;
        if (op == "<" || op == ">" || op == "<=" || op == ">=" || op == "~=" || op == "==") return 3;
        if (op == "|") return 4;
        if (op == "~") return 5;
        if (op == "&") return 6;
        if (op == "<<" || op == ">>") return 7;
        if (op == "..") return 8;
        if (op == "+" || op == "-") return 9;
        if (op == "*" || op == "/" || op == "//" || op == "%") return 10;
        if (op == "^") return 13;
        return 0;
    }

    bool isBinaryOp(const Token& t) const {
        if (t.kind == TokenKind::Word) {
            return t.text == "and" || t.text == "or";
        }
        if (t.kind != TokenKind::Symbol) return false;
        return binPrec(t.text) != 0;
    }

    bool parseSubExpr(ExprPtr& out, int minPrec) {
        if (!parseUnary(out)) return false;
        while (true) {
            const Token& t = peek();
            if (!isBinaryOp(t)) break;
            int prec = binPrec(t.text);
            if (prec < minPrec) break;
            bool rightAssoc = (t.text == "^" || t.text == "..");
            next();
            ExprPtr rhs;
            if (!parseSubExpr(rhs, rightAssoc ? prec : prec + 1)) return false;
            auto b = std::make_unique<BinaryExpr>();
            b->op = t.text;
            b->lhs = std::move(out);
            b->rhs = std::move(rhs);
            out = std::move(b);
        }
        return true;
    }

    bool parseUnary(ExprPtr& out) {
        const Token& t = peek();
        if (t.kind == TokenKind::Symbol && (t.text == "-" || t.text == "#" || t.text == "~")) {
            next();
            ExprPtr operand;
            if (!parseSubExpr(operand, 12)) return false;
            auto u = std::make_unique<UnaryExpr>();
            u->op = t.text;
            u->operand = std::move(operand);
            out = std::move(u);
            return true;
        }
        if (atWord("not")) {
            next();
            ExprPtr operand;
            if (!parseSubExpr(operand, 12)) return false;
            auto u = std::make_unique<UnaryExpr>();
            u->op = "not";
            u->operand = std::move(operand);
            out = std::move(u);
            return true;
        }
        return parseSimpleExpr(out);
    }

    bool parseSimpleExpr(ExprPtr& out) {
        const Token& t = peek();
        if (t.kind == TokenKind::Number) {
            next();
            out = std::make_unique<NumberExpr>(t.text);
        } else if (t.kind == TokenKind::String || t.kind == TokenKind::LongString) {
            next();
            out = std::make_unique<StringExpr>(t.text);
        } else if (t.kind == TokenKind::Word) {
            if (t.text == "nil") {
                next();
                out = std::make_unique<NilExpr>();
            } else if (t.text == "true") {
                next();
                out = std::make_unique<BoolExpr>(true);
            } else if (t.text == "false") {
                next();
                out = std::make_unique<BoolExpr>(false);
            } else if (t.text == "function") {
                next();
                return parseFunctionExpr(out);
            } else {
                next();
                out = std::make_unique<NameExpr>(t.text);
            }
        } else if (t.kind == TokenKind::Symbol && t.text == "...") {
            next();
            out = std::make_unique<VarargExpr>();
        } else if (atSym("(")) {
            next();
            ExprPtr inner;
            if (!parseExpr(inner)) return false;
            if (!eat(")")) return false;
            auto p = std::make_unique<ParenExpr>();
            p->inner = std::move(inner);
            out = std::move(p);
        } else if (atSym("{")) {
            return parseTable(out);
        } else {
            return false;
        }
        return parseSuffixChain(out);
    }

    bool parseSuffixChain(ExprPtr& out) {
        while (true) {
            if (atSym("[")) {
                next();
                ExprPtr idx;
                if (!parseExpr(idx)) return false;
                if (!eat("]")) return false;
                auto i = std::make_unique<IndexExpr>();
                i->obj = std::move(out);
                i->index = std::move(idx);
                i->isDot = false;
                out = std::move(i);
            } else if (atSym(".")) {
                next();
                if (peek().kind != TokenKind::Word) return false;
                auto i = std::make_unique<IndexExpr>();
                i->obj = std::move(out);
                i->name = next().text;
                i->isDot = true;
                out = std::move(i);
            } else if (atSym(":")) {
                next();
                if (peek().kind != TokenKind::Word) return false;
                std::string method = next().text;
                std::vector<ExprPtr> args;
                if (!parseArgs(args)) return false;
                auto c = std::make_unique<MethodCallExpr>();
                c->obj = std::move(out);
                c->method = method;
                c->args = std::move(args);
                out = std::move(c);
            } else if (atSym("::")) {
                next();
                skipType();
            } else if (isCallStart()) {
                std::vector<ExprPtr> args;
                if (!parseArgs(args)) return false;
                auto c = std::make_unique<CallExpr>();
                c->func = std::move(out);
                c->args = std::move(args);
                out = std::move(c);
            } else {
                break;
            }
        }
        return true;
    }

    bool isCallStart() const {
        const Token& t = peek();
        if (t.kind == TokenKind::Symbol) {
            return t.text == "(" || t.text == "{";
        }
        return t.kind == TokenKind::String || t.kind == TokenKind::LongString;
    }

    bool parseArgs(std::vector<ExprPtr>& args) {
        if (atSym("(")) {
            next();
            if (atSym(")")) {
                next();
                return true;
            }
            if (!parseExprList(args)) return false;
            if (!eat(")")) return false;
            return true;
        }
        if (atSym("{")) {
            ExprPtr table;
            if (!parseTable(table)) return false;
            args.push_back(std::move(table));
            return true;
        }
        const Token& t = peek();
        if (t.kind == TokenKind::String || t.kind == TokenKind::LongString) {
            next();
            args.push_back(std::make_unique<StringExpr>(t.text));
            return true;
        }
        return false;
    }

    bool parseTable(ExprPtr& out) {
        next();
        auto tbl = std::make_unique<TableExpr>();
        while (!atSym("}")) {
            if (peek().kind == TokenKind::Eof) return false;
            TableExpr::Field field;
            if (atSym("[")) {
                next();
                if (!parseExpr(field.keyExpr)) return false;
                if (!eat("]")) return false;
                if (!eat("=")) return false;
                if (!parseExpr(field.value)) return false;
            } else if (peek().kind == TokenKind::Word && peek(1).kind == TokenKind::Symbol &&
                       peek(1).text == "=") {
                field.keyName = next().text;
                next();
                if (!parseExpr(field.value)) return false;
            } else {
                if (!parseExpr(field.value)) return false;
                field.isList = true;
            }
            tbl->fields.push_back(std::move(field));
            if (atSym(",") || atSym(";")) {
                next();
                continue;
            }
            if (atSym("}")) break;
            return false;
        }
        next();
        out = std::move(tbl);
        return true;
    }

    bool parseFunctionExpr(ExprPtr& out) {
        auto f = std::make_unique<FunctionExpr>();
        if (!parseFuncSignature(f->params, f->vararg)) return false;
        if (!parseBlock(f->body)) return false;
        if (!eat("end")) return false;
        out = std::move(f);
        return true;
    }

    bool parseExprList(std::vector<ExprPtr>& out) {
        ExprPtr e;
        if (!parseExpr(e)) return false;
        out.push_back(std::move(e));
        while (eat(",")) {
            if (!parseExpr(e)) return false;
            out.push_back(std::move(e));
        }
        return true;
    }
};

}

ParseResult parse(const std::vector<Token>& tokens) {
    Parser parser(tokens);
    return parser.run();
}

}
