// RUN: %clang_cc1 -std=c++11 -fexperimental-transform-pragma -verify %s

void pragma_transform(int *List, int Length) {
// FIXME: This does not emit an error
#pragma clang

/* expected-error@+1 {{expected a transformation name}} */
#pragma clang transform
  for (int i = 0; i < Length; i+=1)
      List[i] = i;

/* expected-error@+1 {{unknown transformation}} */
#pragma clang transform unknown_transformation
  for (int i = 0; i < Length; i+=1)
      List[i] = i;

/* expected-error@+2 {{expected loop after transformation pragma}} */
#pragma clang transform unroll
  pragma_transform(List, Length);

/* expected-error@+1 {{unknown clause name}} */
#pragma clang transform unroll unknown_clause
  for (int i = 0; i < Length; i+=1)
      List[i] = i;

/* expected-error@+1 {{expected '(' after 'partial'}} */
#pragma clang transform unroll partial
  for (int i = 0; i < Length; i+=1)
      List[i] = i;

/* expected-error@+1 {{expected expression}} */
#pragma clang transform unroll partial(
  for (int i = 0; i < Length; i+=1)
      List[i] = i;

/* expected-error@+1 {{expected '(' after 'partial'}} */
#pragma clang transform unroll partial)
  for (int i = 0; i < Length; i+=1)
      List[i] = i;

/* expected-error@+2 {{expected ')'}} */
/* expected-note@+1 {{to match this '('}} */
#pragma clang transform unroll partial(4
  for (int i = 0; i < Length; i+=1)
      List[i] = i;

/* expected-error@+1 {{expected expression}} */
#pragma clang transform unroll partial()
  for (int i = 0; i < Length; i+=1)
      List[i] = i;

/* expected-error@+1 {{use of undeclared identifier 'badvalue'}} */
#pragma clang transform unroll partial(badvalue)
  for (int i = 0; i < Length; i+=1)
      List[i] = i;

  {
/* expected-error@+2 {{expected statement}} */
#pragma clang transform unroll
  }
}

/* expected-error@+1 {{expected unqualified-id}} */
#pragma clang transform unroll
int I;

/* expected-error@+1 {{expected unqualified-id}} */
#pragma clang transform unroll
void func();

class C1 {
/* expected-error@+3 {{this pragma cannot appear in class declaration}} */
/* expected-error@+2 {{expected member name or ';' after declaration specifiers}} */
/* expected-error@+1 {{unknown type name 'unroll'}} */
#pragma clang transform unroll
};

template<int F>
void pragma_transform_template_func(int *List, int Length) {
#pragma clang transform unroll partial(F)
  for (int i = 0; i < Length; i+=1)
      List[i] = i;
}

template<int F>
class C2 {
  void pragma_transform_template_class(int *List, int Length) {
#pragma clang transform unroll partial(F)
    for (int i = 0; i < Length; i+=1)
        List[i] = i;
  }
};
