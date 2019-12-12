// RUN: c-index-test -test-load-source local -Xclang -fexperimental-transform-pragma %s | FileCheck %s

typedef int int_t;
struct foo { long x; };

void test() {
#pragma clang transform unroll partial(2*2)
  for (int i = 0; i < 8; i += 1)
    ;
}


// CHECK: transform.c:7:15: TransformExecutableDirective= Extent=[7:15 - 7:44]
// CHECK: transform.c:7:40: BinaryOperator= Extent=[7:40 - 7:43]
// CHECK: transform.c:7:40: IntegerLiteral= Extent=[7:40 - 7:41]
// CHECK: transform.c:7:42: IntegerLiteral= Extent=[7:42 - 7:43]
