#include "parser.cpp"
#include <fstream>
#include <memory>
#include <utility>

class Emitter {
    std::unique_ptr<ProgramAST> program;
public:
    Emitter(Parser& p): program(std::move(p.parseProgram())) {}
    Emitter(std::unique_ptr<ProgramAST> prog): program(std::move(prog)) {}   
    void emitcode(std::string filename) {
        program->codegen();
    }
};
