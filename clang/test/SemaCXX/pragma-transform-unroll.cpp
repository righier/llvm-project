// RUN: %clang_cc1 -std=c++11 -fexperimental-transform-pragma -fsyntax-only -verify %s

void unroll(int *List, int Length, int Value) {
/* expected-error@+1 {{unroll factor must be at least two}} */
#pragma clang transform unroll partial(0)
  for (int i = 0; i < 8; i++)
      List[i] = Value;

/* expected-error@+1 {{unroll full/partial clauses specified multiple times}} */
#pragma clang transform unroll full full
  for (int i = 0; i < 8; i++)
      List[i] = Value;

/* expected-error@+1 {{unroll full/partial clauses specified multiple times}} */
#pragma clang transform unroll partial(4) partial(4)
  for (int i = 0; i < 8; i++)
      List[i] = Value;

/* expected-error@+1 {{unroll full/partial clauses specified multiple times}} */
#pragma clang transform unroll full partial(4)
  for (int i = 0; i < 8; i++)
      List[i] = Value;

/* expected-error@+1 {{transformation did not find its loop to transform}} */
#pragma clang transform unroll
#pragma clang transform unroll full
  for (int i = 0; i < 8; i++)
      List[i] = Value;

int f = 4;
  /* expected-error@+1 {{unroll partial clause expects an int}} */
#pragma clang transform unroll partial(f)
  for (int i = 0; i < Length; i+=1)
    List[i] = i;

}
