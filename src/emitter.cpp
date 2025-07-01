#include "parser.cpp"
#include <fstream>
#include <memory>
#include <utility>


class Emitter {
    std::unique_ptr<ProgramAST> program;
public:
    Emitter(Parser& p): program(std::move(p.parseProgram())) {}
    void emitcode(std::string filename) {
        std::ofstream file(filename);
        file<<program->codegen();
    }
};
