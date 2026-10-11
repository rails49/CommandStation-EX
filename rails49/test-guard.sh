#!/usr/bin/env bash
#
# Native test of the Guard's per-tick decision: compiles GuardDecision.cpp,
# the same file the firmware builds, with the host C++ compiler and runs
# rails49/guard_test.cpp against it. CXX overrides the compiler.

set -eu

here=$(cd "$(dirname "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

"${CXX:-c++}" -std=c++11 -Wall -Wextra -Werror \
  -o "$tmp/guard_test" "$here/guard_test.cpp" "$here/../GuardDecision.cpp"
"$tmp/guard_test"
