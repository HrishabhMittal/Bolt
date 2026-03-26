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
                    std::string extend = func->pkg_name + "." + func->name.value;
                    program.declare_function({UINT64_MAX, extend, func->proto->type_list(), func->returnType.value});
                    functions.push_back(func);
                } else if (auto ext = dynamic_cast<ExternFunctionAST *>(statement.get())) {
                    std::string extend = ext->pkg_name + "." + ext->name.value;
                    program.declare_function(
                        {UINT64_MAX, extend, ext->proto->type_list(), ext->returnType.value, true});
                } else if (auto glob = dynamic_cast<GlobalDeclarationAST *>(statement.get())) {
                    std::string extend = glob->pkg_name + "." + glob->identifier.value;
                    globals.push_back(glob);
                    global_map[extend] = glob;
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
            std::string extended_name = g->pkg_name + "." + g->identifier.value;
            if (state[extended_name] == 0) {
                doofus(extended_name);
            }
        }
        for (auto g : sorted_globals) {
            g->codegen(program);
        }
        for (auto f : functions) {
            f->codegen(program);
        }
        bvm::program out_prog = program.construct_full_code();
        out_prog.header = "BVM1.0 Executable";
        std::ofstream file(filename, std::ios::binary);
        bvm::dump_bytecode(out_prog, file);
        file.close();
    }
};
