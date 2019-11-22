// RUN: %clang_cc1 -triple x86_64-unknown-unknown -fexperimental-transform-pragma -emit-pch -o %t.pch %s
// RUN: %clang_cc1 -triple x86_64-unknown-unknown -fexperimental-transform-pragma -include-pch %t.pch %s -ast-dump-all -o - | FileCheck %s

#ifndef HEADER
#define HEADER

void interleave_heuristic(int n) {
#pragma clang transform interleave
  for (int i = 0; i < n; i+=1)
    ;
}
// CHECK-LABEL: FunctionDecl {{.*}} imported interleave_heuristic
// CHECK: TransformExecutableDirective
// CHECK-NEXT: ForStmt


void interleave_factor(int n) {
#pragma clang transform interleave factor(4)
  for (int i = 0; i < n; i+=1)
    ;
}
// CHECK-LABEL: FunctionDecl {{.*}} imported interleave_factor
// CHECK: TransformExecutableDirective
// CHECK-NEXT: FactorClause
// CHECK-NEXT:   IntegerLiteral {{.*}} 'int' 4
// CHECK-NEXT: ForStmt

#endif /* HEADER */
