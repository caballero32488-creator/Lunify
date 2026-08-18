#include "lunify/passes.hpp"

#include "lunify/scope.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_set>
#include <vector>

namespace lunify {

namespace {

std::string formatNumber(double v) {
    if (std::isnan(v) || std::isinf(v)) return "";
    if (v == 0.0) return "0";
    if (v == std::floor(v) && std::fabs(v) < 1e15) {
        return std::to_string(static_cast<long long>(v));
    }
    char buf[64];
    for (int prec = 15; prec <= 17; ++prec) {
        std::snprintf(buf, sizeof buf, "%.*g", prec, v);
        double back = std::strtod(buf, nullptr);
        if (back == v) return buf;
    }
    return buf;
}

bool isNumberLit(const ExprPtr& e) {
    return e->kind == ExprKind::Number;
}

bool isStringLit(const ExprPtr& e) {
    return e->kind == ExprKind::String;
}

bool isTruthyLiteral(const ExprPtr& e) {
    if (e->kind == ExprKind::Bool) return static_cast<BoolExpr*>(e.get())->value;
    return false;
}

bool isFalseyLiteral(const ExprPtr& e) {
    return e->kind == ExprKind::Nil ||
           (e->kind == ExprKind::Bool && !static_cast<BoolExpr*>(e.get())->value);
}

double litNumber(const ExprPtr& e) {
    return std::strtod(static_cast<NumberExpr*>(e.get())->text.c_str(), nullptr);
}

bool foldNumber(const std::string& op, double lhs, double rhs, std::string& out) {
    double r = 0.0;
    if (op == "+") r = lhs + rhs;
    else if (op == "-") r = lhs - rhs;
    else if (op == "*") r = lhs * rhs;
    else if (op == "/") r = lhs / rhs;
    else if (op == "//") r = std::floor(lhs / rhs);
    else if (op == "%") r = std::fmod(lhs, rhs);
    else if (op == "^") r = std::pow(lhs, rhs);
    else return false;
    out = formatNumber(r);
    return !out.empty();
}

bool foldStringOp(const std::string& op, const std::string& lhs, const std::string& rhs,
                  std::string& out) {
    if (op != "..") return false;
    if (lhs.size() < 2 || rhs.size() < 2) return false;
    std::string a = lhs.substr(1, lhs.size() - 2);
    std::string b = rhs.substr(1, rhs.size() - 2);
    std::string result = a + b;
    std::string escaped;
    for (char c : result) {
        if (c == '"' || c == '\\') {
            escaped.push_back('\\');
        }
        escaped.push_back(c);
    }
    out = "\"" + escaped + "\"";
    return true;
}

bool foldComparison(const std::string& op, const ExprPtr& lhs, const ExprPtr& rhs,
                    std::string& out) {
    bool result = false;
    if (isNumberLit(lhs) && isNumberLit(rhs)) {
        double a = litNumber(lhs);
        double b = litNumber(rhs);
        if (op == "==") result = a == b;
        else if (op == "~=") result = a != b;
        else if (op == "<") result = a < b;
        else if (op == "<=") result = a <= b;
        else if (op == ">") result = a > b;
        else if (op == ">=") result = a >= b;
        else return false;
    } else if (isStringLit(lhs) && isStringLit(rhs)) {
        std::string a = static_cast<StringExpr*>(lhs.get())->text;
        std::string b = static_cast<StringExpr*>(rhs.get())->text;
        if (op == "==") result = a == b;
        else if (op == "~=") result = a != b;
        else if (op == "<") result = a < b;
        else if (op == "<=") result = a <= b;
        else if (op == ">") result = a > b;
        else if (op == ">=") result = a >= b;
        else return false;
    } else {
        return false;
    }
    out = result ? "true" : "false";
    return true;
}

void foldExpr(ExprPtr& e);
void foldStmt(StmtPtr& s);

void foldExpr(ExprPtr& e) {
    switch (e->kind) {
        case ExprKind::Unary: {
            auto* u = static_cast<UnaryExpr*>(e.get());
            foldExpr(u->operand);
            ExprPtr& o = u->operand;
            if (o->kind == ExprKind::Number && u->op == "-") {
                double v = litNumber(o);
                std::string s = formatNumber(-v);
                if (!s.empty()) e = std::make_unique<NumberExpr>(s);
            } else if (o->kind == ExprKind::String && u->op == "#") {
                std::string t = static_cast<StringExpr*>(o.get())->text;
                if (t.size() >= 2) {
                    e = std::make_unique<NumberExpr>(std::to_string(t.size() - 2));
                }
            } else if (u->op == "not") {
                if (o->kind == ExprKind::Bool) {
                    bool v = static_cast<BoolExpr*>(o.get())->value;
                    e = std::make_unique<BoolExpr>(!v);
                } else if (o->kind == ExprKind::Nil) {
                    e = std::make_unique<BoolExpr>(true);
                }
            }
            break;
        }
        case ExprKind::Binary: {
            auto* b = static_cast<BinaryExpr*>(e.get());
            foldExpr(b->lhs);
            foldExpr(b->rhs);
            ExprPtr& l = b->lhs;
            ExprPtr& r = b->rhs;
            if (b->op == "and") {
                if (isFalseyLiteral(l)) {
                    e = std::move(l);
                } else if (l->kind == ExprKind::Bool && static_cast<BoolExpr*>(l.get())->value) {
                    e = std::move(r);
                }
            } else if (b->op == "or") {
                if (l->kind == ExprKind::Bool && static_cast<BoolExpr*>(l.get())->value) {
                    e = std::move(l);
                } else if (isFalseyLiteral(l)) {
                    e = std::move(r);
                }
            } else if (isNumberLit(l) && isNumberLit(r)) {
                std::string s;
                if (foldNumber(b->op, litNumber(l), litNumber(r), s)) {
                    e = std::make_unique<NumberExpr>(s);
                }
            } else if (isStringLit(l) && isStringLit(r)) {
                std::string s;
                if (foldStringOp(b->op, static_cast<StringExpr*>(l.get())->text,
                                 static_cast<StringExpr*>(r.get())->text, s)) {
                    e = std::make_unique<StringExpr>(s);
                }
            } else if ((isNumberLit(l) || isStringLit(l)) &&
                       (isNumberLit(r) || isStringLit(r))) {
                std::string s;
                if (foldComparison(b->op, l, r, s)) {
                    e = std::make_unique<BoolExpr>(s == "true");
                }
            }
            break;
        }
        case ExprKind::Function: {
            auto* f = static_cast<FunctionExpr*>(e.get());
            for (auto& b : f->body) foldStmt(b);
            break;
        }
        case ExprKind::Index: {
            auto* i = static_cast<IndexExpr*>(e.get());
            foldExpr(i->obj);
            if (i->index) foldExpr(i->index);
            break;
        }
        case ExprKind::Call: {
            auto* c = static_cast<CallExpr*>(e.get());
            foldExpr(c->func);
            for (auto& a : c->args) foldExpr(a);
            break;
        }
        case ExprKind::MethodCall: {
            auto* c = static_cast<MethodCallExpr*>(e.get());
            foldExpr(c->obj);
            for (auto& a : c->args) foldExpr(a);
            break;
        }
        case ExprKind::Table: {
            auto* t = static_cast<TableExpr*>(e.get());
            for (auto& f : t->fields) {
                if (f.keyExpr) foldExpr(f.keyExpr);
                foldExpr(f.value);
            }
            break;
        }
        case ExprKind::Paren: {
            auto* p = static_cast<ParenExpr*>(e.get());
            foldExpr(p->inner);
            break;
        }
        default:
            break;
    }
}

void foldStmt(StmtPtr& s) {
    switch (s->kind) {
        case StmtKind::Local: {
            auto* l = static_cast<LocalStmt*>(s.get());
            for (auto& v : l->values) foldExpr(v);
            break;
        }
        case StmtKind::Assign: {
            auto* a = static_cast<AssignStmt*>(s.get());
            for (auto& t : a->targets) foldExpr(t);
            for (auto& v : a->values) foldExpr(v);
            break;
        }
        case StmtKind::Call: {
            auto* c = static_cast<CallStmt*>(s.get());
            foldExpr(c->call);
            break;
        }
        case StmtKind::Do: {
            auto* d = static_cast<DoStmt*>(s.get());
            for (auto& b : d->body) foldStmt(b);
            break;
        }
        case StmtKind::While: {
            auto* w = static_cast<WhileStmt*>(s.get());
            foldExpr(w->cond);
            for (auto& b : w->body) foldStmt(b);
            break;
        }
        case StmtKind::Repeat: {
            auto* r = static_cast<RepeatStmt*>(s.get());
            for (auto& b : r->body) foldStmt(b);
            foldExpr(r->cond);
            break;
        }
        case StmtKind::If: {
            auto* i = static_cast<IfStmt*>(s.get());
            for (auto& b : i->branches) {
                foldExpr(b.cond);
                for (auto& st : b.body) foldStmt(st);
            }
            for (auto& st : i->elseBody) foldStmt(st);
            break;
        }
        case StmtKind::NumericFor: {
            auto* f = static_cast<NumericForStmt*>(s.get());
            foldExpr(f->start);
            foldExpr(f->stop);
            if (f->step) foldExpr(f->step);
            for (auto& b : f->body) foldStmt(b);
            break;
        }
        case StmtKind::GenericFor: {
            auto* f = static_cast<GenericForStmt*>(s.get());
            for (auto& e : f->exprs) foldExpr(e);
            for (auto& b : f->body) foldStmt(b);
            break;
        }
        case StmtKind::FuncDecl: {
            auto* f = static_cast<FuncDeclStmt*>(s.get());
            foldExpr(f->target);
            for (auto& b : f->body) foldStmt(b);
            break;
        }
        case StmtKind::Return: {
            auto* r = static_cast<ReturnStmt*>(s.get());
            for (auto& v : r->values) foldExpr(v);
            break;
        }
        default:
            break;
    }
}

bool isPureExpr(const ExprPtr& e) {
    switch (e->kind) {
        case ExprKind::Nil:
        case ExprKind::Bool:
        case ExprKind::Number:
        case ExprKind::String:
        case ExprKind::Vararg:
        case ExprKind::Name:
        case ExprKind::Function:
            return true;
        case ExprKind::Unary: {
            auto* u = static_cast<UnaryExpr*>(e.get());
            return isPureExpr(u->operand);
        }
        case ExprKind::Binary: {
            auto* b = static_cast<BinaryExpr*>(e.get());
            return isPureExpr(b->lhs) && isPureExpr(b->rhs);
        }
        case ExprKind::Table: {
            auto* t = static_cast<TableExpr*>(e.get());
            for (const auto& f : t->fields) {
                if (f.keyExpr && !isPureExpr(f.keyExpr)) return false;
                if (!isPureExpr(f.value)) return false;
            }
            return true;
        }
        case ExprKind::Paren: {
            auto* p = static_cast<ParenExpr*>(e.get());
            return isPureExpr(p->inner);
        }
        default:
            return false;
    }
}

void dceStmt(StmtPtr& s);
void dceBlock(std::vector<StmtPtr>& block);

void dceExpr(ExprPtr& e) {
    switch (e->kind) {
        case ExprKind::Unary: {
            auto* u = static_cast<UnaryExpr*>(e.get());
            dceExpr(u->operand);
            break;
        }
        case ExprKind::Binary: {
            auto* b = static_cast<BinaryExpr*>(e.get());
            dceExpr(b->lhs);
            dceExpr(b->rhs);
            break;
        }
        case ExprKind::Function: {
            auto* f = static_cast<FunctionExpr*>(e.get());
            dceBlock(f->body);
            break;
        }
        case ExprKind::Index: {
            auto* i = static_cast<IndexExpr*>(e.get());
            dceExpr(i->obj);
            if (i->index) dceExpr(i->index);
            break;
        }
        case ExprKind::Call: {
            auto* c = static_cast<CallExpr*>(e.get());
            dceExpr(c->func);
            for (auto& a : c->args) dceExpr(a);
            break;
        }
        case ExprKind::MethodCall: {
            auto* c = static_cast<MethodCallExpr*>(e.get());
            dceExpr(c->obj);
            for (auto& a : c->args) dceExpr(a);
            break;
        }
        case ExprKind::Table: {
            auto* t = static_cast<TableExpr*>(e.get());
            for (auto& f : t->fields) {
                if (f.keyExpr) dceExpr(f.keyExpr);
                dceExpr(f.value);
            }
            break;
        }
        case ExprKind::Paren: {
            auto* p = static_cast<ParenExpr*>(e.get());
            dceExpr(p->inner);
            break;
        }
        default:
            break;
    }
}

void dceStmt(StmtPtr& s) {
    switch (s->kind) {
        case StmtKind::Local: {
            auto* l = static_cast<LocalStmt*>(s.get());
            for (auto& v : l->values) dceExpr(v);
            bool allUnused = !l->nameSyms.empty();
            for (auto* sym : l->nameSyms) {
                if (sym->used) {
                    allUnused = false;
                    break;
                }
            }
            if (allUnused) {
                bool pure = true;
                for (auto& v : l->values) {
                    if (!isPureExpr(v)) {
                        pure = false;
                        break;
                    }
                }
                if (pure) {
                    s->dead = true;
                }
            }
            break;
        }
        case StmtKind::FuncDecl: {
            auto* f = static_cast<FuncDeclStmt*>(s.get());
            dceExpr(f->target);
            dceBlock(f->body);
            if (f->isLocal && f->nameSym && !f->nameSym->used) {
                s->dead = true;
            }
            break;
        }
        case StmtKind::Assign: {
            auto* a = static_cast<AssignStmt*>(s.get());
            for (auto& t : a->targets) dceExpr(t);
            for (auto& v : a->values) dceExpr(v);
            break;
        }
        case StmtKind::Call: {
            auto* c = static_cast<CallStmt*>(s.get());
            dceExpr(c->call);
            break;
        }
        case StmtKind::Do: {
            auto* d = static_cast<DoStmt*>(s.get());
            dceBlock(d->body);
            break;
        }
        case StmtKind::While: {
            auto* w = static_cast<WhileStmt*>(s.get());
            dceExpr(w->cond);
            dceBlock(w->body);
            break;
        }
        case StmtKind::Repeat: {
            auto* r = static_cast<RepeatStmt*>(s.get());
            dceBlock(r->body);
            dceExpr(r->cond);
            break;
        }
        case StmtKind::If: {
            auto* i = static_cast<IfStmt*>(s.get());
            for (auto& b : i->branches) {
                dceExpr(b.cond);
                dceBlock(b.body);
            }
            dceBlock(i->elseBody);
            if (!i->branches.empty() && isTruthyLiteral(i->branches[0].cond)) {
                auto d = std::make_unique<DoStmt>();
                d->body = std::move(i->branches[0].body);
                s = std::move(d);
            } else if (!i->branches.empty() && isFalseyLiteral(i->branches[0].cond)) {
                auto d = std::make_unique<DoStmt>();
                d->body = std::move(i->elseBody);
                s = std::move(d);
            }
            break;
        }
        case StmtKind::NumericFor: {
            auto* f = static_cast<NumericForStmt*>(s.get());
            dceExpr(f->start);
            dceExpr(f->stop);
            if (f->step) dceExpr(f->step);
            dceBlock(f->body);
            break;
        }
        case StmtKind::GenericFor: {
            auto* f = static_cast<GenericForStmt*>(s.get());
            for (auto& e : f->exprs) dceExpr(e);
            dceBlock(f->body);
            break;
        }
        case StmtKind::Return: {
            auto* r = static_cast<ReturnStmt*>(s.get());
            for (auto& v : r->values) dceExpr(v);
            break;
        }
        default:
            break;
    }
}

bool isTerminator(const StmtPtr& s) {
    return s->kind == StmtKind::Return || s->kind == StmtKind::Break ||
           s->kind == StmtKind::Continue;
}

void dceBlock(std::vector<StmtPtr>& block) {
    bool dead = false;
    for (auto& s : block) {
        dceStmt(s);
        if (s->dead) continue;
        if (dead) {
            if (s->kind == StmtKind::Label) {
                dead = false;
            } else {
                s->dead = true;
                continue;
            }
        }
        if (isTerminator(s)) dead = true;
    }
}

void deadParamsExpr(ExprPtr& e);
void deadParamsBlock(std::vector<StmtPtr>& block);

void applyDeadParams(std::vector<std::string>& params, std::vector<Symbol*>& paramSyms,
                     bool isMethod) {
    if (paramSyms.size() != params.size()) return;
    size_t keep = isMethod ? 1 : 0;
    while (params.size() > keep) {
        Symbol* last = paramSyms.back();
        if (!last->used && !last->noRename) {
            last->noRename = true;
            params.pop_back();
            paramSyms.pop_back();
        } else {
            break;
        }
    }
    for (size_t i = keep; i < params.size(); ++i) {
        Symbol* sym = paramSyms[i];
        if (!sym->used && !sym->noRename) {
            params[i] = "_";
            sym->noRename = true;
        }
    }
}

void deadParamsExpr(ExprPtr& e) {
    switch (e->kind) {
        case ExprKind::Unary: {
            auto* u = static_cast<UnaryExpr*>(e.get());
            deadParamsExpr(u->operand);
            break;
        }
        case ExprKind::Binary: {
            auto* b = static_cast<BinaryExpr*>(e.get());
            deadParamsExpr(b->lhs);
            deadParamsExpr(b->rhs);
            break;
        }
        case ExprKind::Function: {
            auto* f = static_cast<FunctionExpr*>(e.get());
            applyDeadParams(f->params, f->paramSyms, false);
            deadParamsBlock(f->body);
            break;
        }
        case ExprKind::Index: {
            auto* i = static_cast<IndexExpr*>(e.get());
            deadParamsExpr(i->obj);
            if (i->index) deadParamsExpr(i->index);
            break;
        }
        case ExprKind::Call: {
            auto* c = static_cast<CallExpr*>(e.get());
            deadParamsExpr(c->func);
            for (auto& a : c->args) deadParamsExpr(a);
            break;
        }
        case ExprKind::MethodCall: {
            auto* c = static_cast<MethodCallExpr*>(e.get());
            deadParamsExpr(c->obj);
            for (auto& a : c->args) deadParamsExpr(a);
            break;
        }
        case ExprKind::Table: {
            auto* t = static_cast<TableExpr*>(e.get());
            for (auto& f : t->fields) {
                if (f.keyExpr) deadParamsExpr(f.keyExpr);
                deadParamsExpr(f.value);
            }
            break;
        }
        case ExprKind::Paren: {
            auto* p = static_cast<ParenExpr*>(e.get());
            deadParamsExpr(p->inner);
            break;
        }
        default:
            break;
    }
}

void deadParamsBlock(std::vector<StmtPtr>& block) {
    for (auto& s : block) {
        switch (s->kind) {
            case StmtKind::Local: {
                auto* l = static_cast<LocalStmt*>(s.get());
                for (auto& v : l->values) deadParamsExpr(v);
                break;
            }
            case StmtKind::Assign: {
                auto* a = static_cast<AssignStmt*>(s.get());
                for (auto& t : a->targets) deadParamsExpr(t);
                for (auto& v : a->values) deadParamsExpr(v);
                break;
            }
            case StmtKind::Call: {
                auto* c = static_cast<CallStmt*>(s.get());
                deadParamsExpr(c->call);
                break;
            }
            case StmtKind::Do: {
                auto* d = static_cast<DoStmt*>(s.get());
                deadParamsBlock(d->body);
                break;
            }
            case StmtKind::While: {
                auto* w = static_cast<WhileStmt*>(s.get());
                deadParamsExpr(w->cond);
                deadParamsBlock(w->body);
                break;
            }
            case StmtKind::Repeat: {
                auto* r = static_cast<RepeatStmt*>(s.get());
                deadParamsBlock(r->body);
                deadParamsExpr(r->cond);
                break;
            }
            case StmtKind::If: {
                auto* i = static_cast<IfStmt*>(s.get());
                for (auto& b : i->branches) {
                    deadParamsExpr(b.cond);
                    deadParamsBlock(b.body);
                }
                deadParamsBlock(i->elseBody);
                break;
            }
            case StmtKind::NumericFor: {
                auto* f = static_cast<NumericForStmt*>(s.get());
                deadParamsExpr(f->start);
                deadParamsExpr(f->stop);
                if (f->step) deadParamsExpr(f->step);
                deadParamsBlock(f->body);
                break;
            }
            case StmtKind::GenericFor: {
                auto* f = static_cast<GenericForStmt*>(s.get());
                for (auto& e : f->exprs) deadParamsExpr(e);
                deadParamsBlock(f->body);
                break;
            }
            case StmtKind::FuncDecl: {
                auto* f = static_cast<FuncDeclStmt*>(s.get());
                applyDeadParams(f->params, f->paramSyms, f->isMethod);
                deadParamsExpr(f->target);
                deadParamsBlock(f->body);
                break;
            }
            case StmtKind::Return: {
                auto* r = static_cast<ReturnStmt*>(s.get());
                for (auto& v : r->values) deadParamsExpr(v);
                break;
            }
            default:
                break;
        }
    }
}

}

void constantFold(std::vector<StmtPtr>& block) {
    for (auto& s : block) {
        foldStmt(s);
    }
}

void deadCodeElim(std::vector<StmtPtr>& block) {
    dceBlock(block);
}

void inlineLocals(std::vector<StmtPtr>& block, ScopeResult& scopes, const Options& opts) {
    (void)block;
    std::unordered_set<std::string> keepSet(opts.keep.begin(), opts.keep.end());
    for (auto& symPtr : scopes.symbols) {
        Symbol* sym = symPtr.get();
        if (!sym->inlineValue) continue;
        if (keepSet.count(sym->name)) continue;
        if (sym->refCount != 1) continue;
        if (sym->usedAsWrite) continue;
        if (sym->declStmt && sym->declStmt->kind == StmtKind::Local) {
            auto* l = static_cast<LocalStmt*>(sym->declStmt);
            if (l->names.size() == 1) {
                sym->declStmt->dead = true;
            }
        }
    }
}

void deadParams(std::vector<StmtPtr>& block) {
    deadParamsBlock(block);
}

}
