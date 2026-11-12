// src/util/base64.cpp

#include "util/base64.h"

#include <openssl/evp.h>

#include <cstring>

namespace axiom::util {

std::string base64_encode(const unsigned char* data, std::size_t len) {
    if (len == 0 || data == nullptr) return {};

    // EVP_EncodeBlock writes 4 * ceil(len/3) bytes, then a null terminator.
    std::size_t out_len = 4 * ((len + 2) / 3);
    std::string out(out_len, '\0');

    int n = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(out.data()),
                            data, static_cast<int>(len));
    if (n <= 0) return {};

    out.resize(static_cast<std::size_t>(n));
    return out;
}

std::optional<std::vector<unsigned char>> base64_decode(const std::string& encoded) {
    if (encoded.empty()) return std::vector<unsigned char>{};

    // Reject anything containing whitespace or non-Base64 characters.
    // EVP_DecodeBlock silently ignores junk in some builds; the strict
    // pre-check catches corrupted wire data.
    for (char c : encoded) {
        if (!((c >= 'A' && c <= 'Z') ||
              (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') ||
              c == '+' || c == '/' || c == '=')) {
            return std::nullopt;
        }
    }

    // EVP_DecodeBlock returns the number of bytes written, which may
    // include 1 or 2 trailing zero bytes corresponding to '=' padding.
    // We strip them by counting the trailing '=' characters.
    std::size_t padding = 0;
    if (!encoded.empty() && encoded.back() == '=') {
        ++padding;
        if (encoded.size() >= 2 && encoded[encoded.size() - 2] == '=') {
            ++padding;
        }
    }

    const std::size_t max_out = (encoded.size() / 4) * 3 + 3;
    std::vector<unsigned char> out(max_out, 0);

    const int written = EVP_DecodeBlock(
        out.data(),
        reinterpret_cast<const unsigned char*>(encoded.data()),
        static_cast<int>(encoded.size()));
    if (written < 0) return std::nullopt;

    std::size_t real_size = static_cast<std::size_t>(written);
    if (padding <= real_size) real_size -= padding;
    out.resize(real_size);

    return out;
}

} // namespace axiom::util