#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
BUILD_ARTIFACT="${SCRIPT_DIR}/build/Release/hyprfile"
RELEASE_DIR="${SCRIPT_DIR}/release"
RELEASE_ARTIFACT="${RELEASE_DIR}/hyprfile"
printf -v ESCAPED_RELEASE_DIR '%q' "${RELEASE_DIR}"

path_contains_release_dir() {
  local entry
  local entry_abs
  IFS=':' read -r -a entries <<< "${PATH:-}"

  for entry in "${entries[@]}"; do
    if [[ -z "${entry}" ]]; then
      entry="."
    fi

    if [[ "${entry}" = /* ]]; then
      if [[ ! -d "${entry}" ]]; then
        continue
      fi
      entry_abs="$(cd "${entry}" 2>/dev/null && pwd -P)" || continue
    else
      entry_abs="$(cd "${entry}" 2>/dev/null && pwd -P)" || continue
    fi

    if [[ "${entry_abs}" == "${RELEASE_DIR}" ]]; then
      return 0
    fi
  done

  return 1
}

print_heading() {
  printf '\n%s\n' "== hyprfile Release =="
}

print_heading
printf '\n[1/3] Running Release tests\n'
"${SCRIPT_DIR}/test.sh" -p Release "$@"

printf '\n[2/3] Installing release artifact\n'
if [[ ! -x "${BUILD_ARTIFACT}" ]]; then
  printf 'Error: expected Release artifact was not found or is not executable:\n  %s\n' "${BUILD_ARTIFACT}" >&2
  exit 1
fi

mkdir -p "${RELEASE_DIR}"
cp "${BUILD_ARTIFACT}" "${RELEASE_ARTIFACT}"
chmod +x "${RELEASE_ARTIFACT}"
printf 'Release artifact installed:\n  %s\n' "${RELEASE_ARTIFACT}"

printf '\n[3/3] Checking PATH\n'
printf 'Release folder:\n  %s\n\n' "${RELEASE_DIR}"

if path_contains_release_dir; then
  cat <<EOF
Your release folder is already on PATH.
You can run hyprfile from anywhere with:
  hyprfile
EOF
else
  cat <<EOF
Your release folder is not on PATH.
Run this command to add it for the current shell:
  export PATH=${ESCAPED_RELEASE_DIR}:\$PATH

After it is on PATH, you can run the application from anywhere with:
  hyprfile
EOF
fi
