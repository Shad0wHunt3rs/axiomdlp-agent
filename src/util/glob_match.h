// src/util/glob_match.h
//
// Simple glob pattern matching used to filter filesystem paths.
//
// Supports:
//   *     — matches zero or more characters
//   ?     — matches exactly one character
//
// Also provides path_matches_pattern(), which additionally treats a
// pattern as a directory prefix when it does not end in a wildcard.

#pragma once

#include <string>

namespace axiom::util {

// Does `text` match `pattern`? Supports `*` and `?` wildcards.
bool glob_match(const std::string& pattern, const std::string& text);

// Extended match used for exclude lists:
//   1. Try a straight glob match.
//   2. If the pattern does not end in `*` or `?`, also treat it as a
//      directory prefix (pattern "foo/bar" matches "foo/bar/baz").
bool path_matches_pattern(const std::string& pattern,
                          const std::string& path);

} // namespace axiom::util
