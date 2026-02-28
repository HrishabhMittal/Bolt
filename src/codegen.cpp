#include "codeutil.cpp"
#include <memory>
#include <string>
class StatementAST {
    public:
    virtual void print(int indent=0)=0;
};
void printSpace(int space) {
    for (int i=0;i<space;i++) std::cout<<' ';
} 
class ExprAST: public StatementAST {
    public:
        virtual void print(int indent=0)=0;
};
class BinaryExprAST : public ExprAST {
public:
    std::unique_ptr<ExprAST> lhs;
    Token op;
    std::unique_ptr<ExprAST> rhs;
    BinaryExprAST(std::unique_ptr<ExprAST> l, Token o, std::unique_ptr<ExprAST> r)
        : lhs(std::move(l)), op(o), rhs(std::move(r)) {}
    virtual void print(int indent=0) override {
        printSpace(indent);
        indent+=2;
        std::cout<<"BinaryExprAST: "<<std::endl;
        printSpace(indent);
        std::cout<<"Token: "<<(op)<<std::endl;
        lhs->print(indent);
        rhs->print(indent);
    }
};
class UnaryExprAST : public ExprAST {
public:
    Token op;
    std::unique_ptr<ExprAST> operand;
    UnaryExprAST(Token o,std::unique_ptr<ExprAST> opd):op(o),operand(std::move(opd)) {}
    virtual void print(int indent=0) override {
        printSpace(indent);
        std::cout<<"UnaryExprAST: "<<std::endl;
        indent+=2;
        printSpace(indent);
        std::cout<<"op: "<<(op)<<std::endl;
        operand->print(indent);
    }
};
class BooleanExprAST : public ExprAST {
public:
    Token boolean;
BooleanExprAST(Token b):boolean(b){}
    virtual void print(int indent=0) override {
        printSpace(indent);
        std::cout<<"BooleanExprAST: "<<std::endl;
        indent+=2;
        printSpace(indent);
        std::cout<<"bool: "<<(boolean)<<std::endl;
    }
};
class NumberExprAST : public ExprAST {
public:
    Token number;
    NumberExprAST(Token n): number(n) {}
    virtual void print(int indent=0) override {
        printSpace(indent);
        std::cout<<"NumberExprAST: "<<std::endl;
        indent+=2;
        printSpace(indent);
        std::cout<<"bool: "<<(number)<<std::endl;
    }
};
class StringExprAST : public ExprAST {
public:
    Token str;
    StringExprAST(Token s): str(s) {}
    virtual void print(int indent) override {
        printSpace(indent);
        std::cout<<"StringExprAST: "<<std::endl;
        indent+=2;
        printSpace(indent);
        std::cout<<"bool: "<<str<<std::endl;
    }
};
class IdentifierExprAST : public ExprAST {
public:
    Token identifier;
    IdentifierExprAST(Token id):identifier(id){}
    virtual void print(int indent=0) override {
        printSpace(indent);
        std::cout<<"IdentifierExprAST: "<<std::endl;
        indent+=2;
        printSpace(indent);
        std::cout<<"identifier: "<<(identifier)<<std::endl;
    }
};
class CallExprAST : public ExprAST {
public:
    Token callee;
    std::vector<std::unique_ptr<ExprAST>> args;
    CallExprAST(Token c,std::vector<std::unique_ptr<ExprAST>> a): callee(c), args(std::move(a)) {}
    virtual void print(int indent=0) override {
        printSpace(indent);
        std::cout<<"CallExprAST: "<<std::endl;
        indent+=2;
        printSpace(indent);
        std::cout<<"callee: "<<callee<<std::endl;
        for (auto&& i:args) {
            i->print(indent);
        }
    }
};
class DeclarationAST : public StatementAST {
public:
    Token identifier;
std::unique_ptr<ExprAST> expr;
    DeclarationAST(Token id,std::unique_ptr<ExprAST> e):identifier(id),expr(std::move(e)){}
    virtual void print(int indent=0) override {
        printSpace(indent);
        std::cout<<"DeclarationAST: "<<std::endl;
        indent+=2;
        printSpace(indent);
        std::cout<<"callee: "<<identifier<<std::endl;
        expr->print(indent);
    }
};
class AssignmentAST : public StatementAST {
public:
    Token identifier;
std::unique_ptr<ExprAST> expr;
    AssignmentAST(Token id,std::unique_ptr<ExprAST> e):identifier(id),expr(std::move(e)){}
    virtual void print(int indent=0) override {
        printSpace(indent);
        std::cout<<"AssignmentAST: "<<std::endl;
        indent+=2;
        printSpace(indent);
        std::cout<<"callee: "<<identifier<<std::endl;
        expr->print(indent);
    }
};
class PrototypeAST: public StatementAST {
public:
    std::vector<std::pair<Token,Token>> args;
    PrototypeAST(std::vector<std::pair<Token,Token>> a): args(std::move(a)) {}
    virtual void print(int indent=0) override {
        printSpace(indent);
        std::cout<<"PrototypeAST: "<<std::endl;
        indent+=2;
        for (auto& i:args) {
            printSpace(indent);
            std::cout<<i.first<<" "<<i.second<<std::endl;
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
    virtual void print(int indent=0) override {
        printSpace(indent);
        std::cout<<"PrototypeAST: "<<std::endl;
        indent+=2;
        for (auto&& i:statements) {
            i->print(indent);
        }
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
    virtual void print(int indent=0) override {
        printSpace(indent);
        std::cout<<"FunctionAST: "<<std::endl;
        indent+=2;
        printSpace(indent);
        std::cout<<"name: "<<name<<std::endl;
        printSpace(indent);
        std::cout<<"returnType: "<<returnType<<std::endl;
        proto->print(indent);
        body->print(indent);

    }
};
class ConditionalAST : public StatementAST {
public:
    std::unique_ptr<ExprAST> ifCondition;
    std::unique_ptr<BlockAST> ifBlock;
    std::vector<std::pair<std::unique_ptr<ExprAST>,std::unique_ptr<BlockAST>>> elseIfs;
    std::unique_ptr<BlockAST> elseBlock;
    ConditionalAST(std::unique_ptr<ExprAST> c,std::unique_ptr<BlockAST> b):ifCondition(std::move(c)),ifBlock(std::move(b)){}
    virtual void print(int indent=0) override {
        printSpace(indent);
        std::cout<<"ConditionalAST: "<<std::endl;
        indent+=2;
        printSpace(indent);
        std::cout<<"If Condition: "<<std::endl;
        ifCondition->print(indent+2);
        printSpace(indent);
        std::cout<<"If Block: "<<std::endl;
        ifBlock->print(indent+2);
        for (auto&& i:elseIfs) {
            printSpace(indent);
            std::cout<<"Else If Condition: "<<std::endl;
            i.first->print(indent+2);
            printSpace(indent);
            std::cout<<"Else If Block: "<<std::endl;
            i.second->print(indent+2);
        }
        if (elseBlock) {
            printSpace(indent);
            std::cout<<"Else Block: "<<std::endl;
            elseBlock->print(indent+2);
        }
    }
};
class WhileAST : public StatementAST {
public:
    std::unique_ptr<ExprAST> condition;
    std::unique_ptr<BlockAST> body;
    WhileAST(std::unique_ptr<ExprAST> c,std::unique_ptr<BlockAST> b):condition(std::move(c)),body(std::move(b)){}
    virtual void print(int indent=0) override {
        printSpace(indent);
        std::cout<<"WhileAST: "<<std::endl;
        indent+=2;
        printSpace(indent);
        std::cout<<"Condition: "<<std::endl;
        condition->print(indent+2);
        printSpace(indent);
        std::cout<<"Body: "<<std::endl;
        body->print(indent+2);
    }
};
class ForAST : public StatementAST {
public:
    std::unique_ptr<StatementAST> init;
    std::unique_ptr<ExprAST> condition;
    std::unique_ptr<ExprAST> update;
    std::unique_ptr<BlockAST> body;
    ForAST(std::unique_ptr<StatementAST> init,
           std::unique_ptr<ExprAST> condition,
           std::unique_ptr<ExprAST> update,
           std::unique_ptr<BlockAST> body)
        : init(std::move(init)), condition(std::move(condition)), update(std::move(update)), body(std::move(body)) {}
    virtual void print(int indent=0) override {
        printSpace(indent);
        std::cout<<"ForAST: "<<std::endl;
        indent+=2;
        printSpace(indent);
        std::cout<<"Init: "<<std::endl;
        init->print(indent+2);
        printSpace(indent);
        std::cout<<"Condition: "<<std::endl;
        condition->print(indent+2);
        printSpace(indent);
        std::cout<<"Update: "<<std::endl;
        update->print(indent+2);
        printSpace(indent);
        std::cout<<"Body: "<<std::endl;
        body->print(indent+2);
    }
};
class ReturnAST : public StatementAST {
public:
    std::unique_ptr<ExprAST> expr;
    ReturnAST(std::unique_ptr<ExprAST> expr)
        : expr(std::move(expr)) {}
    virtual void print(int indent=0) override {
        printSpace(indent);
        std::cout<<"ReturnAST: "<<std::endl;
        indent+=2;
        if (expr) expr->print(indent);
    }
};
class BreakContinueAST : public StatementAST {
public:
    Token keyword;
    BreakContinueAST(Token keyword)
        : keyword(keyword) {}
    virtual void print(int indent=0) override {
        printSpace(indent);
        std::cout<<"BreakContinueAST: "<<std::endl;
        indent+=2;
        printSpace(indent);
        std::cout<<"keyword: "<<keyword<<std::endl;
    }
};
class ProgramAST {
public:
    std::vector<std::unique_ptr<StatementAST>> statements;
    ProgramAST() = default;
    void addStatement(std::unique_ptr<StatementAST> stmt) {
        statements.push_back(std::move(stmt));
    }
    void print(int indent=0) {
        printSpace(indent);
        std::cout<<"ProgramAST: "<<std::endl;
        indent+=2;
        for (auto&& i:statements) {
            i->print(indent);
        }
    }
};
