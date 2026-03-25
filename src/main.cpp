#include "emitter.cpp"
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>

int main(int argc, char **argv) {
    if (argc != 3)
        return 1;
    // this is old now
    // Lexer l(argv[1]);
    // Parser p(l);
    // auto program = p.parseProgram();
    // // program->print();
    // Emitter e(std::move(program));
    // e.emitcode(argv[2]);
    //
    //
    // we use packages like go :)
    const std::string src_dir = argv[1];
    const std::string extension = ".bolt";
    const std::string outfile = argv[2];
    Emitter e;
    try {
        if (std::filesystem::exists(src_dir) && std::filesystem::is_directory(src_dir)) {
            for (const auto &entry : std::filesystem::directory_iterator(src_dir)) {
                if (entry.is_regular_file() && entry.path().extension() == extension) {
                    e.add_file(entry.path().string());
                }
            }
        } else {
            std::cerr << "path" << src_dir << " does not exist or is not a directory." << std::endl;
        }
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    e.emitcode(outfile);
    return 0;
}
