#ifndef SPECTRA_DSP_H
#define SPECTRA_DSP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SPECTRA_ABI_VERSION 2
#define SPECTRA_IR_KEY_SIZE 32

typedef enum SpectraStatus {
    SPECTRA_OK = 0,
    SPECTRA_ERR_INVALID = 1,
    SPECTRA_ERR_NOMEM = 2,
    SPECTRA_ERR_UNSUPPORTED = 3,
    SPECTRA_ERR_UNAUTHORIZED = 4
} SpectraStatus;

typedef enum SpectraCaptureKind {
    SPECTRA_CAPTURE_IR = 1,
    SPECTRA_CAPTURE_AMP = 2
} SpectraCaptureKind;

typedef struct SpectraBuffer {
    uint8_t* data;
    size_t size;
} SpectraBuffer;

#ifndef SPECTRA_API
#if defined(_WIN32)
#if defined(SPECTRA_DSP_BUILD)
#define SPECTRA_API __declspec(dllexport)
#else
#define SPECTRA_API __declspec(dllimport)
#endif
#elif defined(SPECTRA_DSP_BUILD)
#define SPECTRA_API __attribute__((visibility("default")))
#else
#define SPECTRA_API
#endif
#endif

SPECTRA_API int spectra_abi_version(void);

SPECTRA_API int spectra_session_unlock(const char* access_token);
SPECTRA_API void spectra_session_lock(void);

SPECTRA_API int spectra_decrypt_model(const uint8_t* cipher, size_t cipher_size, SpectraBuffer* plain);
SPECTRA_API int spectra_decrypt_ir(const uint8_t* cipher, size_t cipher_size, SpectraBuffer* plain);
SPECTRA_API void spectra_buffer_free(SpectraBuffer* buffer);

SPECTRA_API int spectra_capture_begin(int kind);
SPECTRA_API int spectra_capture_push(const float* samples, size_t count);
SPECTRA_API int spectra_capture_finish(void);

SPECTRA_API int spectra_ir_prepare(double sample_rate,
                                   float* sweep,
                                   size_t sweep_capacity,
                                   size_t* sweep_frames,
                                   size_t* record_frames);
SPECTRA_API int spectra_ir_seal(const float* recorded,
                                size_t frames,
                                double sample_rate,
                                uint8_t* key_out,
                                size_t key_out_size,
                                SpectraBuffer* sealed);
SPECTRA_API int spectra_ir_open(const uint8_t* key,
                                size_t key_size,
                                const uint8_t* sealed,
                                size_t sealed_size,
                                SpectraBuffer* plain);

SPECTRA_API int spectra_model_seal(const uint8_t* plain,
                                   size_t plain_size,
                                   uint8_t* key_out,
                                   size_t key_out_size,
                                   SpectraBuffer* sealed);
SPECTRA_API int spectra_model_open(const uint8_t* key,
                                   size_t key_size,
                                   const uint8_t* sealed,
                                   size_t sealed_size,
                                   SpectraBuffer* plain);

#ifdef __cplusplus
}
#endif

#endif
