#include <cstdint>
#include <ostream>
#include <string>
#include <vector>
std::string alone = "(){}[];,.";
std::string symbols = "~!@#$%^&*_+-=`|\\':\"<>?/";
std::vector<std::string> keywords = {
    "if", "else", "while", "break", "continue", "for",
    "struct",
    "return", "function",
    "import",
    "int", "float", "double", "char", "bool", "long",
    "true","false"
};

std::string repeat(const std::string& s,int i) {
    std::string str;
    for (int j=0;j<i;j++) str+=s;
    return str;
}
enum Value {
    DOUBLE,
    FLOAT,
    INT,
    BOOL,
    CHAR,
    STRING
};

enum class TokenType {
    TK_ERR,
    IDENTIFIER,
    KEYWORD,
    PUNCTUATOR,
    STRING,
    NUMBER,
    NEWLINE,
    TK_EOF
};


struct Token {
    TokenType ttype;
    std::string value;
    int64_t lineno;
    int64_t startindex;
    const std::string* line;

};

std::string tokenToString(const Token& tok) {
    std::string typeStr;
    switch (tok.ttype) {
        case TokenType::IDENTIFIER: typeStr = "IDENTIFIER"; break;
        case TokenType::KEYWORD: typeStr = "KEYWORD"; break;
        case TokenType::PUNCTUATOR: typeStr = "PUNCTUATOR"; break;
        case TokenType::STRING: typeStr = "STRING"; break;
        case TokenType::NUMBER: typeStr = "NUMBER"; break;
        case TokenType::TK_EOF: typeStr = "EOF"; break;
        case TokenType::NEWLINE: typeStr = "NL"; break;
        default: typeStr = "UNKNOWN"; break;
    }


    return "Token(" + typeStr +
           ", value='" + tok.value + "')";
}
std::ostream& operator<<(std::ostream& out,Token t) {
    //out<<"Token(type:"<<(int)t.ttype<<", value:"<<t.value<<", lineno:"<<t.lineno<<", startindex:"<<t.startindex<<", line:"<<*t.line<<")";
    out<<tokenToString(t)<<std::endl;
    return out;
}

Value type(std::string type_str) {
    if (type_str == "bool") return BOOL;
    if (type_str == "char") return CHAR;
    if (type_str == "int") return INT;
    if (type_str == "float") return FLOAT;
    if (type_str == "double") return DOUBLE;
    if (type_str == "string") return STRING;
    throw std::runtime_error("Unrecognised Value: " + type_str);
}
std::string typeToStr(Value type) {
    if (type == BOOL) return "BOOL";
    if (type == CHAR) return "CHAR";
    if (type == INT) return "INT";
    if (type == FLOAT) return "FLOAT";
    if (type == DOUBLE) return "DOUBLE";
    if (type == STRING) return "STRING";
    throw std::runtime_error("Unrecognised Value Value");
}
