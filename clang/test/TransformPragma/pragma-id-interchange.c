// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -ast-print %s | FileCheck --check-prefix=PRINT --match-full-lines %s
// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -disable-llvm-passes -o - %s | FileCheck --check-prefix=IR --match-full-lines %s
// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -debug-only=polly-ast -mllvm -polly-use-llvm-names -o /dev/null %s 2>&1 > /dev/null | FileCheck --check-prefix=AST %s
// RUN: %clang_cc1 -flegacy-pass-manager -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -o - %s | FileCheck --check-prefix=TRANS %s
// RUN: %clang                           -DMAIN                                   -std=c99            -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable %s -o %t_pragma_id_interchange%exeext
// RUN: %t_pragma_id_interchange%exeext | FileCheck --check-prefix=RESULT %s

__attribute__((noinline))
void pragma_id_interchange(int n, int m, double C[m][n], double A[n][m]) {
#pragma clang loop(i, j) interchange permutation(j, i)
#pragma clang loop id(i)
  for (int i = 0; i < m; i += 1)
#pragma clang loop id(j)
    for (int j = 0; j < n; j += 1)
      C[i][j] = A[j][i] + i + j;
}

#ifdef MAIN
#include <stdio.h>
#include <string.h>
int main() {
  double C[256][128];
  double A[128][256];
  memset(A, 0, sizeof(A));
  memset(C, 0, sizeof(C));
  A[1][2] = 42;
  pragma_id_interchange(128,256,C,A);
  printf("(%0.0f)\n", C[2][1]);
  return 0;
}
#endif


// PRINT-LABEL: void pragma_id_interchange(int n, int m, double C[m][n], double A[n][m]) __attribute__((noinline)) {
// PRINT-NEXT:  #pragma clang loop(i, j) interchange permutation(j, i)
// PRINT-NEXT:  #pragma clang loop id(i)
// PRINT-NEXT:      for (int i = 0; i < m; i += 1)
// PRINT-NEXT:  #pragma clang loop id(j)
// PRINT-NEXT:          for (int j = 0; j < n; j += 1)
// PRINT-NEXT:              C[i][j] = A[j][i] + i + j;
// PRINT-NEXT:  }


// IR-LABEL: define dso_local void @pragma_id_interchange(i32 %n, i32 %m, double* %C, double* %A) #0 {
// IR:         br label %for.cond1, !llvm.loop !2
// IR:         br label %for.cond, !llvm.loop !5
//
// IR: !2 = distinct !{!2, !3, !4}
// IR: !3 = !{!"llvm.loop.disable_nonforced"}
// IR: !4 = !{!"llvm.loop.id", !"j"}
// IR: !5 = distinct !{!5, !3, !6, !7, !8, !9}
// IR: !6 = !{!"llvm.loop.id", !"i"}
// IR: !7 = !{!"llvm.loop.interchange.enable", i1 true}
// IR: !8 = !{!"llvm.loop.interchange.depth", i32 2}
// IR: !9 = !{!"llvm.loop.interchange.permutation", i32 1, i32 0}


// AST: if (1
// AST:   for (int c0 = 0; c0 < n; c0 += 1) {
// AST: 	for (int c1 = 0; c1 < m; c1 += 1)
// AST: 		Stmt_for_body4(c1, c0);
// AST:   }
// AST: else
// AST:   {  /* original code */ }


// TRANS-LABEL: @pragma_id_interchange
// TRANS: %polly.indvar.us = phi
// TRANS: %polly.indvar70.us = phi


// RESULT: (45)
