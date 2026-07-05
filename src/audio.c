#include <cpkt/audio.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__unix__) || defined(__APPLE__)
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <curl/curl.h>
#define MA_API static
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#define MA_NO_GENERATION
#ifndef CPKT_AUDIO_NATIVE_RUNTIME_DEVICE_IO
#define MA_NO_ALSA
#define MA_NO_PULSEAUDIO
#define MA_NO_JACK
#define MA_NO_RUNTIME_LINKING
#endif
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#if defined(__GNUC__) || defined(__clang__)
#define CPKT_AUDIO_EXPORT __attribute__((visibility("default")))
#else
#define CPKT_AUDIO_EXPORT
#endif

#define CPKT_AUDIO_URL_REWIND_BYTES 1048576U
#define CPKT_AUDIO_URL_BUFFER_CHUNK 16384U
#define CPKT_AUDIO_DECODER_MODE_MA 0
#define CPKT_AUDIO_DECODER_MODE_DRMP3 1
#define CPKT_AUDIO_MP3_INPUT_FRAMES MA_DR_MP3_MAX_PCM_FRAMES_PER_MP3_FRAME
#define CPKT_AUDIO_MP3_BYTE_CHUNK 4096U
#define CPKT_AUDIO_DEVICE_MODE_NATIVE 0
#define CPKT_AUDIO_DEVICE_MODE_PROCESS_ARECORD 1
#define CPKT_AUDIO_PROCESS_FRAME_CHUNK 1024U
#define CPKT_AUDIO_PROCESS_IDLE_RESET_MS 64UL

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
  char content_type[128];
};

struct cpkt_audio_decoder_impl {
  ma_decoder decoder;
  ma_dr_mp3dec mp3;
  ma_data_converter converter;
  cpkt_audio_reader reader;
  struct cpkt_audio_url_source *url_source;
  ma_int16 *mp3_input;
  unsigned char *mp3_bytes;
  size_t mp3_byte_size;
  size_t mp3_byte_cursor;
  size_t mp3_byte_capacity;
  ma_uint64 mp3_input_frames;
  ma_uint64 mp3_input_cursor;
  ma_uint32 mp3_channels;
  ma_uint32 mp3_sample_rate;
  int source_format;
  int mode;
  int owns_reader;
  int callback_error;
  int mp3_eof;
  int converter_initialized;
  int decoder_initialized;
};

struct cpkt_audio_encoder_impl {
  ma_encoder encoder;
  cpkt_audio_writer writer;
  int closed;
  int callback_error;
};

struct cpkt_audio_capture_impl {
  ma_device device;
  ma_pcm_rb rb;
  cpkt_audio_capture_state_sink state_sink;
  void *state_user;
#if defined(__unix__) || defined(__APPLE__)
  pid_t process_pid;
  int process_fd;
#endif
  unsigned char pending_byte;
  unsigned long last_read_ms;
  int mode;
  int device_initialized;
  int rb_initialized;
  int started;
  int overrun;
  int has_pending_byte;
};

struct cpkt_audio_playback_impl {
  ma_device device;
  ma_pcm_rb rb;
#if defined(__unix__) || defined(__APPLE__)
  pid_t process_pid;
  int process_fd;
#endif
  int mode;
  int device_initialized;
  int rb_initialized;
  int started;
  int underrun;
};

struct cpkt_audio_vox_impl {
  cpkt_audio_vox_config config;
  float *frames;
  float *prebuffer_frames;
  size_t frame_capacity;
  size_t prebuffer_capacity;
  size_t memory_frame_count;
  size_t total_frame_count;
  size_t speech_frame_count;
  size_t read_cursor;
  size_t prebuffer_count;
  size_t prebuffer_cursor;
  size_t source_frame_cursor;
  size_t segment_start_frame;
  FILE *spool_file;
  unsigned long release_silence_frames;
  unsigned long max_segment_frames;
  unsigned long min_segment_frames;
  size_t max_spool_frames;
  unsigned long silence_frames;
  unsigned long segment_index;
  float threshold;
  int open;
  int ptt_pressed;
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

static int cpkt_audio_encoding_from_format(int format) {
  switch (format) {
  case CPKT_AUDIO_FORMAT_WAV:
    return CPKT_AUDIO_ENCODING_WAV;
  case CPKT_AUDIO_FORMAT_FLAC:
    return CPKT_AUDIO_ENCODING_FLAC;
  case CPKT_AUDIO_FORMAT_MP3:
    return CPKT_AUDIO_ENCODING_MP3;
  default:
    return CPKT_AUDIO_ENCODING_UNKNOWN;
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

static int cpkt_audio_ascii_tolower(int ch) {
  if (ch >= 'A' && ch <= 'Z') {
    return ch + ('a' - 'A');
  }
  return ch;
}

static int cpkt_audio_ascii_ieq_char(char left, char right) {
  return cpkt_audio_ascii_tolower((unsigned char)left) ==
         cpkt_audio_ascii_tolower((unsigned char)right);
}

static int cpkt_audio_ascii_istarts_with(const char *text, const char *prefix) {
  if (text == NULL || prefix == NULL) {
    return 0;
  }
  while (*prefix != '\0') {
    if (*text == '\0' || !cpkt_audio_ascii_ieq_char(*text, *prefix)) {
      return 0;
    }
    ++text;
    ++prefix;
  }
  return 1;
}

static int cpkt_audio_content_type_matches(const char *content_type,
                                           const char *mime) {
  const char *cursor;

  if (content_type == NULL || mime == NULL) {
    return 0;
  }
  cursor = content_type;
  while (*cursor == ' ' || *cursor == '\t') {
    ++cursor;
  }
  while (*mime != '\0') {
    if (*cursor == '\0' || *cursor == ';' || *cursor == '\r' ||
        *cursor == '\n' || !cpkt_audio_ascii_ieq_char(*cursor, *mime)) {
      return 0;
    }
    ++cursor;
    ++mime;
  }
  return *cursor == '\0' || *cursor == ';' || *cursor == '\r' ||
         *cursor == '\n' || *cursor == ' ' || *cursor == '\t';
}

static int cpkt_audio_format_from_content_type(const char *content_type) {
  if (cpkt_audio_content_type_matches(content_type, "audio/mpeg") ||
      cpkt_audio_content_type_matches(content_type, "audio/mp3") ||
      cpkt_audio_content_type_matches(content_type, "audio/x-mpeg") ||
      cpkt_audio_content_type_matches(content_type, "audio/x-mp3") ||
      cpkt_audio_content_type_matches(content_type, "audio/mpeg3")) {
    return CPKT_AUDIO_FORMAT_MP3;
  }
  if (cpkt_audio_content_type_matches(content_type, "audio/flac") ||
      cpkt_audio_content_type_matches(content_type, "audio/x-flac")) {
    return CPKT_AUDIO_FORMAT_FLAC;
  }
  if (cpkt_audio_content_type_matches(content_type, "audio/wav") ||
      cpkt_audio_content_type_matches(content_type, "audio/wave") ||
      cpkt_audio_content_type_matches(content_type, "audio/x-wav") ||
      cpkt_audio_content_type_matches(content_type, "audio/vnd.wave")) {
    return CPKT_AUDIO_FORMAT_WAV;
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

static void
cpkt_audio_url_source_destroy(struct cpkt_audio_url_source *source) {
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

static size_t
cpkt_audio_url_buffer_end(const struct cpkt_audio_url_source *source) {
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

    next_capacity = source->buffer_capacity == 0U ? CPKT_AUDIO_URL_BUFFER_CHUNK
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

static size_t cpkt_audio_url_header(char *buffer, size_t size, size_t nmemb,
                                    void *user) {
  struct cpkt_audio_url_source *source;
  size_t byte_count;
  const char *cursor;
  size_t copied;

  source = (struct cpkt_audio_url_source *)user;
  if (source == NULL || buffer == NULL || size == 0U) {
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
  if (byte_count < 13U ||
      !cpkt_audio_ascii_istarts_with(buffer, "Content-Type:")) {
    return byte_count;
  }

  cursor = buffer + 13;
  while ((size_t)(cursor - buffer) < byte_count &&
         (*cursor == ' ' || *cursor == '\t')) {
    ++cursor;
  }
  copied = 0U;
  while ((size_t)(cursor - buffer) < byte_count && *cursor != '\r' &&
         *cursor != '\n' && copied + 1U < sizeof(source->content_type)) {
    source->content_type[copied++] = *cursor++;
  }
  while (copied > 0U && (source->content_type[copied - 1U] == ' ' ||
                         source->content_type[copied - 1U] == '\t')) {
    --copied;
  }
  source->content_type[copied] = '\0';
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

static int
cpkt_audio_url_source_progress(struct cpkt_audio_url_source *source) {
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

static cpkt_audio_result
cpkt_audio_url_source_detect_format(struct cpkt_audio_url_source *source,
                                    int *format_out) {
  int format;
  int queued;
  size_t buffer_end;
  size_t inspect_size;

  if (format_out != NULL) {
    *format_out = CPKT_AUDIO_FORMAT_UNKNOWN;
  }
  if (source == NULL || format_out == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }

  format = cpkt_audio_format_from_content_type(source->content_type);
  while (format == CPKT_AUDIO_FORMAT_UNKNOWN && !source->done &&
         !source->failed) {
    buffer_end = cpkt_audio_url_buffer_end(source);
    if (buffer_end >= 16U) {
      break;
    }
    if (cpkt_audio_url_source_progress(source) != 0) {
      break;
    }
    format = cpkt_audio_format_from_content_type(source->content_type);
    buffer_end = cpkt_audio_url_buffer_end(source);
    if (format != CPKT_AUDIO_FORMAT_UNKNOWN || buffer_end >= 16U ||
        source->done || source->failed) {
      break;
    }
    queued = 0;
    if (curl_multi_poll(source->multi, NULL, 0U, 1000, &queued) != CURLM_OK) {
      source->failed = 1;
      break;
    }
  }
  if (source->failed) {
    return CPKT_AUDIO_ERR_IO;
  }

  if (format == CPKT_AUDIO_FORMAT_UNKNOWN) {
    buffer_end = cpkt_audio_url_buffer_end(source);
    if (source->buffer_base == 0U && buffer_end != (size_t)-1) {
      inspect_size =
          buffer_end < source->buffer_size ? buffer_end : source->buffer_size;
      if (inspect_size > 16U) {
        inspect_size = 16U;
      }
      format = cpkt_audio_format_from_signature(source->buffer, inspect_size);
    }
  }
  *format_out = format;
  return CPKT_AUDIO_OK;
}

static int
cpkt_audio_url_source_download_until(struct cpkt_audio_url_source *source,
                                     size_t target_offset) {
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

static size_t cpkt_audio_drmp3_url_read(void *user, void *buffer,
                                        size_t bytes_to_read) {
  struct cpkt_audio_url_source *source;
  size_t buffer_end;
  size_t buffer_offset;
  size_t available;
  size_t to_copy;
  int queued;

  source = (struct cpkt_audio_url_source *)user;
  if (source == NULL || buffer == NULL || bytes_to_read == 0U) {
    return 0U;
  }
  while (!source->done && !source->failed) {
    buffer_end = cpkt_audio_url_buffer_end(source);
    if (source->buffer_cursor < buffer_end) {
      break;
    }
    if (cpkt_audio_url_source_progress(source) != 0) {
      break;
    }
    buffer_end = cpkt_audio_url_buffer_end(source);
    if (source->buffer_cursor < buffer_end) {
      break;
    }
    queued = 0;
    if (curl_multi_poll(source->multi, NULL, 0U, 1000, &queued) != CURLM_OK) {
      source->failed = 1;
      break;
    }
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

static int cpkt_audio_mp3_buffer_reserve(struct cpkt_audio_decoder_impl *impl,
                                         size_t extra) {
  unsigned char *grown;
  size_t needed;
  size_t next_capacity;

  if (impl == NULL || extra > ((size_t)-1) - impl->mp3_byte_size) {
    return 1;
  }
  needed = impl->mp3_byte_size + extra;
  if (needed <= impl->mp3_byte_capacity) {
    return 0;
  }
  next_capacity = impl->mp3_byte_capacity == 0U ? CPKT_AUDIO_MP3_BYTE_CHUNK
                                                : impl->mp3_byte_capacity;
  while (next_capacity < needed) {
    if (next_capacity > ((size_t)-1) / 2U) {
      return 1;
    }
    next_capacity *= 2U;
  }
  grown = (unsigned char *)realloc(impl->mp3_bytes, next_capacity);
  if (grown == NULL) {
    return 1;
  }
  impl->mp3_bytes = grown;
  impl->mp3_byte_capacity = next_capacity;
  return 0;
}

static void
cpkt_audio_mp3_buffer_compact(struct cpkt_audio_decoder_impl *impl) {
  size_t remaining;

  if (impl == NULL || impl->mp3_byte_cursor == 0U) {
    return;
  }
  remaining = impl->mp3_byte_size - impl->mp3_byte_cursor;
  if (remaining > 0U) {
    memmove(impl->mp3_bytes, impl->mp3_bytes + impl->mp3_byte_cursor,
            remaining);
  }
  impl->mp3_byte_size = remaining;
  impl->mp3_byte_cursor = 0U;
}

static int cpkt_audio_mp3_skip_id3v2(struct cpkt_audio_decoder_impl *impl) {
  size_t tag_size;

  if (impl == NULL || impl->mp3_byte_size < 3U ||
      memcmp(impl->mp3_bytes, "ID3", 3U) != 0) {
    return 0;
  }
  if (impl->mp3_byte_size < 10U) {
    return 2;
  }
  if ((impl->mp3_bytes[6] & 0x80U) != 0U ||
      (impl->mp3_bytes[7] & 0x80U) != 0U ||
      (impl->mp3_bytes[8] & 0x80U) != 0U ||
      (impl->mp3_bytes[9] & 0x80U) != 0U) {
    return 1;
  }
  tag_size = ((size_t)impl->mp3_bytes[6] << 21U) |
             ((size_t)impl->mp3_bytes[7] << 14U) |
             ((size_t)impl->mp3_bytes[8] << 7U) | (size_t)impl->mp3_bytes[9];
  tag_size += 10U;
  if ((impl->mp3_bytes[5] & 0x10U) != 0U) {
    tag_size += 10U;
  }
  if (tag_size > impl->mp3_byte_size) {
    return 2;
  }
  impl->mp3_byte_cursor = tag_size;
  cpkt_audio_mp3_buffer_compact(impl);
  return 0;
}

static void cpkt_audio_mp3_skip_to_sync(struct cpkt_audio_decoder_impl *impl) {
  while (impl != NULL && impl->mp3_byte_size - impl->mp3_byte_cursor >= 2U) {
    if (impl->mp3_bytes[impl->mp3_byte_cursor] == 0xffU &&
        (impl->mp3_bytes[impl->mp3_byte_cursor + 1U] & 0xe0U) == 0xe0U) {
      break;
    }
    impl->mp3_byte_cursor += 1U;
  }
  cpkt_audio_mp3_buffer_compact(impl);
}

static cpkt_audio_result
cpkt_audio_mp3_decode_next_frame(struct cpkt_audio_decoder_impl *impl) {
  unsigned char chunk[CPKT_AUDIO_MP3_BYTE_CHUNK];
  ma_dr_mp3dec_frame_info frame_info;
  int frame_count;
  size_t bytes_read;
  size_t available;
  int id3_status;

  if (impl == NULL || impl->url_source == NULL || impl->mp3_input == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  impl->mp3_input_cursor = 0;
  impl->mp3_input_frames = 0;

  for (;;) {
    id3_status = cpkt_audio_mp3_skip_id3v2(impl);
    if (id3_status == 1) {
      return CPKT_AUDIO_ERR_FORMAT;
    }
    if (id3_status == 0) {
      cpkt_audio_mp3_skip_to_sync(impl);
      available = impl->mp3_byte_size - impl->mp3_byte_cursor;
      if (available > 0U) {
        if (available > (size_t)INT_MAX) {
          available = (size_t)INT_MAX;
        }
        memset(&frame_info, 0, sizeof(frame_info));
        frame_count = ma_dr_mp3dec_decode_frame(
            &impl->mp3, impl->mp3_bytes + impl->mp3_byte_cursor, (int)available,
            impl->mp3_input, &frame_info);
        if (frame_info.frame_bytes > 0) {
          impl->mp3_byte_cursor += (size_t)frame_info.frame_bytes;
          cpkt_audio_mp3_buffer_compact(impl);
        }
        if (frame_count > 0) {
          impl->mp3_channels = (ma_uint32)frame_info.channels;
          impl->mp3_sample_rate = (ma_uint32)frame_info.sample_rate;
          impl->mp3_input_frames = (ma_uint64)frame_count;
          return CPKT_AUDIO_OK;
        }
        if (frame_info.frame_bytes == 0 && impl->url_source->done) {
          return CPKT_AUDIO_AT_END;
        }
      }
    }

    bytes_read =
        cpkt_audio_drmp3_url_read(impl->url_source, chunk, sizeof(chunk));
    if (bytes_read == 0U) {
      if (impl->url_source->failed) {
        return impl->mp3_input_frames > 0 ? CPKT_AUDIO_OK : CPKT_AUDIO_AT_END;
      }
      if (impl->url_source->done) {
        return CPKT_AUDIO_AT_END;
      }
      continue;
    }
    if (cpkt_audio_mp3_buffer_reserve(impl, bytes_read) != 0) {
      return CPKT_AUDIO_ERR_ALLOC;
    }
    memcpy(impl->mp3_bytes + impl->mp3_byte_size, chunk, bytes_read);
    impl->mp3_byte_size += bytes_read;
  }
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
  (void)curl_easy_setopt(source->easy, CURLOPT_HEADERFUNCTION,
                         cpkt_audio_url_header);
  (void)curl_easy_setopt(source->easy, CURLOPT_HEADERDATA, source);
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

static cpkt_audio_result
cpkt_audio_detect_reader_format(int *format_out,
                                const cpkt_audio_reader *reader) {
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

static int cpkt_audio_ms_to_16k_frames(unsigned long ms, unsigned long *out) {
  if (out == NULL || ms > ((unsigned long)-1) / 16UL) {
    return 0;
  }
  *out = ms * 16UL;
  return 1;
}

static float cpkt_audio_absf(float value) {
  return value < 0.0f ? -value : value;
}

static long cpkt_audio_16k_frame_to_10ms_floor(size_t frame) {
  size_t value;

  value = frame / 160U;
  if (value > (size_t)LONG_MAX) {
    return LONG_MAX;
  }
  return (long)value;
}

static long cpkt_audio_16k_frame_to_10ms_ceil(size_t frame) {
  size_t value;

  value = frame / 160U;
  if ((frame % 160U) != 0U && value < (size_t)LONG_MAX) {
    ++value;
  }
  if (value > (size_t)LONG_MAX) {
    return LONG_MAX;
  }
  return (long)value;
}

static size_t cpkt_audio_saturating_size_add(size_t left, size_t right) {
  if (left > ((size_t)-1) - right) {
    return (size_t)-1;
  }
  return left + right;
}

static void cpkt_audio_vox_advance_source(struct cpkt_audio_vox_impl *impl,
                                          size_t frame_count) {
  if (impl == NULL) {
    return;
  }
  impl->source_frame_cursor =
      cpkt_audio_saturating_size_add(impl->source_frame_cursor, frame_count);
}

static void cpkt_audio_vox_reset_segment(struct cpkt_audio_vox_impl *impl) {
  if (impl == NULL) {
    return;
  }
  if (impl->spool_file != NULL) {
    fclose(impl->spool_file);
    impl->spool_file = NULL;
  }
  impl->memory_frame_count = 0U;
  impl->total_frame_count = 0U;
  impl->speech_frame_count = 0U;
  impl->read_cursor = 0U;
  impl->silence_frames = 0UL;
  impl->open = 0;
}

static void cpkt_audio_vox_reset_prebuffer(struct cpkt_audio_vox_impl *impl) {
  if (impl == NULL) {
    return;
  }
  impl->prebuffer_count = 0U;
  impl->prebuffer_cursor = 0U;
}

static cpkt_audio_result
cpkt_audio_vox_spill_to_file(struct cpkt_audio_vox_impl *impl) {
  if (impl == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  if (impl->spool_file != NULL) {
    return CPKT_AUDIO_OK;
  }
  impl->spool_file = tmpfile();
  if (impl->spool_file == NULL) {
    return CPKT_AUDIO_ERR_IO;
  }
  if (impl->memory_frame_count > 0U &&
      fwrite(impl->frames, sizeof(float), impl->memory_frame_count,
             impl->spool_file) != impl->memory_frame_count) {
    return CPKT_AUDIO_ERR_IO;
  }
  impl->memory_frame_count = 0U;
  return CPKT_AUDIO_OK;
}

static cpkt_audio_result
cpkt_audio_vox_segment_read_impl(cpkt_audio_vox_segment *self, float *frames,
                                 size_t frame_capacity, size_t *frames_read) {
  struct cpkt_audio_vox_impl *impl;
  size_t remaining;
  size_t to_read;
  size_t read_count;

  if (frames_read != NULL) {
    *frames_read = 0U;
  }
  if (self == NULL || self->impl == NULL || frames_read == NULL ||
      (frames == NULL && frame_capacity != 0U)) {
    return CPKT_AUDIO_ERR_ARG;
  }
  impl = (struct cpkt_audio_vox_impl *)self->impl;
  if (impl->read_cursor >= impl->total_frame_count) {
    return CPKT_AUDIO_AT_END;
  }
  remaining = impl->total_frame_count - impl->read_cursor;
  to_read = frame_capacity < remaining ? frame_capacity : remaining;
  if (to_read == 0U) {
    return CPKT_AUDIO_OK;
  }

  if (impl->spool_file != NULL) {
    read_count = fread(frames, sizeof(float), to_read, impl->spool_file);
    if (read_count != to_read && ferror(impl->spool_file)) {
      return CPKT_AUDIO_ERR_IO;
    }
  } else {
    memcpy(frames, impl->frames + impl->read_cursor, sizeof(float) * to_read);
    read_count = to_read;
  }
  impl->read_cursor += read_count;
  *frames_read = read_count;
  return impl->read_cursor >= impl->total_frame_count ? CPKT_AUDIO_AT_END
                                                      : CPKT_AUDIO_OK;
}

static cpkt_audio_result cpkt_audio_vox_emit(struct cpkt_audio_vox_impl *impl,
                                             int hard_cut, int is_final) {
  cpkt_audio_vox_segment segment;
  int callback_result;

  if (impl == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  if (is_final && impl->speech_frame_count < impl->min_segment_frames) {
    cpkt_audio_vox_reset_segment(impl);
    return CPKT_AUDIO_OK;
  }
  if (impl->total_frame_count < impl->min_segment_frames) {
    cpkt_audio_vox_reset_segment(impl);
    return CPKT_AUDIO_OK;
  }
  if (impl->spool_file != NULL) {
    if (fflush(impl->spool_file) != 0 ||
        fseek(impl->spool_file, 0L, SEEK_SET) != 0) {
      return CPKT_AUDIO_ERR_IO;
    }
  }
  impl->read_cursor = 0U;

  memset(&segment, 0, sizeof(segment));
  segment.impl = impl;
  segment.frame_count = impl->total_frame_count;
  segment.t0 = cpkt_audio_16k_frame_to_10ms_floor(impl->segment_start_frame);
  segment.t1 = cpkt_audio_16k_frame_to_10ms_ceil(
      cpkt_audio_saturating_size_add(impl->segment_start_frame,
                                     impl->total_frame_count));
  segment.segment_index = impl->segment_index;
  segment.hard_cut = hard_cut;
  segment.is_final = is_final;
  segment.read_f32_mono_16k = cpkt_audio_vox_segment_read_impl;
  callback_result =
      impl->config.segment_sink(&segment, impl->config.segment_user);
  if (callback_result != 0) {
    impl->callback_error = 1;
    return CPKT_AUDIO_ERR_IO;
  }

  ++impl->segment_index;
  cpkt_audio_vox_reset_segment(impl);
  return CPKT_AUDIO_OK;
}

static cpkt_audio_result
cpkt_audio_vox_append_frame(struct cpkt_audio_vox_impl *impl, float frame) {
  if (impl == NULL || impl->frames == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  if (impl->total_frame_count >= impl->max_spool_frames) {
    return CPKT_AUDIO_AT_END;
  }
  if (impl->spool_file != NULL) {
    if (fwrite(&frame, sizeof(frame), 1U, impl->spool_file) != 1U) {
      return CPKT_AUDIO_ERR_IO;
    }
  } else if (impl->memory_frame_count < impl->frame_capacity) {
    impl->frames[impl->memory_frame_count] = frame;
    ++impl->memory_frame_count;
  } else {
    cpkt_audio_result result;

    result = cpkt_audio_vox_spill_to_file(impl);
    if (result != CPKT_AUDIO_OK) {
      return result;
    }
    if (fwrite(&frame, sizeof(frame), 1U, impl->spool_file) != 1U) {
      return CPKT_AUDIO_ERR_IO;
    }
  }
  ++impl->total_frame_count;
  return CPKT_AUDIO_OK;
}

static void cpkt_audio_vox_prebuffer_frame(struct cpkt_audio_vox_impl *impl,
                                           float frame) {
  size_t write_index;

  if (impl == NULL || impl->prebuffer_frames == NULL ||
      impl->prebuffer_capacity == 0U) {
    return;
  }
  if (impl->prebuffer_count < impl->prebuffer_capacity) {
    write_index = (impl->prebuffer_cursor + impl->prebuffer_count) %
                  impl->prebuffer_capacity;
    ++impl->prebuffer_count;
  } else {
    write_index = impl->prebuffer_cursor;
    impl->prebuffer_cursor =
        (impl->prebuffer_cursor + 1U) % impl->prebuffer_capacity;
  }
  impl->prebuffer_frames[write_index] = frame;
}

static cpkt_audio_result
cpkt_audio_vox_append_prebuffer(struct cpkt_audio_vox_impl *impl) {
  size_t i;
  cpkt_audio_result result;

  if (impl == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  for (i = 0U; i < impl->prebuffer_count; ++i) {
    size_t index;

    index = (impl->prebuffer_cursor + i) % impl->prebuffer_capacity;
    result = cpkt_audio_vox_append_frame(impl, impl->prebuffer_frames[index]);
    if (result != CPKT_AUDIO_OK) {
      return result;
    }
  }
  cpkt_audio_vox_reset_prebuffer(impl);
  return CPKT_AUDIO_OK;
}

static void cpkt_audio_vox_note_loudness(struct cpkt_audio_vox_impl *impl,
                                         int loud) {
  if (loud) {
    ++impl->speech_frame_count;
    impl->silence_frames = 0UL;
  } else if (impl->silence_frames < (unsigned long)-1) {
    ++impl->silence_frames;
  }
}

static cpkt_audio_result
cpkt_audio_vox_emit_state(struct cpkt_audio_vox_impl *impl, int state) {
  cpkt_audio_vox_state_event event;
  int callback_result;

  if (impl == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  if (impl->config.state_sink == NULL) {
    return CPKT_AUDIO_OK;
  }

  memset(&event, 0, sizeof(event));
  event.state = state;
  event.segment_index = impl->segment_index;
  event.threshold = impl->threshold;
  callback_result = impl->config.state_sink(&event, impl->config.state_user);
  if (callback_result != 0) {
    impl->callback_error = 1;
    return CPKT_AUDIO_ERR_IO;
  }
  return CPKT_AUDIO_OK;
}

static cpkt_audio_result
cpkt_audio_vox_push_f32_mono_16k_impl(cpkt_audio_vox *self, const float *frames,
                                      size_t frame_count) {
  struct cpkt_audio_vox_impl *impl;
  size_t i;
  cpkt_audio_result result;

  if (self == NULL || self->impl == NULL ||
      (frames == NULL && frame_count != 0U)) {
    return CPKT_AUDIO_ERR_ARG;
  }
  impl = (struct cpkt_audio_vox_impl *)self->impl;
  if (impl->callback_error) {
    return CPKT_AUDIO_ERR_IO;
  }

  for (i = 0U; i < frame_count; ++i) {
    int loud;
    size_t frame_position;

    frame_position = impl->source_frame_cursor;
    loud = cpkt_audio_absf(frames[i]) >= impl->threshold ? 1 : 0;
    if (!impl->open && !loud) {
      cpkt_audio_vox_prebuffer_frame(impl, frames[i]);
      cpkt_audio_vox_advance_source(impl, 1U);
      continue;
    }
    if (!impl->open) {
      impl->open = 1;
      impl->segment_start_frame =
          frame_position >= impl->prebuffer_count
              ? frame_position - impl->prebuffer_count
              : 0U;
      impl->silence_frames = 0UL;
      impl->memory_frame_count = 0U;
      impl->total_frame_count = 0U;
      impl->speech_frame_count = 0U;
      impl->read_cursor = 0U;
      result = cpkt_audio_vox_emit_state(impl, CPKT_AUDIO_VOX_TX_ON);
      if (result != CPKT_AUDIO_OK) {
        return result;
      }
      result = cpkt_audio_vox_append_prebuffer(impl);
      if (result != CPKT_AUDIO_OK) {
        return result;
      }
    }

    result = cpkt_audio_vox_append_frame(impl, frames[i]);
    if (result == CPKT_AUDIO_AT_END) {
      result = cpkt_audio_vox_emit_state(impl, CPKT_AUDIO_VOX_HARD_CUT);
      if (result != CPKT_AUDIO_OK) {
        return result;
      }
      result = cpkt_audio_vox_emit(impl, 1, 0);
      if (result != CPKT_AUDIO_OK) {
        return result;
      }
      --i;
      continue;
    }
    if (result != CPKT_AUDIO_OK) {
      return result;
    }

    cpkt_audio_vox_advance_source(impl, 1U);
    cpkt_audio_vox_note_loudness(impl, loud);

    if (impl->release_silence_frames != 0UL &&
        impl->silence_frames >= impl->release_silence_frames) {
      result = cpkt_audio_vox_emit_state(impl, CPKT_AUDIO_VOX_TX_OFF);
      if (result != CPKT_AUDIO_OK) {
        return result;
      }
      result = cpkt_audio_vox_emit(impl, 0, 0);
      if (result != CPKT_AUDIO_OK) {
        return result;
      }
    } else if (impl->max_segment_frames != 0UL &&
               impl->total_frame_count >= impl->max_segment_frames) {
      result = cpkt_audio_vox_emit_state(impl, CPKT_AUDIO_VOX_HARD_CUT);
      if (result != CPKT_AUDIO_OK) {
        return result;
      }
      result = cpkt_audio_vox_emit(impl, 1, 0);
      if (result != CPKT_AUDIO_OK) {
        return result;
      }
    }
  }

  return CPKT_AUDIO_OK;
}

static cpkt_audio_result cpkt_audio_vox_flush_impl(cpkt_audio_vox *self) {
  struct cpkt_audio_vox_impl *impl;
  cpkt_audio_result result;

  if (self == NULL || self->impl == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  impl = (struct cpkt_audio_vox_impl *)self->impl;
  if (impl->callback_error) {
    return CPKT_AUDIO_ERR_IO;
  }
  if (!impl->open || impl->total_frame_count == 0U) {
    return CPKT_AUDIO_OK;
  }
  result = cpkt_audio_vox_emit_state(impl, CPKT_AUDIO_VOX_TX_OFF);
  if (result != CPKT_AUDIO_OK) {
    return result;
  }
  return cpkt_audio_vox_emit(impl, 0, 1);
}

static void cpkt_audio_vox_destroy_impl(cpkt_audio_vox *self) {
  struct cpkt_audio_vox_impl *impl;

  if (self == NULL) {
    return;
  }
  impl = (struct cpkt_audio_vox_impl *)self->impl;
  if (impl != NULL) {
    if (impl->spool_file != NULL) {
      fclose(impl->spool_file);
    }
    free(impl->frames);
    free(impl->prebuffer_frames);
    free(impl);
  }
  free(self);
}

static cpkt_audio_result cpkt_audio_ptt_press_impl(cpkt_audio_ptt *self) {
  struct cpkt_audio_vox_impl *impl;
  cpkt_audio_result result;

  if (self == NULL || self->impl == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  impl = (struct cpkt_audio_vox_impl *)self->impl;
  if (impl->callback_error) {
    return CPKT_AUDIO_ERR_IO;
  }
  impl->ptt_pressed = 1;
  if (impl->open) {
    return CPKT_AUDIO_OK;
  }
  impl->open = 1;
  impl->segment_start_frame = impl->source_frame_cursor;
  impl->silence_frames = 0UL;
  impl->memory_frame_count = 0U;
  impl->total_frame_count = 0U;
  impl->speech_frame_count = 0U;
  impl->read_cursor = 0U;
  result = cpkt_audio_vox_emit_state(impl, CPKT_AUDIO_VOX_TX_ON);
  return result;
}

static cpkt_audio_result cpkt_audio_ptt_release_impl(cpkt_audio_ptt *self) {
  struct cpkt_audio_vox_impl *impl;
  cpkt_audio_result result;

  if (self == NULL || self->impl == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  impl = (struct cpkt_audio_vox_impl *)self->impl;
  if (impl->callback_error) {
    return CPKT_AUDIO_ERR_IO;
  }
  impl->ptt_pressed = 0;
  if (!impl->open) {
    return CPKT_AUDIO_OK;
  }
  result = cpkt_audio_vox_emit_state(impl, CPKT_AUDIO_VOX_TX_OFF);
  if (result != CPKT_AUDIO_OK) {
    return result;
  }
  if (impl->total_frame_count == 0U) {
    cpkt_audio_vox_reset_segment(impl);
    return CPKT_AUDIO_OK;
  }
  return cpkt_audio_vox_emit(impl, 0, 0);
}

static cpkt_audio_result
cpkt_audio_ptt_push_f32_mono_16k_impl(cpkt_audio_ptt *self, const float *frames,
                                      size_t frame_count) {
  struct cpkt_audio_vox_impl *impl;
  size_t i;
  cpkt_audio_result result;

  if (self == NULL || self->impl == NULL ||
      (frames == NULL && frame_count != 0U)) {
    return CPKT_AUDIO_ERR_ARG;
  }
  impl = (struct cpkt_audio_vox_impl *)self->impl;
  if (impl->callback_error) {
    return CPKT_AUDIO_ERR_IO;
  }
  if (!impl->ptt_pressed) {
    cpkt_audio_vox_advance_source(impl, frame_count);
    return CPKT_AUDIO_OK;
  }

  for (i = 0U; i < frame_count; ++i) {
    if (!impl->open) {
      result = cpkt_audio_ptt_press_impl((cpkt_audio_ptt *)self);
      if (result != CPKT_AUDIO_OK) {
        return result;
      }
    }

    result = cpkt_audio_vox_append_frame(impl, frames[i]);
    if (result == CPKT_AUDIO_AT_END) {
      result = cpkt_audio_vox_emit_state(impl, CPKT_AUDIO_VOX_HARD_CUT);
      if (result != CPKT_AUDIO_OK) {
        return result;
      }
      result = cpkt_audio_vox_emit(impl, 1, 0);
      if (result != CPKT_AUDIO_OK) {
        return result;
      }
      --i;
      continue;
    }
    if (result != CPKT_AUDIO_OK) {
      return result;
    }
    cpkt_audio_vox_advance_source(impl, 1U);
    ++impl->speech_frame_count;

    if (impl->max_segment_frames != 0UL &&
        impl->total_frame_count >= impl->max_segment_frames) {
      result = cpkt_audio_vox_emit_state(impl, CPKT_AUDIO_VOX_HARD_CUT);
      if (result != CPKT_AUDIO_OK) {
        return result;
      }
      result = cpkt_audio_vox_emit(impl, 1, 0);
      if (result != CPKT_AUDIO_OK) {
        return result;
      }
    }
  }
  return CPKT_AUDIO_OK;
}

static cpkt_audio_result cpkt_audio_ptt_flush_impl(cpkt_audio_ptt *self) {
  struct cpkt_audio_vox_impl *impl;
  cpkt_audio_result result;

  if (self == NULL || self->impl == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  impl = (struct cpkt_audio_vox_impl *)self->impl;
  if (impl->callback_error) {
    return CPKT_AUDIO_ERR_IO;
  }
  impl->ptt_pressed = 0;
  if (!impl->open || impl->total_frame_count == 0U) {
    return CPKT_AUDIO_OK;
  }
  result = cpkt_audio_vox_emit_state(impl, CPKT_AUDIO_VOX_TX_OFF);
  if (result != CPKT_AUDIO_OK) {
    return result;
  }
  return cpkt_audio_vox_emit(impl, 0, 1);
}

static void cpkt_audio_ptt_destroy_impl(cpkt_audio_ptt *self) {
  struct cpkt_audio_vox_impl *impl;

  if (self == NULL) {
    return;
  }
  impl = (struct cpkt_audio_vox_impl *)self->impl;
  if (impl != NULL) {
    if (impl->spool_file != NULL) {
      fclose(impl->spool_file);
    }
    free(impl->frames);
    free(impl);
  }
  free(self);
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
  if (impl->mode == CPKT_AUDIO_DECODER_MODE_DRMP3) {
    ma_uint64 total_out;
    ma_result converter_result;

    total_out = 0;
    while (total_out < (ma_uint64)frame_capacity) {
      ma_uint64 available_in;
      ma_uint64 consumed_in;
      ma_uint64 produced_out;

      if (impl->mp3_input_cursor >= impl->mp3_input_frames) {
        cpkt_audio_result decode_result;

        impl->mp3_input_cursor = 0;
        impl->mp3_input_frames = 0;
        if (!impl->mp3_eof) {
          decode_result = cpkt_audio_mp3_decode_next_frame(impl);
          if (decode_result == CPKT_AUDIO_AT_END) {
            impl->mp3_eof = 1;
          } else if (decode_result != CPKT_AUDIO_OK) {
            *frames_read = (size_t)total_out;
            return decode_result;
          }
        }
      }

      available_in = impl->mp3_input_frames - impl->mp3_input_cursor;
      consumed_in = available_in;
      produced_out = (ma_uint64)frame_capacity - total_out;
      converter_result = ma_data_converter_process_pcm_frames(
          &impl->converter,
          available_in > 0 ? impl->mp3_input + (impl->mp3_input_cursor *
                                                (ma_uint64)impl->mp3_channels)
                           : NULL,
          &consumed_in, frames + total_out, &produced_out);
      impl->mp3_input_cursor += consumed_in;
      total_out += produced_out;
      if (converter_result != MA_SUCCESS) {
        *frames_read = (size_t)total_out;
        return cpkt_audio_from_ma_result(converter_result);
      }
      if (total_out > 0) {
        break;
      }
      if (produced_out == 0 && consumed_in == 0) {
        break;
      }
    }

    *frames_read = (size_t)total_out;
    if (total_out > 0) {
      return CPKT_AUDIO_OK;
    }
    return impl->mp3_eof ? CPKT_AUDIO_AT_END : CPKT_AUDIO_OK;
  }

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
  if (impl->mode == CPKT_AUDIO_DECODER_MODE_DRMP3) {
    info->source_format = impl->source_format;
    info->output_channels = 1;
    info->output_sample_rate = 16000;
    info->output_frame_count = 0;
    return CPKT_AUDIO_OK;
  }
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
    if (impl->decoder_initialized) {
      ma_decoder_uninit(&impl->decoder);
    }
    if (impl->converter_initialized) {
      ma_data_converter_uninit(&impl->converter, NULL);
    }
    cpkt_audio_url_source_destroy(impl->url_source);
    free(impl->mp3_input);
    free(impl->mp3_bytes);
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

static ma_backend cpkt_audio_to_ma_backend(int backend) {
  switch (backend) {
  case CPKT_AUDIO_DEVICE_BACKEND_COREAUDIO:
#if defined(__APPLE__)
    return ma_backend_coreaudio;
#else
    return ma_backend_null;
#endif
  default:
    return ma_backend_null;
  }
}

static cpkt_audio_result cpkt_audio_resolve_device_backend(int requested,
                                                           ma_backend *out) {
  if (out == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  if (requested == CPKT_AUDIO_DEVICE_BACKEND_AUTO) {
#if defined(__APPLE__)
    *out = ma_backend_coreaudio;
#else
    *out = ma_backend_null;
#endif
    return CPKT_AUDIO_OK;
  }
  *out = cpkt_audio_to_ma_backend(requested);
  if (*out == ma_backend_null) {
    return CPKT_AUDIO_ERR_ARG;
  }
  return CPKT_AUDIO_OK;
}

static int cpkt_audio_backend_is_process(int requested) {
#if defined(__linux__)
  if (requested == CPKT_AUDIO_DEVICE_BACKEND_PROCESS) {
    return 1;
  }
#ifndef CPKT_AUDIO_NATIVE_RUNTIME_DEVICE_IO
  if (requested == CPKT_AUDIO_DEVICE_BACKEND_AUTO) {
    return 1;
  }
#endif
#else
  (void)requested;
#endif
  return 0;
}

static int cpkt_audio_backend_can_process(int requested) {
#if defined(__linux__)
  return requested == CPKT_AUDIO_DEVICE_BACKEND_AUTO ||
         requested == CPKT_AUDIO_DEVICE_BACKEND_PROCESS;
#else
  (void)requested;
  return 0;
#endif
}

static ma_uint32 cpkt_audio_device_buffer_frames(unsigned long buffer_ms) {
  unsigned long ms;

  ms = buffer_ms != 0UL ? buffer_ms : 2000UL;
  if (ms > 268435UL) {
    ms = 268435UL;
  }
  return (ma_uint32)(ms * 16UL);
}

static ma_uint32 cpkt_audio_device_period_ms(unsigned long period_ms) {
  unsigned long ms;

  ms = period_ms != 0UL ? period_ms : 20UL;
  if (ms > 1000UL) {
    ms = 1000UL;
  }
  return (ma_uint32)ms;
}

#if defined(__unix__) || defined(__APPLE__)
static int cpkt_audio_process_command_exists(const char *name) {
  const char *path;
  const char *cursor;
  const char *end;
  const char *dir;
  char candidate[PATH_MAX];
  size_t dir_len;
  size_t name_len;

  if (name == NULL || name[0] == '\0') {
    return 0;
  }
  if (strchr(name, '/') != NULL) {
    return access(name, X_OK) == 0;
  }
  path = getenv("PATH");
  if (path == NULL || path[0] == '\0') {
    path = "/usr/local/bin:/usr/bin:/bin";
  }
  name_len = strlen(name);
  cursor = path;
  while (*cursor != '\0') {
    end = strchr(cursor, ':');
    if (end == NULL) {
      end = cursor + strlen(cursor);
    }
    dir = cursor;
    dir_len = (size_t)(end - cursor);
    if (dir_len == 0U) {
      dir_len = 1U;
      dir = ".";
    }
    if (dir_len + 1U + name_len + 1U <= sizeof(candidate)) {
      memcpy(candidate, dir, dir_len);
      candidate[dir_len] = '/';
      memcpy(candidate + dir_len + 1U, name, name_len + 1U);
      if (access(candidate, X_OK) == 0) {
        return 1;
      }
    }
    cursor = *end == ':' ? end + 1 : end;
  }
  return 0;
}

static void cpkt_audio_process_close_fd(int *fd) {
  if (fd != NULL && *fd >= 0) {
    (void)close(*fd);
    *fd = -1;
  }
}

static void cpkt_audio_process_stop(pid_t *pid, int *fd) {
  int status;

  cpkt_audio_process_close_fd(fd);
  if (pid != NULL && *pid > 0) {
    (void)kill(*pid, SIGTERM);
    if (waitpid(*pid, &status, 0) < 0 && errno == EINTR) {
      (void)waitpid(*pid, &status, 0);
    }
    *pid = (pid_t)-1;
  }
}

static cpkt_audio_result cpkt_audio_process_finish(pid_t *pid, int *fd) {
  int status;

  cpkt_audio_process_close_fd(fd);
  if (pid != NULL && *pid > 0) {
    for (;;) {
      if (waitpid(*pid, &status, 0) >= 0) {
        *pid = (pid_t)-1;
        return CPKT_AUDIO_OK;
      }
      if (errno != EINTR) {
        *pid = (pid_t)-1;
        return CPKT_AUDIO_ERR_IO;
      }
    }
  }
  return CPKT_AUDIO_OK;
}

static cpkt_audio_result cpkt_audio_process_spawn(const char *const argv[],
                                                  int pipe_to_child,
                                                  int nonblock_parent,
                                                  pid_t *pid_out,
                                                  int *fd_out) {
  int pipe_fds[2];
  int devnull;
  pid_t pid;
  int flags;

  if (argv == NULL || argv[0] == NULL || pid_out == NULL || fd_out == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  *pid_out = (pid_t)-1;
  *fd_out = -1;
  if (!cpkt_audio_process_command_exists(argv[0])) {
    return CPKT_AUDIO_ERR_IO;
  }
  if (pipe(pipe_fds) != 0) {
    return CPKT_AUDIO_ERR_IO;
  }
  pid = fork();
  if (pid < 0) {
    (void)close(pipe_fds[0]);
    (void)close(pipe_fds[1]);
    return CPKT_AUDIO_ERR_IO;
  }
  if (pid == 0) {
    devnull = open("/dev/null", O_RDWR);
    if (pipe_to_child) {
      (void)dup2(pipe_fds[0], STDIN_FILENO);
      if (devnull >= 0) {
        (void)dup2(devnull, STDOUT_FILENO);
      }
    } else {
      if (devnull >= 0) {
        (void)dup2(devnull, STDIN_FILENO);
      }
      (void)dup2(pipe_fds[1], STDOUT_FILENO);
    }
    if (devnull >= 0) {
      (void)dup2(devnull, STDERR_FILENO);
      if (devnull > STDERR_FILENO) {
        (void)close(devnull);
      }
    }
    (void)close(pipe_fds[0]);
    (void)close(pipe_fds[1]);
    execvp(argv[0], (char *const *)argv);
    _exit(127);
  }
  if (pipe_to_child) {
    (void)close(pipe_fds[0]);
    *fd_out = pipe_fds[1];
  } else {
    (void)close(pipe_fds[1]);
    *fd_out = pipe_fds[0];
  }
  if (nonblock_parent) {
    flags = fcntl(*fd_out, F_GETFL, 0);
    if (flags >= 0) {
      (void)fcntl(*fd_out, F_SETFL, flags | O_NONBLOCK);
    }
  }
  *pid_out = pid;
  return CPKT_AUDIO_OK;
}

static unsigned long cpkt_audio_process_now_ms(void) {
#if defined(__unix__) || defined(__APPLE__)
  struct timeval tv;

  if (gettimeofday(&tv, NULL) != 0) {
    return 0UL;
  }
  return ((unsigned long)tv.tv_sec * 1000UL) +
         ((unsigned long)tv.tv_usec / 1000UL);
#else
  return 0UL;
#endif
}

static int cpkt_audio_process_elapsed_ms(unsigned long now,
                                         unsigned long then,
                                         unsigned long *elapsed) {
  if (elapsed == NULL || now == 0UL || then == 0UL || now < then) {
    return 0;
  }
  *elapsed = now - then;
  return 1;
}

static int cpkt_audio_wait_expired(unsigned long start_ms,
                                   unsigned long timeout_ms) {
  unsigned long now_ms;
  unsigned long elapsed_ms;

  if (timeout_ms == 0UL) {
    return 0;
  }
  now_ms = cpkt_audio_process_now_ms();
  if (!cpkt_audio_process_elapsed_ms(now_ms, start_ms, &elapsed_ms)) {
    return 0;
  }
  return elapsed_ms >= timeout_ms;
}

static short cpkt_audio_s16le_to_short(const unsigned char *bytes) {
  unsigned int value;

  value = (unsigned int)bytes[0] | ((unsigned int)bytes[1] << 8);
  if (value >= 0x8000U) {
    return (short)((int)value - 0x10000);
  }
  return (short)value;
}

static cpkt_audio_result
cpkt_audio_capture_emit_state(struct cpkt_audio_capture_impl *impl, int state,
                              size_t frame_count) {
  cpkt_audio_capture_state_event event;

  if (impl == NULL || impl->state_sink == NULL) {
    return CPKT_AUDIO_OK;
  }
  event.state = state;
  event.frame_count = frame_count;
  return impl->state_sink(&event, impl->state_user) == 0 ? CPKT_AUDIO_OK
                                                         : CPKT_AUDIO_ERR_IO;
}

static void cpkt_audio_short_to_s16le(short sample, unsigned char *bytes) {
  unsigned int value;

  value = (unsigned int)((int)sample & 0xffff);
  bytes[0] = (unsigned char)(value & 0xffU);
  bytes[1] = (unsigned char)((value >> 8) & 0xffU);
}

static short cpkt_audio_float_to_s16(float sample) {
  if (sample > 1.0f) {
    sample = 1.0f;
  } else if (sample < -1.0f) {
    sample = -1.0f;
  }
  if (sample >= 0.0f) {
    return (short)(sample * 32767.0f);
  }
  return (short)(sample * 32768.0f);
}
#endif

static void cpkt_audio_capture_callback(ma_device *device, void *output,
                                        const void *input,
                                        ma_uint32 frame_count) {
  struct cpkt_audio_capture_impl *impl;
  const float *src;
  ma_uint32 available_write;
  ma_uint32 available_read;
  ma_uint32 drop_frames;
  ma_uint32 remaining;

  (void)output;
  impl = device != NULL ? (struct cpkt_audio_capture_impl *)device->pUserData
                        : NULL;
  if (impl == NULL || input == NULL || frame_count == 0U) {
    return;
  }

  available_write = ma_pcm_rb_available_write(&impl->rb);
  if (available_write < frame_count) {
    drop_frames = frame_count - available_write;
    available_read = ma_pcm_rb_available_read(&impl->rb);
    if (drop_frames > available_read) {
      drop_frames = available_read;
    }
    if (drop_frames > 0U) {
      (void)ma_pcm_rb_seek_read(&impl->rb, drop_frames);
      impl->overrun = 1;
    }
  }

  src = (const float *)input;
  remaining = frame_count;
  while (remaining > 0U) {
    void *dst;
    ma_uint32 chunk;

    dst = NULL;
    chunk = remaining;
    if (ma_pcm_rb_acquire_write(&impl->rb, &chunk, &dst) != MA_SUCCESS ||
        chunk == 0U || dst == NULL) {
      impl->overrun = 1;
      break;
    }
    memcpy(dst, src, sizeof(float) * chunk);
    (void)ma_pcm_rb_commit_write(&impl->rb, chunk);
    src += chunk;
    remaining -= chunk;
  }
}

static void cpkt_audio_playback_callback(ma_device *device, void *output,
                                         const void *input,
                                         ma_uint32 frame_count) {
  struct cpkt_audio_playback_impl *impl;
  float *dst;
  ma_uint32 remaining;

  (void)input;
  impl = device != NULL ? (struct cpkt_audio_playback_impl *)device->pUserData
                        : NULL;
  if (output == NULL || frame_count == 0U) {
    return;
  }

  dst = (float *)output;
  remaining = frame_count;
  while (remaining > 0U) {
    void *src;
    ma_uint32 chunk;

    src = NULL;
    chunk = remaining;
    if (impl == NULL ||
        ma_pcm_rb_acquire_read(&impl->rb, &chunk, &src) != MA_SUCCESS ||
        chunk == 0U || src == NULL) {
      memset(dst, 0, sizeof(float) * remaining);
      if (impl != NULL) {
        impl->underrun = 1;
      }
      break;
    }
    memcpy(dst, src, sizeof(float) * chunk);
    (void)ma_pcm_rb_commit_read(&impl->rb, chunk);
    dst += chunk;
    remaining -= chunk;
  }
}

static cpkt_audio_result
cpkt_audio_capture_start_impl(cpkt_audio_capture *self) {
  struct cpkt_audio_capture_impl *impl;
  ma_result result;
#if defined(__linux__)
  cpkt_audio_result process_result;
  const char *const arecord_argv[] = {"arecord", "-q", "-t", "raw", "-f",
                                      "S16_LE",  "-c", "1",  "-r", "16000",
                                      NULL};
#endif

  if (self == NULL || self->impl == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  impl = (struct cpkt_audio_capture_impl *)self->impl;
  if (impl->started) {
    return CPKT_AUDIO_OK;
  }
  if (impl->mode == CPKT_AUDIO_DEVICE_MODE_PROCESS_ARECORD) {
#if defined(__linux__)
    process_result = cpkt_audio_process_spawn(
        arecord_argv, 0, 1, &impl->process_pid, &impl->process_fd);
    if (process_result != CPKT_AUDIO_OK) {
      return process_result;
    }
    impl->started = 1;
    impl->last_read_ms = cpkt_audio_process_now_ms();
    return CPKT_AUDIO_OK;
#else
    return CPKT_AUDIO_ERR_IO;
#endif
  }
  result = ma_device_start(&impl->device);
  if (result != MA_SUCCESS) {
    return cpkt_audio_from_ma_result(result);
  }
  impl->started = 1;
  return CPKT_AUDIO_OK;
}

static cpkt_audio_result
cpkt_audio_capture_read_f32_mono_16k_impl(cpkt_audio_capture *self,
                                          float *frames, size_t frame_capacity,
                                          size_t *frames_read) {
  struct cpkt_audio_capture_impl *impl;
  size_t total_read;
#if defined(__unix__) || defined(__APPLE__)
  const char *const arecord_argv[] = {"arecord", "-q", "-t", "raw", "-f",
                                      "S16_LE",  "-c", "1",  "-r", "16000",
                                      NULL};
  unsigned char bytes[CPKT_AUDIO_PROCESS_FRAME_CHUNK * 2U];
  unsigned char pair[2];
  size_t byte_count;
  size_t byte_index;
  ssize_t got;
  unsigned long now_ms;
  unsigned long elapsed_ms;
  float sample;
  cpkt_audio_result process_result;
#endif

  if (frames_read != NULL) {
    *frames_read = 0U;
  }
  if (self == NULL || self->impl == NULL || frames_read == NULL ||
      (frames == NULL && frame_capacity != 0U)) {
    return CPKT_AUDIO_ERR_ARG;
  }
  if (frame_capacity == 0U) {
    return CPKT_AUDIO_OK;
  }

  impl = (struct cpkt_audio_capture_impl *)self->impl;
  if (impl->mode == CPKT_AUDIO_DEVICE_MODE_PROCESS_ARECORD) {
#if defined(__unix__) || defined(__APPLE__)
    if (!impl->started || impl->process_fd < 0) {
      return CPKT_AUDIO_OK;
    }
    now_ms = cpkt_audio_process_now_ms();
    if (cpkt_audio_process_elapsed_ms(now_ms, impl->last_read_ms,
                                      &elapsed_ms) &&
        elapsed_ms > CPKT_AUDIO_PROCESS_IDLE_RESET_MS) {
      cpkt_audio_process_stop(&impl->process_pid, &impl->process_fd);
      impl->started = 0;
      impl->has_pending_byte = 0;
      process_result = cpkt_audio_process_spawn(
          arecord_argv, 0, 1, &impl->process_pid, &impl->process_fd);
      if (process_result != CPKT_AUDIO_OK) {
        return process_result;
      }
      impl->started = 1;
      now_ms = cpkt_audio_process_now_ms();
    }
    impl->last_read_ms = now_ms;
    total_read = 0U;
    while (total_read < frame_capacity) {
      byte_count = (frame_capacity - total_read) * 2U;
      if (byte_count > sizeof(bytes)) {
        byte_count = sizeof(bytes);
      }
      if (impl->has_pending_byte) {
        pair[0] = impl->pending_byte;
        got = read(impl->process_fd, pair + 1, 1U);
        if (got < 0) {
          if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            break;
          }
          return CPKT_AUDIO_ERR_IO;
        }
        if (got == 0) {
          break;
        }
        sample = (float)cpkt_audio_s16le_to_short(pair) / 32768.0f;
        frames[total_read++] = sample;
        impl->has_pending_byte = 0;
        continue;
      }
      got = read(impl->process_fd, bytes, byte_count);
      if (got < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
          break;
        }
        return CPKT_AUDIO_ERR_IO;
      }
      if (got == 0) {
        break;
      }
      byte_count = (size_t)got;
      byte_index = 0U;
      while (byte_index + 1U < byte_count && total_read < frame_capacity) {
        sample =
            (float)cpkt_audio_s16le_to_short(bytes + byte_index) / 32768.0f;
        frames[total_read++] = sample;
        byte_index += 2U;
      }
      if (byte_index < byte_count) {
        impl->pending_byte = bytes[byte_index];
        impl->has_pending_byte = 1;
      }
    }
    *frames_read = total_read;
    if (total_read > 0U) {
      return cpkt_audio_capture_emit_state(impl, CPKT_AUDIO_CAPTURE_READY,
                                           total_read);
    }
    return CPKT_AUDIO_OK;
#else
    return CPKT_AUDIO_ERR_IO;
#endif
  }
  total_read = 0U;
  while (total_read < frame_capacity) {
    void *src;
    ma_uint32 chunk;

    src = NULL;
    chunk = (ma_uint32)(frame_capacity - total_read);
    if ((size_t)chunk != frame_capacity - total_read) {
      chunk = 0xffffffffU;
    }
    if (ma_pcm_rb_acquire_read(&impl->rb, &chunk, &src) != MA_SUCCESS ||
        chunk == 0U || src == NULL) {
      break;
    }
    memcpy(frames + total_read, src, sizeof(float) * chunk);
    (void)ma_pcm_rb_commit_read(&impl->rb, chunk);
    total_read += chunk;
  }
  *frames_read = total_read;
  if (total_read > 0U) {
    return cpkt_audio_capture_emit_state(impl, CPKT_AUDIO_CAPTURE_READY,
                                         total_read);
  }
  return CPKT_AUDIO_OK;
}

static cpkt_audio_result
cpkt_audio_capture_wait_ready_impl(cpkt_audio_capture *self,
                                   unsigned long timeout_ms) {
  struct cpkt_audio_capture_impl *impl;
  unsigned long start_ms;
#if defined(__unix__) || defined(__APPLE__)
  const char *const arecord_argv[] = {"arecord", "-q", "-t", "raw", "-f",
                                      "S16_LE",  "-c", "1",  "-r", "16000",
                                      NULL};
  unsigned long now_ms;
  unsigned long elapsed_ms;
  cpkt_audio_result process_result;
  fd_set readfds;
  struct timeval tv;
  int ready;
#endif

  if (self == NULL || self->impl == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  impl = (struct cpkt_audio_capture_impl *)self->impl;
  if (!impl->started) {
    return CPKT_AUDIO_ERR_IO;
  }
  start_ms = cpkt_audio_process_now_ms();

  if (impl->mode == CPKT_AUDIO_DEVICE_MODE_PROCESS_ARECORD) {
#if defined(__unix__) || defined(__APPLE__)
    if (impl->process_fd < 0) {
      return CPKT_AUDIO_ERR_IO;
    }
    now_ms = cpkt_audio_process_now_ms();
    if (cpkt_audio_process_elapsed_ms(now_ms, impl->last_read_ms,
                                      &elapsed_ms) &&
        elapsed_ms > CPKT_AUDIO_PROCESS_IDLE_RESET_MS) {
      cpkt_audio_process_stop(&impl->process_pid, &impl->process_fd);
      impl->started = 0;
      impl->has_pending_byte = 0;
      process_result = cpkt_audio_process_spawn(
          arecord_argv, 0, 1, &impl->process_pid, &impl->process_fd);
      if (process_result != CPKT_AUDIO_OK) {
        return process_result;
      }
      impl->started = 1;
      impl->last_read_ms = cpkt_audio_process_now_ms();
    }
    for (;;) {
      FD_ZERO(&readfds);
      FD_SET(impl->process_fd, &readfds);
      tv.tv_sec = 0;
      tv.tv_usec = 10000;
      ready = select(impl->process_fd + 1, &readfds, NULL, NULL, &tv);
      if (ready > 0 && FD_ISSET(impl->process_fd, &readfds)) {
        return CPKT_AUDIO_OK;
      }
      if (ready < 0 && errno != EINTR) {
        return CPKT_AUDIO_ERR_IO;
      }
      if (cpkt_audio_wait_expired(start_ms, timeout_ms)) {
        return CPKT_AUDIO_TIMEOUT;
      }
    }
#else
    return CPKT_AUDIO_ERR_IO;
#endif
  }

  for (;;) {
    if (ma_pcm_rb_available_read(&impl->rb) > 0U) {
      return CPKT_AUDIO_OK;
    }
    if (cpkt_audio_wait_expired(start_ms, timeout_ms)) {
      return CPKT_AUDIO_TIMEOUT;
    }
    ma_sleep(5);
  }
}

static cpkt_audio_result
cpkt_audio_capture_stop_impl(cpkt_audio_capture *self) {
  struct cpkt_audio_capture_impl *impl;

  if (self == NULL || self->impl == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  impl = (struct cpkt_audio_capture_impl *)self->impl;
  if (impl->mode == CPKT_AUDIO_DEVICE_MODE_PROCESS_ARECORD) {
#if defined(__unix__) || defined(__APPLE__)
    cpkt_audio_process_stop(&impl->process_pid, &impl->process_fd);
    impl->started = 0;
    impl->last_read_ms = 0UL;
    return CPKT_AUDIO_OK;
#else
    return CPKT_AUDIO_ERR_IO;
#endif
  }
  if (impl->started) {
    ma_device_stop(&impl->device);
    impl->started = 0;
  }
  return CPKT_AUDIO_OK;
}

static void cpkt_audio_capture_destroy_impl(cpkt_audio_capture *self) {
  struct cpkt_audio_capture_impl *impl;

  if (self == NULL) {
    return;
  }
  impl = (struct cpkt_audio_capture_impl *)self->impl;
  if (impl != NULL) {
    if (impl->mode == CPKT_AUDIO_DEVICE_MODE_PROCESS_ARECORD) {
#if defined(__unix__) || defined(__APPLE__)
      cpkt_audio_process_stop(&impl->process_pid, &impl->process_fd);
#endif
    }
    if (impl->device_initialized) {
      ma_device_uninit(&impl->device);
    }
    if (impl->rb_initialized) {
      ma_pcm_rb_uninit(&impl->rb);
    }
    free(impl);
  }
  free(self);
}

static cpkt_audio_result
cpkt_audio_playback_start_impl(cpkt_audio_playback *self) {
  struct cpkt_audio_playback_impl *impl;
  ma_result result;
#if defined(__linux__)
  cpkt_audio_result process_result;
  const char *const aplay_argv[] = {"aplay", "-q", "-t", "raw", "-f",
                                    "S16_LE", "-c", "1",  "-r", "16000",
                                    NULL};
#endif

  if (self == NULL || self->impl == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  impl = (struct cpkt_audio_playback_impl *)self->impl;
  if (impl->started) {
    return CPKT_AUDIO_OK;
  }
  if (impl->mode == CPKT_AUDIO_DEVICE_MODE_PROCESS_ARECORD) {
#if defined(__linux__)
    process_result = cpkt_audio_process_spawn(
        aplay_argv, 1, 0, &impl->process_pid, &impl->process_fd);
    if (process_result != CPKT_AUDIO_OK) {
      return process_result;
    }
    impl->started = 1;
    return CPKT_AUDIO_OK;
#else
    return CPKT_AUDIO_ERR_IO;
#endif
  }
  result = ma_device_start(&impl->device);
  if (result != MA_SUCCESS) {
    return cpkt_audio_from_ma_result(result);
  }
  impl->started = 1;
  return CPKT_AUDIO_OK;
}

static cpkt_audio_result cpkt_audio_playback_write_f32_mono_16k_impl(
    cpkt_audio_playback *self, const float *frames, size_t frame_count,
    size_t *frames_written) {
  struct cpkt_audio_playback_impl *impl;
  size_t total_written;
#if defined(__unix__) || defined(__APPLE__)
  unsigned char bytes[CPKT_AUDIO_PROCESS_FRAME_CHUNK * 2U];
  size_t frames_now;
  size_t frame_index;
  size_t byte_count;
  size_t byte_offset;
  ssize_t wrote;
#endif

  if (frames_written != NULL) {
    *frames_written = 0U;
  }
  if (self == NULL || self->impl == NULL || frames_written == NULL ||
      (frames == NULL && frame_count != 0U)) {
    return CPKT_AUDIO_ERR_ARG;
  }
  if (frame_count == 0U) {
    return CPKT_AUDIO_OK;
  }

  impl = (struct cpkt_audio_playback_impl *)self->impl;
  if (impl->mode == CPKT_AUDIO_DEVICE_MODE_PROCESS_ARECORD) {
#if defined(__unix__) || defined(__APPLE__)
    if (!impl->started || impl->process_fd < 0) {
      return CPKT_AUDIO_ERR_IO;
    }
    total_written = 0U;
    while (total_written < frame_count) {
      frames_now = frame_count - total_written;
      if (frames_now > CPKT_AUDIO_PROCESS_FRAME_CHUNK) {
        frames_now = CPKT_AUDIO_PROCESS_FRAME_CHUNK;
      }
      for (frame_index = 0U; frame_index < frames_now; ++frame_index) {
        cpkt_audio_short_to_s16le(
            cpkt_audio_float_to_s16(frames[total_written + frame_index]),
            bytes + (frame_index * 2U));
      }
      byte_count = frames_now * 2U;
      byte_offset = 0U;
      while (byte_offset < byte_count) {
        wrote = write(impl->process_fd, bytes + byte_offset,
                      byte_count - byte_offset);
        if (wrote < 0) {
          if (errno == EINTR) {
            continue;
          }
          return CPKT_AUDIO_ERR_IO;
        }
        if (wrote == 0) {
          return CPKT_AUDIO_ERR_IO;
        }
        byte_offset += (size_t)wrote;
      }
      total_written += frames_now;
    }
    *frames_written = total_written;
    return CPKT_AUDIO_OK;
#else
    return CPKT_AUDIO_ERR_IO;
#endif
  }
  total_written = 0U;
  while (total_written < frame_count) {
    void *dst;
    ma_uint32 chunk;

    dst = NULL;
    chunk = (ma_uint32)(frame_count - total_written);
    if ((size_t)chunk != frame_count - total_written) {
      chunk = 0xffffffffU;
    }
    if (ma_pcm_rb_acquire_write(&impl->rb, &chunk, &dst) != MA_SUCCESS ||
        chunk == 0U || dst == NULL) {
      ma_sleep(5);
      continue;
    }
    memcpy(dst, frames + total_written, sizeof(float) * chunk);
    (void)ma_pcm_rb_commit_write(&impl->rb, chunk);
    total_written += chunk;
  }
  *frames_written = total_written;
  return CPKT_AUDIO_OK;
}

static cpkt_audio_result
cpkt_audio_playback_drain_impl(cpkt_audio_playback *self) {
  struct cpkt_audio_playback_impl *impl;
#if defined(__unix__) || defined(__APPLE__)
  cpkt_audio_result result;
#endif

  if (self == NULL || self->impl == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  impl = (struct cpkt_audio_playback_impl *)self->impl;
  if (impl->mode == CPKT_AUDIO_DEVICE_MODE_PROCESS_ARECORD) {
#if defined(__unix__) || defined(__APPLE__)
    if (!impl->started) {
      return CPKT_AUDIO_OK;
    }
    result = cpkt_audio_process_finish(&impl->process_pid, &impl->process_fd);
    impl->started = 0;
    return result;
#else
    return CPKT_AUDIO_ERR_IO;
#endif
  }
  while (ma_pcm_rb_available_read(&impl->rb) > 0U) {
    ma_sleep(10);
  }
  return CPKT_AUDIO_OK;
}

static cpkt_audio_result
cpkt_audio_playback_stop_impl(cpkt_audio_playback *self) {
  struct cpkt_audio_playback_impl *impl;

  if (self == NULL || self->impl == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  impl = (struct cpkt_audio_playback_impl *)self->impl;
  if (impl->mode == CPKT_AUDIO_DEVICE_MODE_PROCESS_ARECORD) {
#if defined(__unix__) || defined(__APPLE__)
    cpkt_audio_process_stop(&impl->process_pid, &impl->process_fd);
    impl->started = 0;
    return CPKT_AUDIO_OK;
#else
    return CPKT_AUDIO_ERR_IO;
#endif
  }
  if (impl->started) {
    ma_device_stop(&impl->device);
    impl->started = 0;
  }
  return CPKT_AUDIO_OK;
}

static void cpkt_audio_playback_destroy_impl(cpkt_audio_playback *self) {
  struct cpkt_audio_playback_impl *impl;

  if (self == NULL) {
    return;
  }
  impl = (struct cpkt_audio_playback_impl *)self->impl;
  if (impl != NULL) {
    if (impl->mode == CPKT_AUDIO_DEVICE_MODE_PROCESS_ARECORD) {
#if defined(__unix__) || defined(__APPLE__)
      cpkt_audio_process_stop(&impl->process_pid, &impl->process_fd);
#endif
    }
    if (impl->device_initialized) {
      ma_device_uninit(&impl->device);
    }
    if (impl->rb_initialized) {
      ma_pcm_rb_uninit(&impl->rb);
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

static cpkt_audio_result
cpkt_audio_capture_alloc(cpkt_audio_capture **out,
                         cpkt_audio_capture **capture_out,
                         struct cpkt_audio_capture_impl **impl_out) {
  cpkt_audio_capture *capture;
  struct cpkt_audio_capture_impl *impl;

  if (out == NULL || capture_out == NULL || impl_out == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  *out = NULL;
  capture = (cpkt_audio_capture *)calloc(1, sizeof(*capture));
  if (capture == NULL) {
    return CPKT_AUDIO_ERR_ALLOC;
  }
  impl = (struct cpkt_audio_capture_impl *)calloc(1, sizeof(*impl));
  if (impl == NULL) {
    free(capture);
    return CPKT_AUDIO_ERR_ALLOC;
  }
  capture->impl = impl;
#if defined(__unix__) || defined(__APPLE__)
  impl->process_pid = (pid_t)-1;
  impl->process_fd = -1;
#endif
  capture->start = cpkt_audio_capture_start_impl;
  capture->read_f32_mono_16k = cpkt_audio_capture_read_f32_mono_16k_impl;
  capture->wait_ready = cpkt_audio_capture_wait_ready_impl;
  capture->stop = cpkt_audio_capture_stop_impl;
  capture->destroy = cpkt_audio_capture_destroy_impl;
  *capture_out = capture;
  *impl_out = impl;
  return CPKT_AUDIO_OK;
}

static cpkt_audio_result
cpkt_audio_playback_alloc(cpkt_audio_playback **out,
                          cpkt_audio_playback **playback_out,
                          struct cpkt_audio_playback_impl **impl_out) {
  cpkt_audio_playback *playback;
  struct cpkt_audio_playback_impl *impl;

  if (out == NULL || playback_out == NULL || impl_out == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  *out = NULL;
  playback = (cpkt_audio_playback *)calloc(1, sizeof(*playback));
  if (playback == NULL) {
    return CPKT_AUDIO_ERR_ALLOC;
  }
  impl = (struct cpkt_audio_playback_impl *)calloc(1, sizeof(*impl));
  if (impl == NULL) {
    free(playback);
    return CPKT_AUDIO_ERR_ALLOC;
  }
  playback->impl = impl;
#if defined(__unix__) || defined(__APPLE__)
  impl->process_pid = (pid_t)-1;
  impl->process_fd = -1;
#endif
  playback->start = cpkt_audio_playback_start_impl;
  playback->write_f32_mono_16k = cpkt_audio_playback_write_f32_mono_16k_impl;
  playback->drain = cpkt_audio_playback_drain_impl;
  playback->stop = cpkt_audio_playback_stop_impl;
  playback->destroy = cpkt_audio_playback_destroy_impl;
  *playback_out = playback;
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
CPKT_AUDIO_EXPORT cpkt_audio_result
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
  impl->decoder_initialized = 1;

  *out = decoder;
  return CPKT_AUDIO_OK;
}

/** Opens a receiver-shell decoder for streaming URL input. */
CPKT_AUDIO_EXPORT cpkt_audio_result
cpkt_audio_decoder_open_url(cpkt_audio_decoder **out, const char *url,
                            const cpkt_audio_decoder_config *config) {
  cpkt_audio_decoder *decoder;
  struct cpkt_audio_decoder_impl *impl;
  struct cpkt_audio_url_source *source;
  cpkt_audio_reader reader;
  cpkt_audio_decoder_config resolved_config;
  ma_data_converter_config converter_config;
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
  if (impl->source_format == CPKT_AUDIO_FORMAT_UNKNOWN) {
    result = cpkt_audio_url_source_detect_format(source, &impl->source_format);
    if (result != CPKT_AUDIO_OK) {
      decoder->impl = NULL;
      cpkt_audio_url_source_destroy(source);
      free(impl);
      free(decoder);
      return result;
    }
  }

  if (impl->source_format == CPKT_AUDIO_FORMAT_MP3) {
    impl->mode = CPKT_AUDIO_DECODER_MODE_DRMP3;
    ma_dr_mp3dec_init(&impl->mp3);
    impl->mp3_input =
        (ma_int16 *)malloc(sizeof(ma_int16) * MA_DR_MP3_MAX_SAMPLES_PER_FRAME);
    if (impl->mp3_input == NULL) {
      decoder->impl = NULL;
      cpkt_audio_url_source_destroy(source);
      free(impl);
      free(decoder);
      return CPKT_AUDIO_ERR_ALLOC;
    }
    result = cpkt_audio_mp3_decode_next_frame(impl);
    if (result != CPKT_AUDIO_OK || impl->mp3_channels == 0 ||
        impl->mp3_channels > 2 || impl->mp3_sample_rate == 0) {
      decoder->impl = NULL;
      cpkt_audio_url_source_destroy(source);
      free(impl->mp3_input);
      free(impl->mp3_bytes);
      free(impl);
      free(decoder);
      return result == CPKT_AUDIO_OK ? CPKT_AUDIO_ERR_FORMAT : result;
    }
    converter_config = ma_data_converter_config_init(
        ma_format_s16, ma_format_f32, impl->mp3_channels, 1,
        impl->mp3_sample_rate, 16000);
    ma_status =
        ma_data_converter_init(&converter_config, NULL, &impl->converter);
    if (ma_status != MA_SUCCESS) {
      decoder->impl = NULL;
      cpkt_audio_url_source_destroy(source);
      free(impl->mp3_input);
      free(impl->mp3_bytes);
      free(impl);
      free(decoder);
      return cpkt_audio_from_ma_result(ma_status);
    }
    impl->converter_initialized = 1;
    *out = decoder;
    return CPKT_AUDIO_OK;
  }

  if (config != NULL) {
    resolved_config = *config;
  } else {
    memset(&resolved_config, 0, sizeof(resolved_config));
  }
  if (resolved_config.encoding == CPKT_AUDIO_ENCODING_UNKNOWN) {
    resolved_config.encoding =
        cpkt_audio_encoding_from_format(impl->source_format);
  }
  ma_config = cpkt_audio_ma_decoder_config(&resolved_config);
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
  impl->decoder_initialized = 1;

  *out = decoder;
  return CPKT_AUDIO_OK;
}

/** Opens a receiver-shell decoder for callback-based input. */
CPKT_AUDIO_EXPORT cpkt_audio_result cpkt_audio_decoder_open_reader(
    cpkt_audio_decoder **out, const cpkt_audio_reader *reader,
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
  impl->decoder_initialized = 1;

  *out = decoder;
  return CPKT_AUDIO_OK;
}

/** Opens a receiver-shell encoder for a filesystem output path. */
CPKT_AUDIO_EXPORT cpkt_audio_result
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
CPKT_AUDIO_EXPORT cpkt_audio_result cpkt_audio_encoder_open_writer(
    cpkt_audio_encoder **out, const cpkt_audio_writer *writer,
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

/** Opens receiver-shell capture from the platform default input device. */
CPKT_AUDIO_EXPORT cpkt_audio_result cpkt_audio_capture_open_default(
    cpkt_audio_capture **out, const cpkt_audio_capture_config *config) {
  cpkt_audio_capture *capture;
  struct cpkt_audio_capture_impl *impl;
  ma_device_config ma_config;
  ma_backend backend;
  ma_result ma_status;
  cpkt_audio_result result;
  ma_uint32 buffer_frames;
  int requested_backend;

  if (out != NULL) {
    *out = NULL;
  }
  if (out == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  requested_backend = config != NULL ? config->backend : 0;

  buffer_frames =
      cpkt_audio_device_buffer_frames(config != NULL ? config->buffer_ms : 0UL);
  if (buffer_frames == 0U) {
    return CPKT_AUDIO_ERR_ARG;
  }

  result = cpkt_audio_capture_alloc(out, &capture, &impl);
  if (result != CPKT_AUDIO_OK) {
    return result;
  }
  if (config != NULL) {
    impl->state_sink = config->state_sink;
    impl->state_user = config->state_user;
  }
  if (cpkt_audio_backend_is_process(requested_backend)) {
#if defined(__linux__)
    if (!cpkt_audio_process_command_exists("arecord")) {
      capture->impl = NULL;
      free(impl);
      free(capture);
      return CPKT_AUDIO_ERR_IO;
    }
    impl->mode = CPKT_AUDIO_DEVICE_MODE_PROCESS_ARECORD;
    *out = capture;
    return CPKT_AUDIO_OK;
#else
    capture->impl = NULL;
    free(impl);
    free(capture);
    return CPKT_AUDIO_ERR_ARG;
#endif
  }
  ma_status =
      ma_pcm_rb_init(ma_format_f32, 1, buffer_frames, NULL, NULL, &impl->rb);
  if (ma_status != MA_SUCCESS) {
    capture->impl = NULL;
    free(impl);
    free(capture);
    return cpkt_audio_from_ma_result(ma_status);
  }
  impl->rb_initialized = 1;

  ma_config = ma_device_config_init(ma_device_type_capture);
  ma_config.capture.format = ma_format_f32;
  ma_config.capture.channels = 1;
  ma_config.sampleRate = 16000;
  ma_config.periodSizeInMilliseconds =
      cpkt_audio_device_period_ms(config != NULL ? config->period_ms : 0UL);
  ma_config.dataCallback = cpkt_audio_capture_callback;
  ma_config.pUserData = impl;

  result = cpkt_audio_resolve_device_backend(requested_backend, &backend);
  if (result != CPKT_AUDIO_OK) {
    capture->impl = NULL;
    ma_pcm_rb_uninit(&impl->rb);
    free(impl);
    free(capture);
    return result;
  }
  if (backend == ma_backend_null) {
    ma_status = ma_device_init(NULL, &ma_config, &impl->device);
  } else {
    ma_status = ma_device_init_ex(&backend, 1, NULL, &ma_config, &impl->device);
  }
  if (ma_status != MA_SUCCESS) {
#if defined(__linux__) && defined(CPKT_AUDIO_NATIVE_RUNTIME_DEVICE_IO)
    if (cpkt_audio_backend_can_process(requested_backend) &&
        cpkt_audio_process_command_exists("arecord")) {
      ma_pcm_rb_uninit(&impl->rb);
      impl->rb_initialized = 0;
      impl->mode = CPKT_AUDIO_DEVICE_MODE_PROCESS_ARECORD;
      *out = capture;
      return CPKT_AUDIO_OK;
    }
#endif
    capture->impl = NULL;
    ma_pcm_rb_uninit(&impl->rb);
    free(impl);
    free(capture);
    return cpkt_audio_from_ma_result(ma_status);
  }
  impl->device_initialized = 1;
  *out = capture;
  return CPKT_AUDIO_OK;
}

/** Opens receiver-shell playback to the platform default output device. */
CPKT_AUDIO_EXPORT cpkt_audio_result cpkt_audio_playback_open_default(
    cpkt_audio_playback **out, const cpkt_audio_playback_config *config) {
  cpkt_audio_playback *playback;
  struct cpkt_audio_playback_impl *impl;
  ma_device_config ma_config;
  ma_backend backend;
  ma_result ma_status;
  cpkt_audio_result result;
  ma_uint32 buffer_frames;
  int requested_backend;

  if (out != NULL) {
    *out = NULL;
  }
  if (out == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }
  requested_backend = config != NULL ? config->backend : 0;

  buffer_frames =
      cpkt_audio_device_buffer_frames(config != NULL ? config->buffer_ms : 0UL);
  if (buffer_frames == 0U) {
    return CPKT_AUDIO_ERR_ARG;
  }

  result = cpkt_audio_playback_alloc(out, &playback, &impl);
  if (result != CPKT_AUDIO_OK) {
    return result;
  }
  if (cpkt_audio_backend_is_process(requested_backend)) {
#if defined(__linux__)
    if (!cpkt_audio_process_command_exists("aplay")) {
      playback->impl = NULL;
      free(impl);
      free(playback);
      return CPKT_AUDIO_ERR_IO;
    }
    impl->mode = CPKT_AUDIO_DEVICE_MODE_PROCESS_ARECORD;
    *out = playback;
    return CPKT_AUDIO_OK;
#else
    playback->impl = NULL;
    free(impl);
    free(playback);
    return CPKT_AUDIO_ERR_ARG;
#endif
  }
  ma_status =
      ma_pcm_rb_init(ma_format_f32, 1, buffer_frames, NULL, NULL, &impl->rb);
  if (ma_status != MA_SUCCESS) {
    playback->impl = NULL;
    free(impl);
    free(playback);
    return cpkt_audio_from_ma_result(ma_status);
  }
  impl->rb_initialized = 1;

  ma_config = ma_device_config_init(ma_device_type_playback);
  ma_config.playback.format = ma_format_f32;
  ma_config.playback.channels = 1;
  ma_config.sampleRate = 16000;
  ma_config.periodSizeInMilliseconds =
      cpkt_audio_device_period_ms(config != NULL ? config->period_ms : 0UL);
  ma_config.dataCallback = cpkt_audio_playback_callback;
  ma_config.pUserData = impl;

  result = cpkt_audio_resolve_device_backend(requested_backend, &backend);
  if (result != CPKT_AUDIO_OK) {
    playback->impl = NULL;
    ma_pcm_rb_uninit(&impl->rb);
    free(impl);
    free(playback);
    return result;
  }
  if (backend == ma_backend_null) {
    ma_status = ma_device_init(NULL, &ma_config, &impl->device);
  } else {
    ma_status = ma_device_init_ex(&backend, 1, NULL, &ma_config, &impl->device);
  }
  if (ma_status != MA_SUCCESS) {
#if defined(__linux__) && defined(CPKT_AUDIO_NATIVE_RUNTIME_DEVICE_IO)
    if (cpkt_audio_backend_can_process(requested_backend) &&
        cpkt_audio_process_command_exists("aplay")) {
      ma_pcm_rb_uninit(&impl->rb);
      impl->rb_initialized = 0;
      impl->mode = CPKT_AUDIO_DEVICE_MODE_PROCESS_ARECORD;
      *out = playback;
      return CPKT_AUDIO_OK;
    }
#endif
    playback->impl = NULL;
    ma_pcm_rb_uninit(&impl->rb);
    free(impl);
    free(playback);
    return cpkt_audio_from_ma_result(ma_status);
  }
  impl->device_initialized = 1;
  *out = playback;
  return CPKT_AUDIO_OK;
}

/** Opens a receiver-shell VOX segmenter for float32 mono 16000 Hz PCM. */
CPKT_AUDIO_EXPORT cpkt_audio_result
cpkt_audio_vox_open(cpkt_audio_vox **out, const cpkt_audio_vox_config *config) {
  cpkt_audio_vox *vox;
  struct cpkt_audio_vox_impl *impl;
  unsigned long release_frames;
  unsigned long max_frames;
  unsigned long min_frames;
  unsigned long prebuffer_frames_ul;
  unsigned long release_ms;
  unsigned long min_ms;
  unsigned long prebuffer_ms;
  unsigned long memory_spool_bytes;
  unsigned long max_spool_bytes;
  size_t memory_spool_frames;
  size_t max_spool_frames;
  size_t prebuffer_frames;

  if (out != NULL) {
    *out = NULL;
  }
  if (out == NULL || config == NULL || config->segment_sink == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }

  release_ms =
      config->release_silence_ms != 0UL ? config->release_silence_ms : 1500UL;
  min_ms = config->min_segment_ms != 0UL ? config->min_segment_ms : 100UL;
  prebuffer_ms = config->prebuffer_ms != 0UL ? config->prebuffer_ms : 10UL;
  if (!cpkt_audio_ms_to_16k_frames(release_ms, &release_frames) ||
      !cpkt_audio_ms_to_16k_frames(min_ms, &min_frames) ||
      !cpkt_audio_ms_to_16k_frames(prebuffer_ms, &prebuffer_frames_ul) ||
      (config->max_segment_ms != 0UL &&
       !cpkt_audio_ms_to_16k_frames(config->max_segment_ms, &max_frames))) {
    return CPKT_AUDIO_ERR_ARG;
  }
  if (config->max_segment_ms == 0UL) {
    max_frames = 0UL;
  }
  if (max_frames != 0UL && min_frames > max_frames) {
    return CPKT_AUDIO_ERR_ARG;
  }
  memory_spool_bytes = config->memory_spool_bytes != 0UL
                           ? config->memory_spool_bytes
                           : 1048576UL;
  max_spool_bytes =
      config->max_spool_bytes != 0UL ? config->max_spool_bytes : 1073741824UL;
  if (memory_spool_bytes < sizeof(float) || max_spool_bytes < sizeof(float) ||
      memory_spool_bytes > max_spool_bytes) {
    return CPKT_AUDIO_ERR_ARG;
  }
  memory_spool_frames = (size_t)(memory_spool_bytes / sizeof(float));
  max_spool_frames = (size_t)(max_spool_bytes / sizeof(float));
  prebuffer_frames = (size_t)prebuffer_frames_ul;
  if (memory_spool_frames == 0U || max_spool_frames == 0U ||
      memory_spool_frames > ((size_t)-1) / sizeof(float) ||
      (unsigned long)prebuffer_frames != prebuffer_frames_ul ||
      (prebuffer_frames != 0U &&
       prebuffer_frames > ((size_t)-1) / sizeof(float))) {
    return CPKT_AUDIO_ERR_ARG;
  }

  vox = (cpkt_audio_vox *)calloc(1, sizeof(*vox));
  if (vox == NULL) {
    return CPKT_AUDIO_ERR_ALLOC;
  }
  impl = (struct cpkt_audio_vox_impl *)calloc(1, sizeof(*impl));
  if (impl == NULL) {
    free(vox);
    return CPKT_AUDIO_ERR_ALLOC;
  }
  impl->frames = (float *)malloc(sizeof(float) * memory_spool_frames);
  if (impl->frames == NULL) {
    free(impl);
    free(vox);
    return CPKT_AUDIO_ERR_ALLOC;
  }
  if (prebuffer_frames != 0U) {
    impl->prebuffer_frames =
        (float *)malloc(sizeof(float) * prebuffer_frames);
    if (impl->prebuffer_frames == NULL) {
      free(impl->frames);
      free(impl);
      free(vox);
      return CPKT_AUDIO_ERR_ALLOC;
    }
  }
  impl->config = *config;
  impl->release_silence_frames = release_frames;
  impl->max_segment_frames = max_frames;
  impl->min_segment_frames = min_frames;
  impl->frame_capacity = memory_spool_frames;
  impl->prebuffer_capacity = prebuffer_frames;
  impl->max_spool_frames = max_spool_frames;
  impl->threshold = config->threshold > 0.0f ? config->threshold : 0.01f;

  vox->impl = impl;
  vox->push_f32_mono_16k = cpkt_audio_vox_push_f32_mono_16k_impl;
  vox->flush = cpkt_audio_vox_flush_impl;
  vox->destroy = cpkt_audio_vox_destroy_impl;
  *out = vox;
  return CPKT_AUDIO_OK;
}

/** Opens a receiver-shell PTT segmenter for float32 mono 16000 Hz PCM. */
CPKT_AUDIO_EXPORT cpkt_audio_result
cpkt_audio_ptt_open(cpkt_audio_ptt **out, const cpkt_audio_ptt_config *config) {
  cpkt_audio_ptt *ptt;
  struct cpkt_audio_vox_impl *impl;
  unsigned long max_frames;
  unsigned long min_frames;
  unsigned long min_ms;
  unsigned long memory_spool_bytes;
  unsigned long max_spool_bytes;
  size_t memory_spool_frames;
  size_t max_spool_frames;

  if (out != NULL) {
    *out = NULL;
  }
  if (out == NULL || config == NULL || config->segment_sink == NULL) {
    return CPKT_AUDIO_ERR_ARG;
  }

  min_ms = config->min_segment_ms != 0UL ? config->min_segment_ms : 100UL;
  if (!cpkt_audio_ms_to_16k_frames(min_ms, &min_frames) ||
      (config->max_segment_ms != 0UL &&
       !cpkt_audio_ms_to_16k_frames(config->max_segment_ms, &max_frames))) {
    return CPKT_AUDIO_ERR_ARG;
  }
  if (config->max_segment_ms == 0UL) {
    max_frames = 0UL;
  }
  if (max_frames != 0UL && min_frames > max_frames) {
    return CPKT_AUDIO_ERR_ARG;
  }
  memory_spool_bytes = config->memory_spool_bytes != 0UL
                           ? config->memory_spool_bytes
                           : 1048576UL;
  max_spool_bytes =
      config->max_spool_bytes != 0UL ? config->max_spool_bytes : 1073741824UL;
  if (memory_spool_bytes < sizeof(float) || max_spool_bytes < sizeof(float) ||
      memory_spool_bytes > max_spool_bytes) {
    return CPKT_AUDIO_ERR_ARG;
  }
  memory_spool_frames = (size_t)(memory_spool_bytes / sizeof(float));
  max_spool_frames = (size_t)(max_spool_bytes / sizeof(float));
  if (memory_spool_frames == 0U || max_spool_frames == 0U ||
      memory_spool_frames > ((size_t)-1) / sizeof(float)) {
    return CPKT_AUDIO_ERR_ARG;
  }

  ptt = (cpkt_audio_ptt *)calloc(1, sizeof(*ptt));
  if (ptt == NULL) {
    return CPKT_AUDIO_ERR_ALLOC;
  }
  impl = (struct cpkt_audio_vox_impl *)calloc(1, sizeof(*impl));
  if (impl == NULL) {
    free(ptt);
    return CPKT_AUDIO_ERR_ALLOC;
  }
  impl->frames = (float *)malloc(sizeof(float) * memory_spool_frames);
  if (impl->frames == NULL) {
    free(impl);
    free(ptt);
    return CPKT_AUDIO_ERR_ALLOC;
  }

  impl->config.segment_sink = config->segment_sink;
  impl->config.segment_user = config->segment_user;
  impl->config.state_sink = config->state_sink;
  impl->config.state_user = config->state_user;
  impl->max_segment_frames = max_frames;
  impl->min_segment_frames = min_frames;
  impl->frame_capacity = memory_spool_frames;
  impl->max_spool_frames = max_spool_frames;

  ptt->impl = impl;
  ptt->press = cpkt_audio_ptt_press_impl;
  ptt->push_f32_mono_16k = cpkt_audio_ptt_push_f32_mono_16k_impl;
  ptt->release = cpkt_audio_ptt_release_impl;
  ptt->flush = cpkt_audio_ptt_flush_impl;
  ptt->destroy = cpkt_audio_ptt_destroy_impl;
  *out = ptt;
  return CPKT_AUDIO_OK;
}

/** Reports decode support for a public audio format. */
CPKT_AUDIO_EXPORT int cpkt_audio_format_can_decode(int format) {
  return format == CPKT_AUDIO_FORMAT_WAV || format == CPKT_AUDIO_FORMAT_FLAC ||
         format == CPKT_AUDIO_FORMAT_MP3;
}

/** Reports encode support for a public audio format. */
CPKT_AUDIO_EXPORT int cpkt_audio_format_can_encode(int format) {
  return format == CPKT_AUDIO_FORMAT_WAV;
}

/** Converts an audio result code into a stable diagnostic string. */
CPKT_AUDIO_EXPORT const char *
cpkt_audio_result_string(cpkt_audio_result result) {
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
  case CPKT_AUDIO_TIMEOUT:
    return "timeout";
  default:
    return "unknown audio result";
  }
}
