#!/usr/bin/env bash
set -u

exe="${1:-./RMST}"

if [ ! -x "$exe" ]; then
  echo "Executable not found or not executable: $exe"
  echo "Usage: ./check_all_cases.sh ./RMST"
  exit 2
fi

status=0
for case_dir in case1 case2 case3 case4 case5 case6 case7 case8 case9 case10; do
  out_file="${case_dir}/my_output.dat"
  rm -f "$out_file"
  printf "%s ... " "$case_dir"
  if ! timeout 3600 "$exe" "${case_dir}/input.dat" "$out_file"; then
    echo "RE/TLE"
    status=1
    continue
  fi
  if diff -w "$out_file" "${case_dir}/output.dat" >/dev/null; then
    echo "OK"
  else
    echo "Wrong Answer"
    status=1
  fi
done

exit "$status"
