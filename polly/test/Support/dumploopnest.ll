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
define dso_local void @func(double* %A, double* %B, [1024 x double]* %C) #0 !dbg !9 {
entry:
  call void @llvm.dbg.value(metadata [1024 x double]* %C, metadata !19, metadata !DIExpression()), !dbg !31
  call void @llvm.dbg.value(metadata double* %B, metadata !20, metadata !DIExpression()), !dbg !31
  call void @llvm.dbg.value(metadata double* %A, metadata !21, metadata !DIExpression()), !dbg !31
  call void @llvm.dbg.value(metadata i32 0, metadata !22, metadata !DIExpression()), !dbg !32
  br label %for.body, !dbg !33

for.cond15.preheader:                             ; preds = %for.inc11
  call void @llvm.dbg.value(metadata i32 0, metadata !29, metadata !DIExpression()), !dbg !34
  br label %for.body19, !dbg !35

for.body:                                         ; preds = %entry, %for.inc11
  %i.016 = phi i32 [ 0, %entry ], [ %inc12, %for.inc11 ]
  call void @llvm.dbg.value(metadata i32 %i.016, metadata !22, metadata !DIExpression()), !dbg !32
  %conv = sitofp i32 %i.016 to double, !dbg !36
  %idxprom = sext i32 %i.016 to i64, !dbg !36
  %arrayidx = getelementptr inbounds double, double* %A, i64 %idxprom, !dbg !36
  store double %conv, double* %arrayidx, align 8, !dbg !36, !tbaa !37
  call void @llvm.dbg.value(metadata i32 0, metadata !25, metadata !DIExpression()), !dbg !41
  br label %for.body5, !dbg !42

for.body5:                                        ; preds = %for.body, %for.body5
  %j.015 = phi i32 [ 0, %for.body ], [ %inc, %for.body5 ]
  call void @llvm.dbg.value(metadata i32 %j.015, metadata !25, metadata !DIExpression()), !dbg !41
  %add = add nsw i32 %i.016, %j.015, !dbg !43
  %conv6 = sitofp i32 %add to double, !dbg !43
  %idxprom7 = sext i32 %i.016 to i64, !dbg !43
  %arrayidx8 = getelementptr inbounds [1024 x double], [1024 x double]* %C, i64 %idxprom7, !dbg !43
  %idxprom9 = sext i32 %j.015 to i64, !dbg !43
  %arrayidx10 = getelementptr inbounds [1024 x double], [1024 x double]* %arrayidx8, i64 0, i64 %idxprom9, !dbg !43
  store double %conv6, double* %arrayidx10, align 8, !dbg !43, !tbaa !37
  %inc = add nsw i32 %j.015, 1, !dbg !45
  call void @llvm.dbg.value(metadata i32 %inc, metadata !25, metadata !DIExpression()), !dbg !41
  %cmp2 = icmp slt i32 %inc, 1024, !dbg !42
  br i1 %cmp2, label %for.body5, label %for.inc11, !dbg !42, !llvm.loop !46

for.inc11:                                        ; preds = %for.body5
  %inc12 = add nsw i32 %i.016, 1, !dbg !49
  call void @llvm.dbg.value(metadata i32 %inc12, metadata !22, metadata !DIExpression()), !dbg !32
  %cmp = icmp slt i32 %inc12, 1024, !dbg !33
  br i1 %cmp, label %for.body, label %for.cond15.preheader, !dbg !33, !llvm.loop !50

for.body19:                                       ; preds = %for.cond15.preheader, %for.body19
  %i14.017 = phi i32 [ 0, %for.cond15.preheader ], [ %inc24, %for.body19 ]
  call void @llvm.dbg.value(metadata i32 %i14.017, metadata !29, metadata !DIExpression()), !dbg !34
  %conv20 = sitofp i32 %i14.017 to double, !dbg !52
  %idxprom21 = sext i32 %i14.017 to i64, !dbg !52
  %arrayidx22 = getelementptr inbounds double, double* %B, i64 %idxprom21, !dbg !52
  store double %conv20, double* %arrayidx22, align 8, !dbg !52, !tbaa !37
  %inc24 = add nsw i32 %i14.017, 1, !dbg !54
  call void @llvm.dbg.value(metadata i32 %inc24, metadata !29, metadata !DIExpression()), !dbg !34
  %cmp16 = icmp slt i32 %inc24, 1024, !dbg !35
  br i1 %cmp16, label %for.body19, label %for.end25, !dbg !35, !llvm.loop !55

for.end25:                                        ; preds = %for.body19
  ret void, !dbg !57
}

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.start.p0i8(i64 immarg, i8* nocapture) #2

; Function Attrs: argmemonly nofree nosync nounwind willreturn
declare void @llvm.lifetime.end.p0i8(i64 immarg, i8* nocapture) #2

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare void @llvm.dbg.value(metadata, metadata, metadata) #1

attributes #0 = { nounwind uwtable "frame-pointer"="none" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nofree nosync nounwind readnone speculatable willreturn }
attributes #2 = { argmemonly nofree nosync nounwind willreturn }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4, !5, !6, !7}
!llvm.ident = !{!8}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "clang version 13.0.0 (C:/Users/meinersbur/src/llvm-project/clang 537992dab92c60bc47e99238b186fb4100f279dd)", isOptimized: true, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "loopnest.c", directory: "C:\\Users\\meinersbur\\src\\llvm-project\\polly\\test\\Support", checksumkind: CSK_MD5, checksum: "c3fc640f0fe76125e082c820f90faba0")
!2 = !{}
!3 = !{i32 2, !"CodeView", i32 1}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = !{i32 1, !"wchar_size", i32 2}
!6 = !{i32 7, !"PIC Level", i32 2}
!7 = !{i32 7, !"uwtable", i32 1}
!8 = !{!"clang version 13.0.0 (C:/Users/meinersbur/src/llvm-project/clang 537992dab92c60bc47e99238b186fb4100f279dd)"}
!9 = distinct !DISubprogram(name: "func", scope: !1, file: !1, line: 1, type: !10, scopeLine: 1, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition | DISPFlagOptimized, unit: !0, retainedNodes: !18)
!10 = !DISubroutineType(types: !11)
!11 = !{null, !12, !12, !14}
!12 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !13, size: 64)
!13 = !DIBasicType(name: "double", size: 64, encoding: DW_ATE_float)
!14 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !15, size: 64)
!15 = !DICompositeType(tag: DW_TAG_array_type, baseType: !13, size: 65536, elements: !16)
!16 = !{!17}
!17 = !DISubrange(count: 1024)
!18 = !{!19, !20, !21, !22, !25, !29}
!19 = !DILocalVariable(name: "C", arg: 3, scope: !9, file: !1, line: 1, type: !14)
!20 = !DILocalVariable(name: "B", arg: 2, scope: !9, file: !1, line: 1, type: !12)
!21 = !DILocalVariable(name: "A", arg: 1, scope: !9, file: !1, line: 1, type: !12)
!22 = !DILocalVariable(name: "i", scope: !23, file: !1, line: 2, type: !24)
!23 = distinct !DILexicalBlock(scope: !9, file: !1, line: 2)
!24 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!25 = !DILocalVariable(name: "j", scope: !26, file: !1, line: 4, type: !24)
!26 = distinct !DILexicalBlock(scope: !27, file: !1, line: 4)
!27 = distinct !DILexicalBlock(scope: !28, file: !1, line: 2)
!28 = distinct !DILexicalBlock(scope: !23, file: !1, line: 2)
!29 = !DILocalVariable(name: "i", scope: !30, file: !1, line: 7, type: !24)
!30 = distinct !DILexicalBlock(scope: !9, file: !1, line: 7)
!31 = !DILocation(line: 0, scope: !9)
!32 = !DILocation(line: 0, scope: !23)
!33 = !DILocation(line: 2, scope: !23)
!34 = !DILocation(line: 0, scope: !30)
!35 = !DILocation(line: 7, scope: !30)
!36 = !DILocation(line: 3, scope: !27)
!37 = !{!38, !38, i64 0}
!38 = !{!"double", !39, i64 0}
!39 = !{!"omnipotent char", !40, i64 0}
!40 = !{!"Simple C/C++ TBAA"}
!41 = !DILocation(line: 0, scope: !26)
!42 = !DILocation(line: 4, scope: !26)
!43 = !DILocation(line: 5, scope: !44)
!44 = distinct !DILexicalBlock(scope: !26, file: !1, line: 4)
!45 = !DILocation(line: 4, scope: !44)
!46 = distinct !{!46, !42, !47, !48}
!47 = !DILocation(line: 5, scope: !26)
!48 = !{!"llvm.loop.mustprogress"}
!49 = !DILocation(line: 2, scope: !28)
!50 = distinct !{!50, !33, !51, !48}
!51 = !DILocation(line: 6, scope: !23)
!52 = !DILocation(line: 8, scope: !53)
!53 = distinct !DILexicalBlock(scope: !30, file: !1, line: 7)
!54 = !DILocation(line: 7, scope: !53)
!55 = distinct !{!55, !35, !56, !48}
!56 = !DILocation(line: 8, scope: !30)
!57 = !DILocation(line: 9, scope: !9)
