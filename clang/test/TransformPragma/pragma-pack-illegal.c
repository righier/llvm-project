// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -ast-print %s | FileCheck --check-prefix=PRINT --match-full-lines %s
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -debug-info-kind=limited -gno-column-info -disable-llvm-passes -o - %s | FileCheck --check-prefix=IR --match-full-lines %s
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -fno-unroll-loops -O3 -emit-llvm -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-allow-nonaffine -mllvm -polly-use-llvm-names -debug-info-kind=limited -o /dev/null %s 2>&1 > /dev/null | FileCheck %s --check-prefix=WARN
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -fno-unroll-loops -O3 -emit-llvm -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -mllvm -polly-allow-nonaffine -mllvm -polly-use-llvm-names -o /dev/null -mllvm -debug-only=polly-ast %s 2>&1 > /dev/null | FileCheck %s --check-prefix=AST


void pragma_pack(double C[restrict 16][32], double A[restrict 16*32][16]) {
  for (int i = 0; i < 16; i += 1)
    #pragma clang loop pack array(A)
    for (int j = 0; j < 32; j += 1)
      C[i][j] = A[j*i][i] + i + j;
}



// PRINT-LABEL: void pragma_pack(double C[restrict 16][32], double A[restrict 512][16]) {
// PRINT-NEXT:     for (int i = 0; i < 16; i += 1)
// PRINT-NEXT:  #pragma clang loop pack array(A)
// PRINT-NEXT:         for (int j = 0; j < 32; j += 1)
// PRINT-NEXT:             C[i][j] = A[j * i][i] + i + j;
// PRINT-NEXT: }


// IR-LABEL: define dso_local void @pragma_pack([32 x double]* noalias %C, [16 x double]* noalias %A) #0 !dbg !6 {
// IR:         %A.addr = alloca [16 x double]*, align 8
// IR:         %6 = load double, double* %arrayidx5, align 8, !dbg !35, !llvm.access !36
// IR:         br label %for.cond1, !dbg !33, !llvm.loop !37
// IR:         br label %for.cond, !dbg !28, !llvm.loop !45
//
// IR: !6 = distinct !DISubprogram(name: "pragma_pack", scope: !7, file: !7, line: 7, type: !8, scopeLine: 7, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !2)
// IR: !25 = distinct !DILexicalBlock(scope: !6, file: !7, line: 8)
// IR: !29 = distinct !DILexicalBlock(scope: !25, file: !7, line: 8)
// IR: !31 = distinct !DILexicalBlock(scope: !29, file: !7, line: 10)
// IR: !32 = !DILocation(line: 10, scope: !31)
// IR: !36 = distinct !{!"A"}
// IR: !37 = distinct !{!37, !32, !38, !39, !40, !41, !42, !43}
// IR: !38 = !DILocation(line: 11, scope: !31)
// IR: !39 = !{!"llvm.loop.disable_nonforced"}
// IR: !40 = !{!"llvm.data.pack.enable", i1 true}
// IR: !41 = !{!"llvm.data.pack.array", !36}
// IR: !42 = !{!"llvm.data.pack.allocate", !"alloca"}
// IR: !43 = !{!"llvm.data.pack.loc", !44, !44}
// IR: !44 = !DILocation(line: 9, scope: !31)


// WARN: pragma-pack-illegal.c:9:24: warning: array not packed: All array accesses must be affine [-Wpass-failed=polly-opt-manual]
// WARN: #pragma clang loop pack array(A)
// WARN: ^
// WARN: 1 warning generated.


// AST: if (1)
// AST:   for (int c0 = 0; c0 <= 15; c0 += 1) {
// AST:     for (int c1 = 0; c1 <= 31; c1 += 1)
// AST:       Stmt_for_body4(c0, c1);
// AST:   }
// AST: else
// AST:   {  /* original code */ }
