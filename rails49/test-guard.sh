#!/usr/bin/env bash
#
# Native test of the Guard's per-tick decision: compiles GuardDecision.cpp,
# the same file the firmware builds, with the host C++ compiler and runs
# rails49/guard_test.cpp against it. Then runs rails49/track_mode_test.cpp
# against TrackModeChange.h, the rule setTrackMode() uses for a
# modifier-only change, and rails49/track_report_test.cpp against
# TrackModeReport.h, the rule reportedMode() uses, and
# rails49/main_loop_reversal_test.cpp against MainLoopReversal.h, the order
# checkPowerOverload() follows for a Reversal. CXX overrides the compiler.

set -eu

here=$(cd "$(dirname "$0")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

${CXX:-c++} -std=c++11 -Wall -Wextra -Werror \
  -o "$tmp/guard_test" "$here/guard_test.cpp" "$here/../GuardDecision.cpp"
"$tmp/guard_test"

${CXX:-c++} -std=c++11 -Wall -Wextra -Werror \
  -o "$tmp/track_mode_test" "$here/track_mode_test.cpp"
"$tmp/track_mode_test"

${CXX:-c++} -std=c++11 -Wall -Wextra -Werror \
  -o "$tmp/track_report_test" "$here/track_report_test.cpp"
"$tmp/track_report_test"

${CXX:-c++} -std=c++11 -Wall -Wextra -Werror \
  -o "$tmp/main_loop_reversal_test" "$here/main_loop_reversal_test.cpp"
"$tmp/main_loop_reversal_test"
