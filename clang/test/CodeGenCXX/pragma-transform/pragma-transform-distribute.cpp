// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++11 -fexperimental-transform-pragma -emit-llvm -o - %s | FileCheck %s

extern "C" void pragma_transform_distribute(int *List, int Length) {
#pragma clang transform distribute
  for (int i = 0; i < Length; i++) {
// CHECK: br label %{{.*}}, !llvm.loop ![[LOOP:[0-9]+]]
    List[i] = i * 2;
  }
}


// CHECK-DAG: ![[LOOP]] = distinct !{![[LOOP]], ![[DISTRIBUTE_ENABLE:[0-9]+]], ![[DISTRIBUTE_FOLLOWUP_ALL:[0-9]+]], ![[DISABLE_NONFORCED:[0-9]+]]}
// CHECK-DAG: ![[DISTRIBUTE_ENABLE]] = !{!"llvm.loop.distribute.enable", i1 true}
// CHECK-DAG: ![[DISABLE_NONFORCED]] = !{!"llvm.loop.disable_nonforced"}
