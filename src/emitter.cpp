#include "parser.cpp"
#include "vm.hpp"
#include <bvm.hpp>
#include <fstream>
#include <memory>
#include <utility>
class Emitter {
    std::unique_ptr<ProgramAST> program;
    std::vector<bvm::instruction> code;

  public:
    Emitter(Parser &p) : program(std::move(p.parseProgram())) {}
    Emitter(std::unique_ptr<ProgramAST> prog) : program(std::move(prog)) {}
    void emitcode(std::string filename) { program->codegen(code); }
};
