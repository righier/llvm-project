// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -ast-print %s | FileCheck --match-full-lines %s --check-prefix=PRINT
// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -disable-llvm-passes -o - %s | FileCheck %s --check-prefix=IR
// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-use-llvm-names -mllvm -debug-only=polly-ast -o /dev/null %s 2>&1 > /dev/null | FileCheck %s --check-prefix=AST
// RUN: %clang_cc1 -flegacy-pass-manager -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-use-llvm-names -o - %s | FileCheck %s --check-prefix=TRANS
// RUN: %clang                           -DMAIN                                   -std=c99            -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable %s -o %t_pragma_pack%exeext
// RUN: %t_pragma_pack%exeext | FileCheck --check-prefix=RESULT %s

void pragma_id_fuse_reverse(int n, double A[n], double B[n]) {
  #pragma clang loop(k) reverse
  #pragma clang loop(i, j) fuse fused_id(k)

  #pragma clang loop id(i)
  for (int i = 0; i < n; i += 1) 
    A[i] = 3 * i;
  #pragma clang loop id(j)
  for (int j = 0; j < n; j += 1) 
    B[j] = 2 * j;
}

#ifdef MAIN
#include <stdio.h>
#include <string.h>
int main() {
  double A[256], B[256];
  memset(A, 0, sizeof(A));
  memset(B, 0, sizeof(B));
  pragma_id_fuse_reverse(256,A,B);
  printf("(%0.0f %0.0f)\n", A[1], B[1]);
  return 0;
}
#endif


// PRINT-LABEL: void pragma_id_fuse_reverse(int n, double A[n], double B[n]) {
// PRINT-NEXT:   #pragma clang loop(k) reverse
// PRINT-NEXT:   #pragma clang loop(i, j) fuse fused_id(k)
// PRINT-NEXT:   #pragma clang loop id(i)
// PRINT-NEXT:   for (int i = 0; i < n; i += 1)
// PRINT-NEXT:     A[i] = 3 * i;
// PRINT-NEXT:   #pragma clang loop id(j)
// PRINT-NEXT:   for (int j = 0; j < n; j += 1)
// PRINT-NEXT:     B[j] = 2 * j;
// PRINT-NEXT: }


// IR-LABEL: @pragma_id_fuse_reverse(
// IR:         br label %for.cond, !llvm.loop !2
// IR:         br label %for.cond1, !llvm.loop !12
//
// IR: !2 = distinct !{!2, !3, !4, !5, !6, !8}
// IR: !3 = !{!"llvm.loop.disable_nonforced"}
// IR: !4 = !{!"llvm.loop.id", !"i"}
// IR: !5 = !{!"llvm.loop.fuse.enable", i1 true}
// IR: !6 = !{!"llvm.loop.fuse.fuse_group", !7}
// IR: !7 = distinct !{!"Loop Fuse Group"}
// IR: !8 = !{!"llvm.loop.fuse.followup_fused", !9}
// IR: !9 = distinct !{!9, !3, !10, !11}
// IR: !10 = !{!"llvm.loop.id", !"k"}
// IR: !11 = !{!"llvm.loop.reverse.enable", i1 true}
// IR: !12 = distinct !{!12, !3, !13, !5, !6, !8}
// IR: !13 = !{!"llvm.loop.id", !"j"}


// AST: if (1
// AST:     for (int c0 = -n + 1; c0 <= 0; c0 += 1) {
// AST:         Stmt_for_body(-c0);
// AST:         Stmt_for_body5(-c0);
// AST:     }
// AST:   else
// AST:     {  /* original code */ }


// TRANS: polly.start:
// TRANS: polly.loop_header:
// TRANS:   store double %p_conv, double* %scevgep, align 8, !alias.scope !18, !noalias !21
// TRANS:   store double %p_conv7, double* %scevgep35, align 8, !alias.scope !21, !noalias !18
// TRANS:   br i1 %exitcond.not38, label %for.cond.cleanup4, label %polly.loop_header


// RESULT: (3 2)
