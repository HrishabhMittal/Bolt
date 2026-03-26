#include "codeutil.cpp"
#include "header.hpp"
#include "opcode.hpp"
#include "vm.hpp"

struct Identifier {
    std::string name;
    std::string type;
};
struct Function {
    uint64_t ip;
    std::string name;
    std::vector<std::string> argtypes;
    std::string ret_type;
    bool is_external = false;
};
struct call_ref_in_map {
    std::string name;
    uint64_t ins;
};
struct function_code_and_offset {
    std::vector<bvm::instruction> code;
    uint64_t offset = 0;
};
class Program {
    uint64_t iden_stack_size = 0;
    std::string pushing_to_func;

    std::string data_section;
    // instructions i need to patch, which contain function calls
    std::map<std::string, std::vector<call_ref_in_map>> patch_function;

    // function code should be pushed here instead
    // idea is:
    // i need to rearrage the code like this
    // <global declarations>
    // call main
    // halt
    // <function declarations>
    //
    //
    // need to store all function calls and patch them in the end
    std::map<std::string, function_code_and_offset> function_code;
    std::vector<bvm::instruction> code;
    std::vector<std::vector<Identifier>> scope;
    std::map<std::string, Function> funcs;
    void precalc_all_offsets() {
        uint64_t off = code.size();
        for (auto &i : function_code) {
            i.second.offset = off;
            off += i.second.code.size();
        }
    }

  public:
    Program() {
        code.reserve(1024);
        scope.reserve(16);
        scope.push_back({});
    }
    bvm::program construct_full_code() {
        // global decls alr there
        push_call("main.main");
        code.push_back({bvm::OPCODE::HALT});
        precalc_all_offsets();
        for (auto &i : patch_function)
            for (auto &j : i.second) {
                if (!funcs.count(i.first)) {
                    error("function " + i.first + " is called but never declared");
                }
                if (funcs[i.first].is_external) {
                    bvm::instruction &instr = (j.name == "") ? code[j.ins] : function_code[j.name].code[j.ins];
                    instr.op = bvm::OPCODE::CALL_EXTERN;
                    uint64_t string_ptr = data_size();
                    data_push(i.first + '\0');
                    instr.operands[0] = string_ptr;
                    continue;
                }
                if (j.name == "") {
                    code[j.ins].operands[0] = function_code[i.first].offset;
                } else
                    function_code[j.name].code[j.ins].operands[0] = function_code[i.first].offset;
            }
        const std::array<bvm::OPCODE, 9> jumps{bvm::OPCODE::JNC, bvm::OPCODE::JC,  bvm::OPCODE::JNE,
                                               bvm::OPCODE::JE,  bvm::OPCODE::JGE, bvm::OPCODE::JGT,
                                               bvm::OPCODE::JLE, bvm::OPCODE::JLT, bvm::OPCODE::JMP};
        for (auto &i : function_code)
            for (auto &j : i.second.code) {
                for (auto &jump : jumps) {
                    if (j.op == jump) {
                        j.operands[0] += i.second.offset;
                        break;
                    }
                }
                code.push_back(j);
            }
        bvm::program final_prog;
        for (const auto &[name, func] : funcs) {
            if (!func.is_external) {
                final_prog.exported_functions[name] = func.ip + function_code[name].offset;
            }
        }
        if (!scope.empty()) {
            for (size_t i = 0; i < scope[0].size(); i++) {
                final_prog.exported_globals[scope[0][i].name] = i;
            }
        }
        final_prog.code = code;
        final_prog.data = data_section;
        return final_prog;
    }
    const std::string &Data() { return data_section; }
    const std::vector<bvm::instruction> &Code() { return code; }
    size_t get_ip() { return code.size(); }
    void push(const bvm::instruction &i) {
        if (pushing_to_func == "")
            code.push_back(i);
        else
            function_code[pushing_to_func].code.push_back(i);
    }
    void push_call(const std::string &func_name) {
        bvm::instruction i;
        i.op = bvm::OPCODE::CALL;
        if (pushing_to_func == "") {
            patch_function[func_name].push_back({pushing_to_func, code.size()});
            code.push_back(i);
        } else {
            patch_function[func_name].push_back({pushing_to_func, function_code[pushing_to_func].code.size()});
            function_code[pushing_to_func].code.push_back(i);
        }
    }
    uint64_t data_size() { return data_section.size(); }
    void data_push(const std::string &s) { data_section += s; }
    uint64_t size() {
        if (pushing_to_func == "") {
            return code.size();
        }
        return function_code[pushing_to_func].code.size();
    }
    bvm::instruction &operator[](size_t ind) {
        if (pushing_to_func == "") {
            return code[ind];
        }
        return function_code[pushing_to_func].code[ind];
    }
    void new_scope() { scope.push_back({}); }
    void push_undeclare_for_return() {
        for (ssize_t i = static_cast<ssize_t>(scope.size()) - 1; i > 0; i--) {
            if (scope[i].size() > 0)
                push({bvm::OPCODE::UNDECLARE, {scope[i].size()}});
        }
    }
    void push_in_func(const std::string &func_name) { pushing_to_func = func_name; }
    void delete_scope() {
        if (scope.back().size() > 0)
            push({bvm::OPCODE::UNDECLARE, {scope.back().size()}});
        iden_stack_size -= scope.back().size();
        scope.pop_back();
    }
    void declare_function(Function i) {
        // now that im forward declaring, rewrite their pointer on second declaration
        if (funcs.count(i.name)) {
            if (funcs[i.name].ip == UINT64_MAX) {
                funcs[i.name].ip = i.ip;
                return;
            }
            error("redaclaration of " + i.name + " in the same scope.");
        }
        funcs[i.name] = i;
    }
    Function get_function(std::string name, std::string pkg_name) {
        if (funcs.count(name))
            return funcs[name];
        if (name.find('.') == std::string::npos && funcs.count(pkg_name + "." + name)) {
            return funcs[pkg_name + "." + name];
        }
        error("no declaration found for function " + name);
    }
    void declare(Identifier i) {
        for (Identifier j : scope.back()) {
            if (i.name == j.name)
                error("redaclaration of " + i.name + " in the same scope.");
        }
        push({bvm::OPCODE::DECLARE, {}});
        scope.back().push_back(i);
        iden_stack_size++;
    }
    int64_t getaddress(std::string iden, std::string pkg_name) {
        ssize_t top = 0;
        for (ssize_t i = static_cast<ssize_t>(scope.size()) - 1; i >= 0; i--) {
            for (ssize_t j = static_cast<ssize_t>(scope[i].size()) - 1; j >= 0; j--) {
                top--;
                if (scope[i][j].name == iden) {
                    if (i == 0)
                        return j;
                    else
                        return top;
                }
                if (i == 0 && iden.find('.') == std::string::npos && scope[i][j].name == pkg_name + "." + iden) {
                    return j;
                }
            }
        }
        error("identifier " + iden + " doesn't exist.");
    }
    std::string gettype(std::string iden, std::string pkg_name) {
        for (ssize_t i = static_cast<int64_t>(scope.size()) - 1; i >= 0; i--) {
            for (size_t j = 0; j < scope[i].size(); j++) {
                if (scope[i][j].name == iden)
                    return scope[i][j].type;
                if (i == 0 && iden.find('.') == std::string::npos && scope[i][j].name == pkg_name + "." + iden) {
                    return scope[i][j].type;
                }
            }
        }
        error("identifier " + iden + " doesn't exist.");
    }
};

class AST {
  public:
    virtual void print(int indent = 0) = 0;
    virtual void codegen(Program &program) = 0;
};

class GlobalStatementAST : public AST {
  public:
    virtual void print(int indent = 0) = 0;
    virtual void codegen(Program &program) = 0;
    virtual ~GlobalStatementAST() = default;
};

class StatementAST : public AST {
  public:
    virtual void print(int indent = 0) = 0;
    virtual void codegen(Program &program) = 0;
    virtual ~StatementAST() = default;
};

void printSpace(int space) {
    for (int i = 0; i < space; i++)
        std::cout << ' ';
}

class ExprAST : public AST {
  public:
    virtual void print(int indent = 0) = 0;
    virtual void codegen(Program &program) = 0;
    virtual bool implicit_conversion() { return false; }
    virtual std::string evaltype(Program &program) = 0;
    virtual std::vector<std::string> get_dependencies() = 0;
    virtual ~ExprAST() = default;
};
class TypeCastAST : public ExprAST {
  public:
    std::unique_ptr<ExprAST> arg;
    Token cast_to;
    TypeCastAST(std::unique_ptr<ExprAST> arg, Token t) : arg(std::move(arg)), cast_to(t) {}
    virtual void print(int indent = 0) override {
        printSpace(indent);
        indent += 2;
        std::cout << "TypeCastAST: " << std::endl;
        printSpace(indent);
        std::cout << "convert to: " << cast_to << std::endl;
        printSpace(indent);
        std::cout << "converting: " << std::endl;
        arg->print();
    }
    virtual std::vector<std::string> get_dependencies() override { return arg->get_dependencies(); }
    virtual std::string evaltype(Program &program) override { return cast_to.value; }
    virtual void codegen(Program &program) override {
        std::string argtype = arg->evaltype(program);
        std::string target = cast_to.value;
        arg->codegen(program);
        if (argtype == target)
            return;
        bool arg_is_float = (argtype == "f32" || argtype == "f64");
        bool target_is_float = (target == "f32" || target == "f64");

        if (arg_is_float && target_is_float) {
            if (argtype == "f32" && target == "f64") {
                program.push({bvm::OPCODE::F32_TO_F64});
                return;
            } else if (argtype == "f64" && target == "f32") {
                program.push({bvm::OPCODE::F64_TO_F32});
                return;
            }
        } else if (arg_is_float && !target_is_float) {
            bool target_signed = (target[0] == 'i');
            bool target_64 = (target == "i64" || target == "u64");

            if (argtype == "f32") {
                if (target_64) {
                    program.push({target_signed ? bvm::OPCODE::F32_TO_I64 : bvm::OPCODE::F32_TO_U64});
                    return;
                } else {
                    program.push({target_signed ? bvm::OPCODE::F32_TO_I32 : bvm::OPCODE::F32_TO_U32});
                    return;
                }
            } else {
                if (target_64) {
                    program.push({target_signed ? bvm::OPCODE::F64_TO_I64 : bvm::OPCODE::F64_TO_U64});
                    return;
                } else {
                    program.push({target_signed ? bvm::OPCODE::F64_TO_I32 : bvm::OPCODE::F64_TO_U32});
                    return;
                }
            }
        } else if (!arg_is_float && target_is_float) {
            bool arg_signed = (argtype[0] == 'i');
            bool arg_64 = (argtype == "i64" || argtype == "u64");

            if (target == "f32") {
                if (arg_64) {
                    program.push({arg_signed ? bvm::OPCODE::I64_TO_F32 : bvm::OPCODE::U64_TO_F32});
                    return;
                } else {
                    program.push({arg_signed ? bvm::OPCODE::I32_TO_F32 : bvm::OPCODE::U32_TO_F32});
                    return;
                }
            } else {
                if (arg_64) {
                    program.push({arg_signed ? bvm::OPCODE::I64_TO_F64 : bvm::OPCODE::U64_TO_F64});
                    return;
                } else {
                    program.push({arg_signed ? bvm::OPCODE::I32_TO_F64 : bvm::OPCODE::U32_TO_F64});
                    return;
                }
            }
        } else if (!arg_is_float && !target_is_float) {
            bool arg_signed = (argtype[0] == 'i');
            bool arg_64 = (argtype == "i64" || argtype == "u64");
            bool target_64 = (target == "i64" || target == "u64");

            if (!arg_64 && target_64) {
                program.push({arg_signed ? bvm::OPCODE::I32_EXTEND_I64 : bvm::OPCODE::U32_EXTEND_I64});
                return;
            } else if (arg_64 && !target_64) {
                program.push({bvm::OPCODE::I32_WRAP_I64});
                return;
            } else {
                return;
            }
        }
        cast_to.error("conversion from type: " + argtype + " to type: " + cast_to.value + " is not possible.");
    }
};
class BinaryExprAST : public ExprAST {
  public:
    std::unique_ptr<ExprAST> lhs;
    Token op;
    std::unique_ptr<ExprAST> rhs;
    BinaryExprAST(std::unique_ptr<ExprAST> l, Token o, std::unique_ptr<ExprAST> r)
        : lhs(std::move(l)), op(o), rhs(std::move(r)) {}
    virtual void print(int indent = 0) override {
        printSpace(indent);
        indent += 2;
        std::cout << "BinaryExprAST: " << std::endl;
        printSpace(indent);
        std::cout << "op: " << (op) << std::endl;
        lhs->print(indent);
        rhs->print(indent);
    }
    virtual std::vector<std::string> get_dependencies() override {
        std::vector<std::string> ldeps = lhs->get_dependencies();
        std::vector<std::string> rdeps = rhs->get_dependencies();
        ldeps.insert(ldeps.end(), rdeps.begin(), rdeps.end());
        return ldeps;
    }
    virtual std::string evaltype(Program &program) override {
        std::string ltype = lhs->evaltype(program);
        bool limplicit = lhs->implicit_conversion();
        std::string rtype = rhs->evaltype(program);
        bool rimplicit = lhs->implicit_conversion();
        if (ltype != rtype) {
            error("Type mismatch in expression: cannot operate on \"" + ltype + "\" and \"" + rtype + "\"");
        }
        if (op.value == "==" || op.value == "!=" || op.value == "<" || op.value == ">" || op.value == "<=" ||
            op.value == ">=") {
            return "bool";
        }
        if (op.value == "%") {
            if (ltype != "i32" && ltype != "i64") {
                error("Type error: Modulo operator '%' not "
                      "supported for type \"" +
                      ltype + "\"");
            }
            return ltype;
        }
        if (op.value == "+" || op.value == "-" || op.value == "*" || op.value == "/") {
            return ltype;
        }
        error("Type error: Unsupported binary operator \"" + op.value + "\"");
    }
    virtual void codegen(Program &program) override {
        lhs->codegen(program);
        rhs->codegen(program);
        std::string retval1 = lhs->evaltype(program);
        std::string retval2 = rhs->evaltype(program);
        if (retval1 != retval2) {
            error("operation on \"" + retval1 + "\" and \"" + retval2 + "\" is not supported");
        }
        std::string t = retval1;
        if (op.value == "+") {
            if (t == "i32")
                program.push({bvm::OPCODE::I32_ADD, {}});
            else if (t == "i64")
                program.push({bvm::OPCODE::I64_ADD, {}});
            else if (t == "f32")
                program.push({bvm::OPCODE::F32_ADD, {}});
            else if (t == "f64")
                program.push({bvm::OPCODE::F64_ADD, {}});
            else
                error("addition on unsupported type");
            return;
        } else if (op.value == "-") {
            if (t == "i32")
                program.push({bvm::OPCODE::I32_SUB, {}});
            else if (t == "i64")
                program.push({bvm::OPCODE::I64_SUB, {}});
            else if (t == "f32")
                program.push({bvm::OPCODE::F32_SUB, {}});
            else if (t == "f64")
                program.push({bvm::OPCODE::F64_SUB, {}});
            else
                error("subtraction on unsupported type");
            return;
        } else if (op.value == "*") {
            if (t == "i32")
                program.push({bvm::OPCODE::I32_MULT, {}});
            else if (t == "i64")
                program.push({bvm::OPCODE::I64_MULT, {}});
            else if (t == "f32")
                program.push({bvm::OPCODE::F32_MULT, {}});
            else if (t == "f64")
                program.push({bvm::OPCODE::F64_MULT, {}});
            else
                error("multiplication on unsupported type");
            return;
        } else if (op.value == "/") {
            if (t == "i32")
                program.push({bvm::OPCODE::I32_DIV, {}});
            else if (t == "i64")
                program.push({bvm::OPCODE::I64_DIV, {}});
            else if (t == "f32")
                program.push({bvm::OPCODE::F32_DIV, {}});
            else if (t == "f64")
                program.push({bvm::OPCODE::F64_DIV, {}});
            else
                error("division on unsupported type");
            return;
        } else if (op.value == "%") {
            if (t == "i32")
                program.push({bvm::OPCODE::I32_MOD, {}});
            else if (t == "i64")
                program.push({bvm::OPCODE::I64_MOD, {}});
            else
                error("Modulo operator not supported for floating points");
            return;
        }

        if (t == "f32")
            program.push({bvm::OPCODE::F32_CMP, {}});
        else if (t == "f64")
            program.push({bvm::OPCODE::F64_CMP, {}});
        else if (t == "i32")
            program.push({bvm::OPCODE::I32_CMP, {}});
        else
            program.push({bvm::OPCODE::I64_CMP, {}});
        if (op.value == "==")
            program.push({bvm::OPCODE::PE, {}});
        else if (op.value == "!=")
            program.push({bvm::OPCODE::PNE, {}});
        else if (op.value == "<")
            program.push({bvm::OPCODE::PLT, {}});
        else if (op.value == ">")
            program.push({bvm::OPCODE::PGT, {}});
        else if (op.value == "<=")
            program.push({bvm::OPCODE::PLE, {}});
        else if (op.value == ">=")
            program.push({bvm::OPCODE::PGE, {}});
        else
            error("Unsupported binary operator: " + op.value);
    }
};
class UnaryExprAST : public ExprAST {
  public:
    Token op;
    std::unique_ptr<ExprAST> operand;
    UnaryExprAST(Token o, std::unique_ptr<ExprAST> opd) : op(o), operand(std::move(opd)) {}
    virtual void print(int indent = 0) override {
        printSpace(indent);
        std::cout << "UnaryExprAST: " << std::endl;
        indent += 2;
        printSpace(indent);
        std::cout << "op: " << (op) << std::endl;
        operand->print(indent);
    }
    virtual std::string evaltype(Program &program) override {
        std::string retval = operand->evaltype(program);
        return retval;
    }
    virtual std::vector<std::string> get_dependencies() override { return operand->get_dependencies(); }
    virtual void codegen(Program &program) override {
        std::string retval = operand->evaltype(program);
        operand->codegen(program);
        if (op == "-") {
            if (retval[0] == 'u')
                error("can't use negation operator on unsigned types");
            if (retval == "f32") {
                program.push({bvm::OPCODE::F32_NEGATE, {}});
            } else if (retval == "f64") {
                program.push({bvm::OPCODE::F64_NEGATE, {}});
            } else if (retval == "i32") {
                program.push({bvm::OPCODE::I32_NEGATE, {}});
            } else {
                program.push({bvm::OPCODE::I64_NEGATE, {}});
            }
        } else if (op == "+") {
            // do nothing???
        } else if (op == "!") {
            program.push({bvm::OPCODE::BOOL_NOT});
        }
    }
};

class BooleanExprAST : public ExprAST {
  public:
    Token boolean;
    BooleanExprAST(Token b) : boolean(b) {}
    virtual void print(int indent = 0) override {
        printSpace(indent);
        std::cout << "BooleanExprAST: " << std::endl;
        indent += 2;
        printSpace(indent);
        std::cout << "bool: " << (boolean) << std::endl;
    }
    virtual std::vector<std::string> get_dependencies() override { return {}; }
    virtual std::string evaltype(Program &program) override { return "bool"; }
    virtual void codegen(Program &program) override {
        bvm::Value v;
        if (boolean == "false") {
            program.push({bvm::OPCODE::PUSH, {0}});
        } else if (boolean == "true") {
            program.push({bvm::OPCODE::PUSH, {1}});
        } else {
            error("boolean value not equal to true or false");
        }
    }
};
class NumberExprAST : public ExprAST {
  public:
    Token number;
    NumberExprAST(Token n) : number(n) {}
    virtual void print(int indent = 0) override {
        printSpace(indent);
        std::cout << "NumberExprAST: " << std::endl;
        indent += 2;
        printSpace(indent);
        std::cout << "bool: " << (number) << std::endl;
    }
    virtual bool implicit_conversion() override { return true; }
    virtual std::vector<std::string> get_dependencies() override { return {}; }
    virtual std::string evaltype(Program &program) override {
        Value vt = valuetype(number);
        if (vt == INT) {
            long long val = std::stoll(number.value);
            if (val <= 2147483647LL && val >= -2147483648LL) {
                return "i32";
            }
            return "i64";
        } else if (vt == FLOAT)
            return "f32";
        else if (vt == DOUBLE)
            return "f64";
        else
            error("currently unsupported value");
    }
    virtual void codegen(Program &program) override {
        bvm::Value v;
        Value vt = valuetype(number);
        // todo
        if (vt == DOUBLE) {
            // double d=valueToDouble(number.value);
            union {
                double f;
                uint64_t u;
            } some;
            some.u = 0;
            some.f = std::stold(number.value);
            program.push({bvm::OPCODE::PUSH, {some.u}});
            return;
        } else if (vt == INT) {
            uint64_t n = std::stoull(number.value);
            program.push({bvm::OPCODE::PUSH, {n}});
            return;
        } else if (vt == FLOAT) {
            union {
                float f;
                uint64_t u;
            } some;
            some.u = 0;
            some.f = static_cast<float>(std::stold(number.value));
            program.push({bvm::OPCODE::PUSH, {some.u}});
            return;
        }
        number.error("couldn't resolve number literal");
    }
};

class StringExprAST : public ExprAST {
  public:
    Token str;
    StringExprAST(Token s) : str(s) {}
    virtual void print(int indent) override {
        printSpace(indent);
        std::cout << "StringExprAST: " << std::endl;
        indent += 2;
        printSpace(indent);
        std::cout << "string: " << str << std::endl;
    }
    virtual std::vector<std::string> get_dependencies() override { return {}; }
    virtual std::string evaltype(Program &program) override { return "string"; }
    virtual void codegen(Program &program) override {
        uint64_t len = str.value.size() - 2;
        program.push({bvm::OPCODE::PUSH, {len}});
        uint64_t data_ptr = program.data_size();
        program.push({bvm::OPCODE::STRING_FROM, {data_ptr}});
        program.data_push(str.value.substr(1, len));
    }
};

class IdentifierExprAST : public ExprAST {
  public:
    Token identifier;
    std::string pkg_name;
    IdentifierExprAST(Token id, std::string pkg) : identifier(id), pkg_name(pkg) {}
    virtual void print(int indent = 0) override {
        printSpace(indent);
        std::cout << "IdentifierExprAST: " << std::endl;
        indent += 2;
        printSpace(indent);
        std::cout << "identifier: " << (identifier) << std::endl;
    }
    virtual std::string evaltype(Program &program) override { return program.gettype(identifier.value, pkg_name); }
    virtual void codegen(Program &program) override {
        uint64_t ind = std::bit_cast<uint64_t>(program.getaddress(identifier.value, pkg_name));
        program.push({bvm::OPCODE::LOAD, {ind}});
    }
    virtual std::vector<std::string> get_dependencies() override {
        if (identifier.value.find('.') != std::string::npos)
            return {identifier.value};
        return {pkg_name + "." + identifier.value};
    }
};

class CallExprAST : public ExprAST {
  public:
    Token callee;
    std::vector<std::unique_ptr<ExprAST>> args;
    std::string pkg_name;
    CallExprAST(Token c, std::vector<std::unique_ptr<ExprAST>> a, std::string pkg)
        : pkg_name(pkg), callee(c), args(std::move(a)) {}
    virtual void print(int indent = 0) override {
        printSpace(indent);
        std::cout << "CallExprAST: " << std::endl;
        indent += 2;
        printSpace(indent);
        std::cout << "callee: " << callee << std::endl;
        for (auto &&i : args) {
            i->print(indent);
        }
    }
    virtual std::vector<std::string> get_dependencies() override {
        std::vector<std::string> deps;
        for (auto &a : args) {
            std::vector<std::string> d = a->get_dependencies();
            deps.insert(deps.end(), d.begin(), d.end());
        }
        return deps;
    }
    virtual std::string evaltype(Program &program) override {
        Function func = program.get_function(callee.value, pkg_name);
        return func.ret_type;
    }
    virtual void codegen(Program &program) override {
        Function resolved_func = program.get_function(callee.value, pkg_name);
        for (size_t i = 0; i < args.size(); i++) {
            if (i >= resolved_func.argtypes.size()) {
                error("not enough arguments provided");
            }
            if (args[i]->evaltype(program) != resolved_func.argtypes[i]) {
                error("arguments in call list do not match the function signature.");
            }
            args[i]->codegen(program);
        }
        // Function func = program.get_function(callee.value);
        program.push_call(resolved_func.name);
        // program.push({bvm::OPCODE::CALL, {func.ip}});
    }
};

class GlobalDeclarationAST : public GlobalStatementAST {
  public:
    Token identifier;
    std::string pkg_name;
    std::unique_ptr<ExprAST> expr;
    GlobalDeclarationAST(Token id, std::unique_ptr<ExprAST> e, std::string pkg)
        : pkg_name(pkg), identifier(id), expr(std::move(e)) {}
    virtual void print(int indent = 0) override {
        printSpace(indent);
        std::cout << "GlobalDeclarationAST: " << std::endl;
        indent += 2;
        printSpace(indent);
        std::cout << "identifier: " << identifier << std::endl;
        expr->print(indent);
    }
    virtual void codegen(Program &program) override {
        std::string type = expr->evaltype(program);
        Identifier i = {identifier.value, type};
        expr->codegen(program);
        program.declare(i);
        program.push({bvm::OPCODE::STORE, {std::bit_cast<uint64_t>(program.getaddress(i.name, pkg_name))}});
    }
};
class DeclarationAST : public StatementAST {
  public:
    Token identifier;
    std::string pkg_name;
    std::unique_ptr<ExprAST> expr;
    DeclarationAST(Token id, std::unique_ptr<ExprAST> e, std::string pkg)
        : pkg_name(pkg), identifier(id), expr(std::move(e)) {}
    virtual void print(int indent = 0) override {
        printSpace(indent);
        std::cout << "DeclarationAST: " << std::endl;
        indent += 2;
        printSpace(indent);
        std::cout << "identifier: " << identifier << std::endl;
        expr->print(indent);
    }
    virtual void codegen(Program &program) override {
        std::string type = expr->evaltype(program);
        Identifier i = {identifier.value, type};
        expr->codegen(program);
        program.declare(i);
        program.push({bvm::OPCODE::STORE, {std::bit_cast<uint64_t>(program.getaddress(i.name, pkg_name))}});
    }
};

class AssignmentAST : public StatementAST {
  public:
    Token identifier;
    std::unique_ptr<ExprAST> expr;
    std::string pkg_name;
    AssignmentAST(Token id, std::unique_ptr<ExprAST> e, std::string pkg)
        : pkg_name(pkg), identifier(id), expr(std::move(e)) {}
    virtual void print(int indent = 0) override {
        printSpace(indent);
        std::cout << "AssignmentAST: " << std::endl;
        indent += 2;
        printSpace(indent);
        std::cout << "callee: " << identifier << std::endl;
        expr->print(indent);
    }
    virtual void codegen(Program &program) override {
        std::string type = expr->evaltype(program);
        std::string iden_type = program.gettype(identifier.value, pkg_name);
        if (type != iden_type) {
            error("attempt to assign " + type + " to " + identifier.value + " of type " + iden_type +
                                     " failed.");
        }
        expr->codegen(program);
        program.push({bvm::OPCODE::STORE, {std::bit_cast<uint64_t>(program.getaddress(identifier.value, pkg_name))}});
    }
};

class PrototypeAST : public StatementAST {
  public:
    std::string pkg_name;
    std::vector<std::pair<Token, Token>> args;
    PrototypeAST(std::vector<std::pair<Token, Token>> a, std::string pkg) : pkg_name(pkg), args(std::move(a)) {}
    virtual void print(int indent = 0) override {
        printSpace(indent);
        std::cout << "PrototypeAST: " << std::endl;
        indent += 2;
        for (auto &i : args) {
            printSpace(indent);
            std::cout << i.first << " " << i.second << std::endl;
        }
    }
    std::vector<std::string> type_list() {
        std::vector<std::string> v;
        for (auto &i : args)
            v.push_back(i.first.value);
        return v;
    }
    virtual void codegen(Program &program) override {
        if (args.size() == 0)
            return;
        for (ssize_t i = static_cast<int64_t>(args.size()) - 1; i >= 0; i--) {
            Identifier iden{args[i].second.value, args[i].first.value};
            program.declare(iden);
            program.push({bvm::OPCODE::STORE, std::bit_cast<uint64_t>(program.getaddress(iden.name, pkg_name))});
        }
    }
};

class BlockAST : public StatementAST {
  private:
    std::vector<std::unique_ptr<StatementAST>> statements;

  public:
    BlockAST() = default;
    void addStatement(std::unique_ptr<StatementAST> stmt) { statements.push_back(std::move(stmt)); }
    virtual void print(int indent = 0) override {
        printSpace(indent);
        std::cout << "BlockAST: " << std::endl;
        indent += 2;
        for (auto &&i : statements) {
            i->print(indent);
        }
    }
    virtual void codegen(Program &program) override {
        // to see if i have to leave this empty or not
        //
        // NOTE TO FUTURE ME:
        // im thinking i might run into problems with PrototypeAST pushing
        // defining new args if i make proto push to a scope and i make block
        // also push to a scope block will be able to overshadow variables i
        // shouldn't be able to? so yea, BlockAST will be called with a
        // different overload
        // If you think of a soln then remove ts
        error("this method is not meant to be called");
    }
    void codegen(Program &program, bool push_scope) {
        if (push_scope)
            program.new_scope();
        for (auto &&i : statements)
            i->codegen(program);
        if (push_scope)
            program.delete_scope();
    }
};
class ExternFunctionAST : public GlobalStatementAST {
  public:
    Token name;
    std::unique_ptr<PrototypeAST> proto;
    Token returnType;
    std::string pkg_name;
    ExternFunctionAST(Token n, std::unique_ptr<PrototypeAST> p, Token r, std::string pkg)
        : name(n), proto(std::move(p)), returnType(r), pkg_name(pkg) {}
    virtual void print(int indent = 0) override {
        printSpace(indent);
        std::cout << "ExternFunctionAST: " << std::endl;
        indent += 2;
        printSpace(indent);
        std::cout << "name: " << name << std::endl;
        printSpace(indent);
        std::cout << "returnType: " << returnType << std::endl;
        proto->print(indent);
    }
    virtual void codegen(Program &program) override {
        std::string resolved_name = pkg_name + "." + name.value;
        program.declare_function({UINT64_MAX, resolved_name, proto->type_list(), returnType.value, true});
    }
};
class FunctionAST : public GlobalStatementAST {
  public:
    Token name;
    std::unique_ptr<PrototypeAST> proto;
    Token returnType;
    std::string pkg_name;
    std::unique_ptr<BlockAST> body;
    FunctionAST(Token n, std::unique_ptr<PrototypeAST> p, Token r, std::unique_ptr<BlockAST> b, std::string pkg)
        : name(n), proto(std::move(p)), returnType(r), body(std::move(b)), pkg_name(pkg) {}
    virtual void print(int indent = 0) override {
        printSpace(indent);
        std::cout << "FunctionAST: " << std::endl;
        indent += 2;
        printSpace(indent);
        std::cout << "name: " << name << std::endl;
        printSpace(indent);
        std::cout << "returnType: " << returnType << std::endl;
        proto->print(indent);
        body->print(indent);
    }
    virtual void codegen(Program &program) override {
        std::string resolved_name = pkg_name + "." + name.value;
        program.push_in_func(resolved_name);
        const bool push_scope = false;

        // kinda useless now but ok
        // EDIT: i CANNOT remember what above comment was for :skull: (pretend ts is a skull)
        uint64_t ip = program.get_ip();
        Function func{ip, resolved_name, proto->type_list(), returnType.value};
        program.declare_function(func);
        program.new_scope();
        proto->codegen(program);
        body->codegen(program, push_scope);

        program.push({bvm::OPCODE::RET, {}});
        // generates redundant instruction for function calls, but DO NOT remove it
        // the instruction is required in normal scope, just leave it be for now
        // optimisation comes later
        program.delete_scope();
        program.push_in_func("");
    }
};
class ConditionalAST : public StatementAST {
  public:
    std::unique_ptr<ExprAST> ifCondition;
    std::unique_ptr<BlockAST> ifBlock;
    std::vector<std::pair<std::unique_ptr<ExprAST>, std::unique_ptr<BlockAST>>> elseIfs;
    std::unique_ptr<BlockAST> elseBlock;
    ConditionalAST(std::unique_ptr<ExprAST> c, std::unique_ptr<BlockAST> b)
        : ifCondition(std::move(c)), ifBlock(std::move(b)) {}
    virtual void print(int indent = 0) override {
        printSpace(indent);
        std::cout << "ConditionalAST: " << std::endl;
        indent += 2;
        printSpace(indent);
        std::cout << "If Condition: " << std::endl;
        ifCondition->print(indent + 2);
        printSpace(indent);
        std::cout << "If Block: " << std::endl;
        ifBlock->print(indent + 2);
        for (auto &&i : elseIfs) {
            printSpace(indent);
            std::cout << "Else If Condition: " << std::endl;
            i.first->print(indent + 2);
            printSpace(indent);
            std::cout << "Else If Block: " << std::endl;
            i.second->print(indent + 2);
        }
        if (elseBlock) {
            printSpace(indent);
            std::cout << "Else Block: " << std::endl;
            elseBlock->print(indent + 2);
        }
    }
    virtual void codegen(Program &program) override {
        const bool push_scope = true;
        std::vector<size_t> end_jumps;
        ifCondition->codegen(program);
        if (ifCondition->evaltype(program) != "bool")
            error("cannot evaluate expression to type bool");
        size_t jnc = program.size();
        program.push({bvm::OPCODE::JNC, {}});
        ifBlock->codegen(program, push_scope);
        end_jumps.push_back(program.size());
        program.push({bvm::OPCODE::JMP, {}});
        program[jnc].operands[0] = program.size();
        for (auto &&i : elseIfs) {
            if (i.first->evaltype(program) != "bool")
                error("cannot evaluate expression to type bool");
            i.first->codegen(program);
            jnc = program.size();
            program.push({bvm::OPCODE::JNC, {}});
            i.second->codegen(program, push_scope);
            program[jnc].operands[0] = program.size();
            end_jumps.push_back(program.size());
            program.push({bvm::OPCODE::JMP, {}});
        }
        if (elseBlock != nullptr)
            elseBlock->codegen(program, push_scope);
        for (auto &i : end_jumps) {
            program[i].operands[0] = program.size();
        }
    }
};
class WhileAST : public StatementAST {
  public:
    std::unique_ptr<ExprAST> condition;
    std::unique_ptr<BlockAST> body;
    WhileAST(std::unique_ptr<ExprAST> c, std::unique_ptr<BlockAST> b) : condition(std::move(c)), body(std::move(b)) {}
    virtual void print(int indent = 0) override {
        printSpace(indent);
        std::cout << "WhileAST: " << std::endl;
        indent += 2;
        printSpace(indent);
        std::cout << "Condition: " << std::endl;
        condition->print(indent + 2);
        printSpace(indent);
        std::cout << "Body: " << std::endl;
        body->print(indent + 2);
    }
    virtual void codegen(Program &program) override {
        const bool push_scope = true;
        size_t while_start = program.size();
        if (condition->evaltype(program) != "bool")
            error("cannot evaluate expression to type bool");
        condition->codegen(program);
        size_t jnc = program.size();
        program.push({bvm::OPCODE::JNC, {}});
        body->codegen(program, push_scope);
        program.push({bvm::OPCODE::JMP, while_start});
        program[jnc].operands[0] = program.size();
    }
};

class ForAST : public StatementAST {
  public:
    std::unique_ptr<StatementAST> init;
    std::unique_ptr<ExprAST> condition;
    // i = i+1 and stuff doesnt work, so changed to statement
    std::unique_ptr<StatementAST> update;
    std::unique_ptr<BlockAST> body;
    ForAST(std::unique_ptr<StatementAST> init, std::unique_ptr<ExprAST> condition, std::unique_ptr<StatementAST> update,
           std::unique_ptr<BlockAST> body)
        : init(std::move(init)), condition(std::move(condition)), update(std::move(update)), body(std::move(body)) {}
    virtual void print(int indent = 0) override {
        printSpace(indent);
        std::cout << "ForAST: " << std::endl;
        indent += 2;
        printSpace(indent);
        std::cout << "Init: " << std::endl;
        init->print(indent + 2);
        printSpace(indent);
        std::cout << "Condition: " << std::endl;
        condition->print(indent + 2);
        printSpace(indent);
        std::cout << "Update: " << std::endl;
        update->print(indent + 2);
        printSpace(indent);
        std::cout << "Body: " << std::endl;
        body->print(indent + 2);
    }
    virtual void codegen(Program &program) override {
        const bool push_scope = false;
        program.new_scope();
        init->codegen(program);
        size_t for_start = program.size();
        if (condition->evaltype(program) != "bool")
            error("cannot evaluate expression to type bool");
        condition->codegen(program);
        size_t jnc = program.size();
        program.push({bvm::OPCODE::JNC, {}});
        body->codegen(program, push_scope);
        update->codegen(program);
        program.push({bvm::OPCODE::JMP, for_start});
        program[jnc].operands[0] = program.size();
        program.delete_scope();
    }
};

class ReturnAST : public StatementAST {
  public:
    std::unique_ptr<ExprAST> expr;
    ReturnAST(std::unique_ptr<ExprAST> expr) : expr(std::move(expr)) {}
    virtual void print(int indent = 0) override {
        printSpace(indent);
        std::cout << "ReturnAST: " << std::endl;
        indent += 2;
        if (expr)
            expr->print(indent);
    }
    virtual void codegen(Program &program) override {
        if (expr != nullptr)
            expr->codegen(program);
        program.push_undeclare_for_return();
        program.push({bvm::OPCODE::RET, {}});
    }
};

class BreakContinueAST : public StatementAST {
  public:
    Token keyword;
    BreakContinueAST(Token keyword) : keyword(keyword) {}
    virtual void print(int indent = 0) override {
        printSpace(indent);
        std::cout << "BreakContinueAST: " << std::endl;
        indent += 2;
        printSpace(indent);
        std::cout << "keyword: " << keyword << std::endl;
    }
    virtual void codegen(Program &program) override { error("break/continue not implemented"); }
};
class ImportAST : public StatementAST {
  public:
    std::string pkg;
    std::string alias;
    ImportAST(std::string pkg, std::string alias) : pkg(pkg), alias(alias) {}
    virtual void print(int indent = 0) override {
        printSpace(indent);
        std::cout << "ImportAST: ";
        std::cout << "import alias:" << alias << "package_name: " << pkg << std::endl;
    }
    virtual void codegen(Program &program) override {}
};
class PackageAST : public StatementAST {
  public:
    std::string package_name;
    PackageAST(Token name) : package_name(name.value) {}
    virtual void print(int indent = 0) override {
        printSpace(indent);
        std::cout << "PackageAST: ";
        std::cout << "package name: " << package_name << std::endl;
    }
    virtual void codegen(Program &program) override {}
};

// i dont want to inherit statement in expr, thats it
class JustExprAST : public StatementAST {
    std::unique_ptr<ExprAST> expr;

  public:
    JustExprAST(std::unique_ptr<ExprAST> expr) : expr(std::move(expr)) {}
    virtual void print(int indent = 0) override {
        printSpace(indent);
        std::cout << "BLAH BLAH" << std::endl;
    }
    virtual void codegen(Program &program) override { expr->codegen(program); }
};
class ProgramAST : public AST {
  public:
    std::string package;
    std::vector<std::unique_ptr<GlobalStatementAST>> statements;
    ProgramAST() = default;
    void definePackage(std::unique_ptr<PackageAST> pk) { package = pk->package_name; }
    void addStatement(std::unique_ptr<GlobalStatementAST> stmt) { statements.push_back(std::move(stmt)); }
    void print(int indent = 0) {
        printSpace(indent);
        std::cout << "ProgramAST: " << std::endl;
        indent += 2;
        for (auto &&i : statements) {
            i->print(indent);
        }
    }
    void codegen(Program &program) {
        for (auto &&i : statements)
            i->codegen(program);
    }
};
