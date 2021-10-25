// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -ast-print %s | FileCheck --match-full-lines %s --check-prefix=PRINT
// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -disable-llvm-passes -o - %s | FileCheck %s --check-prefix=IR
// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-use-llvm-names -mllvm -debug-only=polly-ast -o /dev/null %s 2>&1 > /dev/null | FileCheck %s --check-prefix=AST
// RUN: %clang_cc1 -flegacy-pass-manager -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-use-llvm-names -o - %s | FileCheck %s --check-prefix=TRANS
// RUN: %clang                           -DMAIN                                   -std=c99            -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable %s -o %t_pragma_pack%exeext
// RUN: %t_pragma_pack%exeext | FileCheck --check-prefix=RESULT %s

void pragma_id_fuse(int n, double A[n], double B[n], double C[n]) {
#pragma clang loop(i, j) fuse

  for (int k = 0; k < n; k += 1)
    C[k] = 3 * k;
  for (int k = 0; k < n; k += 1)
    C[k] = 2 * C[k];
  #pragma clang loop id(i)
  for (int i = 0; i < n - 2; i += 1)
    A[i] = 3 * i;
  #pragma clang loop id(j)
  for (int j = 1; j < n; j += 1)
    B[j] = 2 * j;
  for (int k = 0; k < n; k += 1)
    C[k] = C[k] + 42;
}

#ifdef MAIN
#include <stdio.h>
#include <string.h>
int main() {
  double A[256], B[256], C[256];
  memset(A, 0, sizeof(A));
  memset(B, 0, sizeof(B));
  memset(C, 0, sizeof(B));
  pragma_id_fuse(256,A,B,C);
  printf("(%0.0f %0.0f %0.0f)\n", A[1], B[1], C[1]);
  return 0;
}
#endif


// PRINT-LABEL: void pragma_id_fuse(int n, double A[n], double B[n], double C[n]) {
// PRINT-NEXT:  #pragma clang loop(i, j) fuse
// PRINT-NEXT:    for (int k = 0; k < n; k += 1)
// PRINT-NEXT:      C[k] = 3 * k;
// PRINT-NEXT:    for (int k = 0; k < n; k += 1)
// PRINT-NEXT:      C[k] = 2 * C[k];
// PRINT-NEXT:    #pragma clang loop id(i)
// PRINT-NEXT:    for (int i = 0; i < n - 2; i += 1)
// PRINT-NEXT:      A[i] = 3 * i;
// PRINT-NEXT:    #pragma clang loop id(j)
// PRINT-NEXT:    for (int j = 1; j < n; j += 1)
// PRINT-NEXT:      B[j] = 2 * j;
// PRINT-NEXT:    for (int k = 0; k < n; k += 1)
// PRINT-NEXT:      C[k] = C[k] + 42;
// PRINT-NEXT:   }


// IR-LABEL: @pragma_id_fuse(
// IR:         br label %for.cond14, !llvm.loop !2
// IR:         br label %for.cond25, !llvm.loop !10
//
// IR: !2 = distinct !{!2, !3, !4, !5, !6, !8}
// IR: !3 = !{!"llvm.loop.disable_nonforced"}
// IR: !4 = !{!"llvm.loop.id", !"i"}
// IR: !5 = !{!"llvm.loop.fuse.enable", i1 true}
// IR: !6 = !{!"llvm.loop.fuse.fuse_group", !7}
// IR: !7 = distinct !{!"Loop Fuse Group"}
// IR: !8 = !{!"llvm.loop.fuse.followup_fused", !9}
// IR: !9 = distinct !{!9, !3}
// IR: !10 = distinct !{!10, !3, !11, !5, !6, !8}
// IR: !11 = !{!"llvm.loop.id", !"j"}


// AST: if (1
// AST:     {
// AST:       for (int c0 = 0; c0 < n; c0 += 1)
// AST:         Stmt_for_body(c0);
// AST:       for (int c0 = 0; c0 < n; c0 += 1)
// AST:         Stmt_for_body6(c0);
// AST:       for (int c0 = 0; c0 < n - 1; c0 += 1) {
// AST:         if (n >= c0 + 3)
// AST:           Stmt_for_body19(c0);
// AST:         Stmt_for_body31(c0);
// AST:       }
// AST:       for (int c0 = 0; c0 < n; c0 += 1)
// AST:          Stmt_for_body44(c0);
// AST:     }
// AST: else
// AST:     {  /* original code */ }


// TRANS: polly.start:
// TRANS: polly.loop_header133: ; preds = %polly.loop_exit126, %polly.stmt.for.body31
// TRANS:   br i1 %.not, label %polly.stmt.for.body31, label %polly.stmt.for.body19
// TRANS: polly.stmt.for.body31: ; preds = %polly.loop_header133, %polly.stmt.for.body19
// TRANS: polly.stmt.for.body19: ; preds = %polly.loop_header133


// RESULT: (3 2 48)
