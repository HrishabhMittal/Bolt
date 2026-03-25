#pragma once
#include <cstdint>
#include <ostream>
#include <string>
#include <vector>
inline std::string alone = "(){}[];,.";
inline std::string symbols = "~!@#$%^&*_+-=`|\\':\"<>?/";
inline std::vector<std::string> keywords = {"if",     "else",   "while",    "break",  "continue", "for",  "struct",
                                            "return", "extern", "function", "import", "package",  "void", "bool",
                                            "string", "true",   "false",    "u8",     "u16",      "u32",  "u64",
                                            "i8",     "i16",    "i32",      "i64",    "f32",      "f64"};

inline std::string repeat(const std::string &s, int i) {
    std::string str;
    for (int j = 0; j < i; j++)
        str += s;
    return str;
}
enum Value { DOUBLE, FLOAT, INT, BOOL, CHAR, STRING };

enum class TokenType { TK_ERR, IDENTIFIER, KEYWORD, PUNCTUATOR, STRING, NUMBER, NEWLINE, TK_EOF };

struct Token {
    TokenType ttype;
    std::string value;
    int64_t lineno;
    int64_t startindex;
    const std::string *line;
    // maybe i have to remove this if i run into problems later
    bool operator==(const char *c) { return c == value; }
};
inline bool operator==(const Token &t1, const Token &t2) { return t1.ttype == t2.ttype && t1.value == t2.value; }
inline std::string tokenToString(const Token &tok) {
    std::string typeStr;
    switch (tok.ttype) {
    case TokenType::IDENTIFIER:
        typeStr = "IDENTIFIER";
        break;
    case TokenType::KEYWORD:
        typeStr = "KEYWORD";
        break;
    case TokenType::PUNCTUATOR:
        typeStr = "PUNCTUATOR";
        break;
    case TokenType::STRING:
        typeStr = "STRING";
        break;
    case TokenType::NUMBER:
        typeStr = "NUMBER";
        break;
    case TokenType::TK_EOF:
        typeStr = "EOF";
        break;
    case TokenType::NEWLINE:
        typeStr = "NL";
        break;
    default:
        typeStr = "UNKNOWN";
        break;
    }

    return "Token(" + typeStr + ", value='" + tok.value + "')";
}
inline std::ostream &operator<<(std::ostream &out, Token t) {
    // out<<"Token(type:"<<(int)t.ttype<<", value:"<<t.value<<",
    // lineno:"<<t.lineno<<", startindex:"<<t.startindex<<",
    // line:"<<*t.line<<")";
    out << tokenToString(t) << std::endl;
    return out;
}

inline Value type(std::string type_str) {
    if (type_str == "bool")
        return BOOL;
    if (type_str == "char")
        return CHAR;
    if (type_str == "int")
        return INT;
    if (type_str == "float")
        return FLOAT;
    if (type_str == "double")
        return DOUBLE;
    if (type_str == "string")
        return STRING;
    throw std::runtime_error("Unrecognised Value: " + type_str);
}
inline std::string typeToStr(Value type) {
    if (type == BOOL)
        return "BOOL";
    if (type == CHAR)
        return "CHAR";
    if (type == INT)
        return "INT";
    if (type == FLOAT)
        return "FLOAT";
    if (type == DOUBLE)
        return "DOUBLE";
    if (type == STRING)
        return "STRING";
    throw std::runtime_error("Unrecognised Value Value");
}
