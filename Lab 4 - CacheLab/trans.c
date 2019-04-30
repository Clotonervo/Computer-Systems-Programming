/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Sam Hopkins, h0pkins3
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    if (N==32 && M == 32){
        for (int i = 0; i < 32; i += 8) {
            for (int j = 0; j < 32; j += 8) {
                for (int k = i; k < i + 8; k = k+8) {
                    for (int l = j; l < j + 8; l++) {
                        int tmp = A[l][k];
                        int tmp2 = A[l][k+1];
                        int tmp3 = A[l][k+2];
		                int tmp4 = A[l][k+3];
		                int tmp5 = A[l][k+4];
		                int tmp6 = A[l][k+5];
		                int tmp7 = A[l][k+6];
	                    int tmp8 = A[l][k+7];   
                        B[k][l] = tmp;
                        B[k+1][l] = tmp2;
		                B[k+2][l] = tmp3;
		                B[k+3][l] = tmp4;
		                B[k+4][l] = tmp5;
		                B[k+5][l] = tmp6;
		                B[k+6][l] = tmp7;
		                B[k+7][l] = tmp8;
                    }
                }
            }
        }
    }
    else {
          int blocksize = 8;
          int n = 64;
            for (int a = 0; a < 64; a+=32){
                for (int b = 0; b < 64; b+= 32){
                      for (int i = 0 + a; i < 32; i += 8) {
                        for (int j = 0 + b; j < 32; j += 8) {
                            for (int k = i; k < i + 8; k = k+8) {
                                for (int l = j; l < j + 8; l++) {
                                    int tmp = A[l][k];
                                    int tmp2 = A[l][k+1];
                                    int tmp3 = A[l][k+2];
		                            int tmp4 = A[l][k+3];
		                            int tmp5 = A[l][k+4];
		                            int tmp6 = A[l][k+5];
		                            int tmp7 = A[l][k+6];
	                                int tmp8 = A[l][k+7];   
                                    B[k][l] = tmp;
                                    B[k+1][l] = tmp2;
		                            B[k+2][l] = tmp3;
		                            B[k+3][l] = tmp4;
		                            B[k+4][l] = tmp5;
		                            B[k+5][l] = tmp6;
		                            B[k+6][l] = tmp7;
		                            B[k+7][l] = tmp8;
                                }
                            }
                        }
                    }
                }
        }
    }
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}


