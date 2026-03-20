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
        CodeGenContext ctx;
        
        ctx.text_section << "section .text\n";
        ctx.data_section << "section .data\n";

        ctx.text_section << "\nglobal _start\n";
        ctx.text_section << "_start:\n";
        ctx.text_section << "    call main\n";
        ctx.text_section << "    mov rdi, rax\n";
        ctx.text_section << "    mov rax, 60\n";
        ctx.text_section << "    syscall\n\n";

        program->codegen(ctx);

        std::ofstream file(filename);
        file << ctx.data_section.str() << "\n";
        file << ctx.text_section.str() << "\n";
    }
};
