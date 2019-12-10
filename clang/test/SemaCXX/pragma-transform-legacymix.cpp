// RUN: %clang_cc1 -std=c++11 -fopenmp -fexperimental-transform-pragma -fsyntax-only -verify %s

void legacymix(int *List, int Length, int Value) {

/* expected-error@+2 {{expected a for, while, or do-while loop to follow '#pragma unroll'}} */
#pragma unroll
#pragma clang transform unroll
  for (int i = 0; i < 8; i++)
    List[i] = Value;


/* expected-error@+2 {{expected a for, while, or do-while loop to follow '#pragma clang loop'}} */
#pragma clang loop unroll(enable)
#pragma clang transform unroll
  for (int i = 0; i < 8; i++)
    List[i] = Value;

/* expected-error@+2 {{expected loop after transformation pragma}} */
#pragma clang transform unroll
#pragma clang loop unroll(enable)
  for (int i = 0; i < 8; i++)
    List[i] = Value;

/* expected-error@+2 {{expected loop after transformation pragma}} */
#pragma clang transform unroll
#pragma clang loop unroll(disable)
  for (int i = 0; i < 8; i++)
    List[i] = Value;

/* expected-error@+2 {{expected a for, while, or do-while loop to follow '#pragma clang loop'}} */
#pragma clang loop unroll(disable)
#pragma clang transform unroll
  for (int i = 0; i < 8; i++)
    List[i] = Value;

/* expected-error@+1 {{Cannot combine #pragma clang transform with other transformations}} */
#pragma clang transform unrollandjam partial(2)
  for (int i = 0; i < 8; i++)
#pragma clang loop unroll(disable)
    for (int j = 0; j < 16; j++)
    List[i] = Value;

/* expected-error@+1 {{Cannot combine #pragma clang transform with other transformations}} */
#pragma clang transform unrollandjam
  for (int i = 0; i < 8; i++)
#pragma omp simd
    for (int j = 0; j < 16; j++)
      List[i] = Value;

/* expected-error@+2 {{expected loop after transformation pragma}} */
#pragma clang transform unroll
#pragma omp simd
  for (int i = 0; i < 8; i++)
    List[i] = Value;

/* expected-error@+2 {{statement after '#pragma omp simd' must be a for loop}} */
#pragma omp simd
#pragma clang transform unroll
  for (int i = 0; i < 8; i++)
    List[i] = Value;

}
