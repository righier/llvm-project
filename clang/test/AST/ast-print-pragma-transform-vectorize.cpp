// RUN: %clang_cc1 -fexperimental-transform-pragma -ast-print %s -o - | FileCheck %s

void unroll(int *List, int Length) {
// CHECK: #pragma clang transform vectorize width(4)
#pragma clang transform vectorize width(4)
// CHECK-NEXT: for (int i = 0; i < Length; i += 1)
  for (int i = 0; i < Length; i += 1) 
    List[i] = i; 
}
