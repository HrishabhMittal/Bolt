#include "parser.cpp"
#include "vm.hpp"
#include <bvm.hpp>
#include <fstream>
#include <memory>
#include <utility>

class Emitter {
    std::unique_ptr<ProgramAST> programAST;
    Program program;

  public:
    Emitter(Parser &p) : programAST(std::move(p.parseProgram())) {}
    Emitter(std::unique_ptr<ProgramAST> prog) : programAST(std::move(prog)) {}
    void emitcode(std::string filename) { programAST->codegen(program); }
};
