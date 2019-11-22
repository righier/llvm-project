// RUN: %clang_cc1 -triple x86_64-unknown-unknown -fexperimental-transform-pragma -emit-pch -o %t.pch %s
// RUN: %clang_cc1 -triple x86_64-unknown-unknown -fexperimental-transform-pragma -include-pch %t.pch %s -ast-dump-all -o - | FileCheck %s

#ifndef HEADER
#define HEADER

void vectorize_heuristic(int n) {
#pragma clang transform vectorize
  for (int i = 0; i < n; i+=1)
    ;
}
// CHECK-LABEL: FunctionDecl {{.*}} imported vectorize_heuristic
// CHECK: TransformExecutableDirective
// CHECK-NEXT: ForStmt


void vectorize_width(int n) {
#pragma clang transform vectorize width(4)
  for (int i = 0; i < n; i+=1)
    ;
}
// CHECK-LABEL: FunctionDecl {{.*}} imported vectorize_width
// CHECK: TransformExecutableDirective
// CHECK-NEXT: WidthClause
// CHECK-NEXT:   IntegerLiteral {{.*}} 'int' 4
// CHECK-NEXT: ForStmt

#endif /* HEADER */
