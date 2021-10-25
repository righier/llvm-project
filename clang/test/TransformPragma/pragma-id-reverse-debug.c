// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -debug-info-kind=limited -gno-column-info -disable-llvm-passes -o - %s | FileCheck --check-prefix=IR %s

void pragma_id_reverse(int n, double A[n]) {
#pragma clang loop(myloop) reverse
#pragma clang loop id(myloop)
  for (int i = n - 1; i >= 0; i--)
    A[i] = A[i] + 1;
}


// IR-LABEL: @pragma_id_reverse(
// IR:         br label %for.cond, !dbg !19, !llvm.loop !22
//
// IR: !5 = distinct !DISubprogram(name: "pragma_id_reverse", scope: !6, file: !6, line: 3, type: !7, scopeLine: 3, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !12)
// IR: !17 = distinct !DILexicalBlock(scope: !5, file: !6, line: 6)
// IR: !18 = !DILocation(line: 6, scope: !17)
// IR: !22 = distinct !{!22, !18, !23, !24, !25, !26, !27}
// IR: !23 = !DILocation(line: 7, scope: !17)
// IR: !24 = !{!"llvm.loop.disable_nonforced"}
// IR: !25 = !{!"llvm.loop.id", !"myloop"}
// IR: !26 = !{!"llvm.loop.reverse.enable", i1 true}
// IR: !27 = !{!"llvm.loop.reverse.loc", !28, !28}
// IR: !28 = !DILocation(line: 4, scope: !17)

