/*
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);
void transpose_block(int M, int N, int A[N][M], int B[M][N]);
void transpose_cache_block4(int M, int N, int A[N][M], int B[M][N]);
void transpose_cache_block8(int M, int N, int A[N][M], int B[M][N]);
void transpose_cache_block84(int M, int N, int A[N][M], int B[M][N]);
void trans(int M, int N, int A[N][M], int B[M][N]);

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
    if (N == M && N == 32)
    {
        transpose_cache_block8(M, N, A, B);
    }
    else if (N == M && N == 64)
    {
        transpose_cache_block84(M, N, A, B);
    }
    else if (N >= 32 && M >= 32)
    {
        transpose_cache_block8(M, N, A, B);
    }
    else
    {
        trans(M, N, A, B);
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

    for (i = 0; i < N; i++)
    {
        for (j = 0; j < M; j++)
        {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }

}

char transpose_cache_block4_desc[] = "Transpose cache block4";
void transpose_cache_block4(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, k;
    int temp0, temp1, temp2, temp3;
    // block
    for (i = 0; i != N / 4 * 4; i += 4)
    {
        for (j = 0; j != M / 4 * 4; j += 4)
        {
            // row in A block
            for (k = i; k != i + 4; ++k)
            {
                // caching A
                temp0 = A[k][j + 0];
                temp1 = A[k][j + 1];
                temp2 = A[k][j + 2];
                temp3 = A[k][j + 3];
                // store to B
                B[j + 0][k] = temp0;
                B[j + 1][k] = temp1;
                B[j + 2][k] = temp2;
                B[j + 3][k] = temp3;
            }
        }
        // rest columns;
        for (k = i; k != i + 4; ++k)
        {
            for (int jj = j; jj != M; ++jj)
            {
                B[jj][k] = A[k][jj];
            }
        }
    }
    // rest rows
    for (j = 0; j != M / 4 * 4; j += 4)
    {
        // row in A block
        for (k = i; k != N; ++k)
        {
            // caching A
            temp0 = A[k][j + 0];
            temp1 = A[k][j + 1];
            temp2 = A[k][j + 2];
            temp3 = A[k][j + 3];
            // store to B
            B[j + 0][k] = temp0;
            B[j + 1][k] = temp1;
            B[j + 2][k] = temp2;
            B[j + 3][k] = temp3;
        }
    }
    // rest bottom-right corner;
    for (; i != N; ++i)
    {
        for (k = j; k != M; ++k)
        {
            B[k][i] = A[i][k];
        }
    }
}

char transpose_cache_block8_desc[] = "Transpose cache block8";
void transpose_cache_block8(int M, int N, int A[N][M], int B[M][N])
{
    int temp0, temp1, temp2, temp3, temp4, temp5, temp6, temp7, i, j, k;
    temp0 = temp1 = temp2 = temp3 = temp4 = temp5 = temp6 = temp7 = 0;
    // block
    for (i = 0; i != N / 8 * 8; i += 8)
    {
        for (j = 0; j != M / 8 * 8; j += 8)
        {
            // row in A block
            for (k = 0; k != 8; ++k)
            {
                // caching A
                temp0 = A[i + k][j + 0];
                temp1 = A[i + k][j + 1];
                temp2 = A[i + k][j + 2];
                temp3 = A[i + k][j + 3];
                temp4 = A[i + k][j + 4];
                temp5 = A[i + k][j + 5];
                temp6 = A[i + k][j + 6];
                temp7 = A[i + k][j + 7];
                // store to B
                B[j + 0][i + k] = temp0;
                B[j + 1][i + k] = temp1;
                B[j + 2][i + k] = temp2;
                B[j + 3][i + k] = temp3;
                B[j + 4][i + k] = temp4;
                B[j + 5][i + k] = temp5;
                B[j + 6][i + k] = temp6;
                B[j + 7][i + k] = temp7;
            }
        }
        if (j == M)
        {
            continue;
        }
        for (k = j; k != M; ++k)
        {
            // caching A
            temp0 = A[i + 0][k];
            temp1 = A[i + 1][k];
            temp2 = A[i + 2][k];
            temp3 = A[i + 3][k];
            temp4 = A[i + 4][k];
            temp5 = A[i + 5][k];
            temp6 = A[i + 6][k];
            temp7 = A[i + 7][k];
            // store to B
            B[k][i + 0] = temp0;
            B[k][i + 1] = temp1;
            B[k][i + 2] = temp2;
            B[k][i + 3] = temp3;
            B[k][i + 4] = temp4;
            B[k][i + 5] = temp5;
            B[k][i + 6] = temp6;
            B[k][i + 7] = temp7;
        }
    }
    if (i == M)
    {
        return;
    }
    // rest rows
    for (j = 0; j != M / 8 * 8; j += 8)
    {
        // row in A block
        for (k = i; k != N; ++k)
        {
            // caching A
            temp0 = A[k][j + 0];
            temp1 = A[k][j + 1];
            temp2 = A[k][j + 2];
            temp3 = A[k][j + 3];
            temp4 = A[k][j + 4];
            temp5 = A[k][j + 5];
            temp6 = A[k][j + 6];
            temp7 = A[k][j + 7];
            // store to B
            B[j + 0][k] = temp0;
            B[j + 1][k] = temp1;
            B[j + 2][k] = temp2;
            B[j + 3][k] = temp3;
            B[j + 4][k] = temp4;
            B[j + 5][k] = temp5;
            B[j + 6][k] = temp6;
            B[j + 7][k] = temp7;
        }
    }
    // rest bottom-right corner;
    for (; i != N; ++i)
    {
        for (k = j; k != M; ++k)
        {
            B[k][i] = A[i][k];
        }
    }

}

char transpose_cache_block84_desc[] = "Transpose cache block84";
void transpose_cache_block84(int M, int N, int A[N][M], int B[M][N])
{
    /*
    each number is a 4x4 block
    8x8 block A = 0 1
                  2 3
    8x8 block B = 0 1
                  2 3
    */
    int temp0, temp1, temp2, temp3, temp4, temp5, temp6, temp7, i, j, k;
    temp0 = temp1 = temp2 = temp3 = temp4 = temp5 = temp6 = temp7 = 0;
    // 8x8 block
    for (i = 0; i != N / 8 * 8; i += 8)
    {
        for (j = 0; j != M / 8 * 8; j += 8)
        {
            // transpose and store A0 A1 to B0 B1
            for (k = i; k != i + 4; ++k)
            {
                // caching A
                temp0 = A[k][j + 0];
                temp1 = A[k][j + 1];
                temp2 = A[k][j + 2];
                temp3 = A[k][j + 3];
                temp4 = A[k][j + 4];
                temp5 = A[k][j + 5];
                temp6 = A[k][j + 6];
                temp7 = A[k][j + 7];
                // store to B
                B[j + 0][k] = temp0;
                B[j + 1][k] = temp1;
                B[j + 2][k] = temp2;
                B[j + 3][k] = temp3;
                B[j + 0][k + 4] = temp4;
                B[j + 1][k + 4] = temp5;
                B[j + 2][k + 4] = temp6;
                B[j + 3][k + 4] = temp7;
            }
            // transpose A2 to B1, copy B1 to B2 in the mean time
            for (k = j; k != j + 4; ++k)
            {
                // caching B1
                temp0 = B[k][i + 4];
                temp1 = B[k][i + 5];
                temp2 = B[k][i + 6];
                temp3 = B[k][i + 7];

                // caching A2
                temp4 = A[i + 4][k];
                temp5 = A[i + 5][k];
                temp6 = A[i + 6][k];
                temp7 = A[i + 7][k];

                // transpose A2 to B1
                B[k][i + 4] = temp4;
                B[k][i + 5] = temp5;
                B[k][i + 6] = temp6;
                B[k][i + 7] = temp7;

                // copy B1 to B2
                B[k + 4][i + 0] = temp0;
                B[k + 4][i + 1] = temp1;
                B[k + 4][i + 2] = temp2;
                B[k + 4][i + 3] = temp3;
            }
            // transpose A3 to B3
            for (k = i + 4; k != i + 8; k += 2)
            {
                // caching A3
                temp0 = A[k + 0][j + 4];
                temp1 = A[k + 0][j + 5];
                temp2 = A[k + 0][j + 6];
                temp3 = A[k + 0][j + 7];
                temp4 = A[k + 1][j + 4];
                temp5 = A[k + 1][j + 5];
                temp6 = A[k + 1][j + 6];
                temp7 = A[k + 1][j + 7];

                // store to B3
                B[j + 4][k + 0] = temp0;
                B[j + 5][k + 0] = temp1;
                B[j + 6][k + 0] = temp2;
                B[j + 7][k + 0] = temp3;
                B[j + 4][k + 1] = temp4;
                B[j + 5][k + 1] = temp5;
                B[j + 6][k + 1] = temp6;
                B[j + 7][k + 1] = temp7;
            }
        }
        if (j == M)
        {
            continue;
        }
        // rest columns;
        for (k = j; k != M; ++k)
        {
            // caching A
            temp0 = A[i + 0][k];
            temp1 = A[i + 1][k];
            temp2 = A[i + 2][k];
            temp3 = A[i + 3][k];
            temp4 = A[i + 4][k];
            temp5 = A[i + 5][k];
            temp6 = A[i + 6][k];
            temp7 = A[i + 7][k];
            // store to B
            B[k][i + 0] = temp0;
            B[k][i + 1] = temp1;
            B[k][i + 2] = temp2;
            B[k][i + 3] = temp3;
            B[k][i + 4] = temp4;
            B[k][i + 5] = temp5;
            B[k][i + 6] = temp6;
            B[k][i + 7] = temp7;
        }
    }
    if (i == M)
    {
        return;
    }
    // rest rows
    for (j = 0; j != M / 8 * 8; j += 8)
    {
        // row in A block
        for (k = i; k != N; ++k)
        {
            // caching A
            temp0 = A[k][j + 0];
            temp1 = A[k][j + 1];
            temp2 = A[k][j + 2];
            temp3 = A[k][j + 3];
            temp4 = A[k][j + 4];
            temp5 = A[k][j + 5];
            temp6 = A[k][j + 6];
            temp7 = A[k][j + 7];
            // store to B
            B[j + 0][k] = temp0;
            B[j + 1][k] = temp1;
            B[j + 2][k] = temp2;
            B[j + 3][k] = temp3;
            B[j + 4][k] = temp4;
            B[j + 5][k] = temp5;
            B[j + 6][k] = temp6;
            B[j + 7][k] = temp7;
        }
    }
    // rest bottom-right corner;
    for (; i != N; ++i)
    {
        for (k = j; k != M; ++k)
        {
            B[k][i] = A[i][k];
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

    //registerTransFunction(transpose_cache_block4, transpose_cache_block4_desc);

    //registerTransFunction(transpose_cache_block8, transpose_cache_block8_desc);

    //registerTransFunction(transpose_cache_block84, transpose_cache_block84_desc);

    //registerTransFunction(transpose_cache_block, transpose_cache_block_desc);

    //registerTransFunction(transpose_block, transpose_block_desc);

    /* Register any additional transpose functions */
    //registerTransFunction(trans, trans_desc);

}

/*
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++)
    {
        for (j = 0; j < M; ++j)
        {
            if (A[i][j] != B[j][i])
            {
                return 0;
            }
        }
    }
    return 1;
}

