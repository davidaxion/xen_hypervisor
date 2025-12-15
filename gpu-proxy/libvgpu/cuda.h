/*
 * libvgpu - Virtual GPU Library
 *
 * CUDA Driver API definitions (subset)
 * This file defines the CUDA Driver API functions we intercept.
 */

#ifndef LIBVGPU_CUDA_H
#define LIBVGPU_CUDA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* CUDA types */
typedef int CUresult;
typedef void *CUcontext;
typedef int CUdevice;
typedef unsigned long long CUdeviceptr;

/* CUDA result codes */
#define CUDA_SUCCESS                    0
#define CUDA_ERROR_INVALID_VALUE        1
#define CUDA_ERROR_OUT_OF_MEMORY        2
#define CUDA_ERROR_NOT_INITIALIZED      3
#define CUDA_ERROR_DEINITIALIZED        4
#define CUDA_ERROR_INVALID_CONTEXT      201
#define CUDA_ERROR_INVALID_HANDLE       400

/* CUDA Driver API functions we intercept */

/* Initialization */
CUresult cuInit(unsigned int Flags);
CUresult cuDriverGetVersion(int *driverVersion);

/* Device management */
CUresult cuDeviceGet(CUdevice *device, int ordinal);
CUresult cuDeviceGetCount(int *count);
CUresult cuDeviceGetName(char *name, int len, CUdevice dev);
CUresult cuDeviceGetAttribute(int *pi, int attrib, CUdevice dev);

/* Context management */
CUresult cuCtxCreate(CUcontext *pctx, unsigned int flags, CUdevice dev);
CUresult cuCtxDestroy(CUcontext ctx);
CUresult cuCtxSynchronize(void);
CUresult cuCtxGetCurrent(CUcontext *pctx);
CUresult cuCtxSetCurrent(CUcontext ctx);

/* Memory management */
CUresult cuMemAlloc(CUdeviceptr *dptr, size_t bytesize);
CUresult cuMemFree(CUdeviceptr dptr);
CUresult cuMemcpyHtoD(CUdeviceptr dstDevice, const void *srcHost, size_t ByteCount);
CUresult cuMemcpyDtoH(void *dstHost, CUdeviceptr srcDevice, size_t ByteCount);
CUresult cuMemcpyDtoD(CUdeviceptr dstDevice, CUdeviceptr srcDevice, size_t ByteCount);
CUresult cuMemsetD8(CUdeviceptr dstDevice, unsigned char uc, size_t N);
CUresult cuMemsetD16(CUdeviceptr dstDevice, unsigned short us, size_t N);
CUresult cuMemsetD32(CUdeviceptr dstDevice, unsigned int ui, size_t N);

/* Error handling */
CUresult cuGetErrorString(CUresult error, const char **pStr);
CUresult cuGetErrorName(CUresult error, const char **pStr);

#ifdef __cplusplus
}
#endif

#endif /* LIBVGPU_CUDA_H */
