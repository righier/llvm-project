// RUN: %clang_cc1 -std=c++11 -fexperimental-transform-pragma -fsyntax-only -verify %s

void interleave(int *List, int Length, int Value) {
/* expected-error@+1 {{interleave factor clause can only be used once}} */
#pragma clang transform interleave factor(4) factor(4)
  for (int i = 0; i < Length; i++)
      List[i] = Value;
}
