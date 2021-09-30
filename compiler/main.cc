#include <Ast.hh>
#include <CharStream.hh>
#include <IrGen.hh>
#include <Lexer.hh>
#include <Parser.hh>
#include <Token.hh>

#include <codegen/codegen/Context.hh>
#include <codegen/codegen/CopyInserter.hh>
#include <codegen/codegen/RegisterAllocator.hh>
#include <codegen/ir/Dumper.hh>
#include <codegen/x86/Backend.hh>
#include <fmt/core.h>

#include <fstream>
#include <sys/mman.h>

int main(int argc, char **argv) {
    std::string input_file;
    bool dump_codegen = false;
    bool dump_ir = false;
    bool run = false;
    for (int i = 1; i < argc; i++) {
        std::string_view arg(argv[i]);
        if (arg.length() == 2 && arg == "-r") {
            run = true;
            continue;
        }
        if (arg.length() == 2 && arg == "-v") {
            dump_ir = true;
            continue;
        }
        if (arg.length() == 3 && arg == "-vv") {
            dump_codegen = true;
            dump_ir = true;
            continue;
        }
        if (arg.starts_with('-')) {
            fmt::print("error: unknown option {}\n", arg.substr(1));
            return 1;
        }
        if (!input_file.empty()) {
            fmt::print("error: already specified input file {}\n", input_file);
            return 1;
        }
        input_file = arg;
    }

    if (input_file.empty()) {
        fmt::print("error: no input file specified\n");
        return 1;
    }

    std::ifstream ifstream(input_file);
    CharStream stream(ifstream);
    Lexer lexer(stream);
    Parser parser(lexer);
    auto function = parser.parse();
    auto unit = generate_ir(*function);
    if (dump_ir) {
        fmt::print("============\n");
        fmt::print("GENERATED IR\n");
        fmt::print("============\n");
        ir::dump(unit);
    }

    codegen::Context context(unit);
    codegen::insert_copies(context);
    if (dump_codegen) {
        fmt::print("===============\n");
        fmt::print("INSERTED COPIES\n");
        fmt::print("===============\n");
        ir::dump(unit);
    }
    codegen::register_allocate(context);
    if (dump_codegen) {
        fmt::print("===================\n");
        fmt::print("ALLOCATED REGISTERS\n");
        fmt::print("===================\n");
        ir::dump(unit);
    }

    auto compiled = x86::compile(unit);
    auto encoded = x86::encode(compiled);
    if (run) {
        auto *code_region =
            mmap(nullptr, encoded.size(), PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        memcpy(code_region, encoded.data(), encoded.size());
        return reinterpret_cast<int (*)()>(code_region)();
    }
    std::ofstream output_file("out.bin", std::ios::binary | std::ios::trunc);
    output_file.write(reinterpret_cast<const char *>(encoded.data()), static_cast<std::streamsize>(encoded.size()));
}
