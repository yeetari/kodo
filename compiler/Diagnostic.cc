#include <Diagnostic.hh>

#include <fmt/color.h>
#include <fmt/core.h>

namespace {

void print_message(const SourceLocation &location, const std::string &message, const fmt::text_style &type_style,
                   const char *type_string) {
    // TODO: Source file.
    auto line_source = location.line_source().substr(location.line_source().find_first_not_of(' '));
    fmt::print(stderr, fmt::fg(fmt::color::white) | fmt::emphasis::bold, "source.kd:{}:{}: ", location.line(),
               location.column());
    fmt::print(stderr, type_style, type_string);
    fmt::print(stderr, fmt::fg(fmt::color::white) | fmt::emphasis::bold, "{}\n", message);
    fmt::print(stderr, " {:4} | {}\n      |", location.line(), line_source);
    for (std::size_t i = 0; i < location.column() - (location.line_source().length() - line_source.length()); i++) {
        fmt::print(stderr, " ");
    }
    fmt::print(stderr, fmt::fg(fmt::terminal_color::bright_green) | fmt::emphasis::bold, "^\n");
}

} // namespace

Diagnostic::~Diagnostic() {
    print_message(m_location, m_error, fmt::fg(fmt::terminal_color::bright_red) | fmt::emphasis::bold, "error: ");
    for (const auto &[location, note] : m_notes) {
        print_message(location, note, fmt::fg(fmt::color::pink) | fmt::emphasis::bold, "note: ");
    }
    std::exit(1);
}
