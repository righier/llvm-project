// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc19.0.24215 -std=c99 -emit-llvm -debug-info-kind=limited -gno-column-info -disable-llvm-passes -o - %s | FileCheck --check-prefix=IR --match-full-lines %s

void pragma_id_reverse(int n, double A[n]) {
#pragma clang loop(myloop) reverse
#pragma clang loop id(myloop)
  for (int i = n - 1; i >= 0; i--)
    A[i] = A[i] + 1;
}


// IR-LABEL: define dso_local void @pragma_id_reverse(i32 %n, double* %A) #0 !dbg !6 {
// IR:         br label %for.cond, !dbg !19, !llvm.loop !22
//
// IR: !6 = distinct !DISubprogram(name: "pragma_id_reverse", scope: !7, file: !7, line: 3, type: !8, scopeLine: 3, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !2)
// IR: !17 = distinct !DILexicalBlock(scope: !6, file: !7, line: 6)
// IR: !18 = !DILocation(line: 6, scope: !17)
// IR: !22 = distinct !{!22, !18, !23, !24, !25, !26, !27}
// IR: !23 = !DILocation(line: 7, scope: !17)
// IR: !24 = !{!"llvm.loop.disable_nonforced"}
// IR: !25 = !{!"llvm.loop.id", !"myloop"}
// IR: !26 = !{!"llvm.loop.reverse.enable", i1 true}
// IR: !27 = !{!"llvm.loop.reverse.loc", !28, !28}
// IR: !28 = !DILocation(line: 4, scope: !17)
