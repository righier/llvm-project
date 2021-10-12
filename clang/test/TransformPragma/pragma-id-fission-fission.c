// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -ast-print %s | FileCheck --check-prefix=PRINT --match-full-lines %s
// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -disable-llvm-passes -o - %s | FileCheck --check-prefix=IR %s
// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-use-llvm-names -mllvm -debug-only=polly-ast -o /dev/null %s 2>&1 > /dev/null | FileCheck --check-prefix=AST %s
// RUN: %clang_cc1 -flegacy-pass-manager -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-use-llvm-names -o - %s | FileCheck --check-prefix=TRANS %s
// RUN: %clang                           -DMAIN                                   -std=c99                                                                                              %s -o %t_native%exeext
// RUN: %t_native%exeext | FileCheck --check-prefix=RESULT %s
// RUN: %clang                           -DMAIN                                   -std=c99            -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable %s -o %t_pragma_pack%exeext
// RUN: %t_pragma_pack%exeext | FileCheck --check-prefix=RESULT %s

void pragma_id_fission_fission(int n, double A[n], double B[n]) {
#pragma clang loop(j) fission split_at(1)
#pragma clang loop(i) fission split_at(1)

  #pragma clang loop id(j)
  for (int j = 0; j < n; j += 1) {
    #pragma clang loop id(i)
    for (int i = 0; i < n; i += 1) {
      A[i] = 3 * i + j;
      B[i] = 2 * i + j;
    }
  }
}

#ifdef MAIN
#include <stdio.h>
#include <string.h>
int main() {
  double A[256], B[256];
  memset(A, 0, sizeof(A));
  memset(B, 0, sizeof(B));
  pragma_id_fission_fission(256,A,B);
  printf("(%0.0f %0.0f)\n", A[1], B[1]);
  return 0;
}
#endif


// PRINT-LABEL: void pragma_id_fission_fission(int n, double A[n], double B[n]) {
// PRINT-NEXT:  #pragma clang loop(j) fission split_at(1)
// PRINT-NEXT:  #pragma clang loop(i) fission split_at(1)
// PRINT-NEXT:    #pragma clang loop id(j)
// PRINT-NEXT:    for (int j = 0; j < n; j += 1) {
// PRINT-NEXT:      #pragma clang loop id(i)
// PRINT-NEXT:      for (int i = 0; i < n; i += 1) {
// PRINT-NEXT:        A[i] = 3 * i + j;
// PRINT-NEXT:        B[i] = 2 * i + j;
// PRINT-NEXT:      }
// PRINT-NEXT:    }
// PRINT-NEXT:  }


// IR-LABEL: @pragma_id_fission_fission(
// IR:         br label %for.cond1, !llvm.loop !2
// IR:         br label %for.cond, !llvm.loop !10
//
// IR: !2 = distinct !{!2, !3, !4, !5, !6, !7}
// IR: !3 = !{!"llvm.loop.disable_nonforced"}
// IR: !4 = !{!"llvm.loop.id", !"i"}
// IR: !5 = !{!"llvm.loop.distribute.enable", i1 true}
// IR: !6 = !{!"llvm.loop.distribute.split_at", i64 1}
// IR: !7 = !{!"llvm.loop.distribute.followup_distributed", !8, !9}
// IR: !8 = distinct !{!8, !3}
// IR: !9 = distinct !{!9, !3}
// IR: !10 = distinct !{!10, !3, !11, !5, !6, !12}
// IR: !11 = !{!"llvm.loop.id", !"j"}
// IR: !12 = !{!"llvm.loop.distribute.followup_distributed", !13, !14}
// IR: !13 = distinct !{!13, !3}
// IR: !14 = distinct !{!14, !3}


// AST: if (1
// AST:     {
// AST:        for (int c0 = 0; c0 < n; c0 += 1) {
// AST:          for (int c1 = 0; c1 < n; c1 += 1)
// AST:            Stmt_for_body4(c0, c1);
// AST:        for (int c0 = 0; c0 < n; c0 += 1) {
// AST:          for (int c1 = 0; c1 < n; c1 += 1)
// AST:            Stmt_for_body4_b(c0, c1);
// AST:     }
// AST:   else
// AST:     {  /* original code */ }


// TRANS: polly.start:


// RESULT: (258 257)
