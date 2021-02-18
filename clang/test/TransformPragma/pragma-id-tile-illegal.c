// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -ast-print %s | FileCheck --check-prefix=PRINT --match-full-lines %s
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -debug-info-kind=limited -gno-column-info -disable-llvm-passes -o - %s | FileCheck --check-prefix=IR --match-full-lines %s
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -O3 -emit-llvm -debug-info-kind=limited -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -o /dev/null %s 2>&1 > /dev/null | FileCheck %s --check-prefix=WARN
// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -O3 -emit-llvm -mllvm -polly -mllvm -polly-position=early -mllvm -polly-process-unprofitable -o /dev/null -mllvm -debug-only=polly-ast %s 2>&1 > /dev/null | FileCheck %s --check-prefix=AST

void pragma_id_tile(int m, int n, double C[m][n]) {
#pragma clang loop(i, j) tile sizes(16, 32)
#pragma clang loop id(i)
  for (int i = 0; i < m; i += 1)
#pragma clang loop id(j)
    for (int j = 0; j < n; j += 1)
      C[i][j] = C[j*2][i];
}


// PRINT-LABEL: void pragma_id_tile(int m, int n, double C[m][n]) {
// PRINT-NEXT:  #pragma clang loop(i, j) tile sizes(16, 32)
// PRINT-NEXT:  #pragma clang loop id(i)
// PRINT-NEXT:  	for (int i = 0; i < m; i += 1)
// PRINT-NEXT:  #pragma clang loop id(j)
// PRINT-NEXT:  		for (int j = 0; j < n; j += 1)
// PRINT-NEXT:  			C[i][j] = C[j * 2][i];
// PRINT-NEXT:  }


// IR-LABEL: define dso_local void @pragma_id_tile(i32 %m, i32 %n, double* %C) #0 !dbg !6 {
// IR:         br label %for.cond1, !dbg !28, !llvm.loop !31
// IR:         br label %for.cond, !dbg !23, !llvm.loop !36
//
// IR: !6 = distinct !DISubprogram(name: "pragma_id_tile", scope: !7, file: !7, line: 6, type: !8, scopeLine: 6, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !2)
// IR: !21 = distinct !DILexicalBlock(scope: !6, file: !7, line: 9)
// IR: !22 = !DILocation(line: 9, scope: !21)
// IR: !24 = distinct !DILexicalBlock(scope: !21, file: !7, line: 9)
// IR: !26 = distinct !DILexicalBlock(scope: !24, file: !7, line: 11)
// IR: !27 = !DILocation(line: 11, scope: !26)
// IR: !31 = distinct !{!31, !27, !32, !33, !34, !35}
// IR: !32 = !DILocation(line: 12, scope: !26)
// IR: !33 = !{!"llvm.loop.disable_nonforced"}
// IR: !34 = !{!"llvm.loop.id", !"j"}
// IR: !35 = !{!"llvm.loop.tile.size", i32 32}
// IR: !36 = distinct !{!36, !22, !37, !33, !38, !39, !40, !41, !43}
// IR: !37 = !DILocation(line: 12, scope: !21)
// IR: !38 = !{!"llvm.loop.id", !"i"}
// IR: !39 = !{!"llvm.loop.tile.enable", i1 true}
// IR: !40 = !{!"llvm.loop.tile.depth", i32 2}
// IR: !41 = !{!"llvm.loop.tile.loc", !42, !42}
// IR: !42 = !DILocation(line: 7, scope: !21)
// IR: !43 = !{!"llvm.loop.tile.size", i32 16}


// WARN: pragma-id-tile-illegal.c:7:26: warning: not applying loop tiling: cannot ensure semantic equivalence due to possible dependency violations [-Wpass-failed=polly-opt-manual]
// WARN: #pragma clang loop(i, j) tile sizes(16, 32)
// WARN: ^
// WARN: 1 warning generated.


// AST: if (
// AST:   for (int c0 = 0; c0 < p_0; c0 += 1) {
// AST: 	for (int c1 = 0; c1 < p_1; c1 += 1)
// AST: 		Stmt4(c0, c1);
// AST:   }
// AST: else
// AST:   {  /* original code */ }
