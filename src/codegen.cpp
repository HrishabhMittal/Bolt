#include "codeutil.cpp"
#include "header.hpp"
#include "vm.hpp"
#include <bvm.hpp>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

struct Identifier {
    std::string name;
    std::string type;
};
struct Program {
    std::vector<bvm::instruction> code;
    std::map<std::string, std::vector<uint64_t>> identifier_map;
    std::vector<std::vector<Identifier>> scope;
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
    virtual std::string evaltype() = 0;
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
    virtual std::string evaltype() override {
        static std::string retval = "";
        if (retval != "")
            return retval;
        auto ltype = lhs->evaltype();
        auto rtype = rhs->evaltype();
        if (ltype != rtype) {
            throw std::runtime_error("Type mismatch in BinaryExprAST");
        }
        if (op.value == "==" || op.value == "!=" || op.value == "<" ||
            op.value == ">" || op.value == "<=" || op.value == ">=") {
            retval = "bool";
            return "bool";
        }
        retval = ltype;
        return ltype;
    }
    virtual void codegen(Program &program) override {
        lhs->codegen(program);
        rhs->codegen(program);
        auto retval1 = evaltype();
        auto retval2 = evaltype();
        if (retval1 != retval2) {
            throw std::runtime_error("operation on \"" + retval1 + "\" and \"" +
                                     retval2 + "\" is not supported");
        }

        std::string t = retval1;

        if (op.value == "+") {
            if (t == "i32")
                program.code.push_back({bvm::OPCODE::I32_ADD, {}});
            else if (t == "i64")
                program.code.push_back({bvm::OPCODE::I64_ADD, {}});
            else if (t == "f32")
                program.code.push_back({bvm::OPCODE::F32_ADD, {}});
            else if (t == "f64")
                program.code.push_back({bvm::OPCODE::F64_ADD, {}});
        } else if (op.value == "-") {
            if (t == "i32")
                program.code.push_back({bvm::OPCODE::I32_SUB, {}});
            else if (t == "i64")
                program.code.push_back({bvm::OPCODE::I64_SUB, {}});
            else if (t == "f32")
                program.code.push_back({bvm::OPCODE::F32_SUB, {}});
            else if (t == "f64")
                program.code.push_back({bvm::OPCODE::F64_SUB, {}});
        } else if (op.value == "*") {
            if (t == "i32")
                program.code.push_back({bvm::OPCODE::I32_MULT, {}});
            else if (t == "i64")
                program.code.push_back({bvm::OPCODE::I64_MULT, {}});
            else if (t == "f32")
                program.code.push_back({bvm::OPCODE::F32_MULT, {}});
            else if (t == "f64")
                program.code.push_back({bvm::OPCODE::F64_MULT, {}});
        } else if (op.value == "/") {
            if (t == "i32")
                program.code.push_back({bvm::OPCODE::I32_DIV, {}});
            else if (t == "i64")
                program.code.push_back({bvm::OPCODE::I64_DIV, {}});
            else if (t == "f32")
                program.code.push_back({bvm::OPCODE::F32_DIV, {}});
            else if (t == "f64")
                program.code.push_back({bvm::OPCODE::F64_DIV, {}});
        } else if (op.value == "%") {
            if (t == "i32")
                program.code.push_back({bvm::OPCODE::I32_MOD, {}});
            else if (t == "i64")
                program.code.push_back({bvm::OPCODE::I64_MOD, {}});
            else
                throw std::runtime_error(
                    "Modulo operator not supported for floating points");
        }

        if (t == "f32")
            program.code.push_back({bvm::OPCODE::F32_CMP, {}});
        else if (t == "f64")
            program.code.push_back({bvm::OPCODE::F64_CMP, {}});
        else if (t == "i32")
            program.code.push_back({bvm::OPCODE::I32_CMP, {}});
        else
            program.code.push_back({bvm::OPCODE::I64_CMP, {}});

        if (op.value == "==")
            program.code.push_back({bvm::OPCODE::PE, {}});
        else if (op.value == "!=")
            program.code.push_back({bvm::OPCODE::PNE, {}});
        else if (op.value == "<")
            program.code.push_back({bvm::OPCODE::PLT, {}});
        else if (op.value == ">")
            program.code.push_back({bvm::OPCODE::PGT, {}});
        else if (op.value == "<=")
            program.code.push_back({bvm::OPCODE::PLE, {}});
        else if (op.value == ">=")
            program.code.push_back({bvm::OPCODE::PGE, {}});
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
    virtual std::string evaltype() override {
        static auto retval = operand->evaltype();
        return retval;
    }
    virtual void codegen(Program &program) override {
        auto retval = operand->evaltype();
        operand->codegen(program);
        if (op == "-") {
            if (retval[0] == 'u')
                throw std::runtime_error(
                    "can't use negation operator on unsigned types");
            if (retval == "f32") {
                program.code.push_back({bvm::OPCODE::F32_NEGATE, {}});
            } else if (retval == "f64") {
                program.code.push_back({bvm::OPCODE::F64_NEGATE, {}});
            } else if (retval == "i32") {
                program.code.push_back({bvm::OPCODE::I32_NEGATE, {}});
            } else {
                program.code.push_back({bvm::OPCODE::I64_NEGATE, {}});
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
    virtual std::string evaltype() override { return "bool"; }
    virtual void codegen(Program &program) override {
        bvm::Value v;
        if (boolean == "false") {
            program.code.push_back({bvm::OPCODE::PUSH, {0}});
        } else if (boolean == "true") {
            program.code.push_back({bvm::OPCODE::PUSH, {1}});
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
    virtual std::string evaltype() override {
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
            program.code.push_back({bvm::OPCODE::PUSH, {some.u}});
        } else if (vt == INT) {
            uint64_t n = std::stoull(number.value);
            program.code.push_back({bvm::OPCODE::PUSH, {n}});
        } else if (vt == FLOAT) {
            union {
                float f;
                uint64_t u;
            } some;
            some.u = 0;
            some.f = static_cast<float>(std::stold(number.value));
            program.code.push_back({bvm::OPCODE::PUSH, {some.u}});
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
    virtual std::string evaltype() override { return "string"; }
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
    virtual std::string evaltype() override { return ""; }
    virtual void codegen(Program &program) override {
        // need to keep track of identifiers and implement this fully
        throw std::runtime_error("todo");
        program.code.push_back({bvm::OPCODE::LOAD, {}});
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
    virtual std::string evaltype() override { return ""; }
    virtual void codegen(Program &program) override {
        for (auto &&i : args) {
            i->codegen(program);
        }
        // need to fix this too
        program.code.push_back({bvm::OPCODE::CALL, {}});

        // ans this
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
        // todo
        throw std::runtime_error("to implement declaration");
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
        throw std::runtime_error("to implement assignment");
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
        // never called
        throw std::runtime_error("to implement after adding identifier stuff");
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
        proto->codegen(program);
        body->codegen(program);
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
    virtual void codegen(Program &program) override {}
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
    virtual void codegen(Program &program) override {}
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
    virtual void codegen(Program &program) override {}
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
        expr->codegen(program);
        program.code.push_back({bvm::OPCODE::RET, {}});
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
