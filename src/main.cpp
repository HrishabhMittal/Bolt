#include "emitter.cpp"

int main(int argc,char**argv) {
    if (argc==1) return 1;
    Parser parser = Lexer(argv[1]);
    Emitter emitter(parser);
    emitter.emitcode("a.asm");
    return 0;
}
