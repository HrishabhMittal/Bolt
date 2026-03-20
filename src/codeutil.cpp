#include "lexer.cpp"
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

std::vector<std::string> reg_order_function_args = {
    "rdi","rsi","rdx","rcx","r8","r9"
};
std::vector<std::string> calc_registers_64 = {
    "rax", "rbx", "rcx", "rdx",
    "rsi", "rdi",
    "r8", "r9", "r10", "r11",
    "r12", "r13", "r14", "r15"
};
std::vector<std::string> reg_to_save = {
    "rbx",
    "r12", "r13", "r14", "r15"
};
std::vector<std::string> save_registers_no_func = {
    "rcx", 
    "rsi", "rdi",
    "r8", "r9", "r10", "r11",
    "rbx",
    "r12", "r13", "r14", "r15"
};
std::vector<std::string> save_registers_func = {
    "rcx", "rsx", "rcx", "rdx",
    "rsi", "rdi",
    "r8", "r9", "r10", "r11",
    "r12", "r13", "r14", "r15"
};
std::vector<std::string> calc_registers_simd = {
    "xmm0", "xmm1", "xmm2", "xmm3",
    "xmm4", "xmm5", "xmm6", "xmm7",
    "xmm8", "xmm9", "xmm10", "xmm11",
    "xmm12", "xmm13", "xmm14", "xmm15"
};


std::vector<std::vector<std::string>> ops_by_precedence = {
    { "()", "[]", ".", "->" },
    { "++", "--", "+", "-", "!", "~", "*", "&" },
    { ".*", "->*" },
    { "*", "/", "%" },
    { "+", "-" },
    { "<<", ">>" },
    { "<", "<=", ">", ">=" },
    { "==", "!=" },
    { "&" },
    { "^" },
    { "|" },
    { "&&" },
    { "||" },
    { "?" },
    { "=", "+=", "-=", "*=", "/=", "%=", 
      "&=", "|=", "^=", "<<=", ">>=" },
};
std::vector<std::vector<std::string>> binops_by_precedence = {
    { ".*", "->*" },
    { "*", "/", "%" },
    { "+", "-" },
    { "<<", ">>" },
    { "<", "<=", ">", ">=" },
    { "==", "!=" },
    { "&" },
    { "^" },
    { "|" },
    { "&&" },
    { "||" },
    { "=", "+=", "-=", "*=", "/=", "%=",
      "&=", "|=", "^=", "<<=", ">>=" },
};
