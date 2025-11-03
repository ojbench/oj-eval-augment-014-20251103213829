#pragma once
#ifndef PYTHON_INTERPRETER_EVALVISITOR_H
#define PYTHON_INTERPRETER_EVALVISITOR_H

#include "Python3ParserBaseVisitor.h"
#include <bits/stdc++.h>
#include <boost/multiprecision/cpp_int.hpp>

struct ReturnSignal { std::any value; };
struct BreakSignal {};
struct ContinueSignal {};

// Dynamic value type used by the interpreter
struct Value {
    enum class Type { NONE, BOOL, INT, FLOAT, STR } type{Type::NONE};
    bool b{};
    boost::multiprecision::cpp_int i{};
    double f{};
    std::string s{};

    static Value None() { return Value(); }
    static Value fromBool(bool v) { Value x; x.type=Type::BOOL; x.b=v; return x; }
    static Value fromInt(const boost::multiprecision::cpp_int &v){ Value x; x.type=Type::INT; x.i=v; return x; }
    static Value fromInt(long long v){ return fromInt(boost::multiprecision::cpp_int(v)); }
    static Value fromFloat(double v){ Value x; x.type=Type::FLOAT; x.f=v; return x; }
    static Value fromStr(std::string v){ Value x; x.type=Type::STR; x.s=std::move(v); return x; }
};

struct Function {
    std::vector<std::string> params;            // parameter names
    size_t required_count = 0;                  // number of params without defaults (prefix)
    std::vector<Value> defaults;                // defaults for trailing params (size = params.size() - required_count)
    Python3Parser::SuiteContext* body = nullptr; // function body
};

class EvalVisitor : public Python3ParserBaseVisitor {
public:
    EvalVisitor();

    // entry
    std::any visitFile_input(Python3Parser::File_inputContext *ctx) override;

    // statements
    std::any visitFuncdef(Python3Parser::FuncdefContext *ctx) override;
    std::any visitStmt(Python3Parser::StmtContext *ctx) override;
    std::any visitSimple_stmt(Python3Parser::Simple_stmtContext *ctx) override;
    std::any visitSmall_stmt(Python3Parser::Small_stmtContext *ctx) override;
    std::any visitExpr_stmt(Python3Parser::Expr_stmtContext *ctx) override;
    std::any visitAugassign(Python3Parser::AugassignContext *ctx) override;
    std::any visitFlow_stmt(Python3Parser::Flow_stmtContext *ctx) override;
    std::any visitBreak_stmt(Python3Parser::Break_stmtContext *ctx) override;
    std::any visitContinue_stmt(Python3Parser::Continue_stmtContext *ctx) override;
    std::any visitReturn_stmt(Python3Parser::Return_stmtContext *ctx) override;
    std::any visitCompound_stmt(Python3Parser::Compound_stmtContext *ctx) override;
    std::any visitIf_stmt(Python3Parser::If_stmtContext *ctx) override;
    std::any visitWhile_stmt(Python3Parser::While_stmtContext *ctx) override;
    std::any visitSuite(Python3Parser::SuiteContext *ctx) override;

    // expressions
    std::any visitTest(Python3Parser::TestContext *ctx) override;
    std::any visitOr_test(Python3Parser::Or_testContext *ctx) override;
    std::any visitAnd_test(Python3Parser::And_testContext *ctx) override;
    std::any visitNot_test(Python3Parser::Not_testContext *ctx) override;
    std::any visitComparison(Python3Parser::ComparisonContext *ctx) override;
    std::any visitArith_expr(Python3Parser::Arith_exprContext *ctx) override;
    std::any visitTerm(Python3Parser::TermContext *ctx) override;
    std::any visitFactor(Python3Parser::FactorContext *ctx) override;
    std::any visitAtom_expr(Python3Parser::Atom_exprContext *ctx) override;
    std::any visitAtom(Python3Parser::AtomContext *ctx) override;
    std::any visitFormat_string(Python3Parser::Format_stringContext *ctx) override;

private:
    // environments
    std::unordered_map<std::string, Value> globals_;
    std::unordered_map<std::string, Function> functions_;
    // current function call parameter scope (only parameters live here)
    std::vector<std::unordered_map<std::string, Value>> local_param_stack_;

    // helpers
    Value evalTest(antlr4::ParserRuleContext* ctx);
    std::vector<Value> evalTestlist(Python3Parser::TestlistContext* ctx);

    // name resolution and assignment
    bool hasLocal(const std::string& name) const;
    Value getVar(const std::string& name) const;
    void setVar(const std::string& name, const Value& v);

    // type helpers
    static bool isTruthy(const Value& v);
    static std::string toString(const Value& v);
    static Value add(const Value& a, const Value& b);
    static Value sub(const Value& a, const Value& b);
    static Value mul(const Value& a, const Value& b);
    static Value truediv(const Value& a, const Value& b);
    static Value floordiv(const Value& a, const Value& b);
    static Value mod(const Value& a, const Value& b);

    static int cmp(const Value& a, const Value& b); // -1,0,1 for a<b, a==b, a>b (only for same-ish types)

    // builtins and calls
    Value callFunction(const std::string& name, const std::vector<std::pair<std::string, Value>>& args_pos_and_kw, antlr4::ParserRuleContext* ctx);
    Value callUserFunction(const Function& fn, const std::vector<std::pair<std::string, Value>>& args, antlr4::ParserRuleContext* ctx);

    // parsing utils
    static Value parseNumber(const std::string& text);
    static std::string parseStringToken(const std::string& text);
};

#endif // PYTHON_INTERPRETER_EVALVISITOR_H
