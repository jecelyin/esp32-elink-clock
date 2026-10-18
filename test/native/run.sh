#!/usr/bin/env bash
source ~/.bash_profile
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
test_output_dir="$(mktemp -d /private/tmp/eink-health-test.XXXXXX)"
# 关键逻辑：编译产物仅放在本次创建的临时目录，成功或失败均自动清理。
trap 'rm -f "$test_output_dir/runtime-health-test" "$test_output_dir/alarm-pcm-test"; rmdir "$test_output_dir"' EXIT

c++ -std=c++11 -Wall -Wextra -Werror -DENABLE_SERIAL_DEBUG=1 \
  -I"$project_dir/test/native/fakes" \
  "$project_dir/src/utils/RuntimeHealth.cpp" \
  "$project_dir/src/utils/IdleSleep.cpp" \
  "$project_dir/src/utils/SleepLogger.cpp" \
  "$project_dir/test/native/runtime_health_test.cpp" \
  -o "$test_output_dir/runtime-health-test"

for test_case in stall background long-operation sleep early-wake overdue-sleep \
  long-uptime wake-disable-fail \
  wake-timer-fail wake-source-fail wake-left-fail wake-right-fail wake-enter-fail \
  sleep-reject create-fail start-fail download; do
  "$test_output_dir/runtime-health-test" "$test_case"
done

c++ -std=c++11 -Wall -Wextra -Werror \
  -I"$project_dir/test/native/fakes" \
  "$project_dir/src/drivers/AlarmPcmPlayer.cpp" \
  "$project_dir/test/native/alarm_pcm_test.cpp" \
  -o "$test_output_dir/alarm-pcm-test"

for test_case in playback low-rate i2s-error partial-write; do
  "$test_output_dir/alarm-pcm-test" "$test_case"
done
