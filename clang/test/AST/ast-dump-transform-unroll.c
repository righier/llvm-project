// RUN: %clang_cc1 -triple x86_64-unknown-unknown -fexperimental-transform-pragma -ast-dump %s | FileCheck %s

void unroll_full(int n) {
#pragma clang transform unroll full
  for (int i = 0; i < 4; i+=1)
    ;
}

// CHECK-LABEL: FunctionDecl {{.*}} unroll_full
// CHECK: TransformExecutableDirective
// CHECK-NEXT: FullClause
// CHECK-NEXT: ForStmt


void unroll_partial(int n) {
#pragma clang transform unroll partial(4)
  for (int i = 0; i < n; i+=1)
    ;
}

// CHECK-LABEL: FunctionDecl {{.*}} unroll_partial
// CHECK: TransformExecutableDirective
// CHECK-NEXT: PartialClause
// CHECK-NEXT:   IntegerLiteral
// CHECK-NEXT: ForStmt
