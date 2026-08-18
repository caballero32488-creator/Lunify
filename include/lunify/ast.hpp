#pragma once

#include <memory>
#include <string>
#include <vector>

namespace lunify {

class Expr;
class Stmt;
class Symbol;

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;

enum class ExprKind {
    Nil,
    Bool,
    Number,
    String,
    Vararg,
    Name,
    Function,
    Unary,
    Binary,
    Index,
    Call,
    MethodCall,
    Table,
    Paren,
};

enum class StmtKind {
    Empty,
    Local,
    Assign,
    Call,
    Do,
    While,
    Repeat,
    If,
    NumericFor,
    GenericFor,
    FuncDecl,
    Return,
    Break,
    Continue,
    Goto,
    Label,
};

class Expr {
public:
    ExprKind kind;
    bool dead = false;
    explicit Expr(ExprKind k) : kind(k) {}
    virtual ~Expr() = default;
};

class Stmt {
public:
    StmtKind kind;
    bool dead = false;
    explicit Stmt(StmtKind k) : kind(k) {}
    virtual ~Stmt() = default;
};

class NilExpr final : public Expr {
public:
    NilExpr() : Expr(ExprKind::Nil) {}
};

class BoolExpr final : public Expr {
public:
    bool value = false;
    explicit BoolExpr(bool v) : Expr(ExprKind::Bool), value(v) {}
};

class NumberExpr final : public Expr {
public:
    std::string text;
    explicit NumberExpr(std::string t) : Expr(ExprKind::Number), text(std::move(t)) {}
};

class StringExpr final : public Expr {
public:
    std::string text;
    explicit StringExpr(std::string t) : Expr(ExprKind::String), text(std::move(t)) {}
};

class VarargExpr final : public Expr {
public:
    VarargExpr() : Expr(ExprKind::Vararg) {}
};

class NameExpr final : public Expr {
public:
    std::string name;
    Symbol* symbol = nullptr;
    explicit NameExpr(std::string n) : Expr(ExprKind::Name), name(std::move(n)) {}
};

class FunctionExpr final : public Expr {
public:
    std::vector<std::string> params;
    bool vararg = false;
    std::vector<StmtPtr> body;
    std::vector<Symbol*> paramSyms;
    FunctionExpr() : Expr(ExprKind::Function) {}
};

class UnaryExpr final : public Expr {
public:
    std::string op;
    ExprPtr operand;
    UnaryExpr() : Expr(ExprKind::Unary) {}
};

class BinaryExpr final : public Expr {
public:
    std::string op;
    ExprPtr lhs;
    ExprPtr rhs;
    BinaryExpr() : Expr(ExprKind::Binary) {}
};

class IndexExpr final : public Expr {
public:
    ExprPtr obj;
    bool isDot = false;
    std::string name;
    ExprPtr index;
    IndexExpr() : Expr(ExprKind::Index) {}
};

class CallExpr final : public Expr {
public:
    ExprPtr func;
    std::vector<ExprPtr> args;
    CallExpr() : Expr(ExprKind::Call) {}
};

class MethodCallExpr final : public Expr {
public:
    ExprPtr obj;
    std::string method;
    std::vector<ExprPtr> args;
    MethodCallExpr() : Expr(ExprKind::MethodCall) {}
};

class TableExpr final : public Expr {
public:
    struct Field {
        bool isList = false;
        std::string keyName;
        ExprPtr keyExpr;
        ExprPtr value;
    };
    std::vector<Field> fields;
    TableExpr() : Expr(ExprKind::Table) {}
};

class ParenExpr final : public Expr {
public:
    ExprPtr inner;
    ParenExpr() : Expr(ExprKind::Paren) {}
};

class EmptyStmt final : public Stmt {
public:
    EmptyStmt() : Stmt(StmtKind::Empty) {}
};

class LocalStmt final : public Stmt {
public:
    std::vector<std::string> names;
    std::vector<ExprPtr> values;
    std::vector<Symbol*> nameSyms;
    LocalStmt() : Stmt(StmtKind::Local) {}
};

class AssignStmt final : public Stmt {
public:
    std::vector<ExprPtr> targets;
    std::vector<ExprPtr> values;
    AssignStmt() : Stmt(StmtKind::Assign) {}
};

class CallStmt final : public Stmt {
public:
    ExprPtr call;
    CallStmt() : Stmt(StmtKind::Call) {}
};

class DoStmt final : public Stmt {
public:
    std::vector<StmtPtr> body;
    DoStmt() : Stmt(StmtKind::Do) {}
};

class WhileStmt final : public Stmt {
public:
    ExprPtr cond;
    std::vector<StmtPtr> body;
    WhileStmt() : Stmt(StmtKind::While) {}
};

class RepeatStmt final : public Stmt {
public:
    std::vector<StmtPtr> body;
    ExprPtr cond;
    RepeatStmt() : Stmt(StmtKind::Repeat) {}
};

class IfStmt final : public Stmt {
public:
    struct Branch {
        ExprPtr cond;
        std::vector<StmtPtr> body;
    };
    std::vector<Branch> branches;
    std::vector<StmtPtr> elseBody;
    IfStmt() : Stmt(StmtKind::If) {}
};

class NumericForStmt final : public Stmt {
public:
    std::string var;
    ExprPtr start;
    ExprPtr stop;
    ExprPtr step;
    std::vector<StmtPtr> body;
    Symbol* varSym = nullptr;
    NumericForStmt() : Stmt(StmtKind::NumericFor) {}
};

class GenericForStmt final : public Stmt {
public:
    std::vector<std::string> vars;
    std::vector<ExprPtr> exprs;
    std::vector<StmtPtr> body;
    std::vector<Symbol*> varSyms;
    GenericForStmt() : Stmt(StmtKind::GenericFor) {}
};

class FuncDeclStmt final : public Stmt {
public:
    bool isLocal = false;
    bool isMethod = false;
    ExprPtr target;
    std::vector<std::string> params;
    bool vararg = false;
    std::vector<StmtPtr> body;
    std::vector<Symbol*> paramSyms;
    Symbol* nameSym = nullptr;
    FuncDeclStmt() : Stmt(StmtKind::FuncDecl) {}
};

class ReturnStmt final : public Stmt {
public:
    std::vector<ExprPtr> values;
    ReturnStmt() : Stmt(StmtKind::Return) {}
};

class BreakStmt final : public Stmt {
public:
    BreakStmt() : Stmt(StmtKind::Break) {}
};

class ContinueStmt final : public Stmt {
public:
    ContinueStmt() : Stmt(StmtKind::Continue) {}
};

class GotoStmt final : public Stmt {
public:
    std::string label;
    GotoStmt() : Stmt(StmtKind::Goto) {}
};

class LabelStmt final : public Stmt {
public:
    std::string name;
    LabelStmt() : Stmt(StmtKind::Label) {}
};

ExprPtr cloneExpr(const Expr* expr);

}
