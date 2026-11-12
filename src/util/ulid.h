// src/util/ulid.h
//
// ULID (Universally Unique Lexicographically Sortable Identifier).
//
// 128 bits = 48-bit millisecond timestamp (big-endian) + 80-bit
// random. Encoded as 26 uppercase ASCII characters in Crockford's
// Base32 (excludes I, L, O, U). Two padding bits are placed at the
// most-significant end of the encoded form, so the first character
// is always in the range '0'..'7'.
//
// Lexicographic order of encoded strings matches chronological order
// of generation time. This is why the server uses ULID for jti and
// request IDs.
//
// Thread-safe: each call is fully independent.

#pragma once

#include <string>

namespace axiom::util {

// Generate a fresh ULID. Uses std::random_device + std::mt19937_64
// seeded from the OS entropy source for the 80-bit random portion.
std::string generate_ulid();

} // namespace axiom::util