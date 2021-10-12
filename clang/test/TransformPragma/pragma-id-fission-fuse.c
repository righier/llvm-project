// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -ast-print %s | FileCheck --match-full-lines %s --check-prefix=PRINT
// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -disable-llvm-passes -o - %s | FileCheck %s --check-prefix=IR
// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-use-llvm-names -mllvm -debug-only=polly-ast -o /dev/null %s 2>&1 > /dev/null | FileCheck %s --check-prefix=AST
// RUN: %clang_cc1 -flegacy-pass-manager -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-use-llvm-names -o - %s | FileCheck %s --check-prefix=TRANS
// RUN: %clang                           -DMAIN                                   -std=c99            -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable %s -o %t_pragma_pack%exeext
// RUN: %t_pragma_pack%exeext | FileCheck --check-prefix=RESULT %s

void pragma_id_fission_fuse(int n, double A[n], double B[n]) {
#pragma clang loop(a, b) fuse
#pragma clang loop(i) fission split_at(1) fissioned_ids(a, b)

  #pragma clang loop id(i)
  for (int i = 0; i < n; i += 1) {
    A[i] = 3 * i;
    B[i] = 2 * i;
  }
}

#ifdef MAIN
#include <stdio.h>
#include <string.h>
int main() {
  double A[256], B[256];
  memset(A, 0, sizeof(A));
  memset(B, 0, sizeof(B));
  pragma_id_fission_fuse(256,A,B);
  printf("(%0.0f %0.0f)\n", A[1], B[1]);
  return 0;
}
#endif


// PRINT-LABEL: void pragma_id_fission_fuse(int n, double A[n], double B[n]) {
// PRINT-NEXT:  #pragma clang loop(a, b) fuse
// PRINT-NEXT:  #pragma clang loop(i) fission split_at(1) fissioned_ids(a, b)
// PRINT-NEXT:    #pragma clang loop id(i)
// PRINT-NEXT:    for (int i = 0; i < n; i += 1) {
// PRINT-NEXT:      A[i] = 3 * i;
// PRINT-NEXT:      B[i] = 2 * i;
// PRINT-NEXT:    }
// PRINT-NEXT:  }


// IR-LABEL: @pragma_id_fission_fuse(
// IR:         br label %for.cond, !llvm.loop !2
//
// IR: !2 = distinct !{!2, !3, !4, !5, !6, !7}
// IR: !3 = !{!"llvm.loop.disable_nonforced"}
// IR: !4 = !{!"llvm.loop.id", !"i"}
// IR: !5 = !{!"llvm.loop.distribute.enable", i1 true}
// IR: !6 = !{!"llvm.loop.distribute.split_at", i64 1}
// IR: !7 = !{!"llvm.loop.distribute.followup_distributed", !8, !15}
// IR: !8 = distinct !{!8, !3, !9, !10, !11, !13}
// IR: !9 = !{!"llvm.loop.id", !"a"}
// IR: !10 = !{!"llvm.loop.fuse.enable", i1 true}
// IR: !11 = !{!"llvm.loop.fuse.fuse_group", !12}
// IR: !12 = distinct !{!"Loop Fuse Group"}
// IR: !13 = !{!"llvm.loop.fuse.followup_fused", !14}
// IR: !14 = distinct !{!14, !3}
// IR: !15 = distinct !{!15, !3, !16, !10, !11, !13}
// IR: !16 = !{!"llvm.loop.id", !"b"}


// AST: if (1
// AST:     for (int c0 = 0; c0 < n; c0 += 1) {
// AST:        Stmt_for_body(c0);
// AST:        Stmt_for_body_b(c0);
// AST:     }
// AST:   else
// AST:     {  /* original code */ }


// TRANS: polly.start:
// TRANS: polly.loop_header:
// TRANS:   store double %p_conv, double* %scevgep, align 8, !alias.scope !21, !noalias !24
// TRANS:   store double %p_conv2, double* %scevgep19, align 8, !alias.scope !24, !noalias !21


// RESULT: (3 2)
