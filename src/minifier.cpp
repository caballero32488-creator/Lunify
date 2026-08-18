#include "lunify/minifier.hpp"

#include "lunify/lexer.hpp"
#include "lunify/options.hpp"
#include "lunify/parser.hpp"
#include "lunify/passes.hpp"
#include "lunify/scope.hpp"

#include <string>
#include <vector>

namespace lunify {

namespace {

bool isWordKind(TokenKind k) {
    return k == TokenKind::Word || k == TokenKind::Number;
}

bool needsSpace(TokenKind ka, const std::string& a, TokenKind kb, const std::string& b) {
    if (isWordKind(ka) && isWordKind(kb)) return true;
    if (ka == TokenKind::Number && b == ".") return true;
    if (a.empty() || b.empty()) return false;
    char x = a.back();
    char y = b.front();
    if (x == '-' && y == '-') return true;
    if (x == '/' && y == '/') return true;
    if (x == '[' && (y == '[' || y == '=')) return true;
    if (x == '.' && y == '.') return true;
    if (x == '=' && y == '=') return true;
    if (x == '<' && (y == '<' || y == '=')) return true;
    if (x == '>' && (y == '>' || y == '=')) return true;
    if (x == '~' && y == '=') return true;
    if (x == ':' && y == ':') return true;
    return false;
}

class Emitter {
public:
    std::string out;

    void token(TokenKind k, const std::string& t) {
        if (hasPrev_ && needsSpace(prevKind_, prevText_, k, t)) {
            out.push_back(' ');
        }
        out += t;
        prevKind_ = k;
        prevText_ = t;
        hasPrev_ = true;
    }

    void word(const std::string& t) { token(TokenKind::Word, t); }
    void number(const std::string& t) { token(TokenKind::Number, t); }
    void str(const std::string& t) { token(TokenKind::String, t); }
    void sym(const std::string& t) { token(TokenKind::Symbol, t); }

private:
    bool hasPrev_ = false;
    TokenKind prevKind_ = TokenKind::Symbol;
    std::string prevText_;
};

std::string shortenNumber(const std::string& text) {
    if (text.empty()) return text;
    if (text[0] == '0' && text.size() > 1 &&
        (text[1] == 'x' || text[1] == 'X' || text[1] == 'b' || text[1] == 'B')) {
        return text;
    }
    size_t ep = text.find_first_of("eEpP");
    std::string mant = text;
    std::string exp;
    if (ep != std::string::npos) {
        mant = text.substr(0, ep);
        exp = text.substr(ep);
        if (exp.size() > 2 && exp[1] == '+') {
            exp = exp[0] + exp.substr(2);
        }
    }
    size_t dot = mant.find('.');
    if (dot != std::string::npos) {
        bool allZero = true;
        for (size_t k = dot + 1; k < mant.size(); ++k) {
            if (mant[k] != '0') {
                allZero = false;
                break;
            }
        }
        if (allZero && dot + 1 < mant.size()) {
            mant = mant.substr(0, dot);
        }
    }
    if (mant.size() > 2 && mant[0] == '0' && mant[1] == '.') {
        mant = mant.substr(1);
    }
    return mant + exp;
}

unsigned stringKey(const std::string& s) {
    unsigned h = 2166136261u;
    for (unsigned char c : s) {
        h = (h ^ c) * 16777619u;
    }
    return 1 + (h % 255);
}

void emitString(Emitter& em, const std::string& text, const Options& opts) {
    if (opts.encodeStrings) {
        unsigned key = stringKey(text);
        em.word("_D");
        em.sym("(");
        em.number(std::to_string(key));
        for (unsigned char c : text) {
            em.sym(",");
            em.number(std::to_string(static_cast<unsigned>(c) ^ key));
        }
        em.sym(")");
        return;
    }
    em.str(text);
}

void emitExpr(Emitter& em, const Expr* e, const Options& opts);

void emitExprList(Emitter& em, const std::vector<ExprPtr>& list, const Options& opts) {
    for (std::size_t i = 0; i < list.size(); ++i) {
        if (i > 0) em.sym(",");
        emitExpr(em, list[i].get(), opts);
    }
}

void emitParams(Emitter& em, const std::vector<std::string>& params, bool vararg) {
    em.sym("(");
    for (std::size_t i = 0; i < params.size(); ++i) {
        if (i > 0) em.sym(",");
        em.word(params[i]);
    }
    if (vararg) {
        if (!params.empty()) em.sym(",");
        em.sym("...");
    }
    em.sym(")");
}

void emitBlock(Emitter& em, const std::vector<StmtPtr>& stmts, const Options& opts);

void emitTable(Emitter& em, const TableExpr* t, const Options& opts) {
    em.sym("{");
    for (std::size_t i = 0; i < t->fields.size(); ++i) {
        if (i > 0) em.sym(",");
        const auto& f = t->fields[i];
        if (f.isList) {
            emitExpr(em, f.value.get(), opts);
        } else if (!f.keyName.empty()) {
            em.word(f.keyName);
            em.sym("=");
            emitExpr(em, f.value.get(), opts);
        } else {
            em.sym("[");
            emitExpr(em, f.keyExpr.get(), opts);
            em.sym("]");
            em.sym("=");
            emitExpr(em, f.value.get(), opts);
        }
    }
    em.sym("}");
}

void emitFunction(Emitter& em, const FunctionExpr* f, const Options& opts) {
    em.word("function");
    emitParams(em, f->params, f->vararg);
    emitBlock(em, f->body, opts);
    em.word("end");
}

void emitExpr(Emitter& em, const Expr* e, const Options& opts) {
    switch (e->kind) {
        case ExprKind::Nil:
            em.word("nil");
            break;
        case ExprKind::Bool:
            em.word(static_cast<const BoolExpr*>(e)->value ? "true" : "false");
            break;
        case ExprKind::Number: {
            std::string t = static_cast<const NumberExpr*>(e)->text;
            em.number(opts.shortenNumbers ? shortenNumber(t) : t);
            break;
        }
        case ExprKind::String:
            emitString(em, static_cast<const StringExpr*>(e)->text, opts);
            break;
        case ExprKind::Vararg:
            em.sym("...");
            break;
        case ExprKind::Name: {
            const auto* n = static_cast<const NameExpr*>(e);
            if (n->symbol && n->symbol->inlineValue) {
                emitExpr(em, n->symbol->inlineValue.get(), opts);
                break;
            }
            std::string name = n->name;
            if (n->symbol && n->symbol->renamed_) {
                name = n->symbol->renamed;
            }
            em.word(name);
            break;
        }
        case ExprKind::Unary: {
            const auto* u = static_cast<const UnaryExpr*>(e);
            if (u->op == "not") {
                em.word("not");
            } else {
                em.sym(u->op);
            }
            emitExpr(em, u->operand.get(), opts);
            break;
        }
        case ExprKind::Binary: {
            const auto* b = static_cast<const BinaryExpr*>(e);
            emitExpr(em, b->lhs.get(), opts);
            em.sym(b->op);
            emitExpr(em, b->rhs.get(), opts);
            break;
        }
        case ExprKind::Index: {
            const auto* i = static_cast<const IndexExpr*>(e);
            emitExpr(em, i->obj.get(), opts);
            if (i->isDot) {
                em.sym(".");
                em.word(i->name);
            } else {
                em.sym("[");
                emitExpr(em, i->index.get(), opts);
                em.sym("]");
            }
            break;
        }
        case ExprKind::Call: {
            const auto* c = static_cast<const CallExpr*>(e);
            emitExpr(em, c->func.get(), opts);
            em.sym("(");
            emitExprList(em, c->args, opts);
            em.sym(")");
            break;
        }
        case ExprKind::MethodCall: {
            const auto* c = static_cast<const MethodCallExpr*>(e);
            emitExpr(em, c->obj.get(), opts);
            em.sym(":");
            em.word(c->method);
            em.sym("(");
            emitExprList(em, c->args, opts);
            em.sym(")");
            break;
        }
        case ExprKind::Table:
            emitTable(em, static_cast<const TableExpr*>(e), opts);
            break;
        case ExprKind::Function:
            emitFunction(em, static_cast<const FunctionExpr*>(e), opts);
            break;
        case ExprKind::Paren: {
            em.sym("(");
            emitExpr(em, static_cast<const ParenExpr*>(e)->inner.get(), opts);
            em.sym(")");
            break;
        }
    }
}

bool isDebugCall(const Expr* e) {
    if (!e || e->kind != ExprKind::Call) return false;
    const Expr* func = static_cast<const CallExpr*>(e)->func.get();
    if (!func || func->kind != ExprKind::Name) return false;
    const auto* n = static_cast<const NameExpr*>(func);
    if (n->symbol) return false;
    return n->name == "print" || n->name == "warn";
}

void emitStmt(Emitter& em, const Stmt* s, const Options& opts) {
    switch (s->kind) {
        case StmtKind::Empty:
            break;
        case StmtKind::Local: {
            const auto* l = static_cast<const LocalStmt*>(s);
            em.word("local");
            for (std::size_t i = 0; i < l->names.size(); ++i) {
                if (i > 0) em.sym(",");
                em.word(l->names[i]);
            }
            if (!l->values.empty()) {
                em.sym("=");
                emitExprList(em, l->values, opts);
            }
            break;
        }
        case StmtKind::Assign: {
            const auto* a = static_cast<const AssignStmt*>(s);
            for (std::size_t i = 0; i < a->targets.size(); ++i) {
                if (i > 0) em.sym(",");
                emitExpr(em, a->targets[i].get(), opts);
            }
            em.sym("=");
            emitExprList(em, a->values, opts);
            break;
        }
        case StmtKind::Call: {
            const auto* c = static_cast<const CallStmt*>(s);
            if (opts.stripDebug && isDebugCall(c->call.get())) break;
            emitExpr(em, c->call.get(), opts);
            break;
        }
        case StmtKind::Do: {
            em.word("do");
            emitBlock(em, static_cast<const DoStmt*>(s)->body, opts);
            em.word("end");
            break;
        }
        case StmtKind::While: {
            const auto* w = static_cast<const WhileStmt*>(s);
            em.word("while");
            emitExpr(em, w->cond.get(), opts);
            em.word("do");
            emitBlock(em, w->body, opts);
            em.word("end");
            break;
        }
        case StmtKind::Repeat: {
            const auto* r = static_cast<const RepeatStmt*>(s);
            em.word("repeat");
            emitBlock(em, r->body, opts);
            em.word("until");
            emitExpr(em, r->cond.get(), opts);
            break;
        }
        case StmtKind::If: {
            const auto* f = static_cast<const IfStmt*>(s);
            for (std::size_t i = 0; i < f->branches.size(); ++i) {
                em.word(i == 0 ? "if" : "elseif");
                emitExpr(em, f->branches[i].cond.get(), opts);
                em.word("then");
                emitBlock(em, f->branches[i].body, opts);
            }
            if (!f->elseBody.empty()) {
                em.word("else");
                emitBlock(em, f->elseBody, opts);
            }
            em.word("end");
            break;
        }
        case StmtKind::NumericFor: {
            const auto* f = static_cast<const NumericForStmt*>(s);
            em.word("for");
            em.word(f->var);
            em.sym("=");
            emitExpr(em, f->start.get(), opts);
            em.sym(",");
            emitExpr(em, f->stop.get(), opts);
            if (f->step) {
                em.sym(",");
                emitExpr(em, f->step.get(), opts);
            }
            em.word("do");
            emitBlock(em, f->body, opts);
            em.word("end");
            break;
        }
        case StmtKind::GenericFor: {
            const auto* f = static_cast<const GenericForStmt*>(s);
            em.word("for");
            for (std::size_t i = 0; i < f->vars.size(); ++i) {
                if (i > 0) em.sym(",");
                em.word(f->vars[i]);
            }
            em.word("in");
            emitExprList(em, f->exprs, opts);
            em.word("do");
            emitBlock(em, f->body, opts);
            em.word("end");
            break;
        }
        case StmtKind::FuncDecl: {
            const auto* f = static_cast<const FuncDeclStmt*>(s);
            if (f->isLocal) em.word("local");
            em.word("function");
            emitExpr(em, f->target.get(), opts);
            emitParams(em, f->params, f->vararg);
            emitBlock(em, f->body, opts);
            em.word("end");
            break;
        }
        case StmtKind::Return: {
            const auto* r = static_cast<const ReturnStmt*>(s);
            em.word("return");
            if (!r->values.empty()) {
                emitExprList(em, r->values, opts);
            }
            break;
        }
        case StmtKind::Break:
            em.word("break");
            break;
        case StmtKind::Continue:
            em.word("continue");
            break;
        case StmtKind::Goto:
            em.word("goto");
            em.word(static_cast<const GotoStmt*>(s)->label);
            break;
        case StmtKind::Label: {
            em.sym("::");
            em.word(static_cast<const LabelStmt*>(s)->name);
            em.sym("::");
            break;
        }
    }
}

void emitBlock(Emitter& em, const std::vector<StmtPtr>& stmts, const Options& opts) {
    for (const auto& s : stmts) {
        if (s->dead) continue;
        if (s->kind == StmtKind::Empty) continue;
        emitStmt(em, s.get(), opts);
    }
}

void emitDecoder(Emitter& em) {
    em.word("local");
    em.word("_D");
    em.sym("=");
    em.word("function");
    em.sym("(");
    em.word("k");
    em.sym(",");
    em.sym("...");
    em.sym(")");
    em.word("local");
    em.word("s");
    em.sym("=");
    em.str("''");
    em.word("for");
    em.word("i");
    em.sym("=");
    em.number("1");
    em.sym(",");
    em.word("select");
    em.sym("(");
    em.str("'#'");
    em.sym(",");
    em.sym("...");
    em.sym(")");
    em.word("do");
    em.word("s");
    em.sym("=");
    em.word("s");
    em.sym("..");
    em.word("string");
    em.sym(".");
    em.word("char");
    em.sym("(");
    em.word("select");
    em.sym("(");
    em.word("i");
    em.sym(",");
    em.sym("...");
    em.sym(")");
    em.sym("~");
    em.word("k");
    em.sym(")");
    em.word("end");
    em.word("return");
    em.word("s");
    em.word("end");
}

std::string tokenMinify(const std::string& source, const Options& opts) {
    if (!opts.removeComments) return source;
    std::vector<Token> tokens = lex(source);
    Emitter em;
    for (const Token& t : tokens) {
        if (t.kind == TokenKind::Comment) continue;
        if (t.kind == TokenKind::Eof) break;
        if (t.kind == TokenKind::Number && opts.shortenNumbers) {
            em.number(shortenNumber(t.text));
            continue;
        }
        em.token(t.kind, t.text);
    }
    return em.out;
}

std::string finalizeOutput(std::string out, const Options& opts) {
    if (opts.finalNewline && !out.empty() && out.back() != '\n') {
        out.push_back('\n');
    }
    return out;
}

}

MinifyResult minify(const std::string& source, const Options& opts) {
    MinifyResult result;
    result.originalSize = source.size();

    bool needAst = opts.removeComments && opts.removeWhitespace &&
                   (opts.stripTypes || opts.renameLocals || opts.constantFold ||
                    opts.deadCodeElim || opts.inlineLocals || opts.deadParams ||
                    opts.renameGlobals || opts.encodeStrings);

    std::vector<Token> tokens = lex(source);
    ParseResult pr = parse(tokens);

    if (!needAst || !pr.ok) {
        if (!pr.ok) {
            result.error = pr.error;
            result.ok = false;
        }
        result.output = finalizeOutput(tokenMinify(source, opts), opts);
        result.minifiedSize = result.output.size();
        return result;
    }

    std::vector<StmtPtr> block = std::move(pr.statements);
    ScopeResult scopes = analyzeScopes(block);

    if (opts.constantFold) constantFold(block);
    if (opts.deadCodeElim) deadCodeElim(block);
    if (opts.inlineLocals) inlineLocals(block, scopes, opts);
    if (opts.deadParams) deadParams(block);
    if (opts.renameLocals || opts.renameGlobals) renameVariables(block, scopes, opts);

    Emitter em;
    if (opts.encodeStrings) emitDecoder(em);
    emitBlock(em, block, opts);
    result.output = finalizeOutput(em.out, opts);
    result.minifiedSize = result.output.size();
    return result;
}

}
