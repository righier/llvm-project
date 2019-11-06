// RUN: %clang_cc1 -std=c++11 -fno-experimental-transform-pragma -Wall -verify %s

void pragma_transform(int *List, int Length) {
/* expected-warning@+1 {{unknown pragma ignored}} */
#pragma clang transform unroll partial(4)
  for (int i = 0; i < Length; i+=1)
      List[i] = i;
}

