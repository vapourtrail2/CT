#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#  if defined(CT_INCREMENTAL_FDK_BUILD)
#    define CT_FDK_API __declspec(dllexport)
#  else
#    define CT_FDK_API __declspec(dllimport)
#  endif
#else
#  define CT_FDK_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define CT_FDK_ABI_VERSION 1U
#define CT_FDK_INFINITE_TIMEOUT UINT32_MAX

typedef struct ct_fdk_handle ct_fdk_handle;

typedef int32_t ct_fdk_result;
#define CT_FDK_OK ((ct_fdk_result)0)
#define CT_FDK_INVALID_ARGUMENT ((ct_fdk_result)1)
#define CT_FDK_INVALID_STATE ((ct_fdk_result)2)
#define CT_FDK_TIMEOUT ((ct_fdk_result)3)
#define CT_FDK_BUFFER_TOO_SMALL ((ct_fdk_result)4)
#define CT_FDK_INTERNAL_ERROR ((ct_fdk_result)5)

typedef uint32_t ct_fdk_state;
#define CT_FDK_STATE_IDLE ((ct_fdk_state)0U)
#define CT_FDK_STATE_RUNNING ((ct_fdk_state)1U)
#define CT_FDK_STATE_COMPLETED ((ct_fdk_state)2U)
#define CT_FDK_STATE_CANCELLED ((ct_fdk_state)3U)
#define CT_FDK_STATE_FAILED ((ct_fdk_state)4U)

typedef struct ct_fdk_recon_config {
    uint32_t struct_size;
    uint32_t volume_size_x;
    uint32_t volume_size_y;
    uint32_t volume_size_z;
    double fov_x_mm;
    double fov_y_mm;
    double fov_z_mm;
    double volume_center_x_mm;
    double volume_center_y_mm;
    double volume_center_z_mm;
    uint32_t detector_binning;
    uint32_t maximum_batch_views;
    uint32_t maximum_batch_latency_ms;
    int32_t gpu_index;
} ct_fdk_recon_config;

typedef struct ct_fdk_progress {
    uint32_t struct_size;
    ct_fdk_state state;
    uint32_t expected_frames;
    uint32_t received_frames;
    uint32_t processed_frames;
} ct_fdk_progress;

typedef struct ct_fdk_volume_info {
    uint32_t struct_size;
    uint32_t size_x;
    uint32_t size_y;
    uint32_t size_z;
    uint64_t voxel_count;
} ct_fdk_volume_info;

CT_FDK_API uint32_t ct_fdk_abi_version(void);
// config_ini_utf8 must point to a null-terminated UTF-8 path. The library
// copies all input configuration during this call.
CT_FDK_API ct_fdk_result ct_fdk_create(
    const char* config_ini_utf8,
    const ct_fdk_recon_config* config,
    ct_fdk_handle** output);
CT_FDK_API void ct_fdk_destroy(ct_fdk_handle* handle);
CT_FDK_API ct_fdk_result ct_fdk_start(ct_fdk_handle* handle);
CT_FDK_API ct_fdk_result ct_fdk_cancel(ct_fdk_handle* handle);
CT_FDK_API ct_fdk_result ct_fdk_wait(ct_fdk_handle* handle, uint32_t timeout_ms);
CT_FDK_API ct_fdk_result ct_fdk_get_progress(
    const ct_fdk_handle* handle,
    ct_fdk_progress* output);

// required_size includes the trailing null. A null/short buffer returns
// CT_FDK_BUFFER_TOO_SMALL and reports the required size.
CT_FDK_API ct_fdk_result ct_fdk_get_last_error(
    const ct_fdk_handle* handle,
    char* buffer,
    size_t capacity,
    size_t* required_size);
CT_FDK_API ct_fdk_result ct_fdk_get_volume_info(
    const ct_fdk_handle* handle,
    ct_fdk_volume_info* output);

// Copies float32 voxels in ZYX order (X changes fastest) into caller memory.
CT_FDK_API ct_fdk_result ct_fdk_copy_volume(
    const ct_fdk_handle* handle,
    float* output,
    uint64_t capacity,
    uint64_t* written_or_required);

#ifdef __cplusplus
} // extern "C"
#endif

#undef CT_FDK_API
