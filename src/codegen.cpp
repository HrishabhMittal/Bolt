#include "codeutil.cpp"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <sstream>
#include <stdexcept>

struct VarInfo {
    int bp_offset; 
    Value type;
};

class CodeGenContext {
public:
    std::vector<std::unordered_map<std::string,VarInfo>> scopes;
    int current_stack_offset=0;
    int label_count=0;

    std::vector<std::string> loop_start_labels; 
    std::vector<std::string> loop_end_labels;   

    std::string current_function_name="";

    std::stringstream text_section;
    std::stringstream data_section;

    std::vector<bool> int_regs_used;
    std::vector<bool> float_regs_used;

    CodeGenContext() {
        int_regs_used.resize(calc_registers_64.size(),false);
        float_regs_used.resize(calc_registers_simd.size(),false);
        pushScope(); 
    }

    void pushScope() {
        scopes.push_back({});
    }
    void popScope() {
        scopes.pop_back();
    }

    void declareVar(const std::string& name,Value type) {
        current_stack_offset-=8;
        scopes.back()[name]={current_stack_offset,type};
    }

    VarInfo getVar(const std::string& name) {
        for (auto it=scopes.rbegin();it!=scopes.rend();it++) {
            if (it->find(name)!=it->end()) {
                return it->at(name);
            }
        }
        throw std::runtime_error("Undeclared variable: "+name);
    }

    int allocIntReg() {
        for (size_t i=0;i<int_regs_used.size();i++) {
            if (!int_regs_used[i]) {
                int_regs_used[i]=true;
                return i;
            }
        }
        return -1;
    }
    void freeIntReg(int index) {
        if (index>=0) int_regs_used[index]=false;
    }

    int allocFloatReg() {
        for (size_t i=0;i<float_regs_used.size();i++) {
            if (!float_regs_used[i]) {
                float_regs_used[i]=true;
                return i;
            }
        }
        return -1; 
    }
    void freeFloatReg(int index) {
        if (index>=0) float_regs_used[index]=false;
    }

    std::string newLabel(const std::string& prefix) {
        return prefix+std::to_string(label_count++);
    }
};

struct ExprResult {
    int reg_idx;    
    bool is_float;  
    bool spilled;   
};

class StatementAST {
public:
    virtual void print(int indent=0)=0;
    virtual void codegen(CodeGenContext& ctx)=0;
    virtual ~StatementAST()=default;
};

void printSpace(int space) {
    for (int i=0;i<space;i++) std::cout<<' ';
} 

class ExprAST: public StatementAST {
public:
    virtual ExprResult codegenExpr(CodeGenContext& ctx)=0; 

    virtual void codegen(CodeGenContext& ctx) override {
        ExprResult res=codegenExpr(ctx);
        if (res.is_float) ctx.freeFloatReg(res.reg_idx);
        else ctx.freeIntReg(res.reg_idx);
    }
};

class BinaryExprAST : public ExprAST {
public:
    std::unique_ptr<ExprAST> lhs;
    Token op;
    std::unique_ptr<ExprAST> rhs;
    BinaryExprAST(std::unique_ptr<ExprAST> l,Token o,std::unique_ptr<ExprAST> r): lhs(std::move(l)),op(o),rhs(std::move(r)) {}

    virtual void print(int indent=0) override {
        printSpace(indent);
        indent+=2;
        std::cout<<"BinaryExprAST: "<<std::endl;
        printSpace(indent);
        std::cout<<"Token: "<<(op)<<std::endl;
        lhs->print(indent);
        rhs->print(indent);
    }

    virtual ExprResult codegenExpr(CodeGenContext& ctx) override {
        ExprResult lhs_res=lhs->codegenExpr(ctx);

        bool lhs_spilled=false;
        if (lhs_res.reg_idx==-1) {
            ctx.text_section<<"    push rax ; SPILL LHS\n";
            lhs_spilled=true;
        }

        ExprResult rhs_res=rhs->codegenExpr(ctx);

        if (lhs_spilled) {
            lhs_res.reg_idx=ctx.allocIntReg(); 
            ctx.text_section<<"    pop "<<calc_registers_64[lhs_res.reg_idx]<<" ; RESTORE LHS\n";
        }

        if (lhs_res.is_float||rhs_res.is_float) {
            std::string regL=calc_registers_simd[lhs_res.reg_idx];
            std::string regR=calc_registers_simd[rhs_res.reg_idx];

            if (op.value=="+") ctx.text_section<<"    addsd "<<regL<<","<<regR<<"\n";
            else if (op.value=="-") ctx.text_section<<"    subsd "<<regL<<","<<regR<<"\n";
            else if (op.value=="*") ctx.text_section<<"    mulsd "<<regL<<","<<regR<<"\n";
            else if (op.value=="/") ctx.text_section<<"    divsd "<<regL<<","<<regR<<"\n";

            ctx.freeFloatReg(rhs_res.reg_idx); 
            return {lhs_res.reg_idx,true,false};
        }

        std::string regL=calc_registers_64[lhs_res.reg_idx];
        std::string regR=calc_registers_64[rhs_res.reg_idx];

        if (op.value=="+") {
            ctx.text_section<<"    add "<<regL<<","<<regR<<"\n";
        } else if (op.value=="-") {
            ctx.text_section<<"    sub "<<regL<<","<<regR<<"\n";
        } else if (op.value=="*") {
            ctx.text_section<<"    imul "<<regL<<","<<regR<<"\n";
        } else if (op.value=="/") {
            ctx.text_section<<"    push rax\n    push rdx\n";
            ctx.text_section<<"    mov rax,"<<regL<<"\n";
            ctx.text_section<<"    cqo\n"; 
            ctx.text_section<<"    idiv "<<regR<<"\n";
            ctx.text_section<<"    mov "<<regL<<",rax\n";
            ctx.text_section<<"    pop rdx\n    pop rax\n";
        } else {
            ctx.text_section<<"    cmp "<<regL<<","<<regR<<"\n";
            if (op.value=="==") ctx.text_section<<"    sete al\n";
            else if (op.value=="!=") ctx.text_section<<"    setne al\n";
            else if (op.value==">") ctx.text_section<<"    setg al\n";
            else if (op.value=="<") ctx.text_section<<"    setl al\n";
            else if (op.value==">=") ctx.text_section<<"    setge al\n";
            else if (op.value=="<=") ctx.text_section<<"    setle al\n";
            ctx.text_section<<"    movzx "<<regL<<",al\n";
        }

        ctx.freeIntReg(rhs_res.reg_idx); 
        return {lhs_res.reg_idx,false,false}; 
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
    virtual ExprResult codegenExpr(CodeGenContext& ctx) override {
        ExprResult res=operand->codegenExpr(ctx);
        if (op.value=="-") {
            if (res.is_float) {

            } else {
                ctx.text_section<<"    neg "<<calc_registers_64[res.reg_idx]<<"\n";
            }
        }
        return res;
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
    virtual ExprResult codegenExpr(CodeGenContext& ctx) override {
        int reg=ctx.allocIntReg();
        ctx.text_section<<"    mov "<<calc_registers_64[reg]<<","<<(boolean.value=="true"?"1" : "0")<<"\n";
        return {reg,false,false};
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
    virtual ExprResult codegenExpr(CodeGenContext& ctx) override {
        bool is_float=(number.value.find('.')!=std::string::npos);
        if (is_float) {
            int reg=ctx.allocFloatReg();
            std::string label=ctx.newLabel(".float_const_");
            ctx.data_section<<label<<" dq "<<number.value<<"\n";
            ctx.text_section<<"    movsd "<<calc_registers_simd[reg]<<",["<<label<<"]\n";
            return {reg,true,false};
        } else {
            int reg=ctx.allocIntReg();
            ctx.text_section<<"    mov "<<calc_registers_64[reg]<<","<<number.value<<"\n";
            return {reg,false,false};
        }
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
    virtual ExprResult codegenExpr(CodeGenContext& ctx) override {
        int reg=ctx.allocIntReg();
        std::string label=ctx.newLabel(".str_const_");
        ctx.data_section<<label<<" db "<<str.value<<",0\n";
        ctx.text_section<<"    lea "<<calc_registers_64[reg]<<",["<<label<<"]\n";
        return {reg,false,false};
    }
};

class IdentifierExprAST : public ExprAST {
public:
    Token identifier;
    IdentifierExprAST(Token id): identifier(id) {}
    virtual void print(int indent=0) override {
        printSpace(indent);
        std::cout<<"IdentifierExprAST: "<<std::endl;
        indent+=2;
        printSpace(indent);
        std::cout<<"identifier: "<<(identifier)<<std::endl;
    }
    virtual ExprResult codegenExpr(CodeGenContext& ctx) override {
        VarInfo info=ctx.getVar(identifier.value);
        bool is_float=(info.type==DOUBLE||info.type==FLOAT);
        if (is_float) {
            int reg=ctx.allocFloatReg();
            ctx.text_section<<"    movsd "<<calc_registers_simd[reg]<<",[rbp"<<info.bp_offset<<"]\n";
            return {reg,true,false};
        } else {
            int reg=ctx.allocIntReg();
            ctx.text_section<<"    mov "<<calc_registers_64[reg]<<",[rbp"<<info.bp_offset<<"]\n";
            return {reg,false,false};
        }
    }
};

class CallExprAST : public ExprAST {
public:
    Token callee;
    std::vector<std::unique_ptr<ExprAST>> args;
    CallExprAST(Token c,std::vector<std::unique_ptr<ExprAST>> a): callee(c),args(std::move(a)) {}
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
    virtual ExprResult codegenExpr(CodeGenContext& ctx) override {
        int int_arg_idx=0;
        int float_arg_idx=0;

        for (auto& arg : args) {
            ExprResult res=arg->codegenExpr(ctx);
            if (res.is_float && float_arg_idx < 8) {
                ctx.text_section<<"    movsd xmm"<<float_arg_idx++<<","<<calc_registers_simd[res.reg_idx]<<"\n";
                ctx.freeFloatReg(res.reg_idx);
            } else if (!res.is_float && int_arg_idx < 6) {
                ctx.text_section<<"    mov "<<reg_order_function_args[int_arg_idx++]<<","<<calc_registers_64[res.reg_idx]<<"\n";
                ctx.freeIntReg(res.reg_idx);
            }
        }

        ctx.text_section<<"    call "<<callee.value<<"\n";

        int ret_reg=ctx.allocIntReg(); 
        ctx.text_section<<"    mov "<<calc_registers_64[ret_reg]<<",rax\n"; 
        return {ret_reg,false,false};
    }
};

class DeclarationAST : public StatementAST {
public:
    Token identifier;
    std::unique_ptr<ExprAST> expr;
    DeclarationAST(Token id,std::unique_ptr<ExprAST> e): identifier(id),expr(std::move(e)) {}
    virtual void print(int indent=0) override {
        printSpace(indent);
        std::cout<<"DeclarationAST: "<<std::endl;
        indent+=2;
        printSpace(indent);
        std::cout<<"callee: "<<identifier<<std::endl;
        expr->print(indent);
    }
    virtual void codegen(CodeGenContext& ctx) override {
        ExprResult res=expr->codegenExpr(ctx);
        Value t=res.is_float?DOUBLE:INT; 
        ctx.declareVar(identifier.value,t);
        int offset=ctx.getVar(identifier.value).bp_offset;

        if (res.is_float) {
            ctx.text_section<<"    movsd [rbp"<<offset<<"],"<<calc_registers_simd[res.reg_idx]<<"\n";
            ctx.freeFloatReg(res.reg_idx);
        } else {
            ctx.text_section<<"    mov [rbp"<<offset<<"],"<<calc_registers_64[res.reg_idx]<<"\n";
            ctx.freeIntReg(res.reg_idx);
        }
    }
};

class AssignmentAST : public StatementAST {
public:
    Token identifier;
    std::unique_ptr<ExprAST> expr;
    AssignmentAST(Token id,std::unique_ptr<ExprAST> e): identifier(id),expr(std::move(e)) {}
    virtual void print(int indent=0) override {
        printSpace(indent);
        std::cout<<"AssignmentAST: "<<std::endl;
        indent+=2;
        printSpace(indent);
        std::cout<<"callee: "<<identifier<<std::endl;
        expr->print(indent);
    }
    virtual void codegen(CodeGenContext& ctx) override {
        ExprResult res=expr->codegenExpr(ctx);
        int offset=ctx.getVar(identifier.value).bp_offset;

        if (res.is_float) {
            ctx.text_section<<"    movsd [rbp"<<offset<<"],"<<calc_registers_simd[res.reg_idx]<<"\n";
            ctx.freeFloatReg(res.reg_idx);
        } else {
            ctx.text_section<<"    mov [rbp"<<offset<<"],"<<calc_registers_64[res.reg_idx]<<"\n";
            ctx.freeIntReg(res.reg_idx);
        }
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
    virtual void codegen(CodeGenContext& ctx) override {}
};

class BlockAST : public StatementAST {
private:
    std::vector<std::unique_ptr<StatementAST>> statements;
public:
    BlockAST()=default;
    void addStatement(std::unique_ptr<StatementAST> stmt) {
        statements.push_back(std::move(stmt));
    }
    virtual void print(int indent=0) override {
        printSpace(indent);
        std::cout<<"BlockAST: "<<std::endl;
        indent+=2;
        for (auto&& i:statements) {
            i->print(indent);
        }
    }
    virtual void codegen(CodeGenContext& ctx) override {
        ctx.pushScope();
        for (auto&& i:statements) i->codegen(ctx);
        ctx.popScope();
    }
};

class FunctionAST : public StatementAST {
public:
    Token name;
    std::unique_ptr<PrototypeAST> proto;
    Token returnType;
    std::unique_ptr<BlockAST> body;
    FunctionAST(Token n,std::unique_ptr<PrototypeAST> p,Token r,std::unique_ptr<BlockAST> b)
        : name(n),proto(std::move(p)),returnType(r),body(std::move(b)) {}
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
    virtual void codegen(CodeGenContext& ctx) override {
        std::string funcName=name.value;
        std::string prevFunc=ctx.current_function_name;
        ctx.current_function_name=funcName;

        ctx.text_section<<"\nglobal "<<funcName<<"\n";
        ctx.text_section<<funcName<<":\n";

        ctx.text_section<<"    push rbp\n";
        ctx.text_section<<"    mov rbp,rsp\n";

        std::string stack_placeholder="__LOCAL_SPACE_"+funcName+"__";
        ctx.text_section<<"    sub rsp,"<<stack_placeholder<<" ; Local space\n"; 

        ctx.pushScope();
        ctx.current_stack_offset=0;

        for (size_t i=0;i<proto->args.size();i++) {
            std::string argName=proto->args[i].second.value;
            std::string argType=proto->args[i].first.value;
            Value vType=type(argType);

            ctx.declareVar(argName,vType);
            int bp_off=ctx.getVar(argName).bp_offset;

            if (vType==DOUBLE||vType==FLOAT) {
                if (i < 8) { 
                    ctx.text_section<<"    movsd [rbp"<<bp_off<<"],xmm"<<i<<"\n";
                }
            } else {
                if (i < 6) { 
                    ctx.text_section<<"    mov [rbp"<<bp_off<<"],"<<reg_order_function_args[i]<<"\n";
                }
            }
        }

        body->codegen(ctx);

        ctx.text_section<<".end_"<<funcName<<":\n";
        ctx.text_section<<"    mov rsp,rbp\n";
        ctx.text_section<<"    pop rbp\n";
        ctx.text_section<<"    ret\n";

        ctx.popScope();

        std::string code=ctx.text_section.str();
        size_t pos=code.find(stack_placeholder);
        if (pos!=std::string::npos) {

            int alloc_size=-ctx.current_stack_offset; 

            alloc_size=(alloc_size+15)&~15; 

            code.replace(pos,stack_placeholder.length(),std::to_string(alloc_size));

            ctx.text_section.str("");
            ctx.text_section.clear();
            ctx.text_section<<code;
        }

        ctx.current_function_name=prevFunc;
    }
};
class ConditionalAST : public StatementAST {
public:
    std::unique_ptr<ExprAST> ifCondition;
    std::unique_ptr<BlockAST> ifBlock;
    std::vector<std::pair<std::unique_ptr<ExprAST>,std::unique_ptr<BlockAST>>> elseIfs;
    std::unique_ptr<BlockAST> elseBlock;
    ConditionalAST(std::unique_ptr<ExprAST> c,std::unique_ptr<BlockAST> b): ifCondition(std::move(c)),ifBlock(std::move(b)) {}
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
    virtual void codegen(CodeGenContext& ctx) override {
        std::string endLabel=ctx.newLabel(".L_end_if_");

        ExprResult condRes=ifCondition->codegenExpr(ctx);
        std::string nextLabel=ctx.newLabel(".L_next_cond_");

        ctx.text_section<<"    cmp "<<calc_registers_64[condRes.reg_idx]<<",0\n";
        ctx.text_section<<"    je "<<nextLabel<<"\n";
        ctx.freeIntReg(condRes.reg_idx);

        ifBlock->codegen(ctx);
        ctx.text_section<<"    jmp "<<endLabel<<"\n";
        ctx.text_section<<nextLabel<<":\n";

        for (auto& elif : elseIfs) {
            ExprResult elifCondRes=elif.first->codegenExpr(ctx);
            std::string elifNextLabel=ctx.newLabel(".L_next_cond_");

            ctx.text_section<<"    cmp "<<calc_registers_64[elifCondRes.reg_idx]<<",0\n";
            ctx.text_section<<"    je "<<elifNextLabel<<"\n";
            ctx.freeIntReg(elifCondRes.reg_idx);

            elif.second->codegen(ctx);
            ctx.text_section<<"    jmp "<<endLabel<<"\n";
            ctx.text_section<<elifNextLabel<<":\n";
        }

        if (elseBlock) {
            elseBlock->codegen(ctx);
        }

        ctx.text_section<<endLabel<<":\n";
    }
};

class WhileAST : public StatementAST {
public:
    std::unique_ptr<ExprAST> condition;
    std::unique_ptr<BlockAST> body;
    WhileAST(std::unique_ptr<ExprAST> c,std::unique_ptr<BlockAST> b): condition(std::move(c)),body(std::move(b)) {}
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
    virtual void codegen(CodeGenContext& ctx) override {
        std::string startLabel=ctx.newLabel(".L_while_start_");
        std::string endLabel=ctx.newLabel(".L_while_end_");

        ctx.loop_start_labels.push_back(startLabel);
        ctx.loop_end_labels.push_back(endLabel);

        ctx.text_section<<startLabel<<":\n";

        ExprResult condRes=condition->codegenExpr(ctx);
        ctx.text_section<<"    cmp "<<calc_registers_64[condRes.reg_idx]<<",0\n";
        ctx.text_section<<"    je "<<endLabel<<"\n";
        ctx.freeIntReg(condRes.reg_idx);

        body->codegen(ctx);

        ctx.text_section<<"    jmp "<<startLabel<<"\n";
        ctx.text_section<<endLabel<<":\n";

        ctx.loop_start_labels.pop_back();
        ctx.loop_end_labels.pop_back();
    }
};

class ForAST : public StatementAST {
public:
    std::unique_ptr<StatementAST> init;
    std::unique_ptr<ExprAST> condition;
    std::unique_ptr<ExprAST> update;
    std::unique_ptr<BlockAST> body;
    ForAST(std::unique_ptr<StatementAST> init,std::unique_ptr<ExprAST> condition,std::unique_ptr<ExprAST> update,std::unique_ptr<BlockAST> body)
        : init(std::move(init)),condition(std::move(condition)),update(std::move(update)),body(std::move(body)) {}
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
    virtual void codegen(CodeGenContext& ctx) override {
        ctx.pushScope();

        if (init) init->codegen(ctx);

        std::string startLabel=ctx.newLabel(".L_for_start_");
        std::string endLabel=ctx.newLabel(".L_for_end_");
        std::string updateLabel=ctx.newLabel(".L_for_update_");

        ctx.loop_start_labels.push_back(updateLabel);
        ctx.loop_end_labels.push_back(endLabel);

        ctx.text_section<<startLabel<<":\n";

        if (condition) {
            ExprResult condRes=condition->codegenExpr(ctx);
            ctx.text_section<<"    cmp "<<calc_registers_64[condRes.reg_idx]<<",0\n";
            ctx.text_section<<"    je "<<endLabel<<"\n";
            ctx.freeIntReg(condRes.reg_idx);
        }

        if (body) body->codegen(ctx);

        ctx.text_section<<updateLabel<<":\n";
        if (update) {
            ExprResult updateRes=update->codegenExpr(ctx);
            if (updateRes.is_float) ctx.freeFloatReg(updateRes.reg_idx);
            else ctx.freeIntReg(updateRes.reg_idx);
        }

        ctx.text_section<<"    jmp "<<startLabel<<"\n";
        ctx.text_section<<endLabel<<":\n";

        ctx.loop_start_labels.pop_back();
        ctx.loop_end_labels.pop_back();
        ctx.popScope();
    }
};

class ReturnAST : public StatementAST {
public:
    std::unique_ptr<ExprAST> expr;
    ReturnAST(std::unique_ptr<ExprAST> expr): expr(std::move(expr)) {}
    virtual void print(int indent=0) override {
        printSpace(indent);
        std::cout<<"ReturnAST: "<<std::endl;
        indent+=2;
        if (expr) expr->print(indent);
    }
    virtual void codegen(CodeGenContext& ctx) override {
        if (expr) {
            ExprResult res=expr->codegenExpr(ctx);
            if (res.is_float) {
                ctx.text_section<<"    movsd xmm0,"<<calc_registers_simd[res.reg_idx]<<"\n";
                ctx.freeFloatReg(res.reg_idx);
            } else {
                ctx.text_section<<"    mov rax,"<<calc_registers_64[res.reg_idx]<<"\n";
                ctx.freeIntReg(res.reg_idx);
            }
        }

        ctx.text_section<<"    jmp .end_"<<ctx.current_function_name<<"\n";
    }
};

class BreakContinueAST : public StatementAST {
public:
    Token keyword;
    BreakContinueAST(Token keyword): keyword(keyword) {}
    virtual void print(int indent=0) override {
        printSpace(indent);
        std::cout<<"BreakContinueAST: "<<std::endl;
        indent+=2;
        printSpace(indent);
        std::cout<<"keyword: "<<keyword<<std::endl;
    }
    virtual void codegen(CodeGenContext& ctx) override {
        if (keyword.value=="break") {
            if (!ctx.loop_end_labels.empty()) {
                ctx.text_section<<"    jmp "<<ctx.loop_end_labels.back()<<"\n";
            }
        } else if (keyword.value=="continue") {
            if (!ctx.loop_start_labels.empty()) {
                ctx.text_section<<"    jmp "<<ctx.loop_start_labels.back()<<"\n";
            }
        }
    }
};

class ProgramAST {
public:
    std::vector<std::unique_ptr<StatementAST>> statements;
    ProgramAST()=default;
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
    void codegen(CodeGenContext& ctx) {
        for (auto&& i:statements) i->codegen(ctx);
    }
};
