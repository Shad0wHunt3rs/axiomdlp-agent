// src/util/file_hash.cpp

#include "util/file_hash.h"

#include <openssl/evp.h>

#include <cstdio>
#include <memory>
#include <vector>

namespace axiom::util {

namespace {

constexpr std::size_t kChunkSize = 1024 * 1024;   // 1 MB

std::string bytes_to_hex(const unsigned char* bytes, unsigned int len) {
    static const char hexchars[] = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2);
    for (unsigned int i = 0; i < len; ++i) {
        out.push_back(hexchars[(bytes[i] >> 4) & 0x0F]);
        out.push_back(hexchars[bytes[i] & 0x0F]);
    }
    return out;
}

// Small RAII wrapper for EVP_MD_CTX.
struct MdCtxDeleter {
    void operator()(EVP_MD_CTX* c) const noexcept {
        if (c) EVP_MD_CTX_free(c);
    }
};

using MdCtxPtr = std::unique_ptr<EVP_MD_CTX, MdCtxDeleter>;

} // namespace

FileHashResult compute_sha256(const std::string& path) {
    FileHashResult r;

    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        r.error = "cannot open file";
        return r;
    }

    struct FileCloser {
        std::FILE* p;
        ~FileCloser() { if (p) std::fclose(p); }
    } closer{f};

    MdCtxPtr ctx(EVP_MD_CTX_new());
    if (!ctx) {
        r.error = "EVP_MD_CTX_new failed";
        return r;
    }

    if (EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1) {
        r.error = "EVP_DigestInit_ex failed";
        return r;
    }

    std::vector<unsigned char> buffer(kChunkSize);

    for (;;) {
        const std::size_t n = std::fread(buffer.data(), 1, buffer.size(), f);
        if (n > 0) {
            if (EVP_DigestUpdate(ctx.get(), buffer.data(), n) != 1) {
                r.error = "EVP_DigestUpdate failed";
                return r;
            }
            r.bytes_read += n;
        }
        if (n < buffer.size()) {
            if (std::ferror(f)) {
                r.error = "read error";
                return r;
            }
            break;   // EOF
        }
    }

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int  digest_len = 0;
    if (EVP_DigestFinal_ex(ctx.get(), digest, &digest_len) != 1) {
        r.error = "EVP_DigestFinal_ex failed";
        return r;
    }

    r.hex_digest = bytes_to_hex(digest, digest_len);
    return r;
}

std::string sha256_hex(const void* data, std::size_t len) {
    MdCtxPtr ctx(EVP_MD_CTX_new());
    if (!ctx) return {};

    if (EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1) return {};
    if (EVP_DigestUpdate(ctx.get(), data, len) != 1) return {};

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int  digest_len = 0;
    if (EVP_DigestFinal_ex(ctx.get(), digest, &digest_len) != 1) return {};

    return bytes_to_hex(digest, digest_len);
}

} // namespace axiom::util