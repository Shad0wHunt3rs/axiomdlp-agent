// src/util/file_hash.h
//
// SHA-256 hashing of files and buffers, built on OpenSSL's EVP API.
//
// compute_sha256() reads the file in fixed-size chunks, so memory use
// is bounded regardless of file size. A 100 GB file uses the same
// memory as a 1 KB file.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace axiom::util {

struct FileHashResult {
    std::string   hex_digest;             // 64 lowercase hex chars, or empty on failure
    std::uint64_t bytes_read = 0;         // bytes fed into the hash
    std::string   error;                  // empty on success

    bool ok() const { return error.empty() && !hex_digest.empty(); }
};

// Compute the SHA-256 of a file's full contents.
// Reads in 1 MB chunks; never loads the entire file into memory.
FileHashResult compute_sha256(const std::string& path);

// Compute the SHA-256 of an in-memory buffer.
std::string sha256_hex(const void* data, std::size_t len);

} // namespace axiom::util