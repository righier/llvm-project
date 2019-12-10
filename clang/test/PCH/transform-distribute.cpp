// RUN: %clang_cc1 -triple x86_64-unknown-unknown -fexperimental-transform-pragma -emit-pch -o %t.pch %s
// RUN: %clang_cc1 -triple x86_64-unknown-unknown -fexperimental-transform-pragma -include-pch %t.pch %s -ast-dump-all -o - | FileCheck %s --dump-input=fail -vv

#ifndef HEADER
#define HEADER

void distribute_heuristic(int n) {
#pragma clang transform distribute
  for (int i = 0; i < n; i+=1)
    ;
}
// CHECK-LABEL: FunctionDecl {{.*}} imported distribute_heuristic
// CHECK: TransformExecutableDirective
// CHECK-NEXT: ForStmt

#endif /* HEADER */
