#include <Analysis.hh>
#include <Ast.hh>
#include <AstLowering.hh>
#include <CharStream.hh>
#include <HirLowering.hh>
#include <Lexer.hh>
#include <Parser.hh>
#include <Token.hh>

#include <coel/codegen/Context.hh>
#include <coel/codegen/CopyInserter.hh>
#include <coel/codegen/RegisterAllocator.hh>
#include <coel/ir/Dumper.hh>
#include <coel/x86/Backend.hh>
#include <fmt/core.h>

#include <fstream>
#include <sys/mman.h>

int main(int argc, char **argv) {
    if (argc == 1) {
        fmt::print("Usage: {} [-r] [-v[v]] <input-file>\n", argv[0]);
        return 1;
    }
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

    auto [file, stream] = CharStream::open_file(input_file.c_str());
    Lexer lexer(std::move(stream));
    Parser parser(lexer);
    auto ast_root = parser.parse();
    auto hir_root = lower_ast(*ast_root);
    analyse_hir(hir_root);
    auto unit = lower_hir(hir_root);
    if (dump_ir) {
        fmt::print("============\n");
        fmt::print("GENERATED IR\n");
        fmt::print("============\n");
        coel::ir::dump(unit);
    }

    coel::codegen::Context context(unit);
    coel::codegen::insert_copies(context);
    if (dump_codegen) {
        fmt::print("===============\n");
        fmt::print("INSERTED COPIES\n");
        fmt::print("===============\n");
        coel::ir::dump(unit);
    }
    coel::codegen::register_allocate(context);
    if (dump_codegen) {
        fmt::print("===================\n");
        fmt::print("ALLOCATED REGISTERS\n");
        fmt::print("===================\n");
        coel::ir::dump(unit);
    }

    auto compiled = coel::x86::compile(unit);
    auto [entry, encoded] = coel::x86::encode(compiled, unit.find_function("main"));
    if (run) {
        auto *code_region =
            // NOLINTNEXTLINE
            mmap(nullptr, encoded.size(), PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        memcpy(code_region, encoded.data(), encoded.size());
        return reinterpret_cast<int (*)()>(static_cast<std::uint8_t *>(code_region) + entry)();
    }
    std::ofstream output_file("out.bin", std::ios::binary | std::ios::trunc);
    output_file.write(reinterpret_cast<const char *>(encoded.data()), static_cast<std::streamsize>(encoded.size()));
}
