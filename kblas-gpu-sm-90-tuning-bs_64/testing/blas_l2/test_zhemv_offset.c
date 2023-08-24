/**
 * @copyright (c) 2012- King Abdullah University of Science and
 *                      Technology (KAUST). All rights reserved.
 **/


/**
 * @file testing/blas_l2/test_zhemv_offset.c

 * KBLAS is a high performance CUDA library for subset of BLAS
 *    and LAPACK routines optimized for NVIDIA GPUs.
 * KBLAS is provided by KAUST.
 *
 * @version 3.0.0
 * @author Ahmad Abdelfattah
 * @date 2018-11-14
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <cuda_runtime.h>
#include <cublas_v2.h>
#include "kblas.h"
#include "testing_utils.h"

#define FMULS_SYMV(n) ((n) * (n) + 2. * (n))
#define FADDS_SYMV(n) ((n) * (n)           )

#define PRECISION_z

#if defined(PRECISION_z) || defined(PRECISION_c)
#define FLOPS(n) ( 6. * FMULS_SYMV(n) + 2. * FADDS_SYMV(n))
#else
#define FLOPS(n) (      FMULS_SYMV(n) +      FADDS_SYMV(n))
#endif


int main(int argc, char** argv)
{
	if(argc < 7)
	{
		printf("USAGE: %s  <device-id> <upper'u'-or-lower'l'> <matrix-dim> <start-offset> <stop-offset> <step-offset>\n", argv[0]);
		printf("==> <device-id>: GPU device id to use \n");
		printf("==> <upper'u'-or-lower'l'>: Access either upper or lower triangular part of the matrix \n");
		printf("==> <matrix-dim>: The dimension of the matrix\n");
		printf("==> <start-offset> <stop-offset> <step-offset>: Offset range. For every <offset> in the offset range, test is performed on a submatrix whose dimension is <matrix-dim>-<offset>\n");
		exit(-1);
	}

	int dev = atoi(argv[1]);
	char uplo = *argv[2];
	int dim = atoi(argv[3]);
	int istart = atoi(argv[4]);
	int istop = atoi(argv[5]);
	int istep = atoi(argv[6]);

	const int nruns = NRUNS;

	cudaError_t ed = cudaSetDevice(dev);
	if(ed != cudaSuccess){printf("Error setting device : %s \n", cudaGetErrorString(ed) ); exit(-1);}

	cublasHandle_t cublas_handle;
	cublasAtomicsMode_t mode = CUBLAS_ATOMICS_ALLOWED;
	cublasCreate(&cublas_handle);
	cublasSetAtomicsMode(cublas_handle, mode);

	struct cudaDeviceProp deviceProp;
	cudaGetDeviceProperties(&deviceProp, dev);

    if(istop >= dim){printf("Error: maximum offset value causes zero or negative submatrix dimension\n"); exit(-1);}

    int M = dim;
    int N = M;
    int LDA = M;
    int LDA_ = ((M+31)/32)*32;

	int incx = 1;
	int incy = 1;
	int vecsize_x = N * abs(incx);
	int vecsize_y = M * abs(incy);

	cublasFillMode_t uplo_;
	if(uplo == 'L' || uplo == 'l')
		uplo_ = CUBLAS_FILL_MODE_LOWER;
	else if (uplo == 'U' || uplo == 'u')
		uplo_ = CUBLAS_FILL_MODE_UPPER;

	cuDoubleComplex alpha = kblas_zrand();
	cuDoubleComplex beta = kblas_zrand();

	cudaError_t err;
    cudaEvent_t start, stop;

	cudaEventCreate(&start);
	cudaEventCreate(&stop);

    // point to host memory
    cuDoubleComplex* A = NULL;
    cuDoubleComplex* x = NULL;
    cuDoubleComplex* ycuda = NULL;
    cuDoubleComplex* ykblas = NULL;

    // point to device memory
    cuDoubleComplex* dA = NULL;
    cuDoubleComplex* dx = NULL;
    cuDoubleComplex* dy = NULL;

    if(uplo == 'L' || uplo == 'l')printf("Lower triangular test .. \n");
	else if (uplo == 'U' || uplo == 'u') printf("Upper triangular test .. \n");
	else { printf("upper/lower triangular configuration is not properly specified\n"); exit(-1);}
	printf("Allocating Matrices\n");
    A = (cuDoubleComplex*)malloc(N*LDA*sizeof(cuDoubleComplex));
    x = (cuDoubleComplex*)malloc(vecsize_x*sizeof(cuDoubleComplex));
    ycuda = (cuDoubleComplex*)malloc(vecsize_y*sizeof(cuDoubleComplex));
    ykblas = (cuDoubleComplex*)malloc(vecsize_y*sizeof(cuDoubleComplex));

    err = cudaMalloc((void**)&dA, N*LDA_*sizeof(cuDoubleComplex));
    if(err != cudaSuccess){printf("ERROR: %s \n", cudaGetErrorString(err)); exit(1);}
    err = cudaMalloc((void**)&dx, vecsize_x*sizeof(cuDoubleComplex));
    if(err != cudaSuccess){printf("ERROR: %s \n", cudaGetErrorString(err)); exit(1);}
    err = cudaMalloc((void**)&dy, vecsize_y*sizeof(cuDoubleComplex));
	if(err != cudaSuccess){printf("ERROR: %s \n", cudaGetErrorString(err)); exit(1);}

    // Initialize matrix and vector
    printf("Initializing on cpu .. \n");
    int i, j, m;
    for(i = 0; i < M; i++)
    		for(j = 0; j < N; j++)
    			A[j*LDA+i] = kblas_zrand();

    for(i = 0; i < M; i++)
      for(j = i; j < M; j++)
		A[i * N + j] = A[j * M + i];

    for(i = 0; i < vecsize_x; i++)
      x[i] = kblas_zrand();

    cublasSetMatrix(dim, dim, sizeof(cuDoubleComplex), A, LDA, dA, LDA_);
	cudaMemcpy(dx, x, vecsize_x*sizeof(cuDoubleComplex), cudaMemcpyHostToDevice);

	printf("------------------- Testing ZHEMV ----------------\n");
    printf("  Matrix        CUBLAS       KBLAS          Max.  \n");
    printf(" Dimension     (Gflop/s)   (Gflop/s)       Error  \n");
    printf("-----------   ----------   ----------   ----------\n");

    int r;
    for(m = istart; m <= istop; m += istep)
    {
    	int offset = m;
    	int dim_ = dim-offset;

    	float elapsedTime;
    	float flops = FLOPS( (float)dim_ ) / 1e6;

		for(i = 0; i < vecsize_y; i++)
    	{
      		ycuda[i] = kblas_zrand();
      		ykblas[i] = ycuda[i];
    	}

      	 // handle the offset
        cuDoubleComplex* dA_ = dA + offset * LDA_ + offset;
        cuDoubleComplex* dx_ = dx + offset * incx;
        cuDoubleComplex* dy_ = dy + offset * incy;
        int vecsize_y_ = vecsize_y - offset;
        int vecsize_x_ = vecsize_x - offset;

      	// --- cuda test
      	elapsedTime = 0;
      	for(r = 0; r < nruns; r++)
      	{
      		cudaMemcpy(dy_, ycuda, vecsize_y_ * sizeof(cuDoubleComplex), cudaMemcpyHostToDevice);

      		cudaEventRecord(start, 0);
      		cublasZhemv(cublas_handle, uplo_, dim_, &alpha, dA_, LDA_, dx_, incx, &beta, dy_, incy);
      		cudaEventRecord(stop, 0);
      		cudaEventSynchronize(stop);
      		float time = 0;
      		cudaEventElapsedTime(&time, start, stop);
      		elapsedTime += time;
      	}
      	elapsedTime /= nruns;
      	float cuda_perf = flops / elapsedTime;

      	cudaMemcpy(ycuda, dy_, vecsize_y_ * sizeof(cuDoubleComplex), cudaMemcpyDeviceToHost);
      	// end of cuda test

      	// ---- kblas
      	elapsedTime = 0;
      	for(r = 0; r < nruns; r++)
      	{
      		cudaMemcpy(dy_, ykblas, vecsize_y_ * sizeof(cuDoubleComplex), cudaMemcpyHostToDevice);

      		cudaEventRecord(start, 0);
      		kblas_zhemv_offset(uplo, dim, alpha, dA, LDA_, dx, incx, beta, dy, incy, offset);
      		cudaEventRecord(stop, 0);
      		cudaEventSynchronize(stop);
      		float time = 0.0;
      		cudaEventElapsedTime(&time, start, stop);
      		elapsedTime += time;
      	}
      	elapsedTime /= nruns;
      	float kblas_perf = flops / elapsedTime;

      	cudaMemcpy(ykblas, dy_, vecsize_y_ * sizeof(cuDoubleComplex), cudaMemcpyDeviceToHost);

      	// testing error -- specify ref. vector and result vector
      	cuDoubleComplex* yref = ycuda;
      	cuDoubleComplex* yres = ykblas;

      	double error = zget_max_error(yref, yres, dim_, incy);

      	//printf("-----------   ----------   ----------   ----------   ----------   ----------\n");
    	printf("%-11d   %-10.2f   %-10.2f   %-10e;\n", dim_, cuda_perf, kblas_perf, error);

    }

	cudaEventDestroy(start);
	cudaEventDestroy(stop);

    if(dA)cudaFree(dA);
    if(dx)cudaFree(dx);
    if(dy)cudaFree(dy);

    if(A)free(A);
    if(x)free(x);
    if(ycuda)free(ycuda);
	if(ykblas)free(ykblas);

	cublasDestroy(cublas_handle);
    return EXIT_SUCCESS;
}

