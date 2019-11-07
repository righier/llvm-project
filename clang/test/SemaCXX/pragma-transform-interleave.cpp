// RUN: %clang_cc1 -std=c++11 -fexperimental-transform-pragma -fsyntax-only -verify %s

void interleave(int *List, int Length, int Value) {

/* expected-error@+1 {{the factor clause can be specified at most once}} */
#pragma clang transform interleave factor(4) factor(4)
  for (int i = 0; i < Length; i++)
      List[i] = Value;

/* expected-error@+1 {{clause argument must me at least 2}} */
#pragma clang transform interleave factor(-42)
  for (int i = 0; i < Length; i++)
    List[i] = Value;

}
