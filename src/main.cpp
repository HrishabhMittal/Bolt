#include "emitter.cpp"

int main(int argc,char**argv) {
    if (argc!=3) return 1;
    Lexer l(argv[1]);
    Parser p(l);
    auto program=p.parseProgram();
    program->print();
    Emitter e(std::move(program));
    e.emitcode(argv[2]);
    return 0;
}
