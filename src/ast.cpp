#include "lunify/ast.hpp"

namespace lunify {

namespace {

template <typename T>
ExprPtr cloneLiteral(const Expr* e) {
    const auto* src = static_cast<const T*>(e);
    return std::make_unique<T>(*src);
}

ExprPtr cloneExprImpl(const Expr* e) {
    switch (e->kind) {
        case ExprKind::Nil:
            return std::make_unique<NilExpr>();
        case ExprKind::Bool:
            return cloneLiteral<BoolExpr>(e);
        case ExprKind::Number:
            return cloneLiteral<NumberExpr>(e);
        case ExprKind::String:
            return cloneLiteral<StringExpr>(e);
        case ExprKind::Vararg:
            return std::make_unique<VarargExpr>();
        case ExprKind::Name: {
            const auto* n = static_cast<const NameExpr*>(e);
            auto out = std::make_unique<NameExpr>(n->name);
            out->symbol = n->symbol;
            return out;
        }
        case ExprKind::Unary: {
            const auto* u = static_cast<const UnaryExpr*>(e);
            auto out = std::make_unique<UnaryExpr>();
            out->op = u->op;
            out->operand = cloneExprImpl(u->operand.get());
            return out;
        }
        case ExprKind::Binary: {
            const auto* b = static_cast<const BinaryExpr*>(e);
            auto out = std::make_unique<BinaryExpr>();
            out->op = b->op;
            out->lhs = cloneExprImpl(b->lhs.get());
            out->rhs = cloneExprImpl(b->rhs.get());
            return out;
        }
        case ExprKind::Index: {
            const auto* i = static_cast<const IndexExpr*>(e);
            auto out = std::make_unique<IndexExpr>();
            out->isDot = i->isDot;
            out->name = i->name;
            out->obj = cloneExprImpl(i->obj.get());
            if (i->index) out->index = cloneExprImpl(i->index.get());
            return out;
        }
        case ExprKind::Call: {
            const auto* c = static_cast<const CallExpr*>(e);
            auto out = std::make_unique<CallExpr>();
            out->func = cloneExprImpl(c->func.get());
            for (const auto& a : c->args) {
                out->args.push_back(cloneExprImpl(a.get()));
            }
            return out;
        }
        case ExprKind::MethodCall: {
            const auto* c = static_cast<const MethodCallExpr*>(e);
            auto out = std::make_unique<MethodCallExpr>();
            out->method = c->method;
            out->obj = cloneExprImpl(c->obj.get());
            for (const auto& a : c->args) {
                out->args.push_back(cloneExprImpl(a.get()));
            }
            return out;
        }
        case ExprKind::Table: {
            const auto* t = static_cast<const TableExpr*>(e);
            auto out = std::make_unique<TableExpr>();
            for (const auto& f : t->fields) {
                TableExpr::Field nf;
                nf.isList = f.isList;
                nf.keyName = f.keyName;
                if (f.keyExpr) nf.keyExpr = cloneExprImpl(f.keyExpr.get());
                nf.value = cloneExprImpl(f.value.get());
                out->fields.push_back(std::move(nf));
            }
            return out;
        }
        case ExprKind::Function: {
            const auto* f = static_cast<const FunctionExpr*>(e);
            auto out = std::make_unique<FunctionExpr>();
            out->params = f->params;
            out->vararg = f->vararg;
            out->paramSyms = f->paramSyms;
            return out;
        }
        case ExprKind::Paren: {
            const auto* p = static_cast<const ParenExpr*>(e);
            auto out = std::make_unique<ParenExpr>();
            out->inner = cloneExprImpl(p->inner.get());
            return out;
        }
    }
    return nullptr;
}

}

ExprPtr cloneExpr(const Expr* expr) {
    return cloneExprImpl(expr);
}

}
