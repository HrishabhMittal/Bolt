#include "codeutil.cpp"
#include "header.hpp"
#include "opcode.hpp"
#include "vm.hpp"
#include <algorithm>
#include <bvm.hpp>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

struct Identifier {
    std::string name;
    std::string type;
};
struct Function {
    uint64_t ip;
    std::string name;
    std::string ret_type;
};
class Program {
    uint64_t main_ip = UINT64_MAX;
    uint64_t iden_stack_size = 0;
    std::vector<bvm::instruction> code;
    std::map<std::string, std::vector<uint64_t>> identifier_map;
    std::vector<std::vector<Identifier>> scope;
    std::vector<std::vector<Function>> func_scope;

  public:
    Program() {
        code.reserve(1024);
        scope.reserve(16);
        func_scope.reserve(16);
        scope.push_back({});
        func_scope.push_back({});
    }
    const std::vector<bvm::instruction> &Code() { return code; }
    uint64_t main() { return main_ip; }
    size_t get_ip() { return code.size(); }
    void push(const bvm::instruction &i) { code.push_back(i); }
    uint64_t size() { return code.size(); }
    bvm::instruction &operator[](size_t ind) { return code[ind]; }
    void new_scope() {
        func_scope.push_back({});
        scope.push_back({});
    }
    void delete_scope() {
        for (auto i : scope.back()) {
            identifier_map[i.name].pop_back();
            iden_stack_size--;
        }
        func_scope.pop_back();
        scope.pop_back();
    }
    void declare_function(Function i) {
        for (auto j : func_scope.back()) {
            if (i.name == j.name)
                throw std::runtime_error("redaclaration of " + i.name +
                                         " in the same scope.");
        }
        if (i.name == "main") {
            if (func_scope.size() != 1)
                throw std::runtime_error(
                    "main was not declared in global scope");
            else {
                main_ip = i.ip;
            }
        }
        func_scope.back().push_back(i);
    }
    Function get_function(std::string name) {
        for (ssize_t i = static_cast<int64_t>(func_scope.size()) - 1; i >= 0;
             i--) {
            for (size_t j = 0; j < func_scope[i].size(); j++) {
                if (func_scope[i][j].name == name)
                    return func_scope[i][j];
            }
        }
        throw std::runtime_error("no declaration found for function " + name);
    }
    uint64_t declare(Identifier i) {
        for (auto j : scope.back()) {
            if (i.name == j.name)
                throw std::runtime_error("redaclaration of " + i.name +
                                         " in the same scope.");
        }
        scope.back().push_back(i);
        identifier_map[i.name].push_back(iden_stack_size);
        return iden_stack_size++;
    }
    uint64_t getaddress(std::string i) {
        if (identifier_map.count(i) && identifier_map[i].size() > 0)
            return identifier_map[i].back();
        else
            throw std::runtime_error("identifier " + i + " doesn't exist.");
    }
    std::string gettype(std::string iden) {
        for (ssize_t i = static_cast<int64_t>(scope.size()) - 1; i >= 0; i--) {
            for (size_t j = 0; j < scope[i].size(); j++) {
                if (scope[i][j].name == iden)
                    return scope[i][j].type;
            }
        }
        throw std::runtime_error("identifier " + iden + " doesn't exist.");
    }
};

class AST {
    virtual void print(int indent = 0) = 0;
    virtual void codegen(Program &program) = 0;
};
class StatementAST : AST {
  public:
    virtual void print(int indent = 0) = 0;
    virtual void codegen(Program &program) = 0;
    virtual ~StatementAST() = default;
};

void printSpace(int space) {
    for (int i = 0; i < space; i++)
        std::cout << ' ';
}

class ExprAST : AST {
  public:
    virtual void print(int indent = 0) = 0;
    virtual void codegen(Program &program) = 0;
    virtual std::string evaltype(Program &program) = 0;
    virtual ~ExprAST() = default;
};

class BinaryExprAST : public ExprAST {
  public:
    std::unique_ptr<ExprAST> lhs;
    Token op;
    std::unique_ptr<ExprAST> rhs;
    BinaryExprAST(std::unique_ptr<ExprAST> l, Token o,
                  std::unique_ptr<ExprAST> r)
        : lhs(std::move(l)), op(o), rhs(std::move(r)) {}
    virtual void print(int indent = 0) override {
        printSpace(indent);
        indent += 2;
        std::cout << "BinaryExprAST: " << std::endl;
        printSpace(indent);
        std::cout << "Token: " << (op) << std::endl;
        lhs->print(indent);
        rhs->print(indent);
    }
    virtual std::string evaltype(Program &program) override {
        auto ltype = lhs->evaltype(program);
        auto rtype = rhs->evaltype(program);
        if (ltype != rtype) {
            throw std::runtime_error(
                "Type mismatch in expression: cannot operate on \"" + ltype +
                "\" and \"" + rtype + "\"");
        }
        if (op.value == "==" || op.value == "!=" || op.value == "<" ||
            op.value == ">" || op.value == "<=" || op.value == ">=") {
            return "bool";
        }
        if (op.value == "%") {
            if (ltype != "i32" && ltype != "i64") {
                throw std::runtime_error("Type error: Modulo operator '%' not "
                                         "supported for type \"" +
                                         ltype + "\"");
            }
            return ltype;
        }
        if (op.value == "+" || op.value == "-" || op.value == "*" ||
            op.value == "/") {
            return ltype;
        }
        throw std::runtime_error("Type error: Unsupported binary operator \"" +
                                 op.value + "\"");
    }
    virtual void codegen(Program &program) override {
        lhs->codegen(program);
        rhs->codegen(program);
        auto retval1 = evaltype(program);
        auto retval2 = evaltype(program);
        if (retval1 != retval2) {
            throw std::runtime_error("operation on \"" + retval1 + "\" and \"" +
                                     retval2 + "\" is not supported");
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
                throw std::runtime_error("addition on unsupported type");
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
                throw std::runtime_error("subtraction on unsupported type");
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
                throw std::runtime_error("multiplication on unsupported type");
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
                throw std::runtime_error("division on unsupported type");
            return;
        } else if (op.value == "%") {
            if (t == "i32")
                program.push({bvm::OPCODE::I32_MOD, {}});
            else if (t == "i64")
                program.push({bvm::OPCODE::I64_MOD, {}});
            else
                throw std::runtime_error(
                    "Modulo operator not supported for floating points");
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
            throw std::runtime_error("Unsupported binary operator: " +
                                     op.value);
    }
};
class UnaryExprAST : public ExprAST {
  public:
    Token op;
    std::unique_ptr<ExprAST> operand;
    UnaryExprAST(Token o, std::unique_ptr<ExprAST> opd)
        : op(o), operand(std::move(opd)) {}
    virtual void print(int indent = 0) override {
        printSpace(indent);
        std::cout << "UnaryExprAST: " << std::endl;
        indent += 2;
        printSpace(indent);
        std::cout << "op: " << (op) << std::endl;
        operand->print(indent);
    }
    virtual std::string evaltype(Program &program) override {
        static auto retval = operand->evaltype(program);
        return retval;
    }
    virtual void codegen(Program &program) override {
        auto retval = operand->evaltype(program);
        operand->codegen(program);
        if (op == "-") {
            if (retval[0] == 'u')
                throw std::runtime_error(
                    "can't use negation operator on unsigned types");
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
    virtual std::string evaltype(Program &program) override { return "bool"; }
    virtual void codegen(Program &program) override {
        bvm::Value v;
        if (boolean == "false") {
            program.push({bvm::OPCODE::PUSH, {0}});
        } else if (boolean == "true") {
            program.push({bvm::OPCODE::PUSH, {1}});
        } else {
            throw std::runtime_error(
                "boolean value not equal to true or false");
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
    virtual std::string evaltype(Program &program) override {
        static auto vt = valuetype(number.value);
        if (vt == INT)
            return "i32";
        else if (vt == FLOAT)
            return "f32";
        else if (vt == DOUBLE)
            return "f64";
        else
            throw std::runtime_error("currently unsupported value");
    }
    virtual void codegen(Program &program) override {
        bvm::Value v;
        Value vt = valuetype(number.value);
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
        throw std::runtime_error("couldn't resolve number literal " +
                                 number.value);
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
        std::cout << "bool: " << str << std::endl;
    }
    virtual std::string evaltype(Program &program) override { return "string"; }
    virtual void codegen(Program &program) override {
        throw std::runtime_error("string support not implemented");
    }
};

class IdentifierExprAST : public ExprAST {
  public:
    Token identifier;
    IdentifierExprAST(Token id) : identifier(id) {}
    virtual void print(int indent = 0) override {
        printSpace(indent);
        std::cout << "IdentifierExprAST: " << std::endl;
        indent += 2;
        printSpace(indent);
        std::cout << "identifier: " << (identifier) << std::endl;
    }
    virtual std::string evaltype(Program &program) override {
        return program.gettype(identifier.value);
    }
    virtual void codegen(Program &program) override {
        uint64_t ind = program.getaddress(identifier.value);
        program.push({bvm::OPCODE::LOAD, {ind}});
    }
};

class CallExprAST : public ExprAST {
  public:
    Token callee;
    std::vector<std::unique_ptr<ExprAST>> args;
    CallExprAST(Token c, std::vector<std::unique_ptr<ExprAST>> a)
        : callee(c), args(std::move(a)) {}
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
    virtual std::string evaltype(Program &program) override {
        Function func = program.get_function(callee.value);
        return func.ret_type;
    }
    virtual void codegen(Program &program) override {
        for (auto &&i : args) {
            i->codegen(program);
        }
        Function func = program.get_function(callee.value);
        program.push({bvm::OPCODE::CALL, {func.ip}});
    }
};

class DeclarationAST : public StatementAST {
  public:
    Token identifier;
    std::unique_ptr<ExprAST> expr;
    DeclarationAST(Token id, std::unique_ptr<ExprAST> e)
        : identifier(id), expr(std::move(e)) {}
    virtual void print(int indent = 0) override {
        printSpace(indent);
        std::cout << "DeclarationAST: " << std::endl;
        indent += 2;
        printSpace(indent);
        std::cout << "callee: " << identifier << std::endl;
        expr->print(indent);
    }
    virtual void codegen(Program &program) override {
        auto type = expr->evaltype(program);
        Identifier i = {identifier.value, type};
        expr->codegen(program);
        program.push({bvm::OPCODE::STORE, {program.declare(i)}});
    }
};

class AssignmentAST : public StatementAST {
  public:
    Token identifier;
    std::unique_ptr<ExprAST> expr;
    AssignmentAST(Token id, std::unique_ptr<ExprAST> e)
        : identifier(id), expr(std::move(e)) {}
    virtual void print(int indent = 0) override {
        printSpace(indent);
        std::cout << "AssignmentAST: " << std::endl;
        indent += 2;
        printSpace(indent);
        std::cout << "callee: " << identifier << std::endl;
        expr->print(indent);
    }
    virtual void codegen(Program &program) override {
        auto type = expr->evaltype(program);
        auto iden_type = program.gettype(identifier.value);
        if (type != iden_type) {
            throw std::runtime_error("attempt to assign " + type + " to " +
                                     identifier.value + " of type " +
                                     iden_type + " failed.");
        }
        expr->codegen(program);
        program.push(
            {bvm::OPCODE::STORE, {program.getaddress(identifier.value)}});
    }
};

class PrototypeAST : public StatementAST {
  public:
    std::vector<std::pair<Token, Token>> args;
    PrototypeAST(std::vector<std::pair<Token, Token>> a) : args(std::move(a)) {}
    virtual void print(int indent = 0) override {
        printSpace(indent);
        std::cout << "PrototypeAST: " << std::endl;
        indent += 2;
        for (auto &i : args) {
            printSpace(indent);
            std::cout << i.first << " " << i.second << std::endl;
        }
    }
    virtual void codegen(Program &program) override {
        if (args.size() == 0)
            return;
        for (ssize_t i = static_cast<int64_t>(args.size()) - 1; i >= 0; i--) {
            Identifier iden{args[i].second.value, args[i].first.value};
            // program.declare(iden);
            program.push({bvm::OPCODE::STORE, program.declare(iden)});
        }
    }
};

class BlockAST : public StatementAST {
  private:
    std::vector<std::unique_ptr<StatementAST>> statements;

  public:
    BlockAST() = default;
    void addStatement(std::unique_ptr<StatementAST> stmt) {
        statements.push_back(std::move(stmt));
    }
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
        throw std::runtime_error("this method is not meant to be called");
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

class FunctionAST : public StatementAST {
  public:
    Token name;
    std::unique_ptr<PrototypeAST> proto;
    Token returnType;
    std::unique_ptr<BlockAST> body;
    FunctionAST(Token n, std::unique_ptr<PrototypeAST> p, Token r,
                std::unique_ptr<BlockAST> b)
        : name(n), proto(std::move(p)), returnType(r), body(std::move(b)) {}
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
        const bool push_scope = false;
        uint64_t ip = program.get_ip();
        Function func{ip, name.value, returnType.value};
        program.declare_function(func);
        program.new_scope();
        proto->codegen(program);
        body->codegen(program, push_scope);
        program.delete_scope();
    }
};
class ConditionalAST : public StatementAST {
  public:
    std::unique_ptr<ExprAST> ifCondition;
    std::unique_ptr<BlockAST> ifBlock;
    std::vector<std::pair<std::unique_ptr<ExprAST>, std::unique_ptr<BlockAST>>>
        elseIfs;
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
            throw std::runtime_error("cannot evaluate expression to type bool");
        size_t jnc = program.size();
        program.push({bvm::OPCODE::JNC, {}});
        ifBlock->codegen(program, push_scope);
        end_jumps.push_back(program.size());
        program.push({bvm::OPCODE::JMP, {}});
        program[jnc].operands[0] = program.size();
        for (auto &&i : elseIfs) {
            if (i.first->evaltype(program) != "bool")
                throw std::runtime_error(
                    "cannot evaluate expression to type bool");
            i.first->codegen(program);
            jnc = program.size();
            program.push({bvm::OPCODE::JNC, {}});
            i.second->codegen(program, push_scope);
            program[jnc].operands[0] = program.size();
            end_jumps.push_back(program.size());
            program.push({bvm::OPCODE::JMP, {}});
        }
        if (elseBlock != nullptr)
            elseBlock->codegen(program);
        for (auto i:end_jumps) {
            program[i].operands[0]=program.size();
        }
    }
};
class WhileAST : public StatementAST {
  public:
    std::unique_ptr<ExprAST> condition;
    std::unique_ptr<BlockAST> body;
    WhileAST(std::unique_ptr<ExprAST> c, std::unique_ptr<BlockAST> b)
        : condition(std::move(c)), body(std::move(b)) {}
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
            throw std::runtime_error("cannot evaluate expression to type bool");
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
    std::unique_ptr<ExprAST> update;
    std::unique_ptr<BlockAST> body;
    ForAST(std::unique_ptr<StatementAST> init,
           std::unique_ptr<ExprAST> condition, std::unique_ptr<ExprAST> update,
           std::unique_ptr<BlockAST> body)
        : init(std::move(init)), condition(std::move(condition)),
          update(std::move(update)), body(std::move(body)) {}
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
            throw std::runtime_error("cannot evaluate expression to type bool");
        condition->codegen(program);
        size_t jnc = program.size();
        program.push({bvm::OPCODE::JNC, {}});
        body->codegen(program, push_scope);
        update->codegen(program);
        program.push({bvm::OPCODE::JMP, for_start});
        program.delete_scope();
        program[jnc].operands[0] = program.size();
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
    virtual void codegen(Program &program) override {
        throw std::runtime_error("break/continue not implemented");
    }
};
class ProgramAST : AST {
  public:
    std::vector<std::unique_ptr<StatementAST>> statements;
    ProgramAST() = default;
    void addStatement(std::unique_ptr<StatementAST> stmt) {
        statements.push_back(std::move(stmt));
    }
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
