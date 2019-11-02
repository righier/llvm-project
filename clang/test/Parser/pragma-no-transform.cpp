// RUN: %clang_cc1 -std=c++11 -fno-experimental-transform-pragma -verify %s

void pragma_transform(int *List, int Length) {
/* expected-error@+1 {{use of undeclared identifier 'badvalue'}} */
#pragma clang transform unroll partial(4)
  for (int i = 0; i < Length; i+=1)
      List[i] = i;
}
