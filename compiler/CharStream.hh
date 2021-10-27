#pragma once

#include <SourceLocation.hh>

#include <cstddef>
#include <span>
#include <utility>

class FileHandle {
    int m_fd;

public:
    explicit FileHandle(int fd) : m_fd(fd) {}
    FileHandle(const FileHandle &) = delete;
    FileHandle(FileHandle &&other) noexcept : m_fd(std::exchange(other.m_fd, -1)) {}
    ~FileHandle();

    FileHandle &operator=(const FileHandle &) = delete;
    FileHandle &operator=(FileHandle &&) = delete;

    int fd() const { return m_fd; }
};

class CharStream {
    std::span<char> m_data;
    std::size_t m_position{0};
    std::size_t m_line_start{0};
    std::size_t m_line{1};
    std::size_t m_column{1};

public:
    static std::pair<FileHandle, CharStream> open_file(const char *path);

    explicit CharStream(const FileHandle &handle);
    CharStream(const CharStream &) = delete;
    CharStream(CharStream &&other) noexcept : m_data(other.m_data), m_position(other.m_position) { other.m_data = {}; }
    ~CharStream();

    CharStream &operator=(const CharStream &) = delete;
    CharStream &operator=(CharStream &&) = delete;

    bool has_next();
    char peek();
    char next();

    SourceLocation location() const;
    char *position_ptr() const;
};
