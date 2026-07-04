#include <cpkt/audio.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>
#include <miniaudio.h>

#define CPKT_AUDIO_URL_REWIND_BYTES 1048576U
#define CPKT_AUDIO_URL_BUFFER_CHUNK 16384U

struct cpkt_audio_url_source {
  CURLM *multi;
  CURL *easy;
  unsigned char *buffer;
  size_t buffer_base;
  size_t buffer_size;
  size_t buffer_cursor;
  size_t buffer_capacity;
  int running;
  int done;
  int failed;
  char error[CURL_ERROR_SIZE];
};

struct cpkt_audio_decoder_impl {
  ma_decoder decoder;
  cpkt_audio_reader reader;
  struct cpkt_audio_url_source *url_source;
  int source_format;
  int owns_reader;
  int callback_error;
};

struct cpkt_audio_encoder_impl {
  ma_encoder encoder;
  cpkt_audio_writer writer;
  int closed;
  int callback_error;
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

static int cpkt_audio_format_from_encoding(int encoding) {
  switch (encoding) {
  case CPKT_AUDIO_ENCODING_WAV:
    return CPKT_AUDIO_FORMAT_WAV;
  case CPKT_AUDIO_ENCODING_FLAC:
    return CPKT_AUDIO_FORMAT_FLAC;
  case CPKT_AUDIO_ENCODING_MP3:
    return CPKT_AUDIO_FORMAT_MP3;
  default:
    return CPKT_AUDIO_FORMAT_UNKNOWN;
  }
}

static int cpkt_audio_format_from_signature(const unsigned char *data,
                                            size_t size) {
  if (data == NULL || size < 4U) {
    return CPKT_AUDIO_FORMAT_UNKNOWN;
  }
  if (size >= 12U && memcmp(data, "RIFF", 4U) == 0 &&
      memcmp(data + 8U, "WAVE", 4U) == 0) {
    return CPKT_AUDIO_FORMAT_WAV;
  }
  if (memcmp(data, "fLaC", 4U) == 0) {
    return CPKT_AUDIO_FORMAT_FLAC;
  }
  if (size >= 3U && memcmp(data, "ID3", 3U) == 0) {
    return CPKT_AUDIO_FORMAT_MP3;
  }
  if (size >= 2U && data[0] == 0xffU && (data[1] & 0xe0U) == 0xe0U) {
    return CPKT_AUDIO_FORMAT_MP3;
  }
  return CPKT_AUDIO_FORMAT_UNKNOWN;
}

static int cpkt_audio_detect_file_format(const char *path) {
  unsigned char header[16];
  FILE *file;
  size_t read_size;

  if (path == NULL || path[0] == '\0') {
    return CPKT_AUDIO_FORMAT_UNKNOWN;
  }
  file = fopen(path, "rb");
  if (file == NULL) {
    return CPKT_AUDIO_FORMAT_UNKNOWN;
  }
  read_size = fread(header, 1U, sizeof(header), file);
  fclose(file);
  return cpkt_audio_format_from_signature(header, read_size);
}

static int
cpkt_audio_source_format_from_config(const cpkt_audio_decoder_config *config) {
  if (config == NULL) {
    return CPKT_AUDIO_FORMAT_UNKNOWN;
  }
  return cpkt_audio_format_from_encoding(config->encoding);
}

static void cpkt_audio_url_source_destroy(struct cpkt_audio_url_source *source) {
  if (source == NULL) {
    return;
  }
  if (source->multi != NULL && source->easy != NULL) {
    (void)curl_multi_remove_handle(source->multi, source->easy);
  }
  if (source->easy != NULL) {
    curl_easy_cleanup(source->easy);
  }
  if (source->multi != NULL) {
    curl_multi_cleanup(source->multi);
  }
  free(source->buffer);
  free(source);
}

static int cpkt_audio_size_add(size_t left, size_t right, size_t *out) {
  if (out == NULL || left > ((size_t)-1) - right) {
    return 0;
  }
  *out = left + right;
  return 1;
}

static size_t cpkt_audio_url_buffer_end(
    const struct cpkt_audio_url_source *source) {
  size_t end;

  if (source == NULL ||
      !cpkt_audio_size_add(source->buffer_base, source->buffer_size, &end)) {
    return (size_t)-1;
  }
  return end;
}

static void
cpkt_audio_url_source_discard_consumed(struct cpkt_audio_url_source *source) {
  size_t keep_from;
  size_t discard;

  if (source == NULL || source->buffer_cursor <= source->buffer_base) {
    return;
  }
  keep_from = source->buffer_cursor > CPKT_AUDIO_URL_REWIND_BYTES
                  ? source->buffer_cursor - CPKT_AUDIO_URL_REWIND_BYTES
                  : 0U;
  if (keep_from <= source->buffer_base) {
    return;
  }
  discard = keep_from - source->buffer_base;
  if (discard > source->buffer_size) {
    source->failed = 1;
    return;
  }
  if (discard < source->buffer_size) {
    memmove(source->buffer, source->buffer + discard,
            source->buffer_size - discard);
  }
  source->buffer_base += discard;
  source->buffer_size -= discard;
}

static size_t cpkt_audio_url_write(void *buffer, size_t size, size_t nmemb,
                                   void *user) {
  struct cpkt_audio_url_source *source;
  size_t byte_count;
  size_t needed;
  unsigned char *grown;

  source = (struct cpkt_audio_url_source *)user;
  if (source == NULL || size == 0U) {
    return 0U;
  }
  if (nmemb > ((size_t)-1) / size) {
    source->failed = 1;
    return 0U;
  }
  byte_count = size * nmemb;
  if (byte_count == 0U) {
    return 0U;
  }
  cpkt_audio_url_source_discard_consumed(source);
  if (source->failed) {
    return 0U;
  }
  if (source->buffer_size > ((size_t)-1) - byte_count) {
    source->failed = 1;
    return 0U;
  }
  needed = source->buffer_size + byte_count;
  if (needed > source->buffer_capacity) {
    size_t next_capacity;

    next_capacity = source->buffer_capacity == 0U
                        ? CPKT_AUDIO_URL_BUFFER_CHUNK
                        : source->buffer_capacity;
    while (next_capacity < needed) {
      if (next_capacity > ((size_t)-1) / 2U) {
        source->failed = 1;
        return 0U;
      }
      next_capacity *= 2U;
    }
    grown = (unsigned char *)realloc(source->buffer, next_capacity);
    if (grown == NULL) {
      source->failed = 1;
      return 0U;
    }
    source->buffer = grown;
    source->buffer_capacity = next_capacity;
  }
  memcpy(source->buffer + source->buffer_size, buffer, byte_count);
  source->buffer_size += byte_count;
  return byte_count;
}

static void
cpkt_audio_url_source_collect_done(struct cpkt_audio_url_source *source) {
  CURLMsg *message;
  int queued;
  long status;

  if (source == NULL || source->multi == NULL) {
    return;
  }
  queued = 0;
  while ((message = curl_multi_info_read(source->multi, &queued)) != NULL) {
    if (message->msg == CURLMSG_DONE && message->easy_handle == source->easy) {
      source->done = 1;
      source->running = 0;
      if (message->data.result != CURLE_OK) {
        source->failed = 1;
      }
      status = 0L;
      if (curl_easy_getinfo(source->easy, CURLINFO_RESPONSE_CODE, &status) ==
              CURLE_OK &&
          status >= 400L) {
        source->failed = 1;
      }
    }
  }
}

static int cpkt_audio_url_source_progress(struct cpkt_audio_url_source *source) {
  CURLMcode code;

  if (source == NULL || source->multi == NULL || source->failed) {
    return 1;
  }
  do {
    code = curl_multi_perform(source->multi, &source->running);
  } while (code == CURLM_CALL_MULTI_PERFORM);
  if (code != CURLM_OK) {
    source->failed = 1;
    return 1;
  }
  cpkt_audio_url_source_collect_done(source);
  return source->failed ? 1 : 0;
}

static int cpkt_audio_url_source_download_until(
    struct cpkt_audio_url_source *source, size_t target_offset) {
  int queued;

  if (source == NULL) {
    return 1;
  }
  while (cpkt_audio_url_buffer_end(source) < target_offset && !source->done &&
         !source->failed) {
    if (cpkt_audio_url_source_progress(source) != 0) {
      break;
    }
    if (cpkt_audio_url_buffer_end(source) < target_offset && !source->done &&
        !source->failed) {
      queued = 0;
      if (curl_multi_poll(source->multi, NULL, 0U, 1000, &queued) != CURLM_OK) {
        source->failed = 1;
        break;
      }
    }
  }
  return source->failed ? 1 : 0;
}

static size_t cpkt_audio_url_read(void *user, void *buffer,
                                  size_t bytes_to_read) {
  struct cpkt_audio_url_source *source;
  size_t target_offset;
  size_t buffer_end;
  size_t buffer_offset;
  size_t available;
  size_t to_copy;

  source = (struct cpkt_audio_url_source *)user;
  if (source == NULL || buffer == NULL || bytes_to_read == 0U) {
    return 0U;
  }
  if (source->buffer_cursor > ((size_t)-1) - bytes_to_read) {
    source->failed = 1;
    return 0U;
  }

  target_offset = source->buffer_cursor + bytes_to_read;
  if (cpkt_audio_url_source_download_until(source, target_offset) != 0) {
    return 0U;
  }
  buffer_end = cpkt_audio_url_buffer_end(source);
  if (source->buffer_cursor < source->buffer_base ||
      source->buffer_cursor >= buffer_end) {
    return 0U;
  }
  buffer_offset = source->buffer_cursor - source->buffer_base;
  available = buffer_end - source->buffer_cursor;
  to_copy = available < bytes_to_read ? available : bytes_to_read;
  memcpy(buffer, source->buffer + buffer_offset, to_copy);
  source->buffer_cursor += to_copy;
  cpkt_audio_url_source_discard_consumed(source);
  return to_copy;
}

static int cpkt_audio_url_seek(void *user, long offset, int origin) {
  struct cpkt_audio_url_source *source;
  long base;
  long target_long;
  size_t target;
  size_t buffer_end;

  source = (struct cpkt_audio_url_source *)user;
  if (source == NULL) {
    return -1;
  }
  if (origin == CPKT_AUDIO_SEEK_SET) {
    base = 0L;
  } else if (origin == CPKT_AUDIO_SEEK_CUR) {
    if (source->buffer_cursor > (size_t)LONG_MAX) {
      return -1;
    }
    base = (long)source->buffer_cursor;
  } else {
    return -1;
  }
  if ((offset > 0 && base > LONG_MAX - offset) ||
      (offset < 0 && base < LONG_MIN - offset)) {
    return -1;
  }
  target_long = base + offset;
  if (target_long < 0) {
    return -1;
  }
  target = (size_t)target_long;
  if (cpkt_audio_url_source_download_until(source, target) != 0) {
    return -1;
  }
  buffer_end = cpkt_audio_url_buffer_end(source);
  if (target < source->buffer_base || target > buffer_end) {
    return -1;
  }
  source->buffer_cursor = target;
  return 0;
}

static cpkt_audio_result
cpkt_audio_url_source_create(struct cpkt_audio_url_source **out,
                             const char *url) {
  struct cpkt_audio_url_source *source;
  CURLMcode multi_code;

  if (out == NULL || url == NULL || url[0] == '\0') {
    return CPKT_AUDIO_ERR_ARG;
  }
  *out = NULL;
  if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
    return CPKT_AUDIO_ERR_UPSTREAM;
  }

  source = (struct cpkt_audio_url_source *)calloc(1, sizeof(*source));
  if (source == NULL) {
    return CPKT_AUDIO_ERR_ALLOC;
  }
  source->easy = curl_easy_init();
  source->multi = curl_multi_init();
  if (source->easy == NULL || source->multi == NULL) {
    cpkt_audio_url_source_destroy(source);
    return CPKT_AUDIO_ERR_ALLOC;
  }

  (void)curl_easy_setopt(source->easy, CURLOPT_URL, url);
  (void)curl_easy_setopt(source->easy, CURLOPT_FOLLOWLOCATION, 1L);
  (void)curl_easy_setopt(source->easy, CURLOPT_FAILONERROR, 1L);
  (void)curl_easy_setopt(source->easy, CURLOPT_WRITEFUNCTION,
                         cpkt_audio_url_write);
  (void)curl_easy_setopt(source->easy, CURLOPT_WRITEDATA, source);
  (void)curl_easy_setopt(source->easy, CURLOPT_ERRORBUFFER, source->error);
  (void)curl_easy_setopt(source->easy, CURLOPT_CONNECTTIMEOUT, 30L);
  (void)curl_easy_setopt(source->easy, CURLOPT_LOW_SPEED_LIMIT, 1L);
  (void)curl_easy_setopt(source->easy, CURLOPT_LOW_SPEED_TIME, 60L);
  (void)curl_easy_setopt(source->easy, CURLOPT_USERAGENT,
                         "c.pkt.systems-cpkt-audio/0");

  multi_code = curl_multi_add_handle(source->multi, source->easy);
  if (multi_code != CURLM_OK) {
    cpkt_audio_url_source_destroy(source);
    return CPKT_AUDIO_ERR_UPSTREAM;
  }
  source->running = 1;
  *out = source;
  return CPKT_AUDIO_OK;
}

static cpkt_audio_result cpkt_audio_detect_reader_format(
    int *format_out, const cpkt_audio_reader *reader) {
  unsigned char header[16];
  size_t read_size;

  if (format_out != NULL) {
    *format_out = CPKT_AUDIO_FORMAT_UNKNOWN;
  }
  if (format_out == NULL || reader == NULL || reader->read == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  if (reader->seek == NULL) {
    return CPKT_AUDIO_OK;
  }

  read_size = reader->read(reader->user, header, sizeof(header));
  if (read_size > sizeof(header)) {
    return CPKT_AUDIO_ERR_IO;
  }
  if (reader->seek(reader->user, 0L, CPKT_AUDIO_SEEK_SET) != 0) {
    return CPKT_AUDIO_ERR_IO;
  }
  *format_out = cpkt_audio_format_from_signature(header, read_size);
  return CPKT_AUDIO_OK;
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
      result == MA_ACCESS_DENIED || result == MA_NOT_IMPLEMENTED) {
    return CPKT_AUDIO_ERR_IO;
  }
  return CPKT_AUDIO_ERR_UPSTREAM;
}

static ma_result cpkt_audio_reader_read(ma_decoder *decoder, void *buffer,
                                        size_t bytes_to_read,
                                        size_t *bytes_read) {
  cpkt_audio_reader *reader;
  struct cpkt_audio_decoder_impl *impl;
  size_t nread;
  size_t total_read;
  size_t remaining;

  impl = (struct cpkt_audio_decoder_impl *)decoder->pUserData;
  reader = impl != NULL ? &impl->reader : NULL;
  if (reader == NULL || reader->read == NULL || bytes_read == NULL) {
    if (impl != NULL) {
      impl->callback_error = 1;
    }
    return MA_INVALID_ARGS;
  }

  total_read = 0;
  while (total_read < bytes_to_read) {
    remaining = bytes_to_read - total_read;
    nread = reader->read(reader->user, (unsigned char *)buffer + total_read,
                         remaining);
    if (nread > remaining) {
      impl->callback_error = 1;
      return MA_IO_ERROR;
    }
    if (nread == 0) {
      break;
    }
    total_read += nread;
  }
  *bytes_read = total_read;
  return MA_SUCCESS;
}

static ma_result cpkt_audio_reader_seek(ma_decoder *decoder, ma_int64 offset,
                                        ma_seek_origin origin) {
  cpkt_audio_reader *reader;
  struct cpkt_audio_decoder_impl *impl;
  int cpkt_origin;

  impl = (struct cpkt_audio_decoder_impl *)decoder->pUserData;
  reader = impl != NULL ? &impl->reader : NULL;
  if (reader == NULL || reader->seek == NULL) {
    if (impl != NULL) {
      impl->callback_error = 1;
    }
    return MA_NOT_IMPLEMENTED;
  }
  if (offset > LONG_MAX || offset < LONG_MIN) {
    impl->callback_error = 1;
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
    impl->callback_error = 1;
    return MA_IO_ERROR;
  }
  return MA_SUCCESS;
}

static ma_result cpkt_audio_writer_write(ma_encoder *encoder,
                                         const void *buffer,
                                         size_t bytes_to_write,
                                         size_t *bytes_written) {
  cpkt_audio_writer *writer;
  struct cpkt_audio_encoder_impl *impl;
  size_t nwritten;

  impl = (struct cpkt_audio_encoder_impl *)encoder->pUserData;
  writer = impl != NULL ? &impl->writer : NULL;
  if (writer == NULL || writer->write == NULL || bytes_written == NULL) {
    if (impl != NULL) {
      impl->callback_error = 1;
    }
    return MA_INVALID_ARGS;
  }

  nwritten = writer->write(writer->user, buffer, bytes_to_write);
  if (nwritten > bytes_to_write) {
    impl->callback_error = 1;
    return MA_IO_ERROR;
  }
  *bytes_written = nwritten;
  if (nwritten != bytes_to_write) {
    impl->callback_error = 1;
  }
  return nwritten == bytes_to_write ? MA_SUCCESS : MA_IO_ERROR;
}

static ma_result cpkt_audio_writer_seek(ma_encoder *encoder, ma_int64 offset,
                                        ma_seek_origin origin) {
  cpkt_audio_writer *writer;
  struct cpkt_audio_encoder_impl *impl;
  int cpkt_origin;

  impl = (struct cpkt_audio_encoder_impl *)encoder->pUserData;
  writer = impl != NULL ? &impl->writer : NULL;
  if (writer == NULL || writer->seek == NULL) {
    return MA_NOT_IMPLEMENTED;
  }
  if (offset > LONG_MAX || offset < LONG_MIN) {
    impl->callback_error = 1;
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
    impl->callback_error = 1;
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
  if (result != MA_SUCCESS && result != MA_AT_END && impl->callback_error) {
    return CPKT_AUDIO_ERR_IO;
  }
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
  info->source_format = impl->source_format;
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
    cpkt_audio_url_source_destroy(impl->url_source);
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
  if (result != MA_SUCCESS && impl->callback_error) {
    return CPKT_AUDIO_ERR_IO;
  }
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
  if (impl->callback_error) {
    return CPKT_AUDIO_ERR_IO;
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
  impl->source_format = cpkt_audio_detect_file_format(path);
  if (impl->source_format == CPKT_AUDIO_FORMAT_UNKNOWN) {
    impl->source_format = cpkt_audio_source_format_from_config(config);
  }
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

/** Opens a receiver-shell decoder for streaming URL input. */
cpkt_audio_result
cpkt_audio_decoder_open_url(cpkt_audio_decoder **out, const char *url,
                            const cpkt_audio_decoder_config *config) {
  cpkt_audio_decoder *decoder;
  struct cpkt_audio_decoder_impl *impl;
  struct cpkt_audio_url_source *source;
  cpkt_audio_reader reader;
  ma_decoder_config ma_config;
  ma_result ma_status;
  cpkt_audio_result result;

  if (out == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  *out = NULL;
  if (url == NULL || url[0] == '\0') {
    return CPKT_AUDIO_ERR_ARG;
  }

  source = NULL;
  result = cpkt_audio_url_source_create(&source, url);
  if (result != CPKT_AUDIO_OK) {
    return result;
  }

  result = cpkt_audio_decoder_alloc(out, &decoder, &impl);
  if (result != CPKT_AUDIO_OK) {
    cpkt_audio_url_source_destroy(source);
    return result;
  }

  memset(&reader, 0, sizeof(reader));
  reader.user = source;
  reader.read = cpkt_audio_url_read;
  reader.seek = cpkt_audio_url_seek;
  impl->reader = reader;
  impl->url_source = source;
  impl->source_format = cpkt_audio_source_format_from_config(config);

  ma_config = cpkt_audio_ma_decoder_config(config);
  ma_status = ma_decoder_init(cpkt_audio_reader_read, cpkt_audio_reader_seek,
                              impl, &ma_config, &impl->decoder);
  if (ma_status != MA_SUCCESS) {
    result = impl->callback_error ? CPKT_AUDIO_ERR_IO
                                  : cpkt_audio_from_ma_result(ma_status);
    decoder->impl = NULL;
    cpkt_audio_url_source_destroy(source);
    free(impl);
    free(decoder);
    return result;
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
  result = cpkt_audio_detect_reader_format(&impl->source_format, reader);
  if (result != CPKT_AUDIO_OK) {
    decoder->impl = NULL;
    free(impl);
    free(decoder);
    return result;
  }
  if (impl->source_format == CPKT_AUDIO_FORMAT_UNKNOWN) {
    impl->source_format = cpkt_audio_source_format_from_config(config);
  }
  impl->reader = *reader;

  ma_config = cpkt_audio_ma_decoder_config(config);
  ma_status = ma_decoder_init(cpkt_audio_reader_read, cpkt_audio_reader_seek,
                              impl, &ma_config, &impl->decoder);
  if (ma_status != MA_SUCCESS) {
    result = impl->callback_error ? CPKT_AUDIO_ERR_IO
                                  : cpkt_audio_from_ma_result(ma_status);
    decoder->impl = NULL;
    free(impl);
    free(decoder);
    return result;
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
                              impl, &ma_config, &impl->encoder);
  if (ma_status != MA_SUCCESS) {
    result = impl->callback_error ? CPKT_AUDIO_ERR_IO
                                  : cpkt_audio_from_ma_result(ma_status);
    encoder->impl = NULL;
    free(impl);
    free(encoder);
    return result;
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
