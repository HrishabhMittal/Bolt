#pragma once
#include <algorithm>
#include <bit>
#include <bvm.hpp>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <ios>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
inline std::string alone = "(){}[];,.";
inline std::string symbols = "~!@#$%^&*_+-=`|\\':\"<>?/";
inline std::vector<std::string> keywords = {"if",     "else",   "while",    "break",  "continue", "for",  "struct",
                                            "return", "extern", "function", "import", "package",  "void", "bool",
                                            "string", "true",   "false",    "u8",     "u16",      "u32",  "u64",
                                            "i8",     "i16",    "i32",      "i64",    "f32",      "f64"};
[[noreturn]] inline void error(const std::string &msg, int32_t exitcode = 1) {
    std::cout << msg << std::endl;
    std::exit(exitcode);
}
inline std::string repeat(const std::string &s, int i) {
    std::string str;
    for (int j = 0; j < i; j++)
        str += s;
    return str;
}
enum Value { DOUBLE, FLOAT, INT, BOOL, CHAR, STRING };

enum class TokenType { TK_ERR, IDENTIFIER, KEYWORD, PUNCTUATOR, STRING, NUMBER, NEWLINE, TK_EOF };

inline std::string resolve_string(std::string_view input) {
    if (input.size() < 2)
        return std::string(input);
    if (input.front() == '"' && input.back() == '"') {
        input.remove_prefix(1);
        input.remove_suffix(1);
    }
    std::string out;
    out.reserve(input.size());
    for (size_t i = 0; i < input.size(); i++) {
        if (input[i] == '\\' && i + 1 < input.size()) {
            switch (input[++i]) {
            case 'n':
                out += '\n';
                break;
            case 't':
                out += '\t';
                break;
            case 'r':
                out += '\r';
                break;
            case 'v':
                out += '\v';
                break;
            case 'b':
                out += '\b';
                break;
            case 'f':
                out += '\f';
                break;
            case 'a':
                out += '\a';
                break;
            case '\\':
                out += '\\';
                break;
            case '"':
                out += '"';
                break;
            case '\'':
                out += '\'';
                break;
            case '?':
                out += '?';
                break;
            case 'x': {
                if (i + 2 < input.size()) {
                    std::string hex_str(input.substr(i + 1, 2));
                    out += (char)std::stoi(hex_str, nullptr, 16);
                    i += 2;
                }
                break;
            }
            case '0' ... '7': {
                size_t len = 0;
                while (len < 3 && i + len < input.size() && input[i + len] >= '0' && input[i + len] <= '7') {
                    len++;
                }
                std::string oct_str(input.substr(i, len));
                out += (char)std::stoi(oct_str, nullptr, 8);
                i += len - 1;
                break;
            }
            default:
                out += input[i];
                break;
            }
        } else {
            out += input[i];
        }
    }
    return out;
}
struct Token {
    TokenType ttype;
    std::string value;
    int64_t lineno;
    int64_t startindex;
    const std::string *line;
    void printTokenUnderlined() {
        if (line != nullptr) {
            std::cout << "    " << *line << std::endl
                      << "    " << repeat(" ", startindex) << repeat("^", value.size()) << std::endl;
        }
    }
    [[noreturn]] void error(const std::string &msg) {
        const std::string &safeline = (line == nullptr) ? std::string() : *line;
        std::cerr << "At: " << value << std::endl;
        printTokenUnderlined();
        ::error(msg);
    }
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
    error("Unrecognised Value: " + type_str);
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
    error("Unrecognised Value Value");
}
