PDA PA4 test cases

This package contains the 10 test cases used in the final grading run.

Directory layout:
  case1/input.dat   case1/output.dat
  case2/input.dat   case2/output.dat
  ...
  case10/input.dat  case10/output.dat

The student's executable should support:
  ./RMST input.dat output.dat

Unpack this package:
  tar -xzf testcase.tar.gz
  cd testcase

Run one case manually:
  ./RMST case1/input.dat my_output.dat
  diff -w my_output.dat case1/output.dat

Run all cases:
  ./check_all_cases.sh ./RMST

The script prints OK, Wrong Answer, or RE/TLE for each case.
It uses a 3600-second timeout per case.
