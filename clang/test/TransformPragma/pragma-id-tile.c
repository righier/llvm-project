// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -ast-print %s | FileCheck --check-prefix=PRINT --match-full-lines %s
// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -disable-llvm-passes -o - %s | FileCheck --check-prefix=IR --match-full-lines %s
// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -debug-only=polly-ast -o /dev/null %s 2>&1 > /dev/null | FileCheck --check-prefix=AST %s
// RUN: %clang_cc1 -flegacy-pass-manager -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -o - %s | FileCheck --check-prefix=TRANS %s
// RUN: %clang                           -DMAIN                                   -std=c99            -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable %s -o %t_pragma_pack%exeext
// RUN: %t_pragma_pack%exeext | FileCheck --check-prefix=RESULT %s

void pragma_id_tile(int m, int n, double C[m][n]) {
#pragma clang loop(i, j) tile sizes(32, 16)
#pragma clang loop id(i)
  for (int i = 0; i < m; i += 1)
#pragma clang loop id(j)
    for (int j = 0; j < n; j += 1)
      C[i][j] = i + j;
}

#ifdef MAIN
#include <stdio.h>
#include <string.h>
int main() {
  double C[256][128];
  memset(C, 0, sizeof(C));
  C[1][2] = 42;
  pragma_id_tile(256,128,C);
  printf("(%0.0f)\n", C[2][1]);
  return 0;
}
#endif


// PRINT-LABEL: void pragma_id_tile(int m, int n, double C[m][n]) {
// PRINT-NEXT:  #pragma clang loop(i, j) tile sizes(32, 16)
// PRINT-NEXT:  #pragma clang loop id(i)
// PRINT-NEXT:    for (int i = 0; i < m; i += 1)
// PRINT-NEXT:  #pragma clang loop id(j)
// PRINT-NEXT:      for (int j = 0; j < n; j += 1)
// PRINT-NEXT:         C[i][j] = i + j;
// PRINT-NEXT:  }


// IR-LABEL: define dso_local void @pragma_id_tile(i32 %m, i32 %n, double* %C) #0 {
// IR:         br label %for.cond1, !llvm.loop !2
// IR:         br label %for.cond, !llvm.loop !6
//
// IR: !2 = distinct !{!2, !3, !4, !5}
// IR: !3 = !{!"llvm.loop.disable_nonforced"}
// IR: !4 = !{!"llvm.loop.id", !"j"}
// IR: !5 = !{!"llvm.loop.tile.size", i32 16}
// IR: !6 = distinct !{!6, !3, !7, !8, !9, !10}
// IR: !7 = !{!"llvm.loop.id", !"i"}
// IR: !8 = !{!"llvm.loop.tile.enable", i1 true}
// IR: !9 = !{!"llvm.loop.tile.depth", i32 2}
// IR: !10 = !{!"llvm.loop.tile.size", i32 32}


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


// TRANS: %polly.indvar = phi
// TRANS: %polly.indvar37.us = phi
// TRANS: %polly.indvar45.us.us = phi
// TRANS: %polly.indvar53.us.us = phi


// RESULT: (3)
