// src/util/base64.h
//
// Base64 encoding/decoding per RFC 4648. Uses OpenSSL's EVP API so we
// don't have to hand-roll it.
//
// Encoding is used by the content scanner to encode file bytes into
// JSON-safe strings. Decoding is used by the MQTT transport to unpack
// base64 fields received from the server (bundle_b64, signature_b64,
// payload_b64).

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace axiom::util {

std::string base64_encode(const unsigned char* data, std::size_t len);

inline std::string base64_encode(const char* data, std::size_t len) {
    return base64_encode(reinterpret_cast<const unsigned char*>(data), len);
}

inline std::string base64_encode(const std::string& s) {
    return base64_encode(reinterpret_cast<const unsigned char*>(s.data()), s.size());
}

// Decode a Base64 string. Accepts both standard (with '=' padding)
// and unpadded input; rejects anything containing whitespace or
// characters outside the Base64 alphabet. Returns std::nullopt on
// malformed input.
//
// MQTT 5 property values (Correlation-Data, etc.) arrive as raw bytes
// but the JSON payloads carry Base64-encoded blobs. Both use this
// function via the JSON path.
std::optional<std::vector<unsigned char>> base64_decode(const std::string& encoded);

} // namespace axiom::util