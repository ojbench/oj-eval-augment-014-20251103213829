#include "Evalvisitor.h"
#include "Python3Parser.h"
#include "antlr4-runtime.h"
using boost::multiprecision::cpp_int;
using namespace std;

static string replace_all(string s, const string& from, const string& to){
    if (from.empty()) return s;
    size_t pos=0; while((pos=s.find(from,pos))!=string::npos){ s.replace(pos, from.size(), to); pos += to.size(); }
    return s;
}

// Convert Value to string per assignment requirements
string EvalVisitor::toString(const Value& v){
    switch(v.type){
        case Value::Type::NONE: return "None";
        case Value::Type::BOOL: return v.b?"True":"False";
        case Value::Type::INT: {
            return v.i.convert_to<string>();
        }
        case Value::Type::FLOAT: {
            ostringstream oss; oss.setf(std::ios::fixed); oss<<setprecision(6)<<v.f; return oss.str();
        }
        case Value::Type::STR: return v.s;
    }
    return "";
}

// Truthiness similar to Python
bool EvalVisitor::isTruthy(const Value& v){
    switch(v.type){
        case Value::Type::NONE: return false;
        case Value::Type::BOOL: return v.b;
        case Value::Type::INT: return v.i != 0;
        case Value::Type::FLOAT: return v.f != 0.0;
        case Value::Type::STR: return !v.s.empty();
    }
    return false;
}

// Parse numeric literal
Value EvalVisitor::parseNumber(const string& text){
    if (text.find('.') != string::npos){
        return Value::fromFloat(strtod(text.c_str(), nullptr));
    }else{
        cpp_int x = 0; bool neg=false; size_t p=0; if(text.size()>0 && (text[0]=='+'||text[0]=='-')){neg=text[0]=='-'; p=1;}
        for(;p<text.size();++p){ if(isdigit((unsigned char)text[p])){ x *= 10; x += (text[p]-'0'); } }
        if(neg) x = -x; return Value::fromInt(x);
    }
}

// Remove quotes around a normal STRING token
string EvalVisitor::parseStringToken(const string& t){
    if (t.size()>=2 && ( (t.front()=='"' && t.back()=='"') || (t.front()=='\'' && t.back()=='\'') )){
        string inner = t.substr(1, t.size()-2);
        // No escapes in spec; treat doubled quotes as literal (undefined)
        return inner;
    }
    return t;
}

// Comparison helper: returns -1/0/1, throws if incomparable
int EvalVisitor::cmp(const Value& a, const Value& b){
    // numbers
    if ((a.type==Value::Type::INT || a.type==Value::Type::FLOAT || a.type==Value::Type::BOOL) &&
        (b.type==Value::Type::INT || b.type==Value::Type::FLOAT || b.type==Value::Type::BOOL)){
        // promote to float if any is float
        if (a.type==Value::Type::FLOAT || b.type==Value::Type::FLOAT){
            double x = (a.type==Value::Type::FLOAT)? a.f : (a.type==Value::Type::INT? a.i.convert_to<double>() : (a.b?1.0:0.0));
            double y = (b.type==Value::Type::FLOAT)? b.f : (b.type==Value::Type::INT? b.i.convert_to<double>() : (b.b?1.0:0.0));
            if (x<y) return -1; if (x>y) return 1; return 0;
        }else{ // both integral/bool
            cpp_int x = (a.type==Value::Type::INT)? a.i : cpp_int(a.b?1:0);
            cpp_int y = (b.type==Value::Type::INT)? b.i : cpp_int(b.b?1:0);
            if (x<y) return -1; if (x>y) return 1; return 0;
        }
    }
    if (a.type==Value::Type::STR && b.type==Value::Type::STR){
        if (a.s<b.s) return -1; if (a.s>b.s) return 1; return 0;
    }
    if (a.type==Value::Type::NONE && b.type==Value::Type::NONE) return 0;
    throw runtime_error("incomparable types");
}

// Arithmetic helpers
Value EvalVisitor::add(const Value& a, const Value& b){
    if (a.type==Value::Type::STR && b.type==Value::Type::STR) return Value::fromStr(a.s + b.s);
    if (a.type==Value::Type::FLOAT || b.type==Value::Type::FLOAT){
        double x = (a.type==Value::Type::FLOAT)? a.f : (a.type==Value::Type::INT? a.i.convert_to<double>() : (a.type==Value::Type::BOOL? (a.b?1.0:0.0):0.0));
        double y = (b.type==Value::Type::FLOAT)? b.f : (b.type==Value::Type::INT? b.i.convert_to<double>() : (b.type==Value::Type::BOOL? (b.b?1.0:0.0):0.0));
        return Value::fromFloat(x+y);
    }
    // treat bool as int
    cpp_int x = (a.type==Value::Type::INT)? a.i : cpp_int(a.type==Value::Type::BOOL && a.b);
    cpp_int y = (b.type==Value::Type::INT)? b.i : cpp_int(b.type==Value::Type::BOOL && b.b);
    return Value::fromInt(x+y);
}
Value EvalVisitor::sub(const Value& a, const Value& b){
    if (a.type==Value::Type::FLOAT || b.type==Value::Type::FLOAT){
        double x = (a.type==Value::Type::FLOAT)? a.f : (a.type==Value::Type::INT? a.i.convert_to<double>() : (a.type==Value::Type::BOOL? (a.b?1.0:0.0):0.0));
        double y = (b.type==Value::Type::FLOAT)? b.f : (b.type==Value::Type::INT? b.i.convert_to<double>() : (b.type==Value::Type::BOOL? (b.b?1.0:0.0):0.0));
        return Value::fromFloat(x-y);
    }
    cpp_int x = (a.type==Value::Type::INT)? a.i : cpp_int(a.type==Value::Type::BOOL && a.b);
    cpp_int y = (b.type==Value::Type::INT)? b.i : cpp_int(b.type==Value::Type::BOOL && b.b);
    return Value::fromInt(x-y);
}
Value EvalVisitor::mul(const Value& a, const Value& b){
    // string repeat
    if (a.type==Value::Type::STR && (b.type==Value::Type::INT || b.type==Value::Type::BOOL)){
        long long n = (b.type==Value::Type::INT)? b.i.convert_to<long long>() : (b.b?1:0);
        if (n<=0) return Value::fromStr("");
        string out; out.reserve(a.s.size()* (size_t)n);
        for(long long i=0;i<n;i++) out+=a.s;
        return Value::fromStr(out);
    }
    if (b.type==Value::Type::STR && (a.type==Value::Type::INT || a.type==Value::Type::BOOL)) return mul(b,a);
    if (a.type==Value::Type::FLOAT || b.type==Value::Type::FLOAT){
        double x = (a.type==Value::Type::FLOAT)? a.f : (a.type==Value::Type::INT? a.i.convert_to<double>() : (a.type==Value::Type::BOOL? (a.b?1.0:0.0):0.0));
        double y = (b.type==Value::Type::FLOAT)? b.f : (b.type==Value::Type::INT? b.i.convert_to<double>() : (b.type==Value::Type::BOOL? (b.b?1.0:0.0):0.0));
        return Value::fromFloat(x*y);
    }
    cpp_int x = (a.type==Value::Type::INT)? a.i : cpp_int(a.type==Value::Type::BOOL && a.b);
    cpp_int y = (b.type==Value::Type::INT)? b.i : cpp_int(b.type==Value::Type::BOOL && b.b);
    return Value::fromInt(x*y);
}
Value EvalVisitor::truediv(const Value& a, const Value& b){
    double x = (a.type==Value::Type::FLOAT)? a.f : (a.type==Value::Type::INT? a.i.convert_to<double>() : (a.type==Value::Type::BOOL? (a.b?1.0:0.0):0.0));
    double y = (b.type==Value::Type::FLOAT)? b.f : (b.type==Value::Type::INT? b.i.convert_to<double>() : (b.type==Value::Type::BOOL? (b.b?1.0:0.0):0.0));
    return Value::fromFloat(x/y);
}
static cpp_int floor_div_int(const cpp_int& a, const cpp_int& b){
    cpp_int q = a / b; cpp_int r = a % b; // trunc toward zero
    bool neg = ( (a<0) ^ (b<0) );
    if (neg && r!=0) q -= 1;
    return q;
}
Value EvalVisitor::floordiv(const Value& a, const Value& b){
    if ((a.type==Value::Type::INT || a.type==Value::Type::BOOL) && (b.type==Value::Type::INT || b.type==Value::Type::BOOL)){
        cpp_int x = (a.type==Value::Type::INT)? a.i : cpp_int(a.b?1:0);
        cpp_int y = (b.type==Value::Type::INT)? b.i : cpp_int(b.b?1:0);
        return Value::fromInt(floor_div_int(x,y));
    }
    // numeric floordiv
    double x = (a.type==Value::Type::FLOAT)? a.f : (a.type==Value::Type::INT? a.i.convert_to<double>() : (a.type==Value::Type::BOOL? (a.b?1.0:0.0):0.0));
    double y = (b.type==Value::Type::FLOAT)? b.f : (b.type==Value::Type::INT? b.i.convert_to<double>() : (b.type==Value::Type::BOOL? (b.b?1.0:0.0):0.0));
    return Value::fromInt((cpp_int) floor(x/y));
}
Value EvalVisitor::mod(const Value& a, const Value& b){
    // a % b = a - (a // b)*b
    Value q = floordiv(a,b);
    Value prod = mul(q,b);
    if (a.type==Value::Type::FLOAT || b.type==Value::Type::FLOAT){
        double x = (a.type==Value::Type::FLOAT)? a.f : (a.type==Value::Type::INT? a.i.convert_to<double>() : (a.type==Value::Type::BOOL? (a.b?1.0:0.0):0.0));
        double p = (prod.type==Value::Type::FLOAT)? prod.f : (prod.type==Value::Type::INT? prod.i.convert_to<double>() : (prod.type==Value::Type::BOOL? (prod.b?1.0:0.0):0.0));
        return Value::fromFloat(x - p);
    }else{
        cpp_int x = (a.type==Value::Type::INT)? a.i : cpp_int(a.type==Value::Type::BOOL && a.b);
        cpp_int p = (prod.type==Value::Type::INT)? prod.i : cpp_int(prod.type==Value::Type::BOOL && prod.b);
        return Value::fromInt(x - p);
    }
}

EvalVisitor::EvalVisitor(){
    // nothing
}


// Environment helpers
bool EvalVisitor::hasLocal(const string& name) const{
    if (local_param_stack_.empty()) return false;
    const auto& m = local_param_stack_.back();
    return m.find(name)!=m.end();
}
Value EvalVisitor::getVar(const string& name) const{
    if (!local_param_stack_.empty()){
        const auto& m = local_param_stack_.back();
        auto it = m.find(name);
        if (it!=m.end()) return it->second;
    }
    auto itg = globals_.find(name);
    if (itg!=globals_.end()) return itg->second;
    return Value::None();
}
void EvalVisitor::setVar(const string& name, const Value& v){
    if (!local_param_stack_.empty()){
        auto it = local_param_stack_.back().find(name);
        if (it != local_param_stack_.back().end()){
            local_param_stack_.back()[name] = v; return;
        }
    }
    globals_[name] = v;
}

Value EvalVisitor::evalTest(antlr4::ParserRuleContext* ctx){
    auto ret = visit(ctx);
    if (ret.has_value()) return std::any_cast<Value>(ret);
    return Value::None();
}
vector<Value> EvalVisitor::evalTestlist(Python3Parser::TestlistContext* ctx){
    vector<Value> res; if (!ctx) return res;
    for (auto t : ctx->test()) res.push_back(std::any_cast<Value>(visit(t)));
    return res;
}

// Builtins and function calls
Value EvalVisitor::callFunction(const string& name, const vector<pair<string,Value>>& args, antlr4::ParserRuleContext* ctx){
    if (name == "print"){
        // print all args with space separator
        for (size_t i=0;i<args.size();++i){ if (i) cout<<' '; cout<<toString(args[i].second); }
        cout<<'\n';
        return Value::None();
    }
    if (name == "int" || name == "float" || name == "str" || name == "bool"){
        if (args.size()!=1) return Value::None();
        const Value& v = args[0].second;
        if (name == "int"){
            if (v.type==Value::Type::INT) return v;
            if (v.type==Value::Type::BOOL) return Value::fromInt(v.b?1:0);
            if (v.type==Value::Type::FLOAT) return Value::fromInt((cpp_int) (v.f>=0? floor(v.f): ceil(v.f))); // truncate toward zero
            if (v.type==Value::Type::STR){
                // simple decimal parse
                return parseNumber(v.s);
            }
            return Value::fromInt(0);
        }else if (name == "float"){
            if (v.type==Value::Type::FLOAT) return v;
            if (v.type==Value::Type::INT) return Value::fromFloat(v.i.convert_to<double>());
            if (v.type==Value::Type::BOOL) return Value::fromFloat(v.b?1.0:0.0);
            if (v.type==Value::Type::STR) return Value::fromFloat(strtod(v.s.c_str(), nullptr));
            return Value::fromFloat(0.0);
        }else if (name == "str"){
            return Value::fromStr(toString(v));
        }else { // bool
            return Value::fromBool(isTruthy(v));
        }
    }
    // user-defined
    auto it = functions_.find(name);
    if (it != functions_.end()) return callUserFunction(it->second, args, ctx);
    // unknown callable -> None
    return Value::None();
}

Value EvalVisitor::callUserFunction(const Function& fn, const vector<pair<string,Value>>& args, antlr4::ParserRuleContext* ctx){
    size_t n = fn.params.size();
    vector<Value> actual(n, Value::None());
    vector<char> assigned(n, 0);
    size_t posi = 0;
    // positional first
    for (const auto& pr : args){
        if (pr.first.empty()){
            if (posi>=n) return Value::None();
            actual[posi] = pr.second; assigned[posi]=1; posi++;
        }
    }
    // keywords
    for (const auto& pr : args){
        if (!pr.first.empty()){
            auto it = find(fn.params.begin(), fn.params.end(), pr.first);
            if (it==fn.params.end()) return Value::None();
            size_t idx = it - fn.params.begin();
            actual[idx] = pr.second; assigned[idx]=1;
        }
    }
    // fill defaults
    for (size_t i=0;i<n;++i){
        if (!assigned[i]){
            if (i < fn.required_count) return Value::None();
            size_t j = i - fn.required_count; if (j<fn.defaults.size()) actual[i] = fn.defaults[j];
        }
    }
    // build local param scope
    local_param_stack_.push_back({});
    for (size_t i=0;i<n;++i) local_param_stack_.back()[fn.params[i]] = actual[i];
    try{
        visit(fn.body);
    }catch(const ReturnSignal& rs){
        local_param_stack_.pop_back();
        if (rs.value.has_value()) return std::any_cast<Value>(rs.value);
        return Value::None();
    }
    local_param_stack_.pop_back();
    return Value::None();
}

// Visitor implementations - statements
std::any EvalVisitor::visitFile_input(Python3Parser::File_inputContext *ctx){
    for (auto st : ctx->stmt()){
        visit(st);
    }
    return Value::None();
}

std::any EvalVisitor::visitStmt(Python3Parser::StmtContext *ctx){
    if (ctx->simple_stmt()) return visit(ctx->simple_stmt());
    else return visit(ctx->compound_stmt());
}

std::any EvalVisitor::visitSimple_stmt(Python3Parser::Simple_stmtContext *ctx){
    return visit(ctx->small_stmt());
}

std::any EvalVisitor::visitSmall_stmt(Python3Parser::Small_stmtContext *ctx){
    if (ctx->expr_stmt()) return visit(ctx->expr_stmt());
    else return visit(ctx->flow_stmt());
}

std::any EvalVisitor::visitFuncdef(Python3Parser::FuncdefContext *ctx){
    string fname = ctx->NAME()->getText();
    Function fn;
    // parameters
    auto paramsCtx = ctx->parameters()->typedargslist();
    if (paramsCtx){
        size_t m = paramsCtx->tfpdef().size();
        for(size_t i=0;i<m;++i){
            string p = paramsCtx->tfpdef(i)->NAME()->getText();
            fn.params.push_back(p);
        }
        // defaults: aligned at end
        size_t assigns = paramsCtx->ASSIGN().size();
        fn.required_count = m - assigns;
        for (size_t j=0;j<assigns;++j){
            auto dv = std::any_cast<Value>(visit(paramsCtx->test(fn.required_count + j)));
            fn.defaults.push_back(dv);
        }
    }
    fn.body = ctx->suite();
    functions_[fname] = std::move(fn);
    return Value::None();
}

std::any EvalVisitor::visitSuite(Python3Parser::SuiteContext *ctx){
    if (ctx->simple_stmt()) return visit(ctx->simple_stmt());
    for (auto st : ctx->stmt()){
        visit(st);
    }
    return Value::None();
}

std::any EvalVisitor::visitCompound_stmt(Python3Parser::Compound_stmtContext *ctx){
    if (ctx->if_stmt()) return visit(ctx->if_stmt());
    if (ctx->while_stmt()) return visit(ctx->while_stmt());
    return visit(ctx->funcdef());
}

std::any EvalVisitor::visitIf_stmt(Python3Parser::If_stmtContext *ctx){
    int k = (int)ctx->test().size();
    for (int i=0;i<k;++i){
        Value cond = std::any_cast<Value>(visit(ctx->test(i)));
        if (isTruthy(cond)){
            visit(ctx->suite(i));
            return Value::None();
        }
    }
    if (ctx->ELSE()){
        visit(ctx->suite(k));
    }
    return Value::None();
}

std::any EvalVisitor::visitWhile_stmt(Python3Parser::While_stmtContext *ctx){
    while (true){
        Value cond = std::any_cast<Value>(visit(ctx->test()));
        if (!isTruthy(cond)) break;
        try{
            visit(ctx->suite());
        }catch(const ContinueSignal&){
            continue;
        }catch(const BreakSignal&){
            break;
        }catch(const ReturnSignal&){
            throw; // propagate
        }
    }
    return Value::None();
}

std::any EvalVisitor::visitFlow_stmt(Python3Parser::Flow_stmtContext *ctx){
    if (ctx->break_stmt()) return visit(ctx->break_stmt());
    if (ctx->continue_stmt()) return visit(ctx->continue_stmt());
    return visit(ctx->return_stmt());
}

std::any EvalVisitor::visitBreak_stmt(Python3Parser::Break_stmtContext * /*ctx*/){
    throw BreakSignal();
}

std::any EvalVisitor::visitContinue_stmt(Python3Parser::Continue_stmtContext * /*ctx*/){
    throw ContinueSignal();
}

std::any EvalVisitor::visitReturn_stmt(Python3Parser::Return_stmtContext *ctx){
    if (ctx->testlist()){
        auto vals = evalTestlist(ctx->testlist());
        // Return last value if multiple
        Value ret = vals.empty()? Value::None(): vals.back();
        throw ReturnSignal{ret};
    }else{
        throw ReturnSignal{std::any()};
    }
}

std::any EvalVisitor::visitAugassign(Python3Parser::AugassignContext *ctx){
    return ctx->getText();
}

std::any EvalVisitor::visitExpr_stmt(Python3Parser::Expr_stmtContext *ctx){
    if (ctx->augassign()){
        string lhs = ctx->testlist(0)->getText();
        Value lv = getVar(lhs);
        Value rv = evalTestlist(ctx->testlist(1)).back();
        string op = ctx->augassign()->getText();
        Value res;
        if (op=="+=") res = add(lv,rv);
        else if (op=="-=") res = sub(lv,rv);
        else if (op=="*=") res = mul(lv,rv);
        else if (op=="/=") res = truediv(lv,rv);
        else if (op=="//=") res = floordiv(lv,rv);
        else if (op=="%=") res = mod(lv,rv);
        setVar(lhs,res);
        return Value::None();
    }
    int n = (int)ctx->testlist().size();
    if (n>=2 && !ctx->ASSIGN().empty()){
        // chained assignment: a = b = ... = value (rightmost)
        Value val = evalTestlist(ctx->testlist(n-1)).back();
        for (int i=0;i<n-1;++i){
            string name = ctx->testlist(i)->getText();
            setVar(name, val);
        }
        return Value::None();
    }
    // expression only
    auto vals = evalTestlist(ctx->testlist(0));
    (void)vals;
    return Value::None();
}

// Visitor implementations - expressions
std::any EvalVisitor::visitTest(Python3Parser::TestContext *ctx){
    return visit(ctx->or_test());
}

std::any EvalVisitor::visitOr_test(Python3Parser::Or_testContext *ctx){
    // short-circuit OR over and_test
    bool acc = false; bool first=true;
    for (auto a : ctx->and_test()){
        Value v = std::any_cast<Value>(visit(a));
        bool b = isTruthy(v);
        if (first){ acc=b; first=false; }
        else acc = acc || b;
        if (acc) return Value::fromBool(true);
    }
    return Value::fromBool(acc);
}

std::any EvalVisitor::visitAnd_test(Python3Parser::And_testContext *ctx){
    bool acc = true; bool first=true;
    for (auto n : ctx->not_test()){
        Value v = std::any_cast<Value>(visit(n));
        bool b = isTruthy(v);
        if (first){ acc=b; first=false; }
        else acc = acc && b;
        if (!acc) return Value::fromBool(false);
    }
    return Value::fromBool(acc);
}

std::any EvalVisitor::visitNot_test(Python3Parser::Not_testContext *ctx){
    if (ctx->NOT()){
        Value v = std::any_cast<Value>(visit(ctx->not_test()));
        return Value::fromBool(!isTruthy(v));
    }
    return visit(ctx->comparison());
}

std::any EvalVisitor::visitComparison(Python3Parser::ComparisonContext *ctx){
    vector<Value> vals; vals.reserve(ctx->arith_expr().size());
    for (auto e : ctx->arith_expr()) vals.push_back(std::any_cast<Value>(visit(e)));
    bool ok = true;
    for (size_t i=0;i<ctx->comp_op().size() && ok; ++i){
        string op = ctx->comp_op(i)->getText();
        const Value& A = vals[i];
        const Value& B = vals[i+1];
        int c=0; bool res=false;
        try{ c = cmp(A,B); }
        catch(...){ c = INT_MIN; }
        if (op == "==") res = (c==0);
        else if (op == "!=") res = (c!=0);
        else if (op == "<") res = (c==INT_MIN? false : c<0);
        else if (op == ">") res = (c==INT_MIN? false : c>0);
        else if (op == "<=") res = (c==INT_MIN? false : c<=0);
        else if (op == ">=") res = (c==INT_MIN? false : c>=0);
        ok = ok && res;
    }
    return Value::fromBool(ok);
}

std::any EvalVisitor::visitArith_expr(Python3Parser::Arith_exprContext *ctx){
    Value cur = std::any_cast<Value>(visit(ctx->term(0)));
    for (size_t i=1;i<ctx->term().size();++i){
        string op = ctx->addorsub_op(i-1)->getText();
        Value rhs = std::any_cast<Value>(visit(ctx->term(i)));
        if (op=="+") cur = add(cur,rhs);
        else cur = sub(cur,rhs);
    }
    return cur;
}

std::any EvalVisitor::visitTerm(Python3Parser::TermContext *ctx){
    Value cur = std::any_cast<Value>(visit(ctx->factor(0)));
    for (size_t i=1;i<ctx->factor().size();++i){
        string op = ctx->muldivmod_op(i-1)->getText();
        Value rhs = std::any_cast<Value>(visit(ctx->factor(i)));
        if (op=="*") cur = mul(cur,rhs);
        else if (op=="/") cur = truediv(cur,rhs);
        else if (op=="//") cur = floordiv(cur,rhs);
        else cur = mod(cur,rhs);
    }
    return cur;
}

std::any EvalVisitor::visitFactor(Python3Parser::FactorContext *ctx){
    if (ctx->atom_expr()) return visit(ctx->atom_expr());
    // unary
    string op = ctx->getChild(0)->getText();
    Value v = std::any_cast<Value>(visit(ctx->factor()));
    if (op=="+"){
        if (v.type==Value::Type::INT || v.type==Value::Type::BOOL || v.type==Value::Type::FLOAT) return v;
        return Value::None();
    }else{ // '-'
        if (v.type==Value::Type::FLOAT) return Value::fromFloat(-v.f);
        if (v.type==Value::Type::INT) return Value::fromInt(-v.i);
        if (v.type==Value::Type::BOOL) return Value::fromInt(v.b? -1: 0);
        return Value::None();
    }
}

std::any EvalVisitor::visitAtom_expr(Python3Parser::Atom_exprContext *ctx){
    Value obj = std::any_cast<Value>(visit(ctx->atom()));
    if (!ctx->trailer()) return obj;
    // function call: atom must be NAME (function) or builtin
    string name = ctx->atom()->NAME()? ctx->atom()->NAME()->getText() : string();
    vector<pair<string,Value>> args;
    auto tr = ctx->trailer();
    if (tr->arglist()){
        for (auto arg : tr->arglist()->argument()){
            if (arg->ASSIGN()){
                // keyword
                string key = arg->test(0)->getText();
                Value val = std::any_cast<Value>(visit(arg->test(1)));
                args.push_back({key, val});
            }else{
                Value val = std::any_cast<Value>(visit(arg->test(0)));
                args.push_back({"", val});
            }
        }
    }
    // if obj is a function name stored in variable, not supported; use name or string
    return callFunction(name, args, ctx);
}

std::any EvalVisitor::visitAtom(Python3Parser::AtomContext *ctx){
    if (ctx->NAME()){
        string name = ctx->NAME()->getText();
        return getVar(name);
    }
    if (ctx->NUMBER()){
        string t = ctx->NUMBER()->getText();
        return parseNumber(t);
    }
    if (ctx->NONE()) return Value::None();
    if (ctx->TRUE()) return Value::fromBool(true);
    if (ctx->FALSE()) return Value::fromBool(false);
    if (ctx->OPEN_PAREN()){
        return visit(ctx->test());
    }
    if (!ctx->STRING().empty()){
        string out;
        for (auto tn : ctx->STRING()) out += parseStringToken(tn->getText());
        return Value::fromStr(out);
    }
    // format string
    return visit(ctx->format_string());
}

std::any EvalVisitor::visitFormat_string(Python3Parser::Format_stringContext *ctx){
    string out;
    for (auto *child : ctx->children){
        if (auto* tn = dynamic_cast<antlr4::tree::TerminalNode*>(child)){
            int ttype = tn->getSymbol()->getType();
            if (ttype == Python3Parser::FORMAT_STRING_LITERAL){
                string frag = tn->getText();
                // lexer likely gives raw literal; turn escaped braces into literal braces
                frag = replace_all(frag, "{{", "{");
                frag = replace_all(frag, "}}", "}");
                out += frag;
            }
            // skip quotes and braces tokens here; handled via Testlist nodes
        } else if (auto* tl = dynamic_cast<Python3Parser::TestlistContext*>(child)){
            auto vals = evalTestlist(tl);
            // join by comma? Grammar allows multiple, but spec says expressions allowed are basic types
            // Use last value if multiple
            Value v = vals.empty()? Value::None(): vals.back();
            out += toString(v);
        }
    }
    return Value::fromStr(out);
}
