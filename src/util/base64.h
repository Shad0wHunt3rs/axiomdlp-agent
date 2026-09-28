// src/util/base64.h
//
// Base64 encoding per RFC 4648. Uses OpenSSL's EVP_EncodeBlock so we
// don't have to hand-roll it.
//
// Used by the content scanner to encode file bytes into JSON-safe
// strings. Some text files (.sqlite, .parquet, .db) contain bytes
// that aren't valid UTF-8, and JSON strings must be valid UTF-8.

#pragma once

#include <cstddef>
#include <string>

namespace axiom::util {

std::string base64_encode(const unsigned char* data, std::size_t len);

inline std::string base64_encode(const char* data, std::size_t len) {
    return base64_encode(reinterpret_cast<const unsigned char*>(data), len);
}

inline std::string base64_encode(const std::string& s) {
    return base64_encode(reinterpret_cast<const unsigned char*>(s.data()), s.size());
}

} // namespace axiom::util