#!/usr/bin/env bash
#
# make-bundle.sh — bundle the AxiomDLP agent's OWN source into split
# markdown parts for an LLM/agent. Third-party toolchains, build outputs,
# and dependency trees are skipped; every skip is recorded in the README
# so the agent can ask for anything it needs by name.
#
# Usage:
#   ./make-bundle.sh                       # defaults (8 parts -> combined_code/)
#   NUM_PARTS=12 ./make-bundle.sh          # custom part count
#   OUTPUT_DIR=/tmp/b ./make-bundle.sh     # custom output folder
#
set -euo pipefail

ROOT="${ROOT:-$(pwd)}"
OUTPUT_DIR="${OUTPUT_DIR:-$ROOT/combined_code}"
NUM_PARTS="${NUM_PARTS:-8}"
QUIET="${QUIET:-0}"
MAX_FILE_SIZE="${MAX_FILE_SIZE:-524288}"   # 512 KiB per-file guard (env-overridable)

[[ -f "$ROOT/CMakeLists.txt" ]] || {
    echo "error: $ROOT has no CMakeLists.txt — run from the project root" >&2
    exit 1
}

if ! [[ "$NUM_PARTS" =~ ^[0-9]+$ ]] || [ "$NUM_PARTS" -lt 1 ]; then
    echo "❌ NUM_PARTS must be a positive integer (got: $NUM_PARTS)" >&2
    exit 1
fi

log() { [[ "$QUIET" == 1 ]] || printf '%s\n' "$*" ; }

# ─── Directories we never descend into ────────────────────────────────
THIRD_PARTY_DIRS=(
    .deps _deps deps
    .git .hg .svn
    .cache .venv
    .vscode .idea
    build cmake-build-*
    CMakeFiles node_modules
    data logs
    third_party external vendor
    combined_code
)

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

# ─── Collect files (section-tagged, in original order) ────────────────
# FILES_TO_INCLUDE holds "SectionName|/abs/path" entries.
# FILE_LINES[i] is the line count for FILES_TO_INCLUDE[i].
FILES_TO_INCLUDE=()
FILE_LINES=()

consider_file() {
    local f="$1" section="$2" rel size pretty
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

    FILES_TO_INCLUDE+=("${section}|${f}")
    FILE_LINES+=("$(wc -l < "$f" | tr -d ' ')")
    included=$((included + 1))
}

want_file() {
    local relpath="$1" section="$2" f="$ROOT/$1"
    [[ -f "$f" ]] || { record_skip "$relpath" "not present"; return; }
    consider_file "$f" "$section"
}

want_dir() {
    local dir="$1" label="$2" section="$3" f
    [[ -d "$dir" ]] || { record_skip "$label" "directory absent"; return; }
    while IFS= read -r -d '' f; do
        consider_file "$f" "$section"
    done < <(
        find "$dir" \
            -type d \( "${PRUNE_NAMES[@]}" \) -prune \
            -o -type f -print0 \
        | sort -z
    )
}

# ─── Walk in section order ────────────────────────────────────────────
want_file "CMakeLists.txt"      "Project Root"
want_file "CMakePresets.json"   "Project Root"
want_file "README.md"           "Project Root"
want_file "LICENSE"             "Project Root"
want_file "next.md"             "Project Root"
want_file "done.md"             "Project Root"
want_file ".gitignore"          "Project Root"
want_file "make-bundle.sh"      "Project Root"

want_dir "$ROOT/config"    "config/"    "Configuration"
want_dir "$ROOT/src"       "src/"       "Source"
want_dir "$ROOT/packaging" "packaging/" "Packaging"
want_dir "$ROOT/scripts"   "scripts/"   "Scripts"
want_dir "$ROOT/docs"      "docs/"      "Docs"

# ─── Enumerate top-level entries we deliberately did not bundle ───────
{
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
                combined_code)        record_skip "$name/" "generated bundle — not bundled" ;;
                *)                    record_skip "$name/" "not bundled (ask if needed)" ;;
            esac
        else
            record_skip "$name/" "not bundled (ask if needed)"
        fi
    done < <(find "$ROOT" -mindepth 1 -maxdepth 1 -type d -print0 | sort -z)

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

if [ "${#FILES_TO_INCLUDE[@]}" -eq 0 ]; then
    echo "⚠️  No files to bundle."
    exit 0
fi

# ─── Per-part sizing ──────────────────────────────────────────────────
TOTAL_LINES=0
for l in "${FILE_LINES[@]}"; do
    TOTAL_LINES=$((TOTAL_LINES + l))
done

# Per-file markdown overhead ≈ 10 lines (heading, fence open, fence close, blank)
OVERHEAD_PER_FILE=10
TOTAL_WITH_OVERHEAD=$((TOTAL_LINES + ${#FILES_TO_INCLUDE[@]} * OVERHEAD_PER_FILE))
TARGET_PER_PART=$(( (TOTAL_WITH_OVERHEAD + NUM_PARTS - 1) / NUM_PARTS ))

log
log "Files to bundle:        $included"
log "Files skipped:          $skipped"
log "Total source lines:     $TOTAL_LINES"
log "Target lines per part:  ~$TARGET_PER_PART"
log

# ─── Conflict detection (oversized files) ─────────────────────────────
OVERSIZED_FILES=()          # each entry: "relpath|raw_line_count"
for i in "${!FILES_TO_INCLUDE[@]}"; do
    entry="${FILES_TO_INCLUDE[$i]}"
    path="${entry#*|}"
    rel="${path#"$ROOT"/}"
    size_with_overhead=$(( FILE_LINES[$i] + OVERHEAD_PER_FILE ))
    if [ "$size_with_overhead" -gt "$TARGET_PER_PART" ]; then
        OVERSIZED_FILES+=("${rel}|${FILE_LINES[$i]}")
    fi
done

HAVE_CONFLICT=0
if [ "${#OVERSIZED_FILES[@]}" -gt 0 ]; then
    HAVE_CONFLICT=1
    log "⚠️  CONFLICT: ${#OVERSIZED_FILES[@]} file(s) exceed the per-part target"
    log "    of ~${TARGET_PER_PART} lines. They will occupy a part on their own"
    log "    and may still be too large for the receiving agent:"
    log
    shown=0
    for of in "${OVERSIZED_FILES[@]}"; do
        if [ "$shown" -ge 10 ]; then
            log "      ... and $(( ${#OVERSIZED_FILES[@]} - 10 )) more (see README.md)"
            break
        fi
        path="${of%%|*}"
        lines="${of##*|}"
        log "      - $path ($lines lines)"
        shown=$((shown + 1))
    done
    log
    log "    Suggestion: reduce NUM_PARTS (currently $NUM_PARTS), or manually"
    log "    split the listed file(s) before re-running."
    log
else
    log "✅ No oversized files — every file fits inside one part."
    log
fi

# ─── Greedy pack into N parts, at file boundaries ─────────────────────
declare -a PART_FILES PART_SIZES
current_size=0
current_files=()
part_idx=0

for i in "${!FILES_TO_INCLUDE[@]}"; do
    entry="${FILES_TO_INCLUDE[$i]}"
    l=$(( FILE_LINES[$i] + OVERHEAD_PER_FILE ))

    if [ $(( current_size + l )) -gt "$TARGET_PER_PART" ] && \
       [ ${#current_files[@]} -gt 0 ] && \
       [ "$part_idx" -lt $(( NUM_PARTS - 1 )) ]; then
        PART_FILES[$part_idx]="$(IFS=$'\n'; echo "${current_files[*]}")"
        PART_SIZES[$part_idx]=$current_size
        part_idx=$((part_idx + 1))
        current_files=()
        current_size=0
    fi

    current_files+=("$entry")
    current_size=$((current_size + l))
done

if [ ${#current_files[@]} -gt 0 ]; then
    PART_FILES[$part_idx]="$(IFS=$'\n'; echo "${current_files[*]}")"
    PART_SIZES[$part_idx]=$current_size
fi

ACTUAL_PARTS=$(( part_idx + 1 ))
log "Creating $ACTUAL_PARTS part file(s) ..."
log

# ─── Fresh output dir ─────────────────────────────────────────────────
rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR"

# ─── Emit each part ───────────────────────────────────────────────────
declare -a PART_FILE_COUNT

for p in $(seq 0 $((ACTUAL_PARTS - 1))); do
    PART_NUM=$(printf "%02d" $((p + 1)))
    OUT="$OUTPUT_DIR/part_${PART_NUM}.md"
    file_count=0
    last_section=""

    {
        printf '# AxiomDLP Agent — Source Bundle (Part %d of %d)\n\n' $((p+1)) "$ACTUAL_PARTS"
        printf -- '- **Generated:** %s\n' "$(date -u '+%Y-%m-%d %H:%M:%S UTC')"
        printf -- '- **Root:** `%s`\n' "$ROOT"
        printf -- '- **Part:** %d / %d — approx %d lines\n\n' $((p+1)) "$ACTUAL_PARTS" "${PART_SIZES[$p]}"
        printf -- '---\n\n'
    } > "$OUT"

    # Warning banner at the top of part 1 only
    if [ "$p" -eq 0 ] && [ "$HAVE_CONFLICT" -eq 1 ]; then
        {
            printf '> ## ⚠️  READ FIRST — conflict warning\n'
            printf '>\n'
            printf '> **%d file(s) in this bundle exceed the ~%d-line per-part target.**\n' \
                "${#OVERSIZED_FILES[@]}" "$TARGET_PER_PART"
            printf '> Each occupies a part on its own and may still exceed the\n'
            printf '> receiving agent'"'"'s input limit. If the agent reports truncation,\n'
            printf '> the affected part(s) must be re-sent split in two.\n'
            printf '>\n'
            printf '> Affected files:\n'
            shown=0
            for of in "${OVERSIZED_FILES[@]}"; do
                if [ "$shown" -ge 10 ]; then
                    printf '> - … and %d more (see README.md)\n' $(( ${#OVERSIZED_FILES[@]} - 10 ))
                    break
                fi
                path="${of%%|*}"
                lines="${of##*|}"
                printf '> - `%s` (%s lines)\n' "$path" "$lines"
                shown=$((shown + 1))
            done
            printf '>\n'
            printf '> Full details + mitigation: `README.md` → *Known conflicts*.\n\n'
            printf -- '---\n\n'
        } >> "$OUT"
    fi

    # Emit files in original order; emit section header when section changes
    while IFS= read -r entry; do
        [ -z "$entry" ] && continue
        section="${entry%%|*}"
        path="${entry#*|}"
        rel="${path#"$ROOT"/}"
        lines=$(wc -l < "$path" | tr -d ' ')
        file_count=$((file_count + 1))

        if [ "$section" != "$last_section" ]; then
            printf '## %s\n\n' "$section" >> "$OUT"
            last_section="$section"
        fi

        {
            printf '### `%s`\n\n' "$rel"
            printf '%s%s\n' "$(fence_for "$rel")" "$(lang_for "$rel")"
            cat "$path"
            [[ -s "$path" && "$(tail -c1 "$path" | od -An -t x1 | tr -d ' \n')" != "0a" ]] && printf '\n'
            printf '%s\n\n' "$(fence_for "$rel")"
        } >> "$OUT"

        printf '  [part %02d] %s (%d lines)\n' $((p+1)) "$rel" "$lines"
    done <<< "${PART_FILES[$p]}"

    PART_FILE_COUNT[$p]=$file_count
    printf '\n' >> "$OUT"
    log
done

# ─── README for the AI agent ──────────────────────────────────────────
README="$OUTPUT_DIR/README.md"
{
    printf '# AxiomDLP Agent — Combined Source Bundle\n\n'
    printf '**Generated:** %s\n' "$(date -u '+%Y-%m-%d %H:%M:%S UTC')"
    printf '**Project root:** `%s`\n' "$ROOT"
    printf '**Parts:** %d\n' "$ACTUAL_PARTS"
    printf '**Files bundled:** %d\n' "$included"
    printf '**Files skipped:** %d\n' "$skipped"
    printf '**Total source lines:** %d\n\n' "$TOTAL_LINES"

    printf '## What this folder contains\n\n'
    printf '`part_01.md`, `part_02.md`, …, `part_%02d.md` — the entire project'"'"'s own\n' "$ACTUAL_PARTS"
    printf 'source tree, split across %d files. No third-party code, no build\n' "$ACTUAL_PARTS"
    printf 'outputs, no generated artifacts.\n\n'
    printf 'Each part is a self-contained Markdown file with:\n\n'
    printf -- '- A header showing the part number and approximate line count.\n'
    printf -- '- A `## Section Name` header (Project Root, Configuration, Source,\n'
    printf '  Packaging, Scripts, Docs) whenever the file'"'"'s section changes.\n'
    printf -- '- A `### \\\`path/to/file\\\`\\` section per file, with its language-tagged\n'
    printf '  code block.\n\n'
    printf '**Part boundaries always fall on file boundaries.** No file is split\n'
    printf 'across two parts — every file appears whole inside exactly one part.\n\n'

    printf '## How to hand this to an AI agent\n\n'
    printf '1. Upload or paste `part_01.md` and say: *"This is part 1 of %d."*\n' "$ACTUAL_PARTS"
    printf '2. Upload `part_02.md` and say: *"This is part 2 of %d."*\n' "$ACTUAL_PARTS"
    printf '3. Continue in order through `part_%02d.md`.\n' "$ACTUAL_PARTS"
    printf '4. After the last part, the agent has the complete source tree.\n\n'
    printf '**Do not skip or reorder parts** — the agent reconstructs the tree\n'
    printf 'from the sequence you give it.\n\n'

    # Project structure tree (source only)
    printf '## Project structure (source only)\n\n```text\n'
    ( cd "$ROOT"
      find . -type d \( "${PRUNE_NAMES[@]}" \) -prune -o -print | sort
    )
    printf '```\n\n'

    # Known conflicts
    if [ "$HAVE_CONFLICT" -eq 1 ]; then
        printf '## ⚠️  Known conflicts — oversized files\n\n'
        printf 'The following file(s) exceed the ~%d-line per-part target. Each\n' "$TARGET_PER_PART"
        printf 'occupies a part on its own and may still be too large for the\n'
        printf 'receiving agent. If the agent reports truncation on a part, split\n'
        printf 'that part'"'"'s large file in two and re-send.\n\n'
        for of in "${OVERSIZED_FILES[@]}"; do
            path="${of%%|*}"
            lines="${of##*|}"
            printf -- '- `%s` — %s lines\n' "$path" "$lines"
        done
        printf '\n**Mitigation options:**\n\n'
        printf '1. Re-run with fewer parts: `NUM_PARTS=5 ./make-bundle.sh`.\n'
        printf '2. Manually split the listed file(s) into two files before re-running.\n'
        printf '3. If your agent accepts larger uploads than ~%d lines, the\n' "$TARGET_PER_PART"
        printf '   affected part can be uploaded as-is.\n\n'
    else
        printf '## ✅ No conflicts\n\n'
        printf 'Every file fits within the ~%d-line per-part target.\n\n' "$TARGET_PER_PART"
    fi

    # Inventory
    printf '## Inventory\n\n'
    printf '| Part | Files | Approx lines |\n'
    printf '|---|---|---|\n'
    for p in $(seq 0 $((ACTUAL_PARTS - 1))); do
        n=$(printf "%02d" $((p + 1)))
        printf '| part_%s.md | %d | ~%d |\n' \
            "$n" "${PART_FILE_COUNT[$p]}" "${PART_SIZES[$p]}"
    done
    printf '\n'

    # Skipped files
    printf '## Skipped files\n\n'
    printf 'These paths exist in the project but were deliberately left out of\n'
    printf 'this bundle because they contain third-party code, build output, or\n'
    printf 'generated artifacts. **If you need any of them, ask the user** —\n'
    printf 'do not assume their contents.\n\n'
    if [[ -z "$SKIPPED_LINES" ]]; then
        printf '_Nothing was skipped._\n'
    else
        printf '%s' "$SKIPPED_LINES"
    fi
    printf '\n'

    # Regenerate
    printf '## Regenerate\n\n'
    printf '```bash\n'
    printf '# default (8 parts, output -> combined_code/)\n'
    printf './make-bundle.sh\n\n'
    printf '# custom part count\n'
    printf 'NUM_PARTS=12 ./make-bundle.sh\n'
    printf '```\n'
} > "$README"

# ─── Post-write verification ──────────────────────────────────────────
log "🔍 Verifying completeness ..."
total_packed=0
for p in $(seq 0 $((ACTUAL_PARTS - 1))); do
    total_packed=$((total_packed + PART_FILE_COUNT[$p]))
done
if [ "$total_packed" -eq "$included" ]; then
    log "   ✓ All $included files appear across $ACTUAL_PARTS part(s)."
    log "     No file was dropped, duplicated, or split."
else
    echo "   ✗ MISMATCH: expected $included files, found $total_packed." >&2
    exit 1
fi

# ─── Summary ──────────────────────────────────────────────────────────
log
log "✅ Done."
log "   Folder: $OUTPUT_DIR"
log "   Parts:  $ACTUAL_PARTS"
for p in $(seq 0 $((ACTUAL_PARTS - 1))); do
    n=$(printf "%02d" $((p + 1)))
    f="$OUTPUT_DIR/part_${n}.md"
    log "   part_${n}.md → $(wc -l < "$f" | tr -d ' ') lines (${PART_FILE_COUNT[$p]} files)"
done
log "   README.md   → $(wc -l < "$README" | tr -d ' ') lines"