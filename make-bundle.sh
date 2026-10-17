#!/usr/bin/env bash
#
# make-bundle.sh — bundle the AxiomDLP agent's OWN source into one markdown
# file for an LLM/agent. Third-party toolchains, build outputs, and
# dependency trees are skipped; every skip is recorded in a footer so the
# agent can ask for anything it needs by name.
#
set -euo pipefail

ROOT="${ROOT:-$(pwd)}"
OUT="${OUT:-$ROOT/axiomdlp-agent-bundle.md}"
QUIET="${QUIET:-0}"
MAX_FILE_SIZE="${MAX_FILE_SIZE:-524288}"   # 512 KiB per-file guard (env-overridable)

[[ -f "$ROOT/CMakeLists.txt" ]] || {
    echo "error: $ROOT has no CMakeLists.txt — run from the project root" >&2
    exit 1
}

log() { [[ "$QUIET" == 1 ]] || printf '%s\n' "$*" ; }

# ─── Directories we never descend into ────────────────────────────────
# Build outputs, dependency trees, VCS metadata, editor/tool caches,
# runtime data/logs, and any vendored trees inside src/. These are never
# bundled, never listed in the tree, and always reported in the footer.
THIRD_PARTY_DIRS=(
    .deps _deps deps
    .git .hg .svn
    .cache .venv
    .vscode .idea
    build cmake-build-*
    CMakeFiles node_modules
    data logs
    third_party external vendor
)

# Precompute `-name X -o -name Y -o ...` as a bash array for find(1).
PRUNE_NAMES=()
for p in "${THIRD_PARTY_DIRS[@]}"; do
    if [[ ${#PRUNE_NAMES[@]} -gt 0 ]]; then
        PRUNE_NAMES+=(-o)
    fi
    PRUNE_NAMES+=(-name "$p")
done

is_third_party_dir() {
    local name="$1" p
    for p in "${THIRD_PARTY_DIRS[@]}"; do
        # shellcheck disable=SC2053
        [[ "$name" == $p ]] && return 0
    done
    return 1
}

# ─── Tracked state ────────────────────────────────────────────────────
SKIPPED_LINES=""
included=0
skipped=0

record_skip() {
    SKIPPED_LINES+="- \`$1\` — $2"$'\n'
    skipped=$((skipped + 1))
    log "  · skip: $1 ($2)"
}

# ─── Code fence + language helpers ────────────────────────────────────
fence_for() { case "$1" in *.md|*.markdown) echo '~~~~' ;; *) echo '```' ;; esac; }

lang_for() {
    case "$1" in
        *.c)                                     echo 'c'          ;;
        *.cc|*.cpp|*.cxx)                        echo 'cpp'        ;;
        *.h|*.hh|*.hpp|*.hxx|*.inl|*.ipp|*.tpp)  echo 'cpp'        ;;
        CMakeLists.txt|*.cmake)                  echo 'cmake'      ;;
        *.json|*.jsonc)                          echo 'json'       ;;
        *.yaml|*.yml)                            echo 'yaml'       ;;
        *.toml)                                  echo 'toml'       ;;
        *.sh|*.bash)                             echo 'bash'       ;;
        *.py)                                    echo 'python'     ;;
        *.ps1)                                   echo 'powershell' ;;
        *.bat|*.cmd)                             echo 'batch'      ;;
        *.html|*.htm)                            echo 'html'       ;;
        *.md|*.markdown)                         echo 'markdown'   ;;
        *.ini|*.cfg|*.conf|*.service)            echo 'ini'        ;;
        *.txt)                                   echo 'text'       ;;
        *)                                       echo ''           ;;
    esac
}

is_binary() {
    local enc
    enc="$(file --mime-encoding -b "$1" 2>/dev/null || echo binary)"
    [[ "$enc" == "binary" ]]
}

# Emit a single file, guarding against binaries and huge blobs.
emit_file() {
    local f="$1" rel size pretty
    rel="${f#"$ROOT"/}"

    if is_binary "$f"; then
        record_skip "$rel" "binary file — not bundled"
        return
    fi

    size=$(stat -c%s "$f" 2>/dev/null || echo 0)
    if (( size > MAX_FILE_SIZE )); then
        pretty="$(numfmt --to=iec "$size" 2>/dev/null || echo "$size B")"
        record_skip "$rel" "large file ($pretty) — not bundled"
        return
    fi

    included=$((included + 1))
    log "  + $rel"
    {
        printf '### \`%s\`\n\n' "$rel"
        printf '%s%s\n' "$(fence_for "$rel")" "$(lang_for "$rel")"
        cat "$f"
        [[ -s "$f" && "$(tail -c1 "$f" | od -An -t x1 | tr -d ' \n')" != "0a" ]] && printf '\n'
        printf '%s\n\n' "$(fence_for "$rel")"
    } >> "$OUT"
}

# Emit one specific file if it exists.
want_file() {
    local f="$ROOT/$1"
    [[ -f "$f" ]] || { record_skip "$1" "not present"; return; }
    emit_file "$f"
}

# Walk a directory, emitting every non-third-party file.
want_dir() {
    local dir="$1" label="$2" f rel
    [[ -d "$dir" ]] || { record_skip "$label" "directory absent"; return; }
    while IFS= read -r -d '' f; do
        rel="${f#"$ROOT"/}"
        emit_file "$f"
    done < <(
        find "$dir" \
            -type d \( "${PRUNE_NAMES[@]}" \) -prune \
            -o -type f -print0 \
        | sort -z
    )
}

# ─── Header ───────────────────────────────────────────────────────────
{
    printf '# AxiomDLP Agent — Source Bundle\n\n'
    printf -- '- **Generated:** %s\n' "$(date -u '+%Y-%m-%d %H:%M:%S UTC')"
    printf -- '- **Root:** `%s`\n\n' "$ROOT"
    printf 'This bundle contains **only the project'"'"'s own source code** —\n'
    printf 'C++ sources, the top-level CMake configuration, runtime JSON\n'
    printf 'config, packaging, scripts, and docs.\n\n'
    printf 'Third-party code (dependency trees under `.deps/` or `_deps/`,\n'
    printf 'vendored `third_party/`, cross-compile toolchain files under\n'
    printf '`cmake/toolchains/`), build outputs, editor/tool caches, and\n'
    printf 'generated artifacts are **not** included. Every path that was\n'
    printf 'skipped is listed in the **Skipped Files** section at the end.\n'
    printf 'If you need any of them, ask the user — do not guess.\n\n'
    printf -- '---\n\n'
} > "$OUT"

# ─── Tree (source only) ───────────────────────────────────────────────
{
    printf '## Project Structure\n\n```text\n'
    ( cd "$ROOT"
      find . -type d \( "${PRUNE_NAMES[@]}" \) -prune -o -print | sort
    )
    printf '```\n\n---\n\n'
} >> "$OUT"

# ─── Root files ───────────────────────────────────────────────────────
{
    printf '## Project Root\n\n'
    want_file "CMakeLists.txt"
    want_file "CMakePresets.json"
    want_file "README.md"
    want_file "LICENSE"
    want_file "next.md"
    want_file "done.md"
    want_file ".gitignore"
    want_file "make-bundle.sh"
    printf -- '---\n\n'
} >> "$OUT"

# ─── Runtime configuration ────────────────────────────────────────────
printf '## Configuration\n\n' >> "$OUT"
want_dir "$ROOT/config" "config/"
printf -- '---\n\n' >> "$OUT"

# ─── Source ───────────────────────────────────────────────────────────
printf '## Source\n\n' >> "$OUT"
want_dir "$ROOT/src" "src/"
printf -- '---\n\n' >> "$OUT"

# ─── Packaging ────────────────────────────────────────────────────────
printf '## Packaging\n\n' >> "$OUT"
want_dir "$ROOT/packaging" "packaging/"
printf -- '---\n\n' >> "$OUT"

# ─── Scripts ──────────────────────────────────────────────────────────
printf '## Scripts\n\n' >> "$OUT"
want_dir "$ROOT/scripts" "scripts/"
printf -- '---\n\n' >> "$OUT"

# ─── Docs ─────────────────────────────────────────────────────────────
printf '## Docs\n\n' >> "$OUT"
want_dir "$ROOT/docs" "docs/"
printf -- '---\n\n' >> "$OUT"

# ─── Enumerate top-level entries we deliberately did not bundle ───────
{
    # Top-level directories (find so dot-dirs are included).
    while IFS= read -r -d '' d; do
        name="$(basename "$d")"
        case "$name" in
            config|src|packaging|scripts|docs)  continue ;;
        esac
        if is_third_party_dir "$name"; then
            case "$name" in
                .deps|_deps|deps)     record_skip "$name/" "third-party dependency tree — not bundled" ;;
                .git|.hg|.svn)        record_skip "$name/" "version-control metadata — not bundled" ;;
                .cache|.venv)         record_skip "$name/" "tool/venv cache — not bundled" ;;
                .vscode|.idea)        record_skip "$name/" "editor config — not bundled" ;;
                build|cmake-build-*)  record_skip "$name/" "build output — not bundled" ;;
                CMakeFiles)           record_skip "$name/" "build metadata — not bundled" ;;
                node_modules)         record_skip "$name/" "third-party node deps — not bundled" ;;
                data)                 record_skip "$name/" "runtime data — not bundled (ask if needed)" ;;
                logs)                 record_skip "$name/" "runtime logs — not bundled (ask if needed)" ;;
                third_party|external|vendor) record_skip "$name/" "vendored third-party code — not bundled" ;;
                *)                    record_skip "$name/" "not bundled (ask if needed)" ;;
            esac
        else
            record_skip "$name/" "not bundled (ask if needed)"
        fi
    done < <(find "$ROOT" -mindepth 1 -maxdepth 1 -type d -print0 | sort -z)

    # Top-level files (same, via find so dot-files are visible).
    while IFS= read -r -d '' f; do
        base="$(basename "$f")"
        case "$base" in
            CMakeLists.txt|CMakePresets.json|README.md|LICENSE|next.md|done.md|.gitignore|make-bundle.sh) continue ;;
            compile_commands.json)    record_skip "$base" "generated — not bundled" ;;
            .clangd)                  record_skip "$base" "editor config — not bundled" ;;
            axiomdlp-agent-bundle.md) record_skip "$base" "generated bundle — not bundled" ;;
            *)                        record_skip "$base" "not bundled (ask if needed)" ;;
        esac
    done < <(find "$ROOT" -mindepth 1 -maxdepth 1 -type f -print0 | sort -z)
} 2>/dev/null || true

# ─── Skipped Files appendix ───────────────────────────────────────────
{
    printf '## Skipped Files\n\n'
    printf 'These paths exist in the project but were deliberately left out\n'
    printf 'of this bundle because they contain third-party code, build\n'
    printf 'output, or generated artifacts. **If you need any of them, ask\n'
    printf 'the user** — do not assume their contents.\n\n'
    if [[ -z "$SKIPPED_LINES" ]]; then
        printf '_Nothing was skipped._\n'
    else
        printf '%s\n' "$SKIPPED_LINES"
    fi
    printf '\n---\n\n'
} >> "$OUT"

# ─── Summary ──────────────────────────────────────────────────────────
{
    printf '## Summary\n\n'
    printf -- '- Files included: **%s**\n' "$included"
    printf -- '- Files skipped:  **%s**\n' "$skipped"
    printf -- '- Bundle size:    %s\n' "$(du -h "$OUT" | cut -f1)"
} >> "$OUT"

log
log "✓ bundle written"
log "  path:     $OUT"
log "  size:     $(du -h "$OUT" | cut -f1)"
log "  included: $included"
log "  skipped:  $skipped"