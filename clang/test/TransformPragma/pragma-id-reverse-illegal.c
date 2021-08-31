// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -ast-print %s | FileCheck --check-prefix=PRINT --match-full-lines %s
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -debug-info-kind=limited -gno-column-info -disable-llvm-passes -o - %s | FileCheck --check-prefix=IR --match-full-lines %s
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -O3 -emit-llvm -debug-info-kind=limited -gno-column-info -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -o /dev/null %s 2>&1 > /dev/null | FileCheck %s --check-prefix=WARN
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -O3 -emit-llvm -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -o /dev/null -mllvm -debug-only=polly-ast %s 2>&1 > /dev/null | FileCheck %s --check-prefix=AST

void pragma_id_reverse(double *A, int N) {
#pragma clang loop(myloop) reverse
#pragma clang loop id(myloop)
  for (int i = 1; i < N; i++)
    A[i] = A[i-1] + 1;
}


// PRINT-LABEL: void pragma_id_reverse(double *A, int N) {
// PRINT-NEXT:  #pragma clang loop(myloop) reverse
// PRINT-NEXT:  #pragma clang loop id(myloop)
// PRINT-NEXT:    for (int i = 1; i < N; i++)
// PRINT-NEXT:      A[i] = A[i - 1] + 1;
// PRINT-NEXT:  }


// IR-LABEL: define dso_local void @pragma_id_reverse(double* %A, i32 %N) #0 !dbg !6 {
// IR:         br label %for.cond, !dbg !19, !llvm.loop !22
//
// IR: !6 = distinct !DISubprogram(name: "pragma_id_reverse", scope: !7, file: !7, line: 6, type: !8, scopeLine: 6, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !2)
// IR: !17 = distinct !DILexicalBlock(scope: !6, file: !7, line: 9)
// IR: !18 = !DILocation(line: 9, scope: !17)
// IR: !22 = distinct !{!22, !18, !23, !24, !25, !26, !27}
// IR: !23 = !DILocation(line: 10, scope: !17)
// IR: !24 = !{!"llvm.loop.disable_nonforced"}
// IR: !25 = !{!"llvm.loop.id", !"myloop"}
// IR: !26 = !{!"llvm.loop.reverse.enable", i1 true}
// IR: !27 = !{!"llvm.loop.reverse.loc", !28, !28}
// IR: !28 = !DILocation(line: 7, scope: !17)


// WARN: pragma-id-reverse-illegal.c:7:1: warning: not applying loop reversal: cannot ensure semantic equivalence due to possible dependency violations [-Wpass-failed=polly-opt-manual]
// WARN: #pragma clang loop(myloop) reverse
// WARN: ^
// WARN: 1 warning generated.


// AST: if (1)
// AST:     for (int c0 = 0; c0 < p_0 - 1; c0 += 1)
// AST:       Stmt2(c0);
// AST: else
// AST: {  /* original code */ }
