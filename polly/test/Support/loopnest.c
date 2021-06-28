void func(double A[], double B[], double C[][1024]) {
  for (int i = 0; i < 1024; ++i) {
    A[i] = i;
    for (int j = 0; j < 1024; ++j) 
      C[i][j] = i + j;
  }
  for (int i = 0; i < 1024; ++i) 
    B[i] = i;
}
