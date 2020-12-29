// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -ast-print %s | FileCheck --check-prefix=PRINT --match-full-lines %s
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -debug-info-kind=limited -gno-column-info -disable-llvm-passes -o - %s | FileCheck --check-prefix=IR --match-full-lines %s
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -O3 -emit-llvm -debug-info-kind=limited -gno-column-info -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -o /dev/null %s 2>&1 > /dev/null | FileCheck %s --check-prefix=WARN


void pragma_parallelize_thread(double *A, int N) {
#pragma clang loop parallelize_thread
  for (int i = 1; i < N; i++)
    A[i] = A[i-1] + 1;
}


// PRINT-LABEL: void pragma_parallelize_thread(double *A, int N) {
// PRINT-NEXT:  #pragma clang loop parallelize_thread
// PRINT-NEXT:    for (int i = 1; i < N; i++)
// PRINT-NEXT:      A[i] = A[i - 1] + 1;
// PRINT-NEXT:  }


// IR-LABEL: define dso_local void @pragma_parallelize_thread(double* %A, i32 %N) #0 !dbg !6 {
// IR:         br label %for.cond, !dbg !19, !llvm.loop !22
//
// IR: !6 = distinct !DISubprogram(name: "pragma_parallelize_thread", scope: !7, file: !7, line: 6, type: !8, scopeLine: 6, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !2)
// IR: !17 = distinct !DILexicalBlock(scope: !6, file: !7, line: 8)
// IR: !18 = !DILocation(line: 8, scope: !17)
// IR: !22 = distinct !{!22, !18, !23, !24, !25, !26}
// IR: !23 = !DILocation(line: 9, scope: !17)
// IR: !24 = !{!"llvm.loop.disable_nonforced"}
// IR: !25 = !{!"llvm.loop.parallelize_thread.enable", i1 true}
// IR: !26 = !{!"llvm.loop.parallelize_thread.loc", !27, !27}
// IR: !27 = !DILocation(line: 7, scope: !17)


// WARN: pragma-parallelize_thread-illegal.c:7:1: warning: loop not thread-parallelized: transformation would violate dependencies
// WARN: #pragma clang loop parallelize_thread
// WARN: ^
// WARN: 1 warning generated.
