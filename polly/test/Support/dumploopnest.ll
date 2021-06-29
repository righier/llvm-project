; Legacy pass manager
; RUN: opt %loadPolly -enable-new-pm=0 -O3 -polly -polly-position=early -polly-dump-loopnest-file=%t-legacy-before-early.json --disable-output < %s && FileCheck --input-file=%t-legacy-before-early.json --check-prefix=JSON %s
;
; New pass manager
; RUN: opt %loadPolly -enable-new-pm=1 -O3 -polly -polly-position=early -polly-dump-loopnest-file=%t-npm-before-early.json    --disable-output < %s && FileCheck --input-file=%t-npm-before-early.json    --check-prefix=JSON %s
;
; void func(double A[], double B[], double C[][1024]) {
;   for (int i = 0; i < 1024; ++i) {
;     A[i] = i;
;     for (int j = 0; j < 1024; ++j) 
;       C[i][j] = i + j;
;   }
;   for (int i = 0; i < 1024; ++i) 
;     B[i] = i;
; }
;
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define dso_local void @func(double* nocapture %A, double* nocapture %B, [1024 x double]* nocapture %C) local_unnamed_addr #0 !dbg !8 {
entry:
  call void @llvm.dbg.value(metadata double* %A, metadata !18, metadata !DIExpression()), !dbg !30
  call void @llvm.dbg.value(metadata double* %B, metadata !19, metadata !DIExpression()), !dbg !30
  call void @llvm.dbg.value(metadata [1024 x double]* %C, metadata !20, metadata !DIExpression()), !dbg !30
  call void @llvm.dbg.value(metadata i32 0, metadata !21, metadata !DIExpression()), !dbg !31
  br label %for.body, !dbg !32

for.body:
  %indvars.iv47 = phi i64 [ 0, %entry ], [ %indvars.iv.next48, %for.cond.cleanup4 ]
  call void @llvm.dbg.value(metadata i64 %indvars.iv47, metadata !21, metadata !DIExpression()), !dbg !31
  %0 = trunc i64 %indvars.iv47 to i32, !dbg !33
  %conv = sitofp i32 %0 to double, !dbg !33
  %arrayidx = getelementptr inbounds double, double* %A, i64 %indvars.iv47, !dbg !34
  store double %conv, double* %arrayidx, align 8, !dbg !35, !tbaa !36
  call void @llvm.dbg.value(metadata i32 0, metadata !24, metadata !DIExpression()), !dbg !40
  br label %for.body5, !dbg !41

for.cond.cleanup4:
  %indvars.iv.next48 = add nuw nsw i64 %indvars.iv47, 1, !dbg !42
  call void @llvm.dbg.value(metadata i64 %indvars.iv.next48, metadata !21, metadata !DIExpression()), !dbg !31
  %exitcond49.not = icmp eq i64 %indvars.iv.next48, 1024, !dbg !43
  br i1 %exitcond49.not, label %for.body19, label %for.body, !dbg !32, !llvm.loop !44

for.body5:
  %indvars.iv43 = phi i64 [ 0, %for.body ], [ %indvars.iv.next44, %for.body5 ]
  call void @llvm.dbg.value(metadata i64 %indvars.iv43, metadata !24, metadata !DIExpression()), !dbg !40
  %1 = add nuw nsw i64 %indvars.iv43, %indvars.iv47, !dbg !47
  %2 = trunc i64 %1 to i32, !dbg !49
  %conv6 = sitofp i32 %2 to double, !dbg !49
  %arrayidx10 = getelementptr inbounds [1024 x double], [1024 x double]* %C, i64 %indvars.iv47, i64 %indvars.iv43, !dbg !50
  store double %conv6, double* %arrayidx10, align 8, !dbg !51, !tbaa !36
  %indvars.iv.next44 = add nuw nsw i64 %indvars.iv43, 1, !dbg !52
  call void @llvm.dbg.value(metadata i64 %indvars.iv.next44, metadata !24, metadata !DIExpression()), !dbg !40
  %exitcond46.not = icmp eq i64 %indvars.iv.next44, 1024, !dbg !53
  br i1 %exitcond46.not, label %for.cond.cleanup4, label %for.body5, !dbg !41, !llvm.loop !54

for.cond.cleanup18:
  ret void, !dbg !56

for.body19:
  %indvars.iv = phi i64 [ %indvars.iv.next, %for.body19 ], [ 0, %for.cond.cleanup4 ]
  call void @llvm.dbg.value(metadata i64 %indvars.iv, metadata !28, metadata !DIExpression()), !dbg !57
  %3 = trunc i64 %indvars.iv to i32, !dbg !58
  %conv20 = sitofp i32 %3 to double, !dbg !58
  %arrayidx22 = getelementptr inbounds double, double* %B, i64 %indvars.iv, !dbg !60
  store double %conv20, double* %arrayidx22, align 8, !dbg !61, !tbaa !36
  %indvars.iv.next = add nuw nsw i64 %indvars.iv, 1, !dbg !62
  call void @llvm.dbg.value(metadata i64 %indvars.iv.next, metadata !28, metadata !DIExpression()), !dbg !57
  %exitcond.not = icmp eq i64 %indvars.iv.next, 1024, !dbg !63
  br i1 %exitcond.not, label %for.cond.cleanup18, label %for.body19, !dbg !64, !llvm.loop !65
}

declare void @llvm.dbg.value(metadata, metadata, metadata) #1

attributes #0 = { nofree norecurse nosync nounwind uwtable writeonly "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nofree nosync nounwind readnone speculatable willreturn mustprogress }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4, !5, !6}
!llvm.ident = !{!7}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "clang version 13.0.0 (git@github.com:SOLLVE/llvm-project.git 537992dab92c60bc47e99238b186fb4100f279dd)", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "loopnest.c", directory: "/home/meinersbur/src/llvm-project/polly/test/Support")
!2 = !{}
!3 = !{i32 7, !"Dwarf Version", i32 4}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = !{i32 1, !"wchar_size", i32 4}
!6 = !{i32 7, !"uwtable", i32 1}
!7 = !{!"clang version 13.0.0 (git@github.com:SOLLVE/llvm-project.git 537992dab92c60bc47e99238b186fb4100f279dd)"}
!8 = distinct !DISubprogram(name: "func", scope: !1, file: !1, line: 1, type: !9, scopeLine: 1, flags: DIFlagPrototyped | DIFlagAllCallsDescribed, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !17)
!9 = !DISubroutineType(types: !10)
!10 = !{null, !11, !11, !13}
!11 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !12, size: 64)
!12 = !DIBasicType(name: "double", size: 64, encoding: DW_ATE_float)
!13 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !14, size: 64)
!14 = !DICompositeType(tag: DW_TAG_array_type, baseType: !12, size: 65536, elements: !15)
!15 = !{!16}
!16 = !DISubrange(count: 1024)
!17 = !{!18, !19, !20, !21, !24, !28}
!18 = !DILocalVariable(name: "A", arg: 1, scope: !8, file: !1, line: 1, type: !11)
!19 = !DILocalVariable(name: "B", arg: 2, scope: !8, file: !1, line: 1, type: !11)
!20 = !DILocalVariable(name: "C", arg: 3, scope: !8, file: !1, line: 1, type: !13)
!21 = !DILocalVariable(name: "i", scope: !22, file: !1, line: 2, type: !23)
!22 = distinct !DILexicalBlock(scope: !8, file: !1, line: 2, column: 3)
!23 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!24 = !DILocalVariable(name: "j", scope: !25, file: !1, line: 4, type: !23)
!25 = distinct !DILexicalBlock(scope: !26, file: !1, line: 4, column: 5)
!26 = distinct !DILexicalBlock(scope: !27, file: !1, line: 2, column: 34)
!27 = distinct !DILexicalBlock(scope: !22, file: !1, line: 2, column: 3)
!28 = !DILocalVariable(name: "i", scope: !29, file: !1, line: 7, type: !23)
!29 = distinct !DILexicalBlock(scope: !8, file: !1, line: 7, column: 3)
!30 = !DILocation(line: 0, scope: !8)
!31 = !DILocation(line: 0, scope: !22)
!32 = !DILocation(line: 2, column: 3, scope: !22)
!33 = !DILocation(line: 3, column: 12, scope: !26)
!34 = !DILocation(line: 3, column: 5, scope: !26)
!35 = !DILocation(line: 3, column: 10, scope: !26)
!36 = !{!37, !37, i64 0}
!37 = !{!"double", !38, i64 0}
!38 = !{!"omnipotent char", !39, i64 0}
!39 = !{!"Simple C/C++ TBAA"}
!40 = !DILocation(line: 0, scope: !25)
!41 = !DILocation(line: 4, column: 5, scope: !25)
!42 = !DILocation(line: 2, column: 29, scope: !27)
!43 = !DILocation(line: 2, column: 21, scope: !27)
!44 = distinct !{!44, !32, !45, !46}
!45 = !DILocation(line: 6, column: 3, scope: !22)
!46 = !{!"llvm.loop.mustprogress"}
!47 = !DILocation(line: 5, column: 19, scope: !48)
!48 = distinct !DILexicalBlock(scope: !25, file: !1, line: 4, column: 5)
!49 = !DILocation(line: 5, column: 17, scope: !48)
!50 = !DILocation(line: 5, column: 7, scope: !48)
!51 = !DILocation(line: 5, column: 15, scope: !48)
!52 = !DILocation(line: 4, column: 31, scope: !48)
!53 = !DILocation(line: 4, column: 23, scope: !48)
!54 = distinct !{!54, !41, !55, !46}
!55 = !DILocation(line: 5, column: 21, scope: !25)
!56 = !DILocation(line: 9, column: 1, scope: !8)
!57 = !DILocation(line: 0, scope: !29)
!58 = !DILocation(line: 8, column: 12, scope: !59)
!59 = distinct !DILexicalBlock(scope: !29, file: !1, line: 7, column: 3)
!60 = !DILocation(line: 8, column: 5, scope: !59)
!61 = !DILocation(line: 8, column: 10, scope: !59)
!62 = !DILocation(line: 7, column: 29, scope: !59)
!63 = !DILocation(line: 7, column: 21, scope: !59)
!64 = !DILocation(line: 7, column: 3, scope: !29)
!65 = distinct !{!65, !64, !66, !46}
!66 = !DILocation(line: 8, column: 12, scope: !29)


; JSON: {
; JSON:    "scops": [
; JSON:       {
; JSON:          "children": [
; JSON:             {
; JSON:                "children": [
; JSON:                   {
; JSON:                      "column": 12,
; JSON:                      "filename": "loopnest.c",
; JSON:                      "function": "func",
; JSON:                      "kind": "stmt",
; JSON:                      "line": 3,
; JSON:                      "path": "{{.*}}loopnest.c"
; JSON:                   },
; JSON:                   {
; JSON:                      "children": [
; JSON:                         {
; JSON:                            "column": 17,
; JSON:                            "filename": "loopnest.c",
; JSON:                            "function": "func",
; JSON:                            "kind": "stmt",
; JSON:                            "line": 5,
; JSON:                            "path": "{{.*}}loopnest.c"
; JSON:                         }
; JSON:                      ],
; JSON:                      "column": 5,
; JSON:                      "filename": "loopnest.c",
; JSON:                      "function": "func",
; JSON:                      "kind": "loop",
; JSON:                      "line": 4,
; JSON:                      "path": "{{.*}}loopnest.c",
; JSON:                      "perfectnest": false
; JSON:                   }
; JSON:                ],
; JSON:                "column": 3,
; JSON:                "filename": "loopnest.c",
; JSON:                "function": "func",
; JSON:                "kind": "loop",
; JSON:                "line": 2,
; JSON:                "path": "{{.*}}loopnest.c",
; JSON:                "perfectnest": false
; JSON:             },
; JSON:             {
; JSON:                "children": [
; JSON:                   {
; JSON:                      "column": 12,
; JSON:                      "filename": "loopnest.c",
; JSON:                      "function": "func",
; JSON:                      "kind": "stmt",
; JSON:                      "line": 8,
; JSON:                      "path": "{{.*}}loopnest.c"
; JSON:                   }
; JSON:                ],
; JSON:                "column": 3,
; JSON:                "filename": "loopnest.c",
; JSON:                "function": "func",
; JSON:                "kind": "loop",
; JSON:                "line": 7,
; JSON:                "path": "{{.*}}loopnest.c",
; JSON:                "perfectnest": false
; JSON:             }
; JSON:          ],
; JSON:          "function": "func",
; JSON:          "kind": "scop"
; JSON:       }
; JSON:    ]
; JSON: }
