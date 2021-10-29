#include <CharStream.hh>

#include <coel/support/Assert.hh>

#include <cstddef>
#include <fcntl.h>
#include <span>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

FileHandle::~FileHandle() {
    if (m_fd != -1) {
        close(m_fd);
    }
}

std::pair<FileHandle, CharStream> CharStream::open_file(const char *path) {
    // NOLINTNEXTLINE
    FileHandle handle(open(path, O_RDONLY));
    CharStream stream(handle);
    return {std::move(handle), std::move(stream)};
}

CharStream::CharStream(const FileHandle &handle) {
    struct stat stat {};
    fstat(handle.fd(), &stat);
    void *data = mmap(nullptr, stat.st_size, PROT_READ, MAP_PRIVATE, handle.fd(), 0);
    m_data = {static_cast<char *>(data), static_cast<std::span<char>::size_type>(stat.st_size)};
}

CharStream::~CharStream() {
    if (!m_data.empty()) {
        munmap(m_data.data(), m_data.size_bytes());
    }
}

bool CharStream::has_next() {
    return m_position != m_data.size_bytes();
}

char CharStream::peek() {
    return m_data[m_position];
}

char CharStream::next() {
    COEL_ASSERT(has_next());
    if (m_data[m_position] == '\n') {
        m_line_start = m_position + 1;
        m_line++;
        m_column = 1;
    } else {
        m_column++;
    }
    return m_data[m_position++];
}

SourceLocation CharStream::location() const {
    std::size_t line_length = 0;
    for (std::size_t i = m_line_start; m_data[i] != '\0' && m_data[i] != '\n'; i++, line_length++) {
    }
    return {m_line, m_column, {&m_data[m_line_start], line_length}};
}

char *CharStream::position_ptr() const {
    return &m_data[m_position];
}
