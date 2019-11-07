// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++11 -fexperimental-transform-pragma -emit-llvm -o - %s | FileCheck %s

extern "C" void pragma_transform_unroll_full(int *List, int Length) {
#pragma clang transform unroll full
  for (int i = 0; i < 4; i++) {
//  CHECK: br label %{{.*}}, !llvm.loop ![[LOOP:[0-9]+]]
    List[i] = i * 2;
  }
}


// CHECK-DAG: ![[LOOP]] = distinct !{![[LOOP]], ![[UNROLL_ENABLE:[0-9]+]], ![[UNROLL_FULL:[0-9]+]], ![[DISABLE_NONFORCED:[0-9]+]]}
// CHECK-DAG: ![[UNROLL_ENABLE]] = !{!"llvm.loop.unroll.enable"}
// CHECK-DAG: ![[UNROLL_FULL]] = !{!"llvm.loop.unroll.full"}
// CHECK-DAG: ![[DISABLE_NONFORCED]] = !{!"llvm.loop.disable_nonforced"}
