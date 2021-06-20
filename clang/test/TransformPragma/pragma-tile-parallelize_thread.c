// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -ast-print %s | FileCheck --check-prefix=PRINT --match-full-lines %s
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -disable-llvm-passes -o - %s | FileCheck --check-prefix=IR %s
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-use-llvm-names -mllvm -debug-only=polly-ast -o /dev/null %s 2>&1 > /dev/null | FileCheck --check-prefix=AST %s
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -fno-unroll-loops -mllvm -polly-use-llvm-names -o - %s | FileCheck --check-prefix=TRANS %s
// RUN: %clang -DMAIN -std=c99 -O3 -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable %s -lgomp -o %t_pragma_pack%exeext
// RUN: %t_pragma_pack%exeext | FileCheck --check-prefix=RESULT %s

__attribute__((noinline))
void pragma_tile_parallelize_thread(double C[const restrict static 256], double A[const restrict static 256]) {
  #pragma clang loop(outer) parallelize_thread
  #pragma clang loop tile sizes(32) floor_ids(outer)
  for (int i = 0; i < 256; i += 1)
    C[i] = A[i] + i;
}

#ifdef MAIN
#include <stdio.h>
#include <string.h>
int main() {
  double C[256];
  double A[256];
  memset(A, 0, sizeof(A));
  A[1] = 42;
  pragma_tile_parallelize_thread(C,A);
  printf("(%0.0f)\n", C[1]);
  return 0;
}
#endif


// PRINT-LABEL: void pragma_tile_parallelize_thread(double C[const restrict static 256], double A[const restrict static 256]) __attribute__((noinline)) {
// PRINT-NEXT: #pragma clang loop(outer) parallelize_thread
// PRINT-NEXT: #pragma clang loop tile sizes(32) floor_ids(outer)
// PRINT-NEXT:    for (int i = 0; i < 256; i += 1)
// PRINT-NEXT:      C[i] = A[i] + i;
// PRINT-NEXT: }


// IR-LABEL: @pragma_tile_parallelize_thread(
// IR:         br label %for.cond, !llvm.loop !2
//
// IR: !2 = distinct !{!2, !3, !4, !5, !6, !7}
// IR: !3 = !{!"llvm.loop.disable_nonforced"}
// IR: !4 = !{!"llvm.loop.tile.enable", i1 true}
// IR: !5 = !{!"llvm.loop.tile.depth", i32 1}
// IR: !6 = !{!"llvm.loop.tile.size", i32 32}
// IR: !7 = !{!"llvm.loop.tile.followup_floor", !8}
// IR: !8 = distinct !{!8, !3, !9, !10}
// IR: !9 = !{!"llvm.loop.id", !"outer"}
// IR: !10 = !{!"llvm.loop.parallelize_thread.enable", i1 true}


// AST: if (1)
// AST:   #pragma omp parallel for
// AST:   for (int c0 = 0; c0 <= 7; c0 += 1)
// AST:     for (int c1 = 0; c1 <= 31; c1 += 1)
// AST:       Stmt_for_body(32 * c0 + c1);
// AST: else
// AST:   {  /* original code */ }


// TRANS-LABEL: @pragma_tile_parallelize_thread(
// TRANS:       call void @GOMP_parallel_loop_runtime_start({{.*}} @pragma_tile_parallelize_thread_polly_subfn, {{.*}})


// RESULT: (43)
