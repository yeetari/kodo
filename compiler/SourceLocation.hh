#pragma once

#include <cstddef>
#include <string_view>

class SourceLocation {
    std::size_t m_line;
    std::size_t m_column;
    // TODO: This should be retrieved rather than stored.
    std::string_view m_line_source;

public:
    SourceLocation(std::size_t line, std::size_t column, std::string_view line_source)
        : m_line(line), m_column(column), m_line_source(line_source) {}

    std::size_t line() const { return m_line; }
    std::size_t column() const { return m_column; }
    const std::string_view &line_source() const { return m_line_source; }
};
