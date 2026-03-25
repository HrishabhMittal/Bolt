#include "bytecode.hpp"
#include "opcode.hpp"
#include "parser.cpp"
#include "vm.hpp"
#include <algorithm>
#include <bvm.hpp>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

class Emitter {
    Program program;
    Parser p;
    std::vector<std::unique_ptr<ProgramAST>> progs;

  public:
    Emitter() {}
    void add_file(const std::string &filename) {
        p.newFile(filename);
        progs.push_back(p.parseProgram());
    }
    void emitcode(std::string filename) {
        std::vector<GlobalDeclarationAST *> globals;
        std::vector<FunctionAST *> functions;
        std::map<std::string, GlobalDeclarationAST *> global_map;

        for (auto &prog : progs) {
            for (auto &statement : prog->statements) {
                if (auto func = dynamic_cast<FunctionAST *>(statement.get())) {
                    program.declare_function({UINT64_MAX, func->name.value, func->returnType.value});
                    functions.push_back(func);
                } else if (auto glob = dynamic_cast<GlobalDeclarationAST *>(statement.get())) {
                    globals.push_back(glob);
                    global_map[glob->identifier.value] = glob;
                }
            }
        }

        std::vector<GlobalDeclarationAST *> sorted_globals;
        std::map<std::string, int> state;

        // doofus the dfs function (im going insane rn)
        std::function<void(const std::string &)> doofus = [&](const std::string &name) {
            if (state[name] == 1) {
                throw std::runtime_error("Circular dependency detected in global variable: " + name);
            }
            if (state[name] == 2)
                return;

            state[name] = 1;

            auto glob = global_map[name];
            auto deps = glob->expr->get_dependencies();
            for (const auto &dep : deps) {
                if (global_map.count(dep)) {
                    doofus(dep);
                }
            }

            state[name] = 2;
            sorted_globals.push_back(glob);
        };
        for (auto g : globals) {
            if (state[g->identifier.value] == 0) {
                doofus(g->identifier.value);
            }
        }
        for (auto g : sorted_globals) {
            g->codegen(program);
        }
        for (auto f : functions) {
            f->codegen(program);
        }

        program.construct_full_code();
        std::ofstream file(filename, std::ios::binary);
        bvm::dump_bytecode({"BVM1.0 Executable", program.Code(), program.Data()}, file);
        file.close();
    }
};
