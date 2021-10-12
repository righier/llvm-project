; RUN: opt %loadPolly -polly-opt-isl -polly-reschedule=0 -polly-postopts=0 -analyze < %s | FileCheck %s -match-full-lines --check-prefix=OFF
; RUN: opt %loadPolly -polly-opt-isl -polly-reschedule=1 -polly-postopts=0 -analyze < %s | FileCheck %s -match-full-lines --check-prefix=RESCHEDULE
; RUN: opt %loadPolly -polly-opt-isl -polly-reschedule=0 -polly-postopts=1 -analyze < %s | FileCheck %s -match-full-lines --check-prefix=POSTOPTS
;
; for (int j = 0; j < n; j += 1) {
; bodyA:
;   double val = j;
;
;   for (int i = 0; i < n; i += 1) {
; bodyB:
;     A[0] = val;
;   }
; }
;
; XFAIL: *
;
define void @func(i32 %n, double* noalias nonnull %A) {
entry:
  br label %for

for:
  %j = phi i32 [0, %entry], [%j.inc, %inc]
  %j.cmp = icmp slt i32 %j, %n
  br i1 %j.cmp, label %bodyA, label %exit

    bodyA:
      br label %inner.for


    inner.for:
      %i = phi i32 [0, %bodyA], [%i.inc, %inner.inc]
      %i.cmp = icmp slt i32 %i, %n
      br i1 %i.cmp, label %bodyB, label %inner.exit


        bodyB:
          %mul = mul nsw i32 %j, %n
          %idx = add nsw i32 %mul, %i
          %arrayidx = getelementptr double, double* %A, i32 %idx
          store double 0.0, double* %arrayidx
          br label %inner.inc


    inner.inc:
      %i.inc = add nuw nsw i32 %i, 1
      br label %inner.for

    inner.exit:
      br label %inc

inc:
  %j.inc = add nuw nsw i32 %j, 1
  br label %for

exit:
  br label %return

return:
  ret void
}


; OFF:      Calculated schedule:
; OFF-NEXT: n/a

