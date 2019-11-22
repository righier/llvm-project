// RUN: %clang_cc1 -triple x86_64-unknown-unknown -fexperimental-transform-pragma -emit-pch -o %t.pch %s
// RUN: %clang_cc1 -triple x86_64-unknown-unknown -fexperimental-transform-pragma -include-pch %t.pch %s -ast-dump-all -o - | FileCheck %s

#ifndef HEADER
#define HEADER

void  unrollandjam_heuristic(int n) {
#pragma clang transform unrollandjam
  for (int i = 0; i < n; i+=1)
    for (int j = 0; j < n; j+=1)
      ;
}
// CHECK-LABEL: FunctionDecl {{.*}} imported unrollandjam_heuristic
// CHECK: TransformExecutableDirective
// CHECK-NEXT: ForStmt


void unrollandjam_partial(int n) {
#pragma clang transform unrollandjam partial(4)
  for (int i = 0; i < n; i+=1)
    for (int j = 0; j < n; j+=1)
      ;
}
// CHECK-LABEL: FunctionDecl {{.*}} imported unrollandjam_partial
// CHECK: TransformExecutableDirective
// CHECK-NEXT: PartialClause
// CHECK-NEXT:   IntegerLiteral {{.*}} 'int' 4
// CHECK-NEXT: ForStmt

#endif /* HEADER */
