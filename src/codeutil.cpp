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
std::vector<std::string> calc_registers_simd = {
    "xmm0", "xmm1", "xmm2", "xmm3",
    "xmm4", "xmm5", "xmm6", "xmm7",
    "xmm8", "xmm9", "xmm10", "xmm11",
    "xmm12", "xmm13", "xmm14", "xmm15"
};



namespace IdentifierManager {
    std::vector<std::vector<std::pair<int,Value>>> v;
}
namespace RegisterManager {
    namespace {
        std::unordered_map<std::string,bool> locked;
        std::vector<std::string> pushed; 
    }
    void lock(const std::string& reg) {
        locked[reg]=1;
    }
    void unlock(const std::string& reg) {
        locked[reg]=0;
    }
    std::string save(const std::string& reg) {
        pushed.push_back(reg);
        return "push "+reg+"\n";
    }
    std::string load_back() {
        std::string reg=pushed.back();
        pushed.pop_back();
        return "pop "+reg+"\n";
    }
    std::string load_some(int n) {
        std::string out;
        while (n--) {
            out+="pop "+pushed.back()+'\n';
            pushed.pop_back();
        }
        return out;
    }
    std::string get_new_reg(Value v) {
        if (v==DOUBLE||v==FLOAT) {
            for (auto& i:calc_registers_simd) {
                if (locked[i]!=1) {
                    return i;
                }
            }
            return "stack";
        } else if (v==INT) {
            for (auto& i:calc_registers_64) {
                if (locked[i]!=1) {
                    return i;
                }
            }
            return "stack";
        }
        throw std::runtime_error("type not supported yet");
    }
}
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
    { "," }
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
    { "," }
};
