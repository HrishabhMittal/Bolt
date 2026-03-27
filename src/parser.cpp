#include "codegen.cpp"
#include "header.hpp"
#include <algorithm>

class Parser {
    Lexer l;
    Token currentToken;
    int insideFunction = 0;
    int insideLoop = 0;
    std::string current_package;
    std::map<std::string, std::string> current_imports;
    void next() { currentToken = l.gettoken(); }
    bool matchnext(TokenType type, const std::string &val = "") {
        auto c = l.get_checkpoint();
        Token store = currentToken;
        next();
        bool ans = match(type, val);
        l.restore(c);
        currentToken = store;
        return ans;
    }
    bool match(TokenType type, const std::string &val = "") {
        if (currentToken.ttype == type && (val.empty() || currentToken.value == val)) {
            return true;
        }
        if (type != TokenType::NEWLINE && currentToken.ttype == TokenType::NEWLINE) {
            auto cp = l.get_checkpoint();
            Token store = currentToken;
            while (currentToken.ttype == TokenType::NEWLINE) {
                next();
            }
            if (currentToken.ttype == type && (val.empty() || currentToken.value == val)) {
                return true;
            } else {
                l.restore(cp);
                currentToken = store;
                return false;
            }
        }
        return false;
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
            currentToken.error("Unexpected token");
        }
        Token tok = currentToken;
        next();
        return tok;
    }
    std::string parseTypeName() {
        if (match(TokenType::KEYWORD)) {
            return expect(TokenType::KEYWORD).value;
        } else if (match(TokenType::IDENTIFIER)) {
            Token id = expect(TokenType::IDENTIFIER);
            if (match(TokenType::PUNCTUATOR, ".")) {
                next();
                Token struct_name = expect(TokenType::IDENTIFIER);
                std::string pkg = id.value;
                if (current_imports.count(id.value)) {
                    pkg = current_imports[id.value];
                }
                return pkg + "." + struct_name.value;
            }
            return current_package + "." + id.value;
        } else if (match(TokenType::PUNCTUATOR, "[")) {
            expect(TokenType::PUNCTUATOR, "[");
            expect(TokenType::PUNCTUATOR, "]");
            std::string t = parseTypeName();
            return "[]" + t;
        }
        currentToken.error("expected a type name (built-in or struct)");
    }
    std::unique_ptr<ReturnAST> parseReturn() {
        if (!insideFunction)
            error("return encountered outside function");
        // std::cout << "parsing return: " << std::endl;
        // std::cout << tokenToString(currentToken) << std::endl;
        Token returnToken = expect(TokenType::KEYWORD, "return");

        if (!match(TokenType::NEWLINE)) {
            auto expr = parseExpr();
            expect(TokenType::NEWLINE);
            return std::make_unique<ReturnAST>(std::move(expr));
        }

        expect(TokenType::NEWLINE);
        return std::make_unique<ReturnAST>(nullptr);
    }
    std::unique_ptr<BreakContinueAST> parseBreakContinue() {
        if (!insideLoop)
            error("break/continue found outside loop");
        // std::cout << "parsing breakcont at: ";
        // std::cout << tokenToString(currentToken) << std::endl;
        Token keyword = currentToken;
        next();
        expect(TokenType::NEWLINE);
        return std::make_unique<BreakContinueAST>(keyword);
    }
    std::unique_ptr<ExprAST> parseValue() {
        // std::cout << "parsing value at: ";
        // std::cout << tokenToString(currentToken) << std::endl;
        if (match(TokenType::KEYWORD, "false") || match(TokenType::KEYWORD, "true"))
            return std::make_unique<BooleanExprAST>(expect(TokenType::KEYWORD));
        if (match(TokenType::NUMBER))
            return std::make_unique<NumberExprAST>(expect(TokenType::NUMBER));
        if (match(TokenType::STRING))
            return std::make_unique<StringExprAST>(expect(TokenType::STRING));
        if (match(TokenType::IDENTIFIER)) {
            Token id = expect(TokenType::IDENTIFIER);
            if (match(TokenType::PUNCTUATOR, ".")) {
                next();
                Token field = expect(TokenType::IDENTIFIER);
                std::string pkg = id.value;
                if (current_imports.count(id.value)) {
                    pkg = current_imports[id.value];
                }
                id.value = pkg + "." + field.value;
            }
            if (match(TokenType::PUNCTUATOR, "(")) {
                next();
                std::vector<std::unique_ptr<ExprAST>> args;
                if (!match(TokenType::PUNCTUATOR, ")")) {
                    while (true) {
                        args.push_back(parseExpr());
                        if (!match(TokenType::PUNCTUATOR, ",")) {
                            break;
                        }
                        expect(TokenType::PUNCTUATOR, ",");
                    }
                }
                expect(TokenType::PUNCTUATOR, ")");
                return std::make_unique<CallExprAST>(id, std::move(args), current_package);
            } else if (match(TokenType::PUNCTUATOR, "[")) {
                std::unique_ptr<ExprAST> iden = std::make_unique<IdentifierExprAST>(id, current_package);
                while (match(TokenType::PUNCTUATOR, "[")) {
                    expect(TokenType::PUNCTUATOR, "[");
                    std::unique_ptr<ExprAST> index = parseExpr();
                    expect(TokenType::PUNCTUATOR, "]");
                    iden = std::make_unique<ArrayIndexedAST>(std::move(iden), std::move(index));
                }
                return iden;
            }
            return std::make_unique<IdentifierExprAST>(id, current_package);
        }
        currentToken.error("Invalid value expression");
    }

    std::unique_ptr<ExprAST> parseTerm() {
        // std::cout << "parsing term at: ";
        // std::cout << currentToken << std::endl;
        if (match(TokenType::PUNCTUATOR, "-") || match(TokenType::PUNCTUATOR, "!") ||
            match(TokenType::PUNCTUATOR, "+")) {
            Token op = currentToken;
            next();
            auto operand = parseTerm();
            return std::make_unique<UnaryExprAST>(op, std::move(operand));
        }
        return parseValue();
    }

    std::unique_ptr<ExprAST> parseParenExpr() {
        // std::cout << "parsing parentexpr at: ";
        // std::cout << tokenToString(currentToken) << std::endl;
        expect(TokenType::PUNCTUATOR, "(");
        auto expr = parseExpr();
        expect(TokenType::PUNCTUATOR, ")");
        // std::cout << "parsing parentexpr complete at: ";
        // std::cout << tokenToString(currentToken) << std::endl;
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
    std::unique_ptr<ExprAST> parseArray() {
        expect(TokenType::PUNCTUATOR, "[");
        std::unique_ptr<NumberExprAST> num = nullptr;
        if (match(TokenType::NUMBER)) {
            num = std::make_unique<NumberExprAST>(expect(TokenType::NUMBER));
        }
        expect(TokenType::PUNCTUATOR, "]");
        std::string array_type = parseTypeName();
        expect(TokenType::PUNCTUATOR, "{");
        std::vector<std::unique_ptr<ExprAST>> args;
        if (!match(TokenType::PUNCTUATOR, "}")) {
            while (true) {
                args.push_back(parseExpr());
                if (!match(TokenType::PUNCTUATOR, ",")) {
                    break;
                }
                expect(TokenType::PUNCTUATOR, ",");
            }
        }
        expect(TokenType::PUNCTUATOR, "}");
        return std::make_unique<ArrayExprAST>(array_type, std::move(num), std::move(args));
    }
    std::unique_ptr<ExprAST> parseExpr(int exprPrec = 0) {
        // std::cout << "ExprAST at " << currentToken << std::endl;
        std::unique_ptr<ExprAST> lhs;
        if (match(TokenType::PUNCTUATOR, "("))
            lhs = parseParenExpr();
        else if (match(TokenType::PUNCTUATOR, "["))
            lhs = parseArray();
        else
            lhs = parseTerm();
        while (true) {
            if (match(TokenType::KEYWORD, "as")) {
                next();
                std::string cast_type = parseTypeName();
                lhs = std::make_unique<TypeCastAST>(std::move(lhs), cast_type);
                continue;
            }
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
        // std::cout << "parsing block at: ";
        // std::cout << tokenToString(currentToken) << std::endl;
        expect(TokenType::PUNCTUATOR, "{");
        auto block = std::make_unique<BlockAST>();
        while (!match(TokenType::PUNCTUATOR, "}")) {
            // std::cout << "going for another" << std::endl;
            block->addStatement(parseStatement());
            // std::cout << "added statement" << std::endl;
        }
        expect(TokenType::PUNCTUATOR, "}");
        return block;
    }
    std::unique_ptr<GlobalStatementAST> parseExternFunction() {
        expect(TokenType::KEYWORD, "extern");
        expect(TokenType::KEYWORD, "function");
        Token name = expect(TokenType::IDENTIFIER);
        expect(TokenType::PUNCTUATOR, "(");
        auto proto = parsePrototype();
        expect(TokenType::PUNCTUATOR, ")");
        expect(TokenType::PUNCTUATOR, "(");
        std::string returnType = parseTypeName();
        expect(TokenType::PUNCTUATOR, ")");
        expect(TokenType::NEWLINE);
        return std::make_unique<ExternFunctionAST>(name, std::move(proto), returnType, current_package);
    }
    std::unique_ptr<GlobalStatementAST> parseGlobalDeclaration() {

        // std::cout << tokenToString(currentToken) << std::endl;
        Token id = currentToken;
        next();
        if (match(TokenType::PUNCTUATOR, ":=")) {
            next();
            auto expr = parseExpr();
            expect(TokenType::NEWLINE);
            return std::make_unique<GlobalDeclarationAST>(id, std::move(expr), current_package);
        }
        currentToken.error("Unknown global declaration statement");
    }
    std::unique_ptr<ExprAST> parseLvalue() {
        Token id = expect(TokenType::IDENTIFIER);

        std::string pkg = id.value;
        if (match(TokenType::PUNCTUATOR, ".")) {
            next();
            Token field = expect(TokenType::IDENTIFIER);
            if (current_imports.count(id.value)) {
                pkg = current_imports[id.value];
            }
            id.value = pkg + "." + field.value;
        }

        std::unique_ptr<ExprAST> lhs = std::make_unique<IdentifierExprAST>(id, current_package);

        while (match(TokenType::PUNCTUATOR, "[")) {
            expect(TokenType::PUNCTUATOR, "[");
            std::unique_ptr<ExprAST> index = parseExpr();
            expect(TokenType::PUNCTUATOR, "]");
            lhs = std::make_unique<ArrayIndexedAST>(std::move(lhs), std::move(index));
        }

        return lhs;
    }
    std::unique_ptr<GlobalStatementAST> parseGlobalStatement() {
        // std::cout << "parsing statment at: ";
        // std::cout << tokenToString(currentToken) << std::endl;
        if (match(TokenType::KEYWORD, "function"))
            return parseFunction();
        if (match(TokenType::KEYWORD, "extern"))
            return parseExternFunction();
        if (match(TokenType::IDENTIFIER) && matchnext(TokenType::PUNCTUATOR, ":="))
            return parseGlobalDeclaration();
        // auto x = parseExpr();
        // expect(TokenType::PUNCTUATOR,";");
        // return std::move(x);
        currentToken.error("Unknown global statement");
    }
    std::unique_ptr<StatementAST> parseJustExpr(bool For = false) {
        // std::cout << "parsing justexpr at: ";
        // std::cout << currentToken << std::endl;
        auto x = parseExpr();
        if (!For)
            expect(TokenType::NEWLINE);
        return std::make_unique<JustExprAST>(std::move(x));
    }
    std::unique_ptr<StatementAST> parseDeclarationAssignmentOrExpr(bool For = false) {
        auto lhs = parseExpr();
        if (match(TokenType::PUNCTUATOR, ":=")) {
            next();
            auto rhs = parseExpr();
            if (!For)
                expect(TokenType::NEWLINE);
            if (auto idNode = dynamic_cast<IdentifierExprAST *>(lhs.get())) {
                return std::make_unique<DeclarationAST>(idNode->identifier, std::move(rhs), idNode->pkg_name);
            } else {
                error("Invalid left-hand side for declaration.");
            }
        } else if (match(TokenType::PUNCTUATOR, "=")) {
            next();
            auto rhs = parseExpr();
            if (!For)
                expect(TokenType::NEWLINE);
            return std::make_unique<AssignmentAST>(std::move(lhs), std::move(rhs), current_package);
        } else {
            if (!For)
                expect(TokenType::NEWLINE);
            return std::make_unique<JustExprAST>(std::move(lhs));
        }
    }
    std::unique_ptr<StatementAST> parseStatement() {
        // std::cout << "parsing statment at: ";
        // std::cout << currentToken << std::endl;
        if (match(TokenType::PUNCTUATOR, "{"))
            return parseBlock();
        // std::cout << "not scope " << currentToken << std::endl;
        if (match(TokenType::KEYWORD, "if"))
            return parseConditional();
        // std::cout << "not if " << currentToken << std::endl;
        if (match(TokenType::KEYWORD, "while"))
            return parseWhile();
        // std::cout << "not while " << currentToken << std::endl;
        if (match(TokenType::KEYWORD, "for"))
            return parseFor();
        // std::cout << "not for " << currentToken << std::endl;
        if (insideFunction && match(TokenType::KEYWORD, "return"))
            return parseReturn();
        // std::cout << "not return " << currentToken << std::endl;
        if (insideLoop && (match(TokenType::KEYWORD, "break") || match(TokenType::KEYWORD, "continue")))
            return parseBreakContinue();
        // std::cout << "not bc " << currentToken << std::endl;
        return parseDeclarationAssignmentOrExpr();
    }
    std::unique_ptr<PrototypeAST> parsePrototype() {
        // std::cout << "parsing proto: " << std::endl;
        // std::cout << tokenToString(currentToken) << std::endl;
        std::vector<std::pair<std::string, Token>> args;
        if (!match(TokenType::PUNCTUATOR, ")")) {
            while (true) {
                Token name = expect(TokenType::IDENTIFIER);
                std::string type = parseTypeName();
                args.push_back({type, name});
                if (!match(TokenType::PUNCTUATOR, ","))
                    break;
                expect(TokenType::PUNCTUATOR, ",");
            }
        }
        return std::make_unique<PrototypeAST>(std::move(args), current_package);
    }
    std::unique_ptr<FunctionAST> parseFunction() {
        // std::cout << "parsing function: " << std::endl;
        // std::cout << tokenToString(currentToken) << std::endl;
        expect(TokenType::KEYWORD, "function");
        Token name = expect(TokenType::IDENTIFIER);
        expect(TokenType::PUNCTUATOR, "(");
        auto proto = parsePrototype();
        expect(TokenType::PUNCTUATOR, ")");
        expect(TokenType::PUNCTUATOR, "(");
        std::string returnType = parseTypeName();
        expect(TokenType::PUNCTUATOR, ")");
        insideFunction++;
        auto body = parseBlock();
        insideFunction--;

        return std::make_unique<FunctionAST>(name, std::move(proto), returnType, std::move(body), current_package);
    }
    std::unique_ptr<StatementAST> parseConditional() {
        // std::cout << "parsing condition: " << std::endl;
        // std::cout << tokenToString(currentToken) << std::endl;
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
        // std::cout << "parsing while: " << std::endl;
        // std::cout << tokenToString(currentToken) << std::endl;
        expect(TokenType::KEYWORD, "while");
        auto cond = parseExpr();
        insideLoop++;
        auto body = parseBlock();
        insideLoop--;
        return std::make_unique<WhileAST>(std::move(cond), std::move(body));
    }

    // specifically for for statements
    std::unique_ptr<AssignmentAST> parseAssignmentNoSemicolon() {
        auto update_id = parseLvalue();
        expect(TokenType::PUNCTUATOR, "=");
        auto update_expr = parseExpr();
        auto update = std::make_unique<AssignmentAST>(std::move(update_id), std::move(update_expr), current_package);
        return update;
    }
    std::unique_ptr<StatementAST> parseFor() {
        // std::cout << "parsing for: " << std::endl;
        // std::cout << tokenToString(currentToken) << std::endl;
        expect(TokenType::KEYWORD, "for");
        auto init = parseDeclarationAssignmentOrExpr(true);
        expect(TokenType::PUNCTUATOR, ";");
        auto cond = parseExpr();
        expect(TokenType::PUNCTUATOR, ";");
        auto update = parseDeclarationAssignmentOrExpr(true);
        insideLoop++;
        auto body = parseBlock();
        insideLoop--;
        return std::make_unique<ForAST>(std::move(init), std::move(cond), std::move(update), std::move(body));
    }
    std::unique_ptr<PackageAST> parsePackage() {
        expect(TokenType::KEYWORD, "package");
        auto iden = expect(TokenType::IDENTIFIER);
        return std::make_unique<PackageAST>(iden);
    }
    std::unique_ptr<ImportAST> parseImport() {
        expect(TokenType::KEYWORD, "import");
        std::string alias = "";
        if (match(TokenType::IDENTIFIER)) {
            alias = expect(TokenType::IDENTIFIER).value;
        }
        std::string pkg_name = expect(TokenType::STRING).value;
        pkg_name = pkg_name.substr(1, pkg_name.size() - 2);
        if (alias == "")
            alias = pkg_name;
        return std::make_unique<ImportAST>(pkg_name, alias);
    }

  public:
    Parser() {}
    void newFile(const std::string &filename) { l = Lexer(filename); }
    std::unique_ptr<ProgramAST> parseProgram() {
        // std::cout << "parsing program: " << std::endl;
        auto program = std::make_unique<ProgramAST>();
        next();
        // kinda weird? yeah but fits with the theme of the code
        program->definePackage(parsePackage());
        current_package = program->package;
        current_imports.clear();

        // imports RAHHH
        while (match(TokenType::KEYWORD, "import")) {
            auto imp = parseImport();
            current_imports[imp->alias] = imp->pkg;
        }
        while (!match(TokenType::TK_EOF)) {
            program->addStatement(parseGlobalStatement());
        }
        return program;
    }
};
