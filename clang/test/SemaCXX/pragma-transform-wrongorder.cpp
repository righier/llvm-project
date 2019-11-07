// RUN: %clang_cc1 -std=c++11 -fexperimental-transform-pragma -fsyntax-only -verify %s

void wrongorder(int *List, int Length, int Value) {

/* expected-warning@+1 {{the LLVM pass structure currently is not able to apply the transformations in this order}} */
#pragma clang transform distribute
#pragma clang transform vectorize
  for (int i = 0; i < 8; i++)
    List[i] = Value;

/* expected-warning@+1 {{the LLVM pass structure currently is not able to apply the transformations in this order}} */
#pragma clang transform vectorize
#pragma clang transform unrollandjam
  for (int i = 0; i < 8; i++)
    for (int j = 0; j < 8; j++)
      List[i] += j;

}
