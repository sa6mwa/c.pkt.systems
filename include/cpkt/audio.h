#ifndef CPKT_AUDIO_H
#define CPKT_AUDIO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Handle for a single audio decoder instance. */
typedef struct cpkt_audio_decoder cpkt_audio_decoder;

/** Result codes returned by the audio facade. */
typedef enum cpkt_audio_result {
  /** Operation completed successfully. */
  CPKT_AUDIO_OK = 0,
  /** A required argument was missing, invalid, or inconsistent. */
  CPKT_AUDIO_ERR_ARG = 1,
  /** Memory allocation failed. */
  CPKT_AUDIO_ERR_ALLOC = 2,
  /** The input source could not be read or sought as required. */
  CPKT_AUDIO_ERR_IO = 3,
  /** The input data is not a supported or valid audio stream. */
  CPKT_AUDIO_ERR_FORMAT = 4,
  /** The backend returned an error not covered by a narrower result code. */
  CPKT_AUDIO_ERR_UPSTREAM = 5,
  /** The decoder reached the end of the input stream. */
  CPKT_AUDIO_AT_END = 6
} cpkt_audio_result;

/** Origin values passed to custom reader seek callbacks. */
typedef enum cpkt_audio_seek_origin {
  /** Seek relative to the beginning of the stream. */
  CPKT_AUDIO_SEEK_SET = 0,
  /** Seek relative to the current stream position. */
  CPKT_AUDIO_SEEK_CUR = 1,
  /** Seek relative to the end of the stream. */
  CPKT_AUDIO_SEEK_END = 2
} cpkt_audio_seek_origin;

/** Optional input encoding hint for decoder construction. */
typedef enum cpkt_audio_encoding {
  /** Let the backend detect the input encoding. */
  CPKT_AUDIO_ENCODING_UNKNOWN = 0,
  /** RIFF/WAVE input. */
  CPKT_AUDIO_ENCODING_WAV = 1,
  /** FLAC input. */
  CPKT_AUDIO_ENCODING_FLAC = 2,
  /** MPEG audio input. */
  CPKT_AUDIO_ENCODING_MP3 = 3
} cpkt_audio_encoding;

/** Output stream description for an opened decoder. */
typedef struct cpkt_audio_stream_info {
  /** Output sample rate in hertz. */
  unsigned long output_sample_rate;
  /** Output channel count. The current speech-oriented output path uses mono. */
  unsigned long output_channels;
  /** Total output frame count when the input source can report it, or zero. */
  unsigned long output_frame_count;
} cpkt_audio_stream_info;

/**
 * Reads up to bytes_to_read bytes from a caller-owned input source.
 *
 * Return the number of bytes copied into buffer. Returning zero indicates no
 * data was produced for that call; repeated zero-byte reads may end decoding.
 */
typedef size_t (*cpkt_audio_read_fn)(
    void *user,
    void *buffer,
    size_t bytes_to_read);

/**
 * Repositions a caller-owned input source.
 *
 * Return zero on success and non-zero on failure. origin is one of
 * cpkt_audio_seek_origin.
 */
typedef int (*cpkt_audio_seek_fn)(
    void *user,
    long offset,
    int origin);

/** Callback reader used to decode from caller-owned storage or transport. */
typedef struct cpkt_audio_reader {
  /** Caller-owned value passed to read and seek callbacks. */
  void *user;
  /** Required byte reader callback. */
  cpkt_audio_read_fn read;
  /** Optional seek callback; seekless sources may leave this NULL. */
  cpkt_audio_seek_fn seek;
} cpkt_audio_reader;

/** Decoder construction options. */
typedef struct cpkt_audio_decoder_config {
  /** Optional cpkt_audio_encoding value used as an input format hint. */
  int encoding;
} cpkt_audio_decoder_config;

/** Receiver shell for decoder operations. */
struct cpkt_audio_decoder {
  /** Private implementation pointer. Callers must not inspect or modify it. */
  void *impl;
  /**
   * Reads decoded audio as 32-bit float, mono, 16000 Hz PCM frames.
   *
   * frames receives at most frame_capacity samples. frames_read is always set
   * before return when the arguments are valid. CPKT_AUDIO_AT_END means no
   * additional frames are available.
   */
  cpkt_audio_result (*read_f32_mono_16k)(
      cpkt_audio_decoder *self,
      float *frames,
      size_t frame_capacity,
      size_t *frames_read);
  /** Fills info with the decoder output format and known frame count. */
  cpkt_audio_result (*info)(
      const cpkt_audio_decoder *self,
      cpkt_audio_stream_info *info);
  /** Releases the decoder and all resources owned by the handle. */
  void (*destroy)(cpkt_audio_decoder *self);
};

/**
 * Opens an audio decoder from a filesystem path.
 *
 * On success, *out receives a decoder handle that must be destroyed with its
 * destroy receiver. On failure, *out is set to NULL when out is non-NULL.
 */
cpkt_audio_result cpkt_audio_decoder_open_file(
    cpkt_audio_decoder **out,
    const char *path,
    const cpkt_audio_decoder_config *config);

/**
 * Opens an audio decoder from callback-based input.
 *
 * The reader structure is copied during construction. The caller-owned user
 * value and backing source must remain valid until the decoder is destroyed.
 */
cpkt_audio_result cpkt_audio_decoder_open_reader(
    cpkt_audio_decoder **out,
    const cpkt_audio_reader *reader,
    const cpkt_audio_decoder_config *config);

/** Returns a stable human-readable string for an audio result code. */
const char *cpkt_audio_result_string(cpkt_audio_result result);

#ifdef __cplusplus
}
#endif

#endif
