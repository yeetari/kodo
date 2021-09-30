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

int main(int, char **argv) {
    std::ifstream ifstream(argv[1]);
    CharStream stream(ifstream);
    Lexer lexer(stream);
    Parser parser(lexer);
    auto function = parser.parse();
    auto unit = generate_ir(*function);
    ir::dump(unit);

    codegen::Context context(unit);
    codegen::insert_copies(context);
    codegen::register_allocate(context);

    auto compiled = x86::compile(unit);
    auto encoded = x86::encode(compiled);
    std::ofstream output_file("foo.bin", std::ios::binary | std::ios::trunc);
    output_file.write(reinterpret_cast<const char *>(encoded.data()), static_cast<std::streamsize>(encoded.size()));
}
