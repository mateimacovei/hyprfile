#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="${1:?repository root required}"
SOURCE_RELEASE_SCRIPT="${ROOT_DIR}/release.sh"

fail() {
  printf 'FAIL: %s\n' "$1" >&2
  exit 1
}

assert_contains() {
  local haystack="$1"
  local needle="$2"
  if [[ "${haystack}" != *"${needle}"* ]]; then
    printf 'Expected output to contain:\n%s\n\nActual output:\n%s\n' "${needle}" "${haystack}" >&2
    exit 1
  fi
}

make_project() {
  local name="$1"
  local dir
  dir="$(mktemp -d "${TMPDIR:-/tmp}/hyprfile_release_script_${name}.XXXXXX")"
  cp "${SOURCE_RELEASE_SCRIPT}" "${dir}/release.sh"
  chmod +x "${dir}/release.sh"
  printf '%s\n' "${dir}"
}

make_project_at() {
  local dir="$1"
  mkdir -p "${dir}"
  cp "${SOURCE_RELEASE_SCRIPT}" "${dir}/release.sh"
  chmod +x "${dir}/release.sh"
}

write_successful_test_script() {
  local dir="$1"
  cat >"${dir}/test.sh" <<'SCRIPT'
#!/usr/bin/env bash
set -euo pipefail

if [[ "$#" -lt 2 || "$1" != "-p" || "$2" != "Release" ]]; then
  printf 'unexpected test args: %s\n' "$*" >&2
  exit 2
fi

mkdir -p build/Release
printf '#!/usr/bin/env bash\nprintf "hyprfile\\n"\n' > build/Release/hyprfile
chmod +x build/Release/hyprfile
SCRIPT
  chmod +x "${dir}/test.sh"
}

test_copies_artifact_when_release_dir_is_on_path() {
  local dir output
  dir="$(make_project on_path)"
  trap 'rm -rf "${dir}"' RETURN
  write_successful_test_script "${dir}"

  output="$(cd "${dir}" && PATH="${dir}/release:${PATH}" ./release.sh)"

  [[ -x "${dir}/release/hyprfile" ]] || fail "release artifact was not copied"
  assert_contains "${output}" "Release artifact installed:"
  assert_contains "${output}" "Your release folder is already on PATH."
  assert_contains "${output}" "You can run hyprfile from anywhere with:"
  assert_contains "${output}" "hyprfile"
}

test_prints_path_command_when_release_dir_is_not_on_path() {
  local dir output escaped_release_dir expected_command
  dir="$(make_project not_on_path)"
  trap 'rm -rf "${dir}"' RETURN
  write_successful_test_script "${dir}"

  output="$(cd "${dir}" && PATH="/usr/bin:/bin" ./release.sh)"
  printf -v escaped_release_dir '%q' "${dir}/release"
  expected_command="export PATH=${escaped_release_dir}:\$PATH"

  [[ -x "${dir}/release/hyprfile" ]] || fail "release artifact was not copied"
  assert_contains "${output}" "Your release folder is not on PATH."
  assert_contains "${output}" "${expected_command}"
  assert_contains "${output}" "After it is on PATH, you can run the application from anywhere with:"
  assert_contains "${output}" "hyprfile"
}

test_prints_shell_escaped_path_command_for_special_install_path() {
  local base dir output escaped_release_dir expected_command
  base="$(mktemp -d "${TMPDIR:-/tmp}/hyprfile_release_script_special.XXXXXX")"
  trap 'rm -rf "${base}"' RETURN
  dir="${base}/repo with spaces and \$dollar \"quote\""
  make_project_at "${dir}"
  write_successful_test_script "${dir}"

  output="$(cd "${dir}" && PATH="/usr/bin:/bin" ./release.sh)"
  printf -v escaped_release_dir '%q' "${dir}/release"
  expected_command="export PATH=${escaped_release_dir}:\$PATH"

  assert_contains "${output}" "${expected_command}"
}

test_does_not_install_when_release_tests_fail() {
  local dir status
  dir="$(make_project failing_tests)"
  trap 'rm -rf "${dir}"' RETURN
  cat >"${dir}/test.sh" <<'SCRIPT'
#!/usr/bin/env bash
exit 42
SCRIPT
  chmod +x "${dir}/test.sh"

  set +e
  (cd "${dir}" && ./release.sh >/tmp/hyprfile_release_script_failure_output 2>&1)
  status="$?"
  set -e

  [[ "${status}" -eq 42 ]] || fail "expected release.sh to return failing test status 42, got ${status}"
  [[ ! -e "${dir}/release/hyprfile" ]] || fail "release artifact was installed after failed tests"
}

test_copies_artifact_when_release_dir_is_on_path
test_prints_path_command_when_release_dir_is_not_on_path
test_prints_shell_escaped_path_command_for_special_install_path
test_does_not_install_when_release_tests_fail

printf 'Release script tests passed\n'
