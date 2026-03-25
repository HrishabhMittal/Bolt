#include "emitter.cpp"
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
void get_all_bolt_files(std::vector<std::string> &vec, const std::string &dir) {
    const std::string extension = ".bolt";
    if (std::filesystem::exists(dir) && std::filesystem::is_directory(dir)) {
        for (const auto &entry : std::filesystem::directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == extension) {
                vec.push_back(entry.path().string());
            } else if (entry.is_directory()) {
                get_all_bolt_files(vec, entry.path().string());
            }
        }
    } else {
        std::cerr << "path" << dir << " does not exist or is not a directory." << std::endl;
    }
}
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
    std::vector<std::string> paths;
    try {
        get_all_bolt_files(paths, src_dir);
    } catch (const std::filesystem::filesystem_error &e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    for (auto &i : paths)
        e.add_file(i);
    e.emitcode(outfile);
    return 0;
}
