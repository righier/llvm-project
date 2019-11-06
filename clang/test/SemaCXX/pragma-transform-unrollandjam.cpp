// RUN: %clang_cc1 -std=c++11 -fexperimental-transform-pragma -fsyntax-only -verify %s

void unrollandjam(int *List, int Length, int Value) {
/* expected-error@+1 {{the partial clause can be specified at most once}} */
#pragma clang transform unrollandjam partial(4) partial(4)
  for (int i = 0; i < Length; i++)
    for (int j = 0; j < Length; j++)
      List[i] += j*Value;

/* expected-error@+1 {{unroll-and-jam requires exactly one nested loop}} */
#pragma clang transform unrollandjam
  for (int i = 0; i < Length; i++)
      List[i] = Value;

/* expected-error@+1 {{unroll-and-jam requires exactly one nested loop}} */
#pragma clang transform unrollandjam
  for (int i = 0; i < Length; i++) {
    for (int j = 0; j < Length; j++)
      List[i] += j*Value;
    for (int j = 0; j < Length; j++)
      List[i] += j*Value;
  }

/* expected-error@+1 {{inner loop of unroll-and-jam is not the innermost}} */
#pragma clang transform unrollandjam
  for (int i = 0; i < Length; i++)
    for (int j = 0; j < Length; j++)
      for (int k = 0; k < Length; k++)
        List[i] += k + j*Value;
}
