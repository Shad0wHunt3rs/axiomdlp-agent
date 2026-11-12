// src/util/ulid.cpp

#include "util/ulid.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <random>
#include <string>

namespace axiom::util {

namespace {

// Crockford's Base32 alphabet. Excludes I, L, O, U to reduce
// transcription errors. Uppercase only.
constexpr char kAlphabet[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

// Cryptographically-seeded RNG, constructed once and reused. The
// std::random_device seed comes from the OS CSPRNG on every call to
// operator() and is thread-safe on every implementation we target.
std::mt19937_64& rng() {
    static thread_local std::mt19937_64 instance{std::random_device{}()};
    return instance;
}

// Produce 80 bits of randomness (10 bytes) from the RNG.
void fill_random(std::uint8_t out[10]) {
    std::uint64_t hi = rng()();
    std::uint64_t lo = rng()();
    for (int i = 0; i < 8; ++i) {
        out[i]     = static_cast<std::uint8_t>(hi >> (56 - 8 * i));
        out[8 + i] = static_cast<std::uint8_t>(lo >> (56 - 8 * i));
        if (i == 1) break;  // only need 10 bytes total; loop handles it
    }
    // First 8 bytes from hi, next 2 bytes from top of lo.
    for (int i = 0; i < 8; ++i) {
        out[i] = static_cast<std::uint8_t>(hi >> (56 - 8 * i));
    }
    out[8] = static_cast<std::uint8_t>((lo >> 56) & 0xFF);
    out[9] = static_cast<std::uint8_t>((lo >> 48) & 0xFF);
}

// Encode 16 bytes (128 bits) as 26 Crockford-Base32 characters,
// with 2 zero bits at the leading edge of the encoded form.
std::string encode(const std::uint8_t data[16]) {
    std::string out(26, '0');

    for (int char_idx = 0; char_idx < 26; ++char_idx) {
        // The 130-bit encoded stream consists of 2 leading zero bits
        // followed by the 128 bits of `data`. For each output
        // character, collect its 5 bits.
        std::uint8_t value = 0;
        for (int bit_offset = 0; bit_offset < 5; ++bit_offset) {
            const int bit_pos = char_idx * 5 + bit_offset;
            std::uint8_t bit = 0;
            if (bit_pos >= 2) {
                const int data_bit = bit_pos - 2;
                const int byte_idx = data_bit / 8;
                const int off      = data_bit % 8;
                bit = static_cast<std::uint8_t>(
                    (data[byte_idx] >> (7 - off)) & 1);
            }
            value = static_cast<std::uint8_t>((value << 1) | bit);
        }
        out[char_idx] = kAlphabet[value];
    }
    return out;
}

} // namespace

std::string generate_ulid() {
    std::uint8_t buf[16];

    // 48-bit timestamp, big-endian.
    const std::uint64_t ms = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    buf[0] = static_cast<std::uint8_t>((ms >> 40) & 0xFF);
    buf[1] = static_cast<std::uint8_t>((ms >> 32) & 0xFF);
    buf[2] = static_cast<std::uint8_t>((ms >> 24) & 0xFF);
    buf[3] = static_cast<std::uint8_t>((ms >> 16) & 0xFF);
    buf[4] = static_cast<std::uint8_t>((ms >>  8) & 0xFF);
    buf[5] = static_cast<std::uint8_t>((ms      ) & 0xFF);

    // 80-bit randomness.
    fill_random(buf + 6);

    return encode(buf);
}

} // namespace axiom::util