// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -ast-print %s | FileCheck %s --match-full-lines --check-prefix=PRINT
// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -disable-llvm-passes -o - %s | FileCheck %s --check-prefix=IR
// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-use-llvm-names -mllvm -debug-only=polly-ast -o /dev/null %s 2>&1 > /dev/null | FileCheck %s --check-prefix=AST
// RUN: %clang_cc1 -flegacy-pass-manager -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-use-llvm-names -o - %s | FileCheck %s --check-prefix=TRANS
// RUN: %clang                           -DMAIN                                   -std=c99            -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable %s -o %t_pragma_pack%exeext
// RUN: %t_pragma_pack%exeext | FileCheck --check-prefix=RESULT %s

void pragma_unrollingandjam_factor_interchange(double C[const restrict static 128][128], double A[const restrict static 256]) {
#pragma clang loop(ui, uj) interchange
#pragma clang loop(i, j) unrollingandjam factor(2) unrolled_ids(ui, uj)

  #pragma clang loop id(i)
  for (int i = 0; i < 128; i += 1)
    #pragma clang loop id(j)
    for (int j = 0; j < 128; j += 1)
      C[i][j] = A[i+j] + i + j;
}

#ifdef MAIN
#include <stdio.h>
#include <string.h>
int main() {
  double C[128][128];
  double A[256];
  memset(A, 0, sizeof(A));
  A[2] = 42;
  pragma_unrollingandjam_factor_interchange(C,A);
  // C[1][1] = A[1+1] + 1 + 1 = 42 + 1 + 1 = 44
  printf("(%0.0f)\n", C[1][1]);
  return 0;
}
#endif


// PRINT-LABEL: void pragma_unrollingandjam_factor_interchange(double C[const restrict static 128][128], double A[const restrict static 256]) {
// PRINT-NEXT:  #pragma clang loop(ui, uj) interchange
// PRINT-NEXT:  #pragma clang loop(i, j) unrollingandjam factor(2) unrolled_ids(ui, uj)
// PRINT-NEXT:    #pragma clang loop id(i)
// PRINT-NEXT:    for (int i = 0; i < 128; i += 1)
// PRINT-NEXT:      #pragma clang loop id(j)
// PRINT-NEXT:      for (int j = 0; j < 128; j += 1)
// PRINT-NEXT:        C[i][j] = A[i + j] + i + j;
// PRINT-NEXT:  }


// IR-LABEL: @pragma_unrollingandjam_factor_interchange(
// IR:         br label %for.cond1, !llvm.loop !2
// IR:         br label %for.cond, !llvm.loop !8
//
// IR: !2 = distinct !{!2, !3, !4, !5}
// IR: !3 = !{!"llvm.loop.disable_nonforced"}
// IR: !4 = !{!"llvm.loop.id", !"j"}
// IR: !5 = !{!"llvm.loop.unroll_and_jam.followup_inner_unrolled", !6}
//
// IR: !6 = distinct !{!6, !3, !7}
// IR: !7 = !{!"llvm.loop.id", !"uj"}
//
// IR: !8 = distinct !{!8, !3, !9, !10, !11, !12}
// IR: !9 = !{!"llvm.loop.id", !"i"}
// IR: !10 = !{!"llvm.loop.unroll_and_jam.enable", i1 true}
// IR: !11 = !{!"llvm.loop.unroll_and_jam.count", i3 2}
// IR: !12 = !{!"llvm.loop.unroll_and_jam.followup_outer_unrolled", !13}
//
// IR: !13 = distinct !{!13, !3, !14, !15, !16, !17}
// IR: !14 = !{!"llvm.loop.id", !"ui"}
// IR: !15 = !{!"llvm.loop.interchange.enable", i1 true}
// IR: !16 = !{!"llvm.loop.interchange.depth", i32 2}
// IR: !17 = !{!"llvm.loop.interchange.permutation", i32 1, i32 0}


// AST: if (1
// AST:   for (int c0 = 0; c0 <= 127; c0 += 1) {
// AST:     for (int c1 = 0; c1 <= 127; c1 += 2) {
// AST:       Stmt_for_body4(c1, c0);
// AST:       Stmt_for_body4(c1 + 1, c0);
// AST:     }
// AST:   }


// TRANS: polly.loop_header32:
// TRANS:   %polly.indvar35 = phi i64 [ 0, %polly.loop_header ], [ %polly.indvar_next36, %polly.loop_header32 ]
// TRANS:   store double %p_add7, double* %scevgep40, align 8, !alias.scope !5, !noalias !2
// TRANS:   %3 = or i64 %polly.indvar35, 1
// TRANS:   store double %p_add748, double* %scevgep50, align 8, !alias.scope !5, !noalias !2
// TRANS:   %polly.indvar_next36 = add nuw nsw i64 %polly.indvar35, 2


// RESULT: (44)
