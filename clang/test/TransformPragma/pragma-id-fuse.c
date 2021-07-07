// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -ast-print %s | FileCheck --check-prefix=PRINT --match-full-lines %s
// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -disable-llvm-passes -o - %s | FileCheck --check-prefix=IR %s
// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -debug-only=polly-ast -o /dev/null %s 2>&1 > /dev/null | FileCheck --check-prefix=AST %s
// RUN: %clang_cc1 -flegacy-pass-manager -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -o - %s | FileCheck --check-prefix=TRANS %s
// RUN: %clang                           -DMAIN                                   -std=c99            -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable %s -o %t_pragma_pack%exeext
// RUN: %t_pragma_pack%exeext | FileCheck --check-prefix=RESULT %s

void pragma_id_fuse(int n, double A[n], double B[n]) {
#pragma clang loop(i, j) fuse

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
  pragma_id_fuse(256,A,B);
  printf("(%0.0f %0.0f)\n", A[1], B[1]);
  return 0;
}
#endif


// PRINT-LABEL: void pragma_id_fuse(int n, double A[n], double B[n]) {
// PRINT-NEXT: #pragma clang loop(i, j) fuse
// PRINT-NEXT:   #pragma clang loop id(i)
// PRINT-NEXT:   for (int i = 0; i < n; i += 1) 
// PRINT-NEXT:     A[i] = 3 * i;
// PRINT-NEXT:   #pragma clang loop id(j)
// PRINT-NEXT:   for (int j = 0; j < n; j += 1) 
// PRINT-NEXT:     B[j] = 2 * j;
// PRINT-NEXT:  }


// IR-LABEL: @pragma_id_fuse(
// IR:         br label %for.cond, !llvm.loop !2
// IR:         br label %for.cond1, !llvm.loop !7
//
// IR: !2 = distinct !{!2, !3, !4, !5, !6, !9}
// IR: !3 = !{!"llvm.loop.disable_nonforced"}
// IR: !4 = !{!"llvm.loop.id", !"i"}
// IR: !5 = !{!"llvm.loop.fuse.enable", i1 true}
// IR: !6 = !{!"llvm.loop.fuse.fuse_with", !2, !7}
// IR: !7 = distinct !{!7, !3, !8, !5, !6}
// IR: !8 = !{!"llvm.loop.id", !"j"}
// IR: !9 = !{!"llvm.loop.fuse.followup_fused", !10}
// IR: !10 = distinct !{!10}


// AST: if (1 
// AST:     {
// AST:       for (int c0 = 0; c0 < p_0; c0 += 1)
// AST:         Stmt2(c0);
// AST:       for (int c0 = 0; c0 < p_0; c0 += 1)
// AST:         Stmt2b(c0);
// AST:     }
// AST:   else
// AST:     {  /* original code */ }


// TRANS: polly.loop_header:
// TRANS:   store double %p_conv, double* %scevgep, align 8, !alias.scope !11, !noalias !13
// TRANS: polly.loop_header19:
// TRANS:   store double %p_conv2, double* %scevgep27, align 8, !alias.scope !14, !noalias !15


// RESULT: (3 2)
