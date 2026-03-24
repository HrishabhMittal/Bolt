#include "bytecode.hpp"
#include "opcode.hpp"
#include "parser.cpp"
#include "vm.hpp"
#include <bvm.hpp>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <memory>
#include <stdexcept>
#include <utility>

class Emitter {
    std::unique_ptr<ProgramAST> programAST;
    Program program;

  public:
    Emitter(Parser &p) : programAST(std::move(p.parseProgram())) {}
    Emitter(std::unique_ptr<ProgramAST> prog) : programAST(std::move(prog)) {}
    void emitcode(std::string filename) {
        size_t main_jmp = program.size();
        program.push({bvm::OPCODE::CALL, {}});
        program.push({bvm::OPCODE::HALT, {}});
        programAST->codegen(program);
        uint64_t main_ip = program.main();
        if (main_ip == UINT64_MAX) {
            throw std::runtime_error("main was not defined in the file");
        }
        program[main_jmp].operands[0] = main_ip;
        std::ofstream file(filename, std::ios::binary);
        bvm::dump_bytecode({"BVM1.0 Executable", program.Code(), ""}, file);
        file.close();
    }
};
