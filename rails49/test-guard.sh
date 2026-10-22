#!/usr/bin/env bash
#
# Native tests: compiles each rails49/*_test.cpp with the host C++ compiler
# and runs it. Each test's header names the rule it tests. guard_test.cpp is
# linked with GuardDecision.cpp, the same file the firmware builds; the others
# test header-only rules. CXX overrides the compiler.

set -eu

here=$(cd "$(dirname "$0")" && pwd)
tmp=$(mktemp -d "${TMPDIR:-/tmp}/tmp.XXXXXX")
trap 'rm -rf "$tmp"' EXIT

for test in "$here"/*_test.cpp; do
  name=$(basename "$test" .cpp)
  extra=()
  [ "$name" = guard_test ] && extra=("$here/../GuardDecision.cpp")
  ${CXX:-c++} -std=c++11 -Wall -Wextra -Werror \
    -o "$tmp/$name" "$test" ${extra[@]+"${extra[@]}"}
  "$tmp/$name"
done
