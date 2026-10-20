#!/usr/bin/env bash
#
# Native test of the Guard's per-tick decision: compiles GuardDecision.cpp,
# the same file the firmware builds, with the host C++ compiler and runs
# rails49/guard_test.cpp against it. Then runs rails49/track_mode_test.cpp
# against TrackModeChange.h, the rule setTrackMode() uses for a
# modifier-only change, and rails49/track_report_test.cpp against
# TrackModeReport.h, the rule reportedMode() uses, and
# rails49/main_loop_reversal_test.cpp against MainLoopReversal.h, the order
# checkPowerOverload() follows for a Reversal, and
# rails49/current_mean_test.cpp against CurrentMean.h, the mean <JI>
# reports, and rails49/overload_mean_test.cpp against OverloadMean.h, what
# entering Overload does to that mean, and rails49/power_line_test.cpp
# against PowerLine.h, what the power lines report for a track, and
# rails49/power_switch_test.cpp against PowerSwitch.h, when setPower()
# drives the power pin. CXX overrides the compiler.

set -eu

here=$(cd "$(dirname "$0")" && pwd)
tmp=$(mktemp -d "${TMPDIR:-/tmp}/tmp.XXXXXX")
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

${CXX:-c++} -std=c++11 -Wall -Wextra -Werror \
  -o "$tmp/current_mean_test" "$here/current_mean_test.cpp"
"$tmp/current_mean_test"

${CXX:-c++} -std=c++11 -Wall -Wextra -Werror \
  -o "$tmp/overload_mean_test" "$here/overload_mean_test.cpp"
"$tmp/overload_mean_test"

${CXX:-c++} -std=c++11 -Wall -Wextra -Werror \
  -o "$tmp/power_line_test" "$here/power_line_test.cpp"
"$tmp/power_line_test"

${CXX:-c++} -std=c++11 -Wall -Wextra -Werror \
  -o "$tmp/power_switch_test" "$here/power_switch_test.cpp"
"$tmp/power_switch_test"
