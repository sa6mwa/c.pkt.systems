#ifndef CPKT_AUDIO_H
#define CPKT_AUDIO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cpkt_audio_decoder cpkt_audio_decoder;

typedef enum cpkt_audio_result {
  CPKT_AUDIO_OK = 0,
  CPKT_AUDIO_ERR_ARG = 1,
  CPKT_AUDIO_ERR_ALLOC = 2,
  CPKT_AUDIO_ERR_IO = 3,
  CPKT_AUDIO_ERR_FORMAT = 4,
  CPKT_AUDIO_ERR_UPSTREAM = 5,
  CPKT_AUDIO_AT_END = 6
} cpkt_audio_result;

typedef enum cpkt_audio_seek_origin {
  CPKT_AUDIO_SEEK_SET = 0,
  CPKT_AUDIO_SEEK_CUR = 1,
  CPKT_AUDIO_SEEK_END = 2
} cpkt_audio_seek_origin;

typedef enum cpkt_audio_encoding {
  CPKT_AUDIO_ENCODING_UNKNOWN = 0,
  CPKT_AUDIO_ENCODING_WAV = 1,
  CPKT_AUDIO_ENCODING_FLAC = 2,
  CPKT_AUDIO_ENCODING_MP3 = 3
} cpkt_audio_encoding;

typedef struct cpkt_audio_stream_info {
  unsigned long output_sample_rate;
  unsigned long output_channels;
  unsigned long output_frame_count;
} cpkt_audio_stream_info;

typedef size_t (*cpkt_audio_read_fn)(
    void *user,
    void *buffer,
    size_t bytes_to_read);

typedef int (*cpkt_audio_seek_fn)(
    void *user,
    long offset,
    int origin);

typedef struct cpkt_audio_reader {
  void *user;
  cpkt_audio_read_fn read;
  cpkt_audio_seek_fn seek;
} cpkt_audio_reader;

typedef struct cpkt_audio_decoder_config {
  int encoding;
} cpkt_audio_decoder_config;

struct cpkt_audio_decoder {
  void *impl;
  cpkt_audio_result (*read_f32_mono_16k)(
      cpkt_audio_decoder *self,
      float *frames,
      size_t frame_capacity,
      size_t *frames_read);
  cpkt_audio_result (*info)(
      const cpkt_audio_decoder *self,
      cpkt_audio_stream_info *info);
  void (*destroy)(cpkt_audio_decoder *self);
};

cpkt_audio_result cpkt_audio_decoder_open_file(
    cpkt_audio_decoder **out,
    const char *path,
    const cpkt_audio_decoder_config *config);

cpkt_audio_result cpkt_audio_decoder_open_reader(
    cpkt_audio_decoder **out,
    const cpkt_audio_reader *reader,
    const cpkt_audio_decoder_config *config);

const char *cpkt_audio_result_string(cpkt_audio_result result);

#ifdef __cplusplus
}
#endif

#endif
