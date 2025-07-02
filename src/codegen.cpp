#include "codeutil.cpp"
#include <memory>
#include <string>
class StatementAST {
    public:
    virtual std::string codegen()=0;
};
class ExprAST: public StatementAST {
    public:
};
class BinaryExprAST : public ExprAST {
public:
    std::unique_ptr<ExprAST> lhs;
    Token op;
    std::unique_ptr<ExprAST> rhs;
    BinaryExprAST(std::unique_ptr<ExprAST> l, Token o, std::unique_ptr<ExprAST> r)
        : lhs(std::move(l)), op(o), rhs(std::move(r)) {}
    std::string codegen() override {
    }
};
class UnaryExprAST : public ExprAST {
public:
    Token op;
    std::unique_ptr<ExprAST> operand;
    UnaryExprAST(Token o,std::unique_ptr<ExprAST> opd):op(o),operand(std::move(opd)) {}
    virtual std::string codegen() override {
    }
};
class BooleanExprAST : public ExprAST {
public:
    Token boolean;
BooleanExprAST(Token b):boolean(b){}
    virtual std::string codegen() override {
    }
};
class NumberExprAST : public ExprAST {
public:
    Token number;
    NumberExprAST(Token n): number(n) {}
    virtual std::string codegen() override {
    }
};
class StringExprAST : public ExprAST {
public:
    Token str;
    StringExprAST(Token s): str(s) {}
    virtual std::string codegen() override {
    }
};
class IdentifierExprAST : public ExprAST {
public:
    Token identifier;
    IdentifierExprAST(Token id):identifier(id){}
    virtual std::string codegen() override {
    }
};
class CallExprAST : public ExprAST {
public:
    Token callee;
    std::vector<std::unique_ptr<ExprAST>> args;
    CallExprAST(Token c,std::vector<std::unique_ptr<ExprAST>> a): callee(c), args(std::move(a)) {}
    virtual std::string codegen() override {
    }
};
class DeclarationAST : public StatementAST {
public:
    Token identifier;
std::unique_ptr<ExprAST> expr;
    DeclarationAST(Token id,std::unique_ptr<ExprAST> e):identifier(id),expr(std::move(e)){}
    virtual std::string codegen() override {
    }
};
class AssignmentAST : public StatementAST {
public:
    Token identifier;
std::unique_ptr<ExprAST> expr;
    AssignmentAST(Token id,std::unique_ptr<ExprAST> e):identifier(id),expr(std::move(e)){}
    virtual std::string codegen() override {
    }
};
class PrototypeAST {
public:
    std::vector<std::pair<Token,Token>> args;
    PrototypeAST(std::vector<std::pair<Token,Token>> a):args(std::move(a)){}
    virtual std::string codegen() const {
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
    std::string codegen() override {
    }
};
class FunctionAST : public StatementAST {
public:
    Token name;
std::unique_ptr<PrototypeAST> proto;
Token returnType;
std::unique_ptr<BlockAST> body;
    FunctionAST(Token n,std::unique_ptr<PrototypeAST> p,Token r,std::unique_ptr<BlockAST> b)
        :name(n),proto(std::move(p)),returnType(r),body(std::move(b)){}
    virtual std::string codegen() override {
    }
};
class ConditionalAST : public StatementAST {
public:
    std::unique_ptr<ExprAST> ifCondition;
std::unique_ptr<BlockAST> ifBlock;
    std::vector<std::pair<std::unique_ptr<ExprAST>,std::unique_ptr<BlockAST>>> elseIfs;
    std::unique_ptr<BlockAST> elseBlock;
    ConditionalAST(std::unique_ptr<ExprAST> c,std::unique_ptr<BlockAST> b):ifCondition(std::move(c)),ifBlock(std::move(b)){}
    virtual std::string codegen() override {
    }
};
class WhileAST : public StatementAST {
public:
    std::unique_ptr<ExprAST> condition;
std::unique_ptr<BlockAST> body;
    WhileAST(std::unique_ptr<ExprAST> c,std::unique_ptr<BlockAST> b):condition(std::move(c)),body(std::move(b)){}
    virtual std::string codegen() override {
    }
};
class ForAST : public StatementAST {
public:
    std::unique_ptr<StatementAST> init;
    std::unique_ptr<ExprAST> condition;
    std::unique_ptr<ExprAST> update;
    std::unique_ptr<BlockAST> body;
    virtual std::string codegen() override {
    }
    ForAST(std::unique_ptr<StatementAST> init,
           std::unique_ptr<ExprAST> condition,
           std::unique_ptr<ExprAST> update,
           std::unique_ptr<BlockAST> body)
        : init(std::move(init)), condition(std::move(condition)), update(std::move(update)), body(std::move(body)) {}
};
class ReturnAST : public StatementAST {
public:
    std::unique_ptr<ExprAST> expr;
    virtual std::string codegen() override {
    }
    ReturnAST(std::unique_ptr<ExprAST> expr)
        : expr(std::move(expr)) {}
};
class BreakContinueAST : public StatementAST {
public:
    Token keyword;
    virtual std::string codegen() override {
    }
    BreakContinueAST(Token keyword)
        : keyword(keyword) {}
};
class ProgramAST {
public:
    std::vector<std::unique_ptr<StatementAST>> statements;
    ProgramAST() = default;
    void addStatement(std::unique_ptr<StatementAST> stmt) {
        statements.push_back(std::move(stmt));
    }
    std::string codegen() {
        std::string code="section .text\n"
                         "global _start:\n";
        for (auto&& i:statements) {
            code+=i->codegen();
        }
        code+="_start:\n"
              "call main\n"
              "mov rdi,rax\n"
              "mov rax, 60\n"
              "syscall";
        return code;
    }
};
