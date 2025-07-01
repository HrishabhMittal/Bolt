#include "header.hpp"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>
Value valuetype(const std::string &s) {
    std::cout<<"calc for: "<<s<<std::endl;
    if (s.front() == '"') return STRING;
    if (s == "true" || s == "false") return BOOL;
    if (std::isdigit(s.front())) {
        if (s.back() == 'f' || s.back() == 'F') return FLOAT;
        if (s.find('.') != std::string::npos || 
            s.find('e') != std::string::npos ||
            s.find('E') != std::string::npos)
            return DOUBLE;
        return INT;
    }
    throw std::runtime_error("Unrecognised literal: " + s);
}
bool ispresentin(char c, const std::string& s) {
    for (char i : s) if (c == i) return true;
    return false;
}
class Lexer {
    std::vector<std::string> data;
    int64_t pos_line = 0;
    int64_t pos_char = 0;
public:
    Lexer(const std::string& filename) {
        std::ifstream infile(filename);
        std::string line;
        while (std::getline(infile, line)) {
            data.push_back(line + '\n');
        }
    }
    Token peektoken() {
        int64_t p_line = pos_line;
        int64_t p_char = pos_char;
       Token out=gettoken();
       pos_line=p_line;
       pos_char=p_char;
       return out;
    }
    Token gettoken() {
        Token t=_gettoken();
        std::cout<<t<<std::endl;
        return t;
    }
    Token _gettoken() {
        while (pos_line < (int64_t)data.size()) {
            std::string& line_str = data[pos_line];
            if (pos_char >= (int64_t)line_str.size()) {
                pos_line++;
                pos_char = 0;
                continue;
            }
            char c = line_str[pos_char++];
            while (c == ' ' || c == '\t' || c == '\r') {
                if (pos_char >= (int64_t)line_str.size()) {
                    pos_line++;
                    pos_char = 0;
                    if (pos_line >= (int64_t)data.size()) 
                        break;
                    line_str = data[pos_line];
                }
                c = line_str[pos_char++];
            }
            if (c == '\n') {
                return Token{TokenType::NEWLINE, "\n", pos_line+1, pos_char-1, &line_str};
            }
            if (pos_line >= (int64_t)data.size()) {
                break;
            }
            int64_t startindex = pos_char - 1;
            std::string value;
            if (std::isalpha(c) || c == '_') {
                value += c;
                while (pos_char < (int64_t)line_str.size() &&
                       (std::isalnum(line_str[pos_char]) || line_str[pos_char] == '_')) {
                    value += line_str[pos_char++];
                }
                return findtoken(value, pos_line + 1, startindex, &line_str);
            }
            if (std::isdigit(c)) {
                value += c;
                bool dot=false;
                while (pos_char < (int64_t)line_str.size() && (std::isdigit(line_str[pos_char])||(line_str[pos_char]=='.'&&!dot))) {
                    if (line_str[pos_char]=='.') dot=true;
                    value += line_str[pos_char++];
                }
                if (pos_char < (int64_t)line_str.size() && line_str[pos_char]=='f') 
                    value += line_str[pos_char++];
                return findtoken(value, pos_line + 1, startindex, &line_str);
            }
            if (c == '"') {
                value += c;
                while (pos_char < (int64_t)line_str.size()) {
                    char ch = line_str[pos_char++];
                    value += ch;
                    if (ch == '"') break;
                }
                return findtoken(value, pos_line + 1, startindex, &line_str);
            }
            if (ispresentin(c, symbols)) {
                value += c;
                while (pos_char < (int64_t)line_str.size() && ispresentin(line_str[pos_char],symbols)) {
                    value += line_str[pos_char++];
                }
                return findtoken(value, pos_line + 1, startindex, &line_str);
            }
            value += c;
            return findtoken(value, pos_line + 1, startindex, &line_str);
        }
        return Token{TokenType::TK_EOF, "", pos_line + 1, pos_char, nullptr};
    }
private:
    Token findtoken(const std::string& s, int64_t line, int64_t index, const std::string* line_text) {
        TokenType ttype;
        if (s.empty()) return {TokenType::TK_EOF, "", line, index, line_text};
        if (std::isdigit(s[0])) {
            ttype = TokenType::NUMBER;
        } else if (std::isalpha(s[0]) || s[0] == '_') {
            if (std::find(keywords.begin(), keywords.end(), s) != keywords.end()) {
                ttype = TokenType::KEYWORD;
            } else {
                ttype = TokenType::IDENTIFIER;
            }
        } else if (s[0] == '"' && s.back() == '"') {
            ttype = TokenType::STRING;
        } else if (ispresentin(s[0], symbols)||ispresentin(s[0], alone)) {
            ttype = TokenType::PUNCTUATOR;
        } else if (s=="\n") {
            ttype = TokenType::NEWLINE;
        } else {
            ttype = TokenType::TK_ERR;
        }
        return Token{ttype, s, line, index, line_text};
    }
};
