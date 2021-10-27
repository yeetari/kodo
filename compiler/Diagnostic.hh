#pragma once

#include <SourceLocation.hh>

#include <fmt/format.h>

#include <string>
#include <vector>

class Diagnostic {
    const SourceLocation m_location;
    const std::string m_error;
    std::vector<std::pair<const SourceLocation &, std::string>> m_notes;

public:
    template <typename... Args>
    Diagnostic(const SourceLocation &location, const char *fmt, Args &&...args)
        : m_location(location), m_error(fmt::format(fmt, std::forward<Args>(args)...)) {}
    Diagnostic(const Diagnostic &) = delete;
    Diagnostic(Diagnostic &&) = delete;
    ~Diagnostic();

    Diagnostic &operator=(const Diagnostic &) = delete;
    Diagnostic &operator=(Diagnostic &&) = delete;

    template <typename... Args>
    void add_note(const SourceLocation &location, const char *fmt, Args &&... args) {
        m_notes.emplace_back(location, fmt::format(fmt, std::forward<Args>(args)...));
    }
};
