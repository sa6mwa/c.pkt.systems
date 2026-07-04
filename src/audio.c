#include <cpkt/audio.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <miniaudio.h>

struct cpkt_audio_decoder_impl {
  ma_decoder decoder;
  cpkt_audio_reader reader;
  int owns_reader;
};

struct cpkt_audio_encoder_impl {
  ma_encoder encoder;
  cpkt_audio_writer writer;
  int closed;
};

static ma_encoding_format cpkt_audio_to_ma_encoding(int encoding) {
  switch (encoding) {
  case CPKT_AUDIO_ENCODING_WAV:
    return ma_encoding_format_wav;
  case CPKT_AUDIO_ENCODING_FLAC:
    return ma_encoding_format_flac;
  case CPKT_AUDIO_ENCODING_MP3:
    return ma_encoding_format_mp3;
  default:
    return ma_encoding_format_unknown;
  }
}

static ma_encoding_format cpkt_audio_format_to_ma_encoding(int format) {
  switch (format) {
  case CPKT_AUDIO_FORMAT_WAV:
    return ma_encoding_format_wav;
  case CPKT_AUDIO_FORMAT_FLAC:
    return ma_encoding_format_flac;
  case CPKT_AUDIO_FORMAT_MP3:
    return ma_encoding_format_mp3;
  default:
    return ma_encoding_format_unknown;
  }
}

static cpkt_audio_result cpkt_audio_from_ma_result(ma_result result) {
  if (result == MA_SUCCESS) {
    return CPKT_AUDIO_OK;
  }
  if (result == MA_AT_END) {
    return CPKT_AUDIO_AT_END;
  }
  if (result == MA_INVALID_ARGS) {
    return CPKT_AUDIO_ERR_ARG;
  }
  if (result == MA_OUT_OF_MEMORY) {
    return CPKT_AUDIO_ERR_ALLOC;
  }
  if (result == MA_FORMAT_NOT_SUPPORTED || result == MA_INVALID_FILE) {
    return CPKT_AUDIO_ERR_FORMAT;
  }
  if (result == MA_IO_ERROR || result == MA_DOES_NOT_EXIST ||
      result == MA_ACCESS_DENIED) {
    return CPKT_AUDIO_ERR_IO;
  }
  return CPKT_AUDIO_ERR_UPSTREAM;
}

static ma_result cpkt_audio_reader_read(ma_decoder *decoder, void *buffer,
                                        size_t bytes_to_read,
                                        size_t *bytes_read) {
  cpkt_audio_reader *reader;
  size_t nread;

  reader = (cpkt_audio_reader *)decoder->pUserData;
  if (reader == NULL || reader->read == NULL || bytes_read == NULL) {
    return MA_INVALID_ARGS;
  }

  nread = reader->read(reader->user, buffer, bytes_to_read);
  if (nread > bytes_to_read) {
    return MA_IO_ERROR;
  }
  *bytes_read = nread;
  return MA_SUCCESS;
}

static ma_result cpkt_audio_reader_seek(ma_decoder *decoder, ma_int64 offset,
                                        ma_seek_origin origin) {
  cpkt_audio_reader *reader;
  int cpkt_origin;

  reader = (cpkt_audio_reader *)decoder->pUserData;
  if (reader == NULL || reader->seek == NULL) {
    return MA_NOT_IMPLEMENTED;
  }
  if (offset > LONG_MAX || offset < LONG_MIN) {
    return MA_INVALID_ARGS;
  }

  if (origin == ma_seek_origin_start) {
    cpkt_origin = CPKT_AUDIO_SEEK_SET;
  } else if (origin == ma_seek_origin_current) {
    cpkt_origin = CPKT_AUDIO_SEEK_CUR;
  } else {
    cpkt_origin = CPKT_AUDIO_SEEK_END;
  }

  if (reader->seek(reader->user, (long)offset, cpkt_origin) != 0) {
    return MA_IO_ERROR;
  }
  return MA_SUCCESS;
}

static ma_result cpkt_audio_writer_write(ma_encoder *encoder,
                                         const void *buffer,
                                         size_t bytes_to_write,
                                         size_t *bytes_written) {
  cpkt_audio_writer *writer;
  size_t nwritten;

  writer = (cpkt_audio_writer *)encoder->pUserData;
  if (writer == NULL || writer->write == NULL || bytes_written == NULL) {
    return MA_INVALID_ARGS;
  }

  nwritten = writer->write(writer->user, buffer, bytes_to_write);
  if (nwritten > bytes_to_write) {
    return MA_IO_ERROR;
  }
  *bytes_written = nwritten;
  return nwritten == bytes_to_write ? MA_SUCCESS : MA_IO_ERROR;
}

static ma_result cpkt_audio_writer_seek(ma_encoder *encoder, ma_int64 offset,
                                        ma_seek_origin origin) {
  cpkt_audio_writer *writer;
  int cpkt_origin;

  writer = (cpkt_audio_writer *)encoder->pUserData;
  if (writer == NULL || writer->seek == NULL) {
    return MA_NOT_IMPLEMENTED;
  }
  if (offset > LONG_MAX || offset < LONG_MIN) {
    return MA_INVALID_ARGS;
  }

  if (origin == ma_seek_origin_start) {
    cpkt_origin = CPKT_AUDIO_SEEK_SET;
  } else if (origin == ma_seek_origin_current) {
    cpkt_origin = CPKT_AUDIO_SEEK_CUR;
  } else {
    cpkt_origin = CPKT_AUDIO_SEEK_END;
  }

  if (writer->seek(writer->user, (long)offset, cpkt_origin) != 0) {
    return MA_IO_ERROR;
  }
  return MA_SUCCESS;
}

static cpkt_audio_result
cpkt_audio_decoder_read_f32_mono_16k_impl(cpkt_audio_decoder *self,
                                          float *frames, size_t frame_capacity,
                                          size_t *frames_read) {
  struct cpkt_audio_decoder_impl *impl;
  ma_uint64 requested;
  ma_uint64 actual;
  ma_result result;

  if (frames_read != NULL) {
    *frames_read = 0;
  }
  if (self == NULL || self->impl == NULL || frames == NULL ||
      frames_read == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  if (frame_capacity == 0) {
    return CPKT_AUDIO_OK;
  }

  impl = (struct cpkt_audio_decoder_impl *)self->impl;
  requested = (ma_uint64)frame_capacity;
  if ((size_t)requested != frame_capacity) {
    return CPKT_AUDIO_ERR_ARG;
  }

  actual = 0;
  result =
      ma_decoder_read_pcm_frames(&impl->decoder, frames, requested, &actual);
  *frames_read = (size_t)actual;
  if (result == MA_AT_END && actual > 0) {
    return CPKT_AUDIO_OK;
  }
  return cpkt_audio_from_ma_result(result);
}

static cpkt_audio_result
cpkt_audio_decoder_info_impl(const cpkt_audio_decoder *self,
                             cpkt_audio_stream_info *info) {
  struct cpkt_audio_decoder_impl *impl;
  ma_format format;
  ma_uint32 channels;
  ma_uint32 sample_rate;
  ma_uint64 length;
  ma_result result;

  if (info != NULL) {
    memset(info, 0, sizeof(*info));
  }
  if (self == NULL || self->impl == NULL || info == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }

  impl = (struct cpkt_audio_decoder_impl *)self->impl;
  result = ma_decoder_get_data_format(&impl->decoder, &format, &channels,
                                      &sample_rate, NULL, 0);
  if (result != MA_SUCCESS) {
    return cpkt_audio_from_ma_result(result);
  }
  (void)format;
  info->output_channels = channels;
  info->output_sample_rate = sample_rate;
  length = 0;
  result = ma_decoder_get_length_in_pcm_frames(&impl->decoder, &length);
  if (result == MA_SUCCESS) {
    info->output_frame_count = (unsigned long)length;
  }
  return CPKT_AUDIO_OK;
}

static void cpkt_audio_decoder_destroy_impl(cpkt_audio_decoder *self) {
  struct cpkt_audio_decoder_impl *impl;

  if (self == NULL) {
    return;
  }
  impl = (struct cpkt_audio_decoder_impl *)self->impl;
  if (impl != NULL) {
    ma_decoder_uninit(&impl->decoder);
    free(impl);
  }
  free(self);
}

static cpkt_audio_result
cpkt_audio_encoder_write_f32_impl(cpkt_audio_encoder *self, const float *frames,
                                  size_t frame_count, size_t *frames_written) {
  struct cpkt_audio_encoder_impl *impl;
  ma_uint64 requested;
  ma_uint64 actual;
  ma_result result;

  if (frames_written != NULL) {
    *frames_written = 0;
  }
  if (self == NULL || self->impl == NULL || frames == NULL ||
      frames_written == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  if (frame_count == 0) {
    return CPKT_AUDIO_OK;
  }

  impl = (struct cpkt_audio_encoder_impl *)self->impl;
  if (impl->closed) {
    return CPKT_AUDIO_ERR_IO;
  }
  requested = (ma_uint64)frame_count;
  if ((size_t)requested != frame_count) {
    return CPKT_AUDIO_ERR_ARG;
  }

  actual = 0;
  result =
      ma_encoder_write_pcm_frames(&impl->encoder, frames, requested, &actual);
  *frames_written = (size_t)actual;
  return cpkt_audio_from_ma_result(result);
}

static cpkt_audio_result
cpkt_audio_encoder_close_impl(cpkt_audio_encoder *self) {
  struct cpkt_audio_encoder_impl *impl;

  if (self == NULL || self->impl == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }

  impl = (struct cpkt_audio_encoder_impl *)self->impl;
  if (!impl->closed) {
    ma_encoder_uninit(&impl->encoder);
    impl->closed = 1;
  }
  return CPKT_AUDIO_OK;
}

static void cpkt_audio_encoder_destroy_impl(cpkt_audio_encoder *self) {
  struct cpkt_audio_encoder_impl *impl;

  if (self == NULL) {
    return;
  }
  impl = (struct cpkt_audio_encoder_impl *)self->impl;
  if (impl != NULL) {
    if (!impl->closed) {
      ma_encoder_uninit(&impl->encoder);
    }
    free(impl);
  }
  free(self);
}

static cpkt_audio_result
cpkt_audio_decoder_alloc(cpkt_audio_decoder **out,
                         cpkt_audio_decoder **decoder_out,
                         struct cpkt_audio_decoder_impl **impl_out) {
  cpkt_audio_decoder *decoder;
  struct cpkt_audio_decoder_impl *impl;

  if (out == NULL || decoder_out == NULL || impl_out == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  *out = NULL;
  decoder = (cpkt_audio_decoder *)calloc(1, sizeof(*decoder));
  if (decoder == NULL) {
    return CPKT_AUDIO_ERR_ALLOC;
  }
  impl = (struct cpkt_audio_decoder_impl *)calloc(1, sizeof(*impl));
  if (impl == NULL) {
    free(decoder);
    return CPKT_AUDIO_ERR_ALLOC;
  }
  decoder->impl = impl;
  decoder->read_f32_mono_16k = cpkt_audio_decoder_read_f32_mono_16k_impl;
  decoder->info = cpkt_audio_decoder_info_impl;
  decoder->destroy = cpkt_audio_decoder_destroy_impl;
  *decoder_out = decoder;
  *impl_out = impl;
  return CPKT_AUDIO_OK;
}

static cpkt_audio_result
cpkt_audio_encoder_alloc(cpkt_audio_encoder **out,
                         cpkt_audio_encoder **encoder_out,
                         struct cpkt_audio_encoder_impl **impl_out) {
  cpkt_audio_encoder *encoder;
  struct cpkt_audio_encoder_impl *impl;

  if (out == NULL || encoder_out == NULL || impl_out == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  *out = NULL;
  encoder = (cpkt_audio_encoder *)calloc(1, sizeof(*encoder));
  if (encoder == NULL) {
    return CPKT_AUDIO_ERR_ALLOC;
  }
  impl = (struct cpkt_audio_encoder_impl *)calloc(1, sizeof(*impl));
  if (impl == NULL) {
    free(encoder);
    return CPKT_AUDIO_ERR_ALLOC;
  }
  encoder->impl = impl;
  encoder->write_f32 = cpkt_audio_encoder_write_f32_impl;
  encoder->close = cpkt_audio_encoder_close_impl;
  encoder->destroy = cpkt_audio_encoder_destroy_impl;
  *encoder_out = encoder;
  *impl_out = impl;
  return CPKT_AUDIO_OK;
}

static ma_decoder_config
cpkt_audio_ma_decoder_config(const cpkt_audio_decoder_config *config) {
  ma_decoder_config ma_config;

  ma_config = ma_decoder_config_init(ma_format_f32, 1, 16000);
  ma_config.encodingFormat = config != NULL
                                 ? cpkt_audio_to_ma_encoding(config->encoding)
                                 : ma_encoding_format_unknown;
  return ma_config;
}

static cpkt_audio_result
cpkt_audio_ma_encoder_config(const cpkt_audio_encoder_config *config,
                             ma_encoder_config *ma_config_out) {
  int format;
  unsigned long sample_rate;
  unsigned long channels;
  ma_encoding_format ma_format;

  if (ma_config_out == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }

  format = config != NULL && config->format != CPKT_AUDIO_FORMAT_UNKNOWN
               ? config->format
               : CPKT_AUDIO_FORMAT_WAV;
  if (!cpkt_audio_format_can_encode(format)) {
    return CPKT_AUDIO_ERR_FORMAT;
  }
  sample_rate =
      config != NULL && config->sample_rate != 0 ? config->sample_rate : 16000;
  channels = config != NULL && config->channels != 0 ? config->channels : 1;
  if (sample_rate > 0xffffffffUL || channels > 0xffffffffUL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  ma_format = cpkt_audio_format_to_ma_encoding(format);
  *ma_config_out = ma_encoder_config_init(
      ma_format, ma_format_f32, (ma_uint32)channels, (ma_uint32)sample_rate);
  return CPKT_AUDIO_OK;
}

/** Opens a receiver-shell decoder for a filesystem input path. */
cpkt_audio_result
cpkt_audio_decoder_open_file(cpkt_audio_decoder **out, const char *path,
                             const cpkt_audio_decoder_config *config) {
  cpkt_audio_decoder *decoder;
  struct cpkt_audio_decoder_impl *impl;
  ma_decoder_config ma_config;
  ma_result ma_status;
  cpkt_audio_result result;

  if (out == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  *out = NULL;
  if (path == NULL || path[0] == '\0') {
    return CPKT_AUDIO_ERR_ARG;
  }

  result = cpkt_audio_decoder_alloc(out, &decoder, &impl);
  if (result != CPKT_AUDIO_OK) {
    return result;
  }

  ma_config = cpkt_audio_ma_decoder_config(config);
  ma_status = ma_decoder_init_file(path, &ma_config, &impl->decoder);
  if (ma_status != MA_SUCCESS) {
    decoder->impl = NULL;
    free(impl);
    free(decoder);
    return cpkt_audio_from_ma_result(ma_status);
  }

  *out = decoder;
  return CPKT_AUDIO_OK;
}

/** Opens a receiver-shell decoder for callback-based input. */
cpkt_audio_result
cpkt_audio_decoder_open_reader(cpkt_audio_decoder **out,
                               const cpkt_audio_reader *reader,
                               const cpkt_audio_decoder_config *config) {
  cpkt_audio_decoder *decoder;
  struct cpkt_audio_decoder_impl *impl;
  ma_decoder_config ma_config;
  ma_result ma_status;
  cpkt_audio_result result;

  if (out == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  *out = NULL;
  if (reader == NULL || reader->read == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }

  result = cpkt_audio_decoder_alloc(out, &decoder, &impl);
  if (result != CPKT_AUDIO_OK) {
    return result;
  }
  impl->reader = *reader;

  ma_config = cpkt_audio_ma_decoder_config(config);
  ma_status = ma_decoder_init(cpkt_audio_reader_read, cpkt_audio_reader_seek,
                              &impl->reader, &ma_config, &impl->decoder);
  if (ma_status != MA_SUCCESS) {
    decoder->impl = NULL;
    free(impl);
    free(decoder);
    return cpkt_audio_from_ma_result(ma_status);
  }

  *out = decoder;
  return CPKT_AUDIO_OK;
}

/** Opens a receiver-shell encoder for a filesystem output path. */
cpkt_audio_result
cpkt_audio_encoder_open_file(cpkt_audio_encoder **out, const char *path,
                             const cpkt_audio_encoder_config *config) {
  cpkt_audio_encoder *encoder;
  struct cpkt_audio_encoder_impl *impl;
  ma_encoder_config ma_config;
  ma_result ma_status;
  cpkt_audio_result result;

  if (out == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  *out = NULL;
  if (path == NULL || path[0] == '\0') {
    return CPKT_AUDIO_ERR_ARG;
  }

  result = cpkt_audio_ma_encoder_config(config, &ma_config);
  if (result != CPKT_AUDIO_OK) {
    return result;
  }
  result = cpkt_audio_encoder_alloc(out, &encoder, &impl);
  if (result != CPKT_AUDIO_OK) {
    return result;
  }

  ma_status = ma_encoder_init_file(path, &ma_config, &impl->encoder);
  if (ma_status != MA_SUCCESS) {
    encoder->impl = NULL;
    free(impl);
    free(encoder);
    return cpkt_audio_from_ma_result(ma_status);
  }

  *out = encoder;
  return CPKT_AUDIO_OK;
}

/** Opens a receiver-shell encoder for callback-based output. */
cpkt_audio_result
cpkt_audio_encoder_open_writer(cpkt_audio_encoder **out,
                               const cpkt_audio_writer *writer,
                               const cpkt_audio_encoder_config *config) {
  cpkt_audio_encoder *encoder;
  struct cpkt_audio_encoder_impl *impl;
  ma_encoder_config ma_config;
  ma_result ma_status;
  cpkt_audio_result result;

  if (out == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  *out = NULL;
  if (writer == NULL || writer->write == NULL || writer->seek == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }

  result = cpkt_audio_ma_encoder_config(config, &ma_config);
  if (result != CPKT_AUDIO_OK) {
    return result;
  }
  result = cpkt_audio_encoder_alloc(out, &encoder, &impl);
  if (result != CPKT_AUDIO_OK) {
    return result;
  }
  impl->writer = *writer;

  ma_status = ma_encoder_init(cpkt_audio_writer_write, cpkt_audio_writer_seek,
                              &impl->writer, &ma_config, &impl->encoder);
  if (ma_status != MA_SUCCESS) {
    encoder->impl = NULL;
    free(impl);
    free(encoder);
    return cpkt_audio_from_ma_result(ma_status);
  }

  *out = encoder;
  return CPKT_AUDIO_OK;
}

/** Reports decode support for a public audio format. */
int cpkt_audio_format_can_decode(int format) {
  return format == CPKT_AUDIO_FORMAT_WAV || format == CPKT_AUDIO_FORMAT_FLAC ||
         format == CPKT_AUDIO_FORMAT_MP3;
}

/** Reports encode support for a public audio format. */
int cpkt_audio_format_can_encode(int format) {
  return format == CPKT_AUDIO_FORMAT_WAV;
}

/** Converts an audio result code into a stable diagnostic string. */
const char *cpkt_audio_result_string(cpkt_audio_result result) {
  switch (result) {
  case CPKT_AUDIO_OK:
    return "ok";
  case CPKT_AUDIO_ERR_ARG:
    return "invalid argument";
  case CPKT_AUDIO_ERR_ALLOC:
    return "allocation failed";
  case CPKT_AUDIO_ERR_IO:
    return "I/O error";
  case CPKT_AUDIO_ERR_FORMAT:
    return "unsupported or invalid audio format";
  case CPKT_AUDIO_ERR_UPSTREAM:
    return "audio backend error";
  case CPKT_AUDIO_AT_END:
    return "end of stream";
  default:
    return "unknown audio result";
  }
}
