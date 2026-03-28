#include "lexer.cpp"
std::vector<std::vector<std::string>> ops_by_precedence = {
    {"()", "[]", ".", "->"},
    {"++", "--", "+", "-", "!", "~", "*", "&"},
    {".*", "->*"},
    {"*", "/", "%"},
    {"+", "-"},
    {"<<", ">>"},
    {"<", "<=", ">", ">="},
    {"==", "!="},
    {"&"},
    {"^"},
    {"|"},
    {"&&"},
    {"||"},
    {"?"},
    {"=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>="},
};
std::vector<std::vector<std::string>> binops_by_precedence = {
    {"*", "/", "%"}, {"+", "-"}, {"<<", ">>"}, {"<", "<=", ">", ">="}, {"==", "!="}, {"&"}, {"^"},
    {"|"},           {"&&"},     {"||"},
};

bool type_is_unsigned(const std::string &type) {
    return type[0] != 'i'; // hacky but works for now
}
int32_t get_type_size(const std::string &s) {
    if (s == "i32" || s == "u32" || s == "f32")
        return 4;
    else if (s == "i8" || s == "u8")
        return 1;
    else if (s == "i16" || s == "u16")
        return 2;
    else
        return 8;
}
bvm::OPCODE load_type(int size, bool is_unsigned) {
    if (size == 1) {
        return is_unsigned ? bvm::OPCODE::U8_ALOAD : bvm::OPCODE::I8_ALOAD;
    } else if (size == 2) {
        return is_unsigned ? bvm::OPCODE::U16_ALOAD : bvm::OPCODE::I16_ALOAD;
    } else if (size == 4) {
        return is_unsigned ? bvm::OPCODE::U32_ALOAD : bvm::OPCODE::I32_ALOAD;
    } else if (size == 8) {
        return bvm::OPCODE::I64_ALOAD;
    }
    error("Unsupported load size or unreachable code reached.");
}

bvm::OPCODE store_type(int size) {
    if (size == 1)
        return bvm::OPCODE::I8_ASTORE;
    else if (size == 2)
        return bvm::OPCODE::I16_ASTORE;
    else if (size == 4)
        return bvm::OPCODE::I32_ASTORE;
    else if (size == 8)
        return bvm::OPCODE::I64_ASTORE;
    error("unreachable code reached, crazy.");
}
struct Identifier {
    std::string name;
    std::string type;
};
struct Function {
    uint64_t ip;
    std::string name;
    std::vector<std::string> argtypes;
    std::string ret_type;
    bool is_external = false;
};
struct call_ref_in_map {
    std::string name;
    uint64_t ins;
};
struct function_code_and_offset {
    std::vector<bvm::instruction> code;
    uint64_t offset = 0;
};
struct loop_break_continue {
    uint64_t undeclare;
    uint64_t index;
};
class Program {
    uint64_t iden_stack_size = 0;
    std::string pushing_to_func;

    std::string data_section;
    // instructions i need to patch, which contain function calls
    std::map<std::string, std::vector<call_ref_in_map>> patch_function;
    // function code should be pushed here instead
    // idea is:
    // i need to rearrage the code like this
    // <global declarations>
    // call main
    // halt
    // <function declarations>
    //
    //
    // need to store all function calls and patch them in the end
    std::map<std::string, function_code_and_offset> function_code;
    std::vector<std::vector<loop_break_continue>> breaks, continues;
    std::vector<uint64_t> declared;
    std::vector<bvm::instruction> code;
    std::vector<std::vector<Identifier>> scope;
    std::map<std::string, Function> funcs;
    void precalc_all_offsets() {
        uint64_t off = code.size();
        for (auto &i : function_code) {
            i.second.offset = off;
            off += i.second.code.size();
        }
    }

  public:
    Program() {
        code.reserve(1024);
        scope.reserve(16);
        scope.push_back({});
    }
    void add_break(uint64_t undeclare_then_jmp_ptr) {
        loop_break_continue bc;
        bc.undeclare = scope.back().size();
        bc.index = undeclare_then_jmp_ptr;
        breaks.back().push_back(bc);
    }
    void add_continues(uint64_t undeclare_then_jmp_ptr) {
        loop_break_continue bc;
        bc.undeclare = scope.back().size();
        bc.index = undeclare_then_jmp_ptr;
        continues.back().push_back(bc);
    }
    void new_loop() {
        breaks.push_back({});
        continues.push_back({});
        declared.push_back(iden_stack_size);
    }
    void end_loop() {
        breaks.pop_back();
        continues.pop_back();
        declared.pop_back();
    }
    void patch_bc(uint64_t start, uint64_t end) {
        for (auto i : breaks.back()) {
            (*this)[i.index].operands[0] = i.undeclare;
            (*this)[i.index + 1].operands[0] = end;
        }
        for (auto i : continues.back()) {
            (*this)[i.index].operands[0] = i.undeclare;
            (*this)[i.index + 1].operands[0] = start;
        }
    }
    bvm::program construct_full_code() {
        // global decls alr there
        push_call("main.main");
        code.push_back({bvm::OPCODE::HALT});
        precalc_all_offsets();
        for (auto &i : patch_function)
            for (auto &j : i.second) {
                if (!funcs.count(i.first)) {
                    error("function " + i.first + " is called but never declared");
                }
                if (funcs[i.first].is_external) {
                    bvm::instruction &instr = (j.name == "") ? code[j.ins] : function_code[j.name].code[j.ins];
                    instr.op = bvm::OPCODE::CALL_EXTERN;
                    uint64_t string_ptr = data_size();
                    data_push(i.first + '\0');
                    instr.operands[0] = string_ptr;
                    continue;
                }
                if (j.name == "") {
                    code[j.ins].operands[0] = function_code[i.first].offset;
                } else
                    function_code[j.name].code[j.ins].operands[0] = function_code[i.first].offset;
            }
        const std::array<bvm::OPCODE, 9> jumps{bvm::OPCODE::JNC, bvm::OPCODE::JC,  bvm::OPCODE::JNE,
                                               bvm::OPCODE::JE,  bvm::OPCODE::JGE, bvm::OPCODE::JGT,
                                               bvm::OPCODE::JLE, bvm::OPCODE::JLT, bvm::OPCODE::JMP};
        for (auto &i : function_code)
            for (auto &j : i.second.code) {
                for (auto &jump : jumps) {
                    if (j.op == jump) {
                        j.operands[0] += i.second.offset;
                        break;
                    }
                }
                code.push_back(j);
            }
        bvm::program final_prog;
        for (const auto &[name, func] : funcs) {
            if (!func.is_external) {
                final_prog.exported_functions[name] = func.ip + function_code[name].offset;
            }
        }
        if (!scope.empty()) {
            for (size_t i = 0; i < scope[0].size(); i++) {
                final_prog.exported_globals[scope[0][i].name] = i;
            }
        }
        final_prog.code = code;
        final_prog.data = data_section;
        return final_prog;
    }
    const std::string &Data() { return data_section; }
    const std::vector<bvm::instruction> &Code() { return code; }
    size_t get_ip() { return code.size(); }
    void push(const bvm::instruction &i) {
        if (pushing_to_func == "")
            code.push_back(i);
        else
            function_code[pushing_to_func].code.push_back(i);
    }
    void push_call(const std::string &func_name) {
        bvm::instruction i;
        i.op = bvm::OPCODE::CALL;
        if (pushing_to_func == "") {
            patch_function[func_name].push_back({pushing_to_func, code.size()});
            code.push_back(i);
        } else {
            patch_function[func_name].push_back({pushing_to_func, function_code[pushing_to_func].code.size()});
            function_code[pushing_to_func].code.push_back(i);
        }
    }
    uint64_t data_size() { return data_section.size(); }
    void data_push(const std::string &s) { data_section += s; }
    uint64_t size() {
        if (pushing_to_func == "") {
            return code.size();
        }
        return function_code[pushing_to_func].code.size();
    }
    bvm::instruction &operator[](size_t ind) {
        if (pushing_to_func == "") {
            return code[ind];
        }
        return function_code[pushing_to_func].code[ind];
    }
    void new_scope() { scope.push_back({}); }
    void push_undeclare_for_return() {
        for (ssize_t i = static_cast<ssize_t>(scope.size()) - 1; i > 0; i--) {
            if (scope[i].size() > 0)
                push({bvm::OPCODE::UNDECLARE, {scope[i].size()}});
        }
    }
    void push_in_func(const std::string &func_name) { pushing_to_func = func_name; }
    void delete_scope() {
        if (scope.back().size() > 0)
            push({bvm::OPCODE::UNDECLARE, {scope.back().size()}});
        iden_stack_size -= scope.back().size();
        scope.pop_back();
    }
    void declare_function(Function i) {
        // now that im forward declaring, rewrite their pointer on second declaration
        if (funcs.count(i.name)) {
            if (funcs[i.name].ip == UINT64_MAX) {
                funcs[i.name].ip = i.ip;
                return;
            }
            error("redaclaration of " + i.name + " in the same scope.");
        }
        funcs[i.name] = i;
    }
    Function get_function(std::string name, std::string pkg_name) {
        if (funcs.count(name))
            return funcs[name];
        if (name.find('.') == std::string::npos && funcs.count(pkg_name + "." + name)) {
            return funcs[pkg_name + "." + name];
        }
        error("no declaration found for function " + name);
    }
    void declare(Identifier i) {
        for (Identifier j : scope.back()) {
            if (i.name == j.name)
                error("redaclaration of " + i.name + " in the same scope.");
        }
        push({bvm::OPCODE::DECLARE, {}});
        scope.back().push_back(i);
        iden_stack_size++;
    }
    int64_t getaddress(std::string iden, std::string pkg_name) {
        ssize_t top = 0;
        for (ssize_t i = static_cast<ssize_t>(scope.size()) - 1; i >= 0; i--) {
            for (ssize_t j = static_cast<ssize_t>(scope[i].size()) - 1; j >= 0; j--) {
                top--;
                if (scope[i][j].name == iden) {
                    if (i == 0)
                        return j;
                    else
                        return top;
                }
                if (i == 0 && iden.find('.') == std::string::npos && scope[i][j].name == pkg_name + "." + iden) {
                    return j;
                }
            }
        }
        error("identifier " + iden + " doesn't exist.");
    }
    std::string gettype(std::string iden, std::string pkg_name) {
        for (ssize_t i = static_cast<int64_t>(scope.size()) - 1; i >= 0; i--) {
            for (size_t j = 0; j < scope[i].size(); j++) {
                if (scope[i][j].name == iden)
                    return scope[i][j].type;
                if (i == 0 && iden.find('.') == std::string::npos && scope[i][j].name == pkg_name + "." + iden) {
                    return scope[i][j].type;
                }
            }
        }
        error("identifier " + iden + " doesn't exist.");
    }
};
