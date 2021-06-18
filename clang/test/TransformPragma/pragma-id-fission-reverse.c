// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -ast-print %s | FileCheck --check-prefix=PRINT --match-full-lines %s
// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -disable-llvm-passes -o - %s | FileCheck --check-prefix=IR %s
// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -debug-only=polly-ast -o /dev/null %s 2>&1 > /dev/null | FileCheck --check-prefix=AST %s
// RUN: %clang_cc1 -flegacy-pass-manager -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -o - %s | FileCheck --check-prefix=TRANS %s
// RUN: %clang                           -DMAIN                                   -std=c99            -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable %s -o %t_pragma_pack%exeext
// RUN: %t_pragma_pack%exeext | FileCheck --check-prefix=RESULT %s

void pragma_id_fission_reverse(int n, double A[n], double B[n]) {
#pragma clang loop(a) reverse
#pragma clang loop(b) reverse
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
  pragma_id_fission_reverse(256,A,B);
  printf("(%0.0f %0.0f)\n", A[1], B[1]);
  return 0;
}
#endif


// PRINT-LABEL: void pragma_id_fission_reverse(int n, double A[n], double B[n]) {
// PRINT-NEXT:  #pragma clang loop(a) reverse
// PRINT-NEXT:  #pragma clang loop(b) reverse
// PRINT-NEXT:  #pragma clang loop(i) fission split_at(1) fissioned_ids(a, b)
// PRINT-NEXT:    #pragma clang loop id(i)
// PRINT-NEXT:    for (int i = 0; i < n; i += 1) {
// PRINT-NEXT:      A[i] = 3 * i;
// PRINT-NEXT:      B[i] = 2 * i;
// PRINT-NEXT:    }
// PRINT-NEXT:  }


// IR-LABEL: @pragma_id_fission_reverse(
// IR:         br label %for.cond, !llvm.loop !2
//
// IR: !2 = distinct !{!2, !3, !4, !5, !6, !7}
// IR: !3 = !{!"llvm.loop.disable_nonforced"}
// IR: !4 = !{!"llvm.loop.id", !"i"}
// IR: !5 = !{!"llvm.loop.fission.enable", i1 true}
// IR: !6 = !{!"llvm.loop.fission.split_at", i64 1}
// IR: !7 = !{!"llvm.loop.fission.followup_fissioned", !8, !11}
// IR: !8 = distinct !{!8, !3, !9, !10}
// IR: !9 = !{!"llvm.loop.id", !"a"}
// IR: !10 = !{!"llvm.loop.reverse.enable", i1 true}
// IR: !11 = distinct !{!11, !3, !12, !10}
// IR: !12 = !{!"llvm.loop.id", !"b"}


// AST: if (1
// AST:   for (int c0 = 0; c0 <= floord(p_0 - 1, 32); c0 += 1) {
// AST:     for (int c1 = 0; c1 <= floord(p_1 - 1, 16); c1 += 1) {
// AST:       for (int c2 = 0; c2 <= min(31, p_0 - 32 * c0 - 1); c2 += 1) {
// AST:         for (int c3 = 0; c3 <= min(15, p_1 - 16 * c1 - 1); c3 += 1)
// AST:           Stmt4(32 * c0 + c2, 16 * c1 + c3);
// AST:       }
// AST:     }
// AST:   }
// AST: else
// AST:   {  /* original code */ }


// TRANS: %polly.indvar{{[0-9]*}} = phi
// TRANS: %polly.indvar{{[0-9]*}}.us = phi
// TRANS: %polly.indvar{{[0-9]*}}.us.us = phi
// TRANS: %polly.indvar{{[0-9]*}}.us.us = phi


// RESULT: (3)
