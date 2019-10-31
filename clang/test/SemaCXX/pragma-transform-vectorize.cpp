// RUN: %clang_cc1 -std=c++11 -fsyntax-only -verify %s

void vectorize(int *List, int Length, int Value) {
/* expected-error@+1 {{vectorize width clause can only be used once}} */
#pragma clang transform vectorize width(4) width(4)
  for (int i = 0; i < Length; i++)
      List[i] = Value;
}
