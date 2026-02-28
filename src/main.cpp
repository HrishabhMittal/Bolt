#include "emitter.cpp"

int main(int argc,char**argv) {
    if (argc==1) return 1;
    Lexer l(argv[1]);
    Parser p(l);
    auto program=p.parseProgram();
    program->print();
    // Emitter e(p);
    // e.emitcode();
    return 0;
}
