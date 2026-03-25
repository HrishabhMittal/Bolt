#include "codegen.cpp"
#include "header.hpp"
#include <iostream>
#include <memory>
#include <string>

class Parser {
    Lexer l;
    Token currentToken;
    int insideFunction = 0;
    int insideLoop = 0;
    void next() {
        do {
            currentToken = l.gettoken();
        } while (currentToken.ttype == TokenType::NEWLINE);
    }
    bool matchnext(TokenType type, const std::string &val = "") {
        Token x = l.peektoken();
        return x.ttype == type && (val.empty() || x.value == val);
    }
    bool match(TokenType type, const std::string &val = "") {
        return currentToken.ttype == type && (val.empty() || currentToken.value == val);
    }
    bool match_binop() {
        for (auto &i : binops_by_precedence)
            for (auto &j : i) {
                if (j == currentToken.value)
                    return true;
            }
        return false;
    }
    Token expect(TokenType type, const std::string &val = "") {
        if (!match(type, val)) {
            throw std::runtime_error("Unexpected token: \"" + currentToken.value + "\", at line: " +
                                     std::to_string(currentToken.lineno) + "\n    " + *currentToken.line + "\n    " +
                                     repeat(" ", currentToken.startindex) + repeat("^", currentToken.value.size()));
        }
        Token tok = currentToken;
        next();
        return tok;
    }
    void error(std::string s) { throw std::runtime_error(s); }
    std::unique_ptr<ReturnAST> parseReturn() {
        if (!insideFunction)
            error("return encountered outside function");
        // std::cout<<"parsing return: "<<std::endl;
        // std::cout<<tokenToString(currentToken)<<std::endl;
        Token returnToken = expect(TokenType::KEYWORD, "return");

        if (!match(TokenType::PUNCTUATOR, ";")) {
            auto expr = parseExpr();
            expect(TokenType::PUNCTUATOR, ";");
            return std::make_unique<ReturnAST>(std::move(expr));
        }

        expect(TokenType::PUNCTUATOR, ";");
        return std::make_unique<ReturnAST>(nullptr);
    }
    std::unique_ptr<BreakContinueAST> parseBreakContinue() {
        if (!insideLoop)
            error("break/continue found outside loop");
        // std::cout<<"parsing breakcont at: ";
        // std::cout<<tokenToString(currentToken)<<std::endl;
        Token keyword = currentToken;
        next();
        expect(TokenType::PUNCTUATOR, ";");
        return std::make_unique<BreakContinueAST>(keyword);
    }
    std::unique_ptr<ExprAST> parseValue() {
        // std::cout<<"parsing value at: ";
        // std::cout<<tokenToString(currentToken)<<std::endl;
        if (match(TokenType::KEYWORD, "false") || match(TokenType::KEYWORD, "true"))
            return std::make_unique<BooleanExprAST>(expect(TokenType::KEYWORD));
        if (match(TokenType::NUMBER))
            return std::make_unique<NumberExprAST>(expect(TokenType::NUMBER));
        if (match(TokenType::STRING))
            return std::make_unique<StringExprAST>(expect(TokenType::STRING));
        if (match(TokenType::IDENTIFIER)) {
            Token id = expect(TokenType::IDENTIFIER);
            if (match(TokenType::PUNCTUATOR, "(")) {
                next();
                std::vector<std::unique_ptr<ExprAST>> args;
                if (!match(TokenType::PUNCTUATOR, ")")) {
                    do {
                        args.push_back(parseExpr());
                    } while (match(TokenType::PUNCTUATOR, ","));
                }
                expect(TokenType::PUNCTUATOR, ")");
                return std::make_unique<CallExprAST>(id, std::move(args));
            }
            return std::make_unique<IdentifierExprAST>(id);
        }
        throw std::runtime_error("Invalid value expression" + tokenToString(currentToken));
    }

    std::unique_ptr<ExprAST> parseTerm() {
        // std::cout<<"parsing term at: ";
        // std::cout<<tokenToString(currentToken)<<std::endl;
        if (match(TokenType::PUNCTUATOR, "-") || match(TokenType::PUNCTUATOR, "!")) {
            Token op = currentToken;
            next();
            auto operand = parseTerm();
            return std::make_unique<UnaryExprAST>(op, std::move(operand));
        }
        return parseValue();
    }

    std::unique_ptr<ExprAST> parseParenExpr() {
        // std::cout<<"parsing parentexpr at: ";
        // std::cout<<tokenToString(currentToken)<<std::endl;
        expect(TokenType::PUNCTUATOR, "(");
        auto expr = parseExpr();
        expect(TokenType::PUNCTUATOR, ")");
        // std::cout<<"parsing parentexpr complete at: ";
        // std::cout<<tokenToString(currentToken)<<std::endl;
        return expr;
    }
    int getOpPrecedence() {
        if (currentToken.ttype != TokenType::PUNCTUATOR)
            return -1;

        for (size_t i = 0; i < binops_by_precedence.size(); ++i) {
            for (const auto &op : binops_by_precedence[i]) {
                if (op == currentToken.value) {
                    return binops_by_precedence.size() - i;
                }
            }
        }
        return -1;
    }
    std::unique_ptr<ExprAST> parseTypeCast() {
        auto cast_type = expect(TokenType::KEYWORD);
        expect(TokenType::PUNCTUATOR,"(");
        auto expr = parseExpr();
        expect(TokenType::PUNCTUATOR,")");
        auto cast=std::make_unique<TypeCastAST>(std::move(expr),cast_type);
        return cast;
    }
    std::unique_ptr<ExprAST> parseExpr(int exprPrec = 0) {
        std::unique_ptr<ExprAST> lhs;
        if (match(TokenType::KEYWORD)) {
            lhs = parseTypeCast();
        } else
            lhs = match(TokenType::PUNCTUATOR, "(") ? parseParenExpr() : parseTerm();
        while (true) {
            if (!match_binop()) {
                return lhs;
            }
            int tokPrec = getOpPrecedence();

            if (tokPrec < exprPrec) {
                return lhs;
            }
            Token op = currentToken;
            next();
            int nextPrec = (tokPrec == 1) ? tokPrec : tokPrec + 1;
            auto rhs = parseExpr(nextPrec);
            lhs = std::make_unique<BinaryExprAST>(std::move(lhs), op, std::move(rhs));
        }
    }
    // std::unique_ptr<ExprAST> parseExpr() {
    //     // std::cout<<"parsing expr at: ";
    //     // std::cout<<tokenToString(currentToken)<<std::endl;
    //     auto lhs = match(TokenType::PUNCTUATOR, "(") ? parseParenExpr() :
    //     parseTerm(); if (!match_binop()) {
    //         return lhs;
    //     }
    //     auto op = expect(TokenType::PUNCTUATOR);
    //     auto rhs = parseExpr();
    //     return
    //     std::make_unique<BinaryExprAST>(std::move(lhs),op,std::move(rhs));
    // }
    std::unique_ptr<BlockAST> parseBlock() {
        // std::cout<<"parsing block at: ";
        // std::cout<<tokenToString(currentToken)<<std::endl;
        expect(TokenType::PUNCTUATOR, "{");
        auto block = std::make_unique<BlockAST>();
        while (!match(TokenType::PUNCTUATOR, "}")) {
            block->addStatement(parseStatement());
        }
        expect(TokenType::PUNCTUATOR, "}");
        return block;
    }
    std::unique_ptr<GlobalStatementAST> parseGlobalDeclaration() {

        // std::cout<<tokenToString(currentToken)<<std::endl;
        Token id = currentToken;
        next();
        if (match(TokenType::PUNCTUATOR, ":=")) {
            next();
            auto expr = parseExpr();
            expect(TokenType::PUNCTUATOR, ";");

            return std::make_unique<GlobalDeclarationAST>(id, std::move(expr));
        }
        throw std::runtime_error("Unknown statement at token: " + tokenToString(currentToken));
    }
    std::unique_ptr<StatementAST> parseDeclarationAssignment() {
        // std::cout<<"parsing dec/ass at: ";

        // std::cout<<tokenToString(currentToken)<<std::endl;
        Token id = currentToken;
        next();
        if (match(TokenType::PUNCTUATOR, ":=")) {
            next();
            auto expr = parseExpr();
            expect(TokenType::PUNCTUATOR, ";");

            return std::make_unique<DeclarationAST>(id, std::move(expr));
        } else if (match(TokenType::PUNCTUATOR, "=")) {
            next();
            auto expr = parseExpr();
            expect(TokenType::PUNCTUATOR, ";");
            return std::make_unique<AssignmentAST>(id, std::move(expr));
        }
        throw std::runtime_error("Unknown statement at token: " + tokenToString(currentToken));
    }
    std::unique_ptr<GlobalStatementAST> parseGlobalStatement() {
        // std::cout<<"parsing statment at: ";
        // std::cout<<tokenToString(currentToken)<<std::endl;
        if (match(TokenType::KEYWORD, "function"))
            return parseFunction();
        if (match(TokenType::IDENTIFIER) && matchnext(TokenType::PUNCTUATOR, ":="))
            return parseGlobalDeclaration();
        // auto x = parseExpr();
        // expect(TokenType::PUNCTUATOR,";");
        // return std::move(x);
        throw std::runtime_error("Unknown statement at token: " + tokenToString(currentToken));
    }
    std::unique_ptr<StatementAST> parseStatement() {
        // std::cout<<"parsing statment at: ";
        // std::cout<<tokenToString(currentToken)<<std::endl;
        if (match(TokenType::PUNCTUATOR, "{"))
            return parseBlock();
        if (match(TokenType::KEYWORD, "if"))
            return parseConditional();
        if (match(TokenType::KEYWORD, "while"))
            return parseWhile();
        if (match(TokenType::KEYWORD, "for"))
            return parseFor();
        if (insideFunction && match(TokenType::KEYWORD, "return"))
            return parseReturn();
        if (insideLoop && match(TokenType::KEYWORD, "break") || match(TokenType::KEYWORD, "continue"))
            return parseBreakContinue();
        if (match(TokenType::IDENTIFIER) &&
            (matchnext(TokenType::PUNCTUATOR, ":=") || matchnext(TokenType::PUNCTUATOR, "=")))
            return parseDeclarationAssignment();
        // auto x = parseExpr();
        // expect(TokenType::PUNCTUATOR,";");
        // return std::move(x);
        throw std::runtime_error("Unknown statement at token: " + tokenToString(currentToken));
    }
    std::unique_ptr<PrototypeAST> parsePrototype() {
        // std::cout << "parsing proto: " << std::endl;
        // std::cout << tokenToString(currentToken) << std::endl;
        std::vector<std::pair<Token, Token>> args;
        if (!match(TokenType::PUNCTUATOR, ")")) {
            while (true) {
                Token name = expect(TokenType::IDENTIFIER);
                Token type = expect(TokenType::KEYWORD);
                args.push_back({type, name});
                if (!match(TokenType::PUNCTUATOR, ","))
                    break;
                expect(TokenType::PUNCTUATOR, ",");
            }
        }
        return std::make_unique<PrototypeAST>(std::move(args));
    }
    std::unique_ptr<FunctionAST> parseFunction() {
        // std::cout<<"parsing function: "<<std::endl;
        // std::cout<<tokenToString(currentToken)<<std::endl;
        expect(TokenType::KEYWORD, "function");
        Token name = expect(TokenType::IDENTIFIER);
        expect(TokenType::PUNCTUATOR, "(");
        auto proto = parsePrototype();
        expect(TokenType::PUNCTUATOR, ")");
        expect(TokenType::PUNCTUATOR, "(");
        Token returnType = expect(TokenType::KEYWORD);
        expect(TokenType::PUNCTUATOR, ")");
        insideFunction++;
        auto body = parseBlock();
        insideFunction--;

        return std::make_unique<FunctionAST>(name, std::move(proto), returnType, std::move(body));
    }
    std::unique_ptr<StatementAST> parseConditional() {
        // std::cout<<"parsing condition: "<<std::endl;
        // std::cout<<tokenToString(currentToken)<<std::endl;
        expect(TokenType::KEYWORD, "if");
        auto ifCond = parseExpr();
        auto ifBlock = parseBlock();
        auto condAST = std::make_unique<ConditionalAST>(std::move(ifCond), std::move(ifBlock));

        while (match(TokenType::KEYWORD, "else")) {
            next();
            if (match(TokenType::KEYWORD, "if")) {
                next();
                auto elifCond = parseExpr();
                auto elifBlock = parseBlock();
                condAST->elseIfs.push_back({std::move(elifCond), std::move(elifBlock)});
            } else {
                condAST->elseBlock = parseBlock();
                break;
            }
        }
        return condAST;
    }
    std::unique_ptr<StatementAST> parseWhile() {
        // std::cout<<"parsing while: "<<std::endl;
        // std::cout<<tokenToString(currentToken)<<std::endl;
        expect(TokenType::KEYWORD, "while");
        auto cond = parseExpr();
        insideLoop++;
        auto body = parseBlock();
        insideLoop--;
        return std::make_unique<WhileAST>(std::move(cond), std::move(body));
    }

    std::unique_ptr<StatementAST> parseFor() {
        // std::cout<<"parsing for: "<<std::endl;
        // std::cout<<tokenToString(currentToken)<<std::endl;
        expect(TokenType::KEYWORD, "for");
        expect(TokenType::PUNCTUATOR, "(");
        auto init = parseDeclarationAssignment();
        auto cond = parseExpr();
        expect(TokenType::PUNCTUATOR, ";");
        auto update = parseExpr();
        expect(TokenType::PUNCTUATOR, ")");
        insideLoop++;
        auto body = parseBlock();
        insideLoop--;
        return std::make_unique<ForAST>(std::move(init), std::move(cond), std::move(update), std::move(body));
    }

  public:
    Parser(Lexer l) : l(std::move(l)) {}
    std::unique_ptr<ProgramAST> parseProgram() {
        // std::cout<<"parsing program: "<<std::endl;
        auto program = std::make_unique<ProgramAST>();
        next();
        while (currentToken.ttype != TokenType::TK_EOF) {
            program->addStatement(parseGlobalStatement());
        }
        return program;
    }
};
