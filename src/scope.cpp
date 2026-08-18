#include "lunify/scope.hpp"

#include "lunify/lexer.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace lunify {

namespace {

struct Scope {
    Scope* parent = nullptr;
    std::unordered_map<std::string, Symbol*> locals;
};

Symbol* findLocal(Scope* scope, const std::string& name) {
    for (Scope* s = scope; s; s = s->parent) {
        auto it = s->locals.find(name);
        if (it != s->locals.end()) return it->second;
    }
    return nullptr;
}

class Resolver {
public:
    ScopeResult result;

    void resolveBlock(std::vector<StmtPtr>& stmts, Scope* scope) {
        for (auto& s : stmts) {
            resolveStmt(s, scope);
        }
    }

    Symbol* declare(Scope* scope, std::string* nameRef) {
        auto sym = std::make_unique<Symbol>();
        sym->name = *nameRef;
        Symbol* raw = sym.get();
        scope->locals[*nameRef] = raw;
        result.declSites.emplace_back(nameRef, raw);
        result.symbols.push_back(std::move(sym));
        return raw;
    }

    void resolveStmt(StmtPtr& stmt, Scope* scope) {
        switch (stmt->kind) {
            case StmtKind::Empty:
            case StmtKind::Break:
            case StmtKind::Continue:
            case StmtKind::Goto:
            case StmtKind::Label:
                break;
            case StmtKind::Local: {
                auto* l = static_cast<LocalStmt*>(stmt.get());
                for (auto& v : l->values) {
                    resolveExpr(v, scope, false);
                }
                if (l->names.size() == 1 && l->values.size() == 1 && isLiteral(l->values[0])) {
                    Symbol* sym = declare(scope, &l->names[0]);
                    sym->declStmt = stmt.get();
                    sym->inlineValue = cloneExpr(l->values[0].get());
                    l->nameSyms.push_back(sym);
                } else {
                    for (auto& n : l->names) {
                        l->nameSyms.push_back(declare(scope, &n));
                    }
                }
                break;
            }
            case StmtKind::Assign: {
                auto* a = static_cast<AssignStmt*>(stmt.get());
                for (auto& t : a->targets) {
                    resolveExpr(t, scope, true);
                }
                for (auto& v : a->values) {
                    resolveExpr(v, scope, false);
                }
                break;
            }
            case StmtKind::Call: {
                auto* c = static_cast<CallStmt*>(stmt.get());
                resolveExpr(c->call, scope, false);
                break;
            }
            case StmtKind::Do: {
                auto* s = static_cast<DoStmt*>(stmt.get());
                Scope child;
                child.parent = scope;
                resolveBlock(s->body, &child);
                break;
            }
            case StmtKind::While: {
                auto* s = static_cast<WhileStmt*>(stmt.get());
                resolveExpr(s->cond, scope, false);
                Scope child;
                child.parent = scope;
                resolveBlock(s->body, &child);
                break;
            }
            case StmtKind::Repeat: {
                auto* s = static_cast<RepeatStmt*>(stmt.get());
                Scope child;
                child.parent = scope;
                resolveBlock(s->body, &child);
                resolveExpr(s->cond, &child, false);
                break;
            }
            case StmtKind::If: {
                auto* s = static_cast<IfStmt*>(stmt.get());
                for (auto& b : s->branches) {
                    resolveExpr(b.cond, scope, false);
                    Scope child;
                    child.parent = scope;
                    resolveBlock(b.body, &child);
                }
                Scope child;
                child.parent = scope;
                resolveBlock(s->elseBody, &child);
                break;
            }
            case StmtKind::NumericFor: {
                auto* s = static_cast<NumericForStmt*>(stmt.get());
                resolveExpr(s->start, scope, false);
                resolveExpr(s->stop, scope, false);
                if (s->step) resolveExpr(s->step, scope, false);
                Scope child;
                child.parent = scope;
                s->varSym = declare(&child, &s->var);
                resolveBlock(s->body, &child);
                break;
            }
            case StmtKind::GenericFor: {
                auto* s = static_cast<GenericForStmt*>(stmt.get());
                for (auto& e : s->exprs) {
                    resolveExpr(e, scope, false);
                }
                Scope child;
                child.parent = scope;
                for (auto& v : s->vars) {
                    s->varSyms.push_back(declare(&child, &v));
                }
                resolveBlock(s->body, &child);
                break;
            }
            case StmtKind::FuncDecl: {
                auto* f = static_cast<FuncDeclStmt*>(stmt.get());
                if (f->isLocal) {
                    if (f->target->kind == ExprKind::Name) {
                        auto* n = static_cast<NameExpr*>(f->target.get());
                        f->nameSym = declare(scope, &n->name);
                    }
                } else {
                    resolveExpr(f->target, scope, true);
                }
                Scope child;
                child.parent = scope;
                resolveFuncBody(f->params, f->vararg, f->body, f->paramSyms, &child);
                break;
            }
            case StmtKind::Return: {
                auto* s = static_cast<ReturnStmt*>(stmt.get());
                for (auto& v : s->values) {
                    resolveExpr(v, scope, false);
                }
                break;
            }
        }
    }

    void resolveFuncBody(std::vector<std::string>& params, bool vararg,
                         std::vector<StmtPtr>& body, std::vector<Symbol*>& paramSyms,
                         Scope* scope) {
        (void)vararg;
        for (auto& p : params) {
            Symbol* sym = declare(scope, &p);
            sym->isParam = true;
            paramSyms.push_back(sym);
        }
        resolveBlock(body, scope);
    }

    void resolveExpr(ExprPtr& e, Scope* scope, bool asTarget) {
        switch (e->kind) {
            case ExprKind::Name: {
                auto* n = static_cast<NameExpr*>(e.get());
                Symbol* sym = findLocal(scope, n->name);
                if (sym) {
                    n->symbol = sym;
                    sym->used = true;
                    ++sym->refCount;
                    if (asTarget) sym->usedAsWrite = true;
                } else {
                    GlobalInfo& g = result.globals[n->name];
                    if (asTarget) {
                        g.written = true;
                    } else {
                        g.read = true;
                    }
                }
                break;
            }
            case ExprKind::Unary: {
                auto* u = static_cast<UnaryExpr*>(e.get());
                resolveExpr(u->operand, scope, false);
                break;
            }
            case ExprKind::Binary: {
                auto* b = static_cast<BinaryExpr*>(e.get());
                resolveExpr(b->lhs, scope, false);
                resolveExpr(b->rhs, scope, false);
                break;
            }
            case ExprKind::Index: {
                auto* i = static_cast<IndexExpr*>(e.get());
                resolveExpr(i->obj, scope, false);
                if (i->index) resolveExpr(i->index, scope, false);
                break;
            }
            case ExprKind::Call: {
                auto* c = static_cast<CallExpr*>(e.get());
                resolveExpr(c->func, scope, false);
                for (auto& a : c->args) {
                    resolveExpr(a, scope, false);
                }
                break;
            }
            case ExprKind::MethodCall: {
                auto* c = static_cast<MethodCallExpr*>(e.get());
                resolveExpr(c->obj, scope, false);
                for (auto& a : c->args) {
                    resolveExpr(a, scope, false);
                }
                break;
            }
            case ExprKind::Table: {
                auto* t = static_cast<TableExpr*>(e.get());
                for (auto& f : t->fields) {
                    if (f.keyExpr) resolveExpr(f.keyExpr, scope, false);
                    resolveExpr(f.value, scope, false);
                }
                break;
            }
            case ExprKind::Function: {
                auto* f = static_cast<FunctionExpr*>(e.get());
                Scope child;
                child.parent = scope;
                resolveFuncBody(f->params, f->vararg, f->body, f->paramSyms, &child);
                break;
            }
            case ExprKind::Paren: {
                auto* p = static_cast<ParenExpr*>(e.get());
                resolveExpr(p->inner, scope, false);
                break;
            }
            default:
                break;
        }
    }

    static bool isLiteral(const ExprPtr& e) {
        return e->kind == ExprKind::Nil || e->kind == ExprKind::Bool ||
               e->kind == ExprKind::Number || e->kind == ExprKind::String;
    }
};

std::string nextName(std::size_t& counter) {
    std::string s;
    std::size_t n = counter++;
    do {
        s.insert(s.begin(), static_cast<char>('a' + (n % 26)));
        n = n / 26 - 1;
    } while (n != static_cast<std::size_t>(-1));
    return s;
}

}

ScopeResult analyzeScopes(std::vector<StmtPtr>& block) {
    Resolver resolver;
    Scope root;
    resolver.resolveBlock(block, &root);
    return std::move(resolver.result);
}

void renameVariables(std::vector<StmtPtr>& block, ScopeResult& scopes, const Options& opts) {
    (void)block;
    std::unordered_set<std::string> keywords;
    for (std::size_t i = 0; i < 26; ++i) {
        std::string w(1, static_cast<char>('a' + i));
        if (isKeyword(w)) keywords.insert(w);
    }
    std::string prefix = opts.renamePrefix;

    std::unordered_set<std::string> globalNames;
    for (const auto& kv : scopes.globals) {
        globalNames.insert(kv.first);
    }

    std::unordered_set<std::string> keepSet(opts.keep.begin(), opts.keep.end());
    std::unordered_set<std::string> reservedLocal;
    for (const auto& g : globalNames) reservedLocal.insert(g);
    for (const auto& k : keywords) reservedLocal.insert(k);
    for (const auto& k : keepSet) reservedLocal.insert(k);
    reservedLocal.insert("_D");

    std::vector<Symbol*> ordered;
    for (auto& s : scopes.symbols) {
        if (!s->noRename && !keepSet.count(s->name)) {
            ordered.push_back(s.get());
        }
    }
    std::stable_sort(ordered.begin(), ordered.end(),
                     [](const Symbol* a, const Symbol* b) { return a->refCount > b->refCount; });

    std::size_t counter = 0;
    for (Symbol* sym : ordered) {
        std::string candidate;
        do {
            candidate = prefix.empty() ? nextName(counter) : prefix + std::to_string(counter++);
        } while (reservedLocal.count(candidate));
        sym->renamed = candidate;
        sym->renamed_ = true;
    }

    if (opts.renameGlobals) {
        std::unordered_set<std::string> reservedGlobal;
        for (auto& s : scopes.symbols) {
            reservedGlobal.insert(s->name);
            if (s->renamed_) reservedGlobal.insert(s->renamed);
        }
        for (const auto& k : keywords) reservedGlobal.insert(k);
        for (const auto& k : keepSet) reservedGlobal.insert(k);
        reservedGlobal.insert("_D");

        std::vector<std::pair<std::string, GlobalInfo>> candidates;
        for (const auto& kv : scopes.globals) {
            if (kv.second.read && kv.second.written && !keepSet.count(kv.first)) {
                candidates.push_back(kv);
            }
        }
        std::size_t gCounter = 0;
        for (const auto& kv : candidates) {
            std::string candidate;
            do {
                candidate = prefix.empty() ? nextName(gCounter) : prefix + std::to_string(gCounter++);
            } while (reservedGlobal.count(candidate));
            reservedGlobal.insert(candidate);
            scopes.globalRenames[kv.first] = candidate;
        }
    }

    for (const auto& ds : scopes.declSites) {
        if (ds.second->noRename) continue;
        if (ds.second->renamed_) {
            *ds.first = ds.second->renamed;
        }
    }
}

}
