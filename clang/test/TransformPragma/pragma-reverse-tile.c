// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -ast-print %s | FileCheck --check-prefix=PRINT --match-full-lines %s
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -disable-llvm-passes -o - %s | FileCheck --check-prefix=IR --match-full-lines %s
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-use-llvm-names -mllvm -debug-only=polly-ast -o /dev/null %s 2>&1 > /dev/null | FileCheck --check-prefix=AST %s
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -o - %s | FileCheck --check-prefix=TRANS %s
// RUN: %clang -DMAIN -std=c99 -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable %s -o %t_pragma_pack%exeext
// RUN: %t_pragma_pack%exeext | FileCheck --check-prefix=RESULT %s

__attribute__((noinline))
void pragma_reverse_tile(int n, double A[n]) {
#pragma clang loop tile sizes(128)
#pragma clang loop reverse
  for (int i = n - 1; i >= 0; i--)
    A[i] = A[i] + 1;
}

#ifdef MAIN
#include <stdio.h>
#include <string.h>
int main() {
  double A[2048];
  memset(A, 0, sizeof(A));
  A[2] = 42;
  pragma_reverse_tile(2048,A);
  printf("(%0.0f)\n", A[2]);
  return 0;
}
#endif


// PRINT-LABEL: void pragma_reverse_tile(int n, double A[n]) __attribute__((noinline)) {
// PRINT-NEXT:   #pragma clang loop tile sizes(128)
// PRINT-NEXT:   #pragma clang loop reverse
// PRINT-NEXT:    for (int i = n - 1; i >= 0; i--)
// PRINT-NEXT:      A[i] = A[i] + 1;
// PRINT-NEXT:  }


// IR-LABEL: define dso_local void @pragma_reverse_tile(i32 %n, double* %A) #0 {
// IR:         br label %for.cond, !llvm.loop !2
//
// IR: !2 = distinct !{!2, !3, !4, !5}
// IR: !3 = !{!"llvm.loop.disable_nonforced"}
// IR: !4 = !{!"llvm.loop.reverse.enable", i1 true}
// IR: !5 = !{!"llvm.loop.reverse.followup_reversed", !6}
// IR: !6 = distinct !{!6, !3, !7, !8, !9}
// IR: !7 = !{!"llvm.loop.tile.enable", i1 true}
// IR: !8 = !{!"llvm.loop.tile.depth", i32 1}
// IR: !9 = !{!"llvm.loop.tile.size", i32 128}


// AST: if (1)
// AST:     for (int c0 = floord(-n + 1, 128); c0 <= 0; c0 += 1)
// AST:       for (int c1 = max(0, -n - 128 * c0 + 1); c1 <= min(127, -128 * c0); c1 += 1)
// AST:         Stmt_for_body(-128 * c0 - c1);
// AST: else
// AST:     {  /* original code */ }


// TRANS: polly.loop_preheader:
// TRANS: add nsw i64 %{{[0-9a-zA-Z.]+}}, -1


// RESULT: (43)
