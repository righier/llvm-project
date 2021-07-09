// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -ast-print %s | FileCheck --check-prefix=PRINT --match-full-lines %s
// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -disable-llvm-passes -o - %s | FileCheck --check-prefix=IR %s
// RUN: %clang_cc1                       -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-use-llvm-names -mllvm -debug-only=polly-ast -o /dev/null %s 2>&1 > /dev/null | FileCheck --check-prefix=AST %s
// RUN: %clang_cc1 -flegacy-pass-manager -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-use-llvm-names -o - %s | FileCheck --check-prefix=TRANS %s
// RUN: %clang                           -DMAIN                                   -std=c99            -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable %s -o %t_pragma_pack%exeext
// RUN: %t_pragma_pack%exeext | FileCheck --check-prefix=RESULT %s

void pragma_id_fuse_tile(int n, double A[n], double B[n]) {
  #pragma clang loop(k, fused) tile sizes(8, 8)
  #pragma clang loop(i, j) fuse fused_id(fused)

  #pragma clang loop id(k)
  for (int k = 0; k < n; k += 1) {
    #pragma clang loop id(i)
    for (int i = 0; i < n; i += 1) 
        A[i] = 3 * i + k;
    #pragma clang loop id(j)
    for (int j = 0; j < n; j += 1) 
        B[j] = 2 * j + k;
  }
}


#ifdef MAIN
#include <stdio.h>
#include <string.h>
int main() {
  double A[256], B[256];
  memset(A, 0, sizeof(A));
  memset(B, 0, sizeof(B));
  pragma_id_fuse_tile(256,A,B);
  printf("(%0.0f %0.0f)\n", A[1], B[1]);
  return 0;
}
#endif


// PRINT-LABEL: void pragma_id_fuse_tile(int n, double A[n], double B[n]) {
// PRINT-NEXT:  #pragma clang loop(k, fused) tile sizes(8, 8)
// PRINT-NEXT:  #pragma clang loop(i, j) fuse fused_id(fused)
// PRINT-NEXT:    #pragma clang loop id(k)
// PRINT-NEXT:    for (int k = 0; k < n; k += 1) {
// PRINT-NEXT:      #pragma clang loop id(i)
// PRINT-NEXT:      for (int i = 0; i < n; i += 1) 
// PRINT-NEXT:          A[i] = 3 * i + k;
// PRINT-NEXT:      #pragma clang loop id(j)
// PRINT-NEXT:      for (int j = 0; j < n; j += 1) 
// PRINT-NEXT:          B[j] = 2 * j + k;
// PRINT-NEXT:    }
// PRINT-NEXT:  }


// IR-LABEL: @pragma_id_fuse_tile(
// IR:         br label %for.cond1, !llvm.loop !2
// IR:         br label %for.cond5, !llvm.loop !12
// IR:         br label %for.cond, !llvm.loop !14
//
// IR: !2 = distinct !{!2, !3, !4, !5, !6, !8}
// IR: !3 = !{!"llvm.loop.disable_nonforced"}
// IR: !4 = !{!"llvm.loop.id", !"i"}
// IR: !5 = !{!"llvm.loop.fuse.enable", i1 true}
// IR: !6 = !{!"llvm.loop.fuse.fuse_group", !7}
// IR: !7 = distinct !{!"Loop Fuse Group"}
// IR: !8 = !{!"llvm.loop.fuse.followup_fused", !9}
// IR: !9 = distinct !{!9, !3, !10, !11}
// IR: !10 = !{!"llvm.loop.id", !"fused"}
// IR: !11 = !{!"llvm.loop.tile.size", i32 8}
// IR: !12 = distinct !{!12, !3, !13, !5, !6, !8}
// IR: !13 = !{!"llvm.loop.id", !"j"}
// IR: !14 = distinct !{!14, !3, !15, !16, !17, !11}
// IR: !15 = !{!"llvm.loop.id", !"k"}
// IR: !16 = !{!"llvm.loop.tile.enable", i1 true}
// IR: !17 = !{!"llvm.loop.tile.depth", i32 2}


// AST: if (1 
// AST:     for (int c0 = 0; c0 <= floord(n - 1, 8); c0 += 1) {
// AST:       for (int c1 = 0; c1 <= (n - 1) / 8; c1 += 1) {
// AST:         for (int c2 = 0; c2 <= min(7, n - 8 * c0 - 1); c2 += 1) {
// AST:           for (int c3 = 0; c3 <= min(7, n - 8 * c1 - 1); c3 += 1) {
// AST:              Stmt_for_body4(8 * c0 + c2, 8 * c1 + c3);
// AST:   else
// AST:     {  /* original code */ }


// TRANS: polly.start:


// RESULT: (258 257)
