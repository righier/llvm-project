// RUN: %clang_cc1 -triple x86_64-unknown-unknown -fexperimental-transform-pragma -emit-pch -o %t.a %s
// RUN: %clang_cc1 -triple x86_64-unknown-unknown -fexperimental-transform-pragma -include-pch %t.a %s -ast-print -o - | FileCheck %s --dump-input=fail -vv

#ifndef HEADER
#define HEADER

void unroll_heuristic(int n) {
#pragma clang transform unroll
  for (int i = 0; i < 4; i+=1)
    ;
}

// CHECK-LABEL: FunctionDecl {{.*}} unroll_heuristic
// CHECK: TransformExecutableDirective
// CHECK-NEXT: ForStmt

#if 0

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


template<int FACTOR>
void unroll_template_function(int n) {
#pragma clang transform unroll partial(FACTOR)
  for (int i = 0; i < n; i+=1)
    ;
}


// CHECK-LABEL: FunctionDecl {{.*}} unroll_template_function
// CHECK: TransformExecutableDirective
// CHECK-NEXT: PartialClause
// CHECK-NEXT:   IntegerLiteral
// CHECK-NEXT: ForStmt

template void unroll_template_function<5>(int);


template<int FACTOR>
struct Unroll {
  void unroll_template_method(int n) {
#pragma clang transform unroll partial(FACTOR)
    for (int i = 0; i < n; i += 1)
      ;
  }
};

// CHECK-LABEL: FunctionDecl {{.*}} unroll_template_function
// CHECK: TransformExecutableDirective
// CHECK-NEXT: PartialClause
// CHECK-NEXT:   IntegerLiteral
// CHECK-NEXT: ForStmt

template struct Unroll<6>;
#endif
#endif /* HEADER */
