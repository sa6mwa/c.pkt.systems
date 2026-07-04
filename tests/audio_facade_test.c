#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <cmocka.h>

#include <cpkt/audio.h>

struct memory_reader {
  const unsigned char *data;
  size_t size;
  size_t cursor;
  size_t max_chunk;
};

struct memory_writer {
  unsigned char data[1024];
  size_t size;
  size_t cursor;
  size_t fail_after_size;
};

static const unsigned char test_wav_mono_8000[] = {
    'R',  'I',  'F',  'F',  52,   0,    0,    0,    'W',  'A',  'V',  'E',
    'f',  'm',  't',  ' ',  16,   0,    0,    0,    1,    0,    1,    0,
    0x40, 0x1f, 0,    0,    0x80, 0x3e, 0,    0,    2,    0,    16,   0,
    'd',  'a',  't',  'a',  16,   0,    0,    0,    0x00, 0x00, 0xff, 0x1f,
    0x00, 0x40, 0xff, 0x1f, 0x00, 0x00, 0x01, 0xe0, 0x00, 0xc0, 0x01, 0xe0};

static const unsigned char test_flac_mono_8000[] = {
    0x66, 0x4c, 0x61, 0x43, 0x00, 0x00, 0x00, 0x22, 0x10, 0x00, 0x10,
    0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x0c, 0x01, 0xf4, 0x00, 0xf0,
    0x00, 0x00, 0x00, 0x28, 0xbb, 0xf7, 0xc6, 0x07, 0x79, 0x62, 0xa7,
    0xc2, 0x81, 0x14, 0xdb, 0xd1, 0x0b, 0xe9, 0x47, 0xcd, 0x84, 0x00,
    0x00, 0x28, 0x20, 0x00, 0x00, 0x00, 0x72, 0x65, 0x66, 0x65, 0x72,
    0x65, 0x6e, 0x63, 0x65, 0x20, 0x6c, 0x69, 0x62, 0x46, 0x4c, 0x41,
    0x43, 0x20, 0x31, 0x2e, 0x35, 0x2e, 0x30, 0x20, 0x32, 0x30, 0x32,
    0x35, 0x30, 0x32, 0x31, 0x31, 0x00, 0x00, 0x00, 0x00, 0xff, 0xf8,
    0x64, 0x08, 0x00, 0x27, 0x16, 0x00, 0x00, 0x00, 0x62, 0x62};

static const char test_mp3_mono_44100_base64[] =
    "SUQzBAAAAAAAIlRTU0UAAAAOAAADTGF2ZjYxLjcuMTAwAAAAAAAAAAAAAAD/"
    "+1DAAAAAAAAAAAAA"
    "AAAAAAAAAABJbmZvAAAADwAAAAMAAANCAH9/f39/f39/f39/f39/f39/f39/f39/f39/f39/"
    "f39/"
    "f7+/v7+/v7+/v7+/v7+/v7+/v7+/v7+/v7+/v7+/v7+/v/////////////////////////////"
    "//"
    "/////////////"
    "wAAAABMYXZjNjEuMTkAAAAAAAAAAAAAAAAkAqMAAAAAAAADQlOBWkoAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA//"
    "tQxAAACixDLlWUgAGBFWYjOtAAB6rDjjOF02TxocEI"
    "HFgfXh8YHEYEHonlsy5aD6g7E2vw+GBQFAQDAoJEDE5o0aNsH3iAEAQxOD/"
    "BB050+c5fznL+7p93"
    "Lg+"
    "D4Ph8EAxKAMH9IAEIAAEEABhqOhh7cQgAQxTkZGgxtMo1uIUAgcb7B0p2czA4omYlhMYRhIt7"
    "r9bBHQHT4DlEmC1fhah2jCiW/47hhhLiRHr/5ImReLxiXf/y6ZF4vIl0u/xEFQVER7/"
    "gqgBSsYA4"
    "AdmAPP/"
    "7UsQEggpgIRC98QABWQSh4r4gAAHZgTgFyYAgBPGAjgcBltKwGZHsKjmFcA3JgpAHOYEm"
    "BeGBUgUhgXIEUYBiAVQNRw6CztWZp1+qijh78q3/rXb9NWjq/enG///"
    "vAgngcAMmAKgJZgVIFGYD"
    "ECiGCfBRRnWSrqZkaM9mG9hMZg6YGyYEOCKmB5gbZgdYGeYBiAfprvPDFiwco4UZ1nFaqN+S+/"
    "D9"
    "3bQmxVm9Fn7l/oM+iuYqMAIAp1B2B3CHH43AwFAAA9mFYgg9oFI2fyYX//tSxA2ADeTJe/"
    "mJhJEw"
    "jaMDsvAAU8oeWf0puguE1Ir4jcugdxAHR4uMey6CMwHEwASfFLjTLo4wt6AGEFoIlL5XY3K7Bc"
    "MH"
    "oizg+IV39N7vGeEJhqj4GOH3//lApFMwOGZc/"
    "6gcEZwCBcg14DjgNtAWCL6mMeai5pFrDmQqbTJo"
    "DqhXMyZYEtKSYnouKEwFdHL6PSqm1WxYT67Chr7MXVrZesMbNfa3q9PWFQW6////"
    "fy3numpMQU1F"
    "My4xMDCqqqqqqqqqqqqqqqqqqqqqqqo=";

static size_t memory_read(void *user, void *buffer, size_t bytes_to_read) {
  struct memory_reader *reader;
  size_t remaining;
  size_t to_copy;

  reader = (struct memory_reader *)user;
  remaining = reader->size - reader->cursor;
  to_copy = remaining < bytes_to_read ? remaining : bytes_to_read;
  if (reader->max_chunk != 0 && to_copy > reader->max_chunk) {
    to_copy = reader->max_chunk;
  }
  if (to_copy > 0) {
    memcpy(buffer, reader->data + reader->cursor, to_copy);
    reader->cursor += to_copy;
  }
  return to_copy;
}

static int memory_seek(void *user, long offset, int origin) {
  struct memory_reader *reader;
  long base;
  long next;

  reader = (struct memory_reader *)user;
  if (origin == CPKT_AUDIO_SEEK_SET) {
    base = 0;
  } else if (origin == CPKT_AUDIO_SEEK_CUR) {
    base = (long)reader->cursor;
  } else if (origin == CPKT_AUDIO_SEEK_END) {
    base = (long)reader->size;
  } else {
    return -1;
  }
  next = base + offset;
  if (next < 0 || (size_t)next > reader->size) {
    return -1;
  }
  reader->cursor = (size_t)next;
  return 0;
}

static int base64_value(char ch) {
  if (ch >= 'A' && ch <= 'Z') {
    return ch - 'A';
  }
  if (ch >= 'a' && ch <= 'z') {
    return ch - 'a' + 26;
  }
  if (ch >= '0' && ch <= '9') {
    return ch - '0' + 52;
  }
  if (ch == '+') {
    return 62;
  }
  if (ch == '/') {
    return 63;
  }
  return -1;
}

static size_t decode_base64_fixture(const char *input, unsigned char *output,
                                    size_t output_capacity) {
  unsigned long accumulator;
  int bits;
  int value;
  size_t out_size;

  accumulator = 0;
  bits = 0;
  out_size = 0;
  while (*input != '\0') {
    if (*input == '=') {
      break;
    }
    value = base64_value(*input);
    assert_true(value >= 0);
    accumulator = (accumulator << 6) | (unsigned long)value;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      assert_true(out_size < output_capacity);
      output[out_size++] = (unsigned char)((accumulator >> bits) & 0xffU);
    }
    ++input;
  }
  return out_size;
}

static int cpkt_test_send_all(int fd, const void *buffer, size_t size) {
  const unsigned char *cursor;
  ssize_t sent;

  cursor = (const unsigned char *)buffer;
  while (size > 0U) {
    sent = send(fd, cursor, size, 0);
    if (sent <= 0) {
      return -1;
    }
    cursor += (size_t)sent;
    size -= (size_t)sent;
  }
  return 0;
}

static int cpkt_test_start_prefix_http_server(const unsigned char *data,
                                              size_t data_size,
                                              unsigned short *port_out,
                                              pid_t *pid_out) {
  struct sockaddr_in address;
  socklen_t address_length;
  int listen_fd;
  pid_t child;

  if (data == NULL || port_out == NULL || pid_out == NULL) {
    return -1;
  }
  *port_out = 0U;
  *pid_out = (pid_t)-1;
  listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) {
    return -1;
  }
  memset(&address, 0, sizeof(address));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = 0;
  if (bind(listen_fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
    close(listen_fd);
    return -1;
  }
  if (listen(listen_fd, 1) != 0) {
    close(listen_fd);
    return -1;
  }
  address_length = (socklen_t)sizeof(address);
  if (getsockname(listen_fd, (struct sockaddr *)&address, &address_length) !=
      0) {
    close(listen_fd);
    return -1;
  }

  child = fork();
  if (child < 0) {
    close(listen_fd);
    return -1;
  }
  if (child == 0) {
    char request[512];
    char header[256];
    int client_fd;
    size_t advertised_size;

    client_fd = accept(listen_fd, NULL, NULL);
    if (client_fd < 0) {
      _exit(2);
    }
    (void)recv(client_fd, request, sizeof(request), 0);
    advertised_size = data_size + 1048576U;
    (void)sprintf(header,
                  "HTTP/1.1 200 OK\r\n"
                  "Content-Type: audio/mpeg\r\n"
                  "Content-Length: %lu\r\n"
                  "Connection: close\r\n"
                  "\r\n",
                  (unsigned long)advertised_size);
    if (cpkt_test_send_all(client_fd, header, strlen(header)) != 0 ||
        cpkt_test_send_all(client_fd, data, data_size) != 0) {
      close(client_fd);
      close(listen_fd);
      _exit(3);
    }
    sleep(10);
    close(client_fd);
    close(listen_fd);
    _exit(0);
  }

  close(listen_fd);
  *port_out = ntohs(address.sin_port);
  *pid_out = child;
  return 0;
}

static void cpkt_test_stop_http_server(pid_t pid) {
  int status;

  if (pid <= 0) {
    return;
  }
  (void)kill(pid, SIGTERM);
  (void)waitpid(pid, &status, 0);
}

static int failing_seek(void *user, long offset, int origin) {
  (void)user;
  (void)offset;
  (void)origin;
  return -1;
}

static size_t invalid_read(void *user, void *buffer, size_t bytes_to_read) {
  (void)user;
  (void)buffer;
  return bytes_to_read + 1U;
}

static size_t memory_write(void *user, const void *buffer,
                           size_t bytes_to_write) {
  struct memory_writer *writer;
  size_t remaining;
  size_t to_copy;

  writer = (struct memory_writer *)user;
  remaining = sizeof(writer->data) - writer->cursor;
  if (writer->fail_after_size != 0) {
    if (writer->cursor >= writer->fail_after_size) {
      return 0;
    }
    if (remaining > writer->fail_after_size - writer->cursor) {
      remaining = writer->fail_after_size - writer->cursor;
    }
  }
  to_copy = remaining < bytes_to_write ? remaining : bytes_to_write;
  if (to_copy > 0) {
    memcpy(writer->data + writer->cursor, buffer, to_copy);
    writer->cursor += to_copy;
    if (writer->cursor > writer->size) {
      writer->size = writer->cursor;
    }
  }
  return to_copy;
}

static int memory_writer_seek(void *user, long offset, int origin) {
  struct memory_writer *writer;
  long base;
  long next;

  writer = (struct memory_writer *)user;
  if (origin == CPKT_AUDIO_SEEK_SET) {
    base = 0;
  } else if (origin == CPKT_AUDIO_SEEK_CUR) {
    base = (long)writer->cursor;
  } else if (origin == CPKT_AUDIO_SEEK_END) {
    base = (long)writer->size;
  } else {
    return -1;
  }
  next = base + offset;
  if (next < 0 || (size_t)next > sizeof(writer->data)) {
    return -1;
  }
  writer->cursor = (size_t)next;
  if (writer->cursor > writer->size) {
    writer->size = writer->cursor;
  }
  return 0;
}

static void assert_decoder_reads_f32_mono_16k(cpkt_audio_decoder *decoder,
                                              int expected_source_format) {
  cpkt_audio_stream_info info;
  float frames[64];
  size_t frames_read;
  size_t total;
  float peak;
  cpkt_audio_result result;

  memset(&info, 0, sizeof(info));
  assert_int_equal(decoder->info(decoder, &info), CPKT_AUDIO_OK);
  assert_int_equal(info.source_format, expected_source_format);
  assert_int_equal(info.output_channels, 1);
  assert_int_equal(info.output_sample_rate, 16000);

  total = 0;
  peak = 0.0f;
  do {
    size_t i;

    frames_read = 0;
    result = decoder->read_f32_mono_16k(decoder, frames, 64, &frames_read);
    assert_true(result == CPKT_AUDIO_OK || result == CPKT_AUDIO_AT_END);
    for (i = 0U; i < frames_read; ++i) {
      float sample;

      sample = frames[i] < 0.0f ? -frames[i] : frames[i];
      if (sample > peak) {
        peak = sample;
      }
    }
    total += frames_read;
  } while (result != CPKT_AUDIO_AT_END);

  assert_true(total >= 8);
  if (expected_source_format == CPKT_AUDIO_FORMAT_MP3) {
    assert_true(peak > 0.000001f);
  }
  frames_read = 1;
  assert_int_equal(
      decoder->read_f32_mono_16k(decoder, frames, 64, &frames_read),
      CPKT_AUDIO_AT_END);
  assert_int_equal(frames_read, 0);
}

static double cpkt_test_elapsed_seconds(const struct timespec *start,
                                        const struct timespec *end) {
  return (double)(end->tv_sec - start->tv_sec) +
         (double)(end->tv_nsec - start->tv_nsec) / 1000000000.0;
}

static void assert_decoder_reads_fixture_file(
    const char *path, const unsigned char *data, size_t size,
    const cpkt_audio_decoder_config *config, int expected_source_format) {
  FILE *file;
  cpkt_audio_decoder *decoder;

  (void)remove(path);
  file = fopen(path, "wb");
  assert_non_null(file);
  assert_int_equal(fwrite(data, 1, size, file), size);
  assert_int_equal(fclose(file), 0);

  decoder = NULL;
  assert_int_equal(cpkt_audio_decoder_open_file(&decoder, path, config),
                   CPKT_AUDIO_OK);
  assert_non_null(decoder);
  assert_decoder_reads_f32_mono_16k(decoder, expected_source_format);
  decoder->destroy(decoder);
  assert_int_equal(remove(path), 0);
}

static void write_test_pcm(cpkt_audio_encoder *encoder) {
  float frames[16];
  size_t frames_written;
  size_t i;

  for (i = 0; i < 16; ++i) {
    frames[i] = (float)i / 16.0f;
  }
  frames_written = 0;
  assert_int_equal(encoder->write_f32(encoder, frames, 16, &frames_written),
                   CPKT_AUDIO_OK);
  assert_int_equal(frames_written, 16);
}

static void test_decoder_reads_from_callback_reader(void **state) {
  struct memory_reader memory;
  cpkt_audio_reader reader;
  cpkt_audio_decoder *decoder;

  (void)state;
  memset(&memory, 0, sizeof(memory));
  memory.data = test_wav_mono_8000;
  memory.size = sizeof(test_wav_mono_8000);
  memset(&reader, 0, sizeof(reader));
  reader.user = &memory;
  reader.read = memory_read;
  reader.seek = memory_seek;

  decoder = NULL;
  assert_int_equal(cpkt_audio_decoder_open_reader(&decoder, &reader, NULL),
                   CPKT_AUDIO_OK);
  assert_non_null(decoder);
  assert_non_null(decoder->read_f32_mono_16k);
  assert_non_null(decoder->info);
  assert_non_null(decoder->destroy);
  assert_decoder_reads_f32_mono_16k(decoder, CPKT_AUDIO_FORMAT_WAV);
  decoder->destroy(decoder);
}

static void test_decoder_reads_fragmented_callback_reader(void **state) {
  struct memory_reader memory;
  cpkt_audio_reader reader;
  cpkt_audio_decoder *decoder;

  (void)state;
  memset(&memory, 0, sizeof(memory));
  memory.data = test_wav_mono_8000;
  memory.size = sizeof(test_wav_mono_8000);
  memory.max_chunk = 16;
  memset(&reader, 0, sizeof(reader));
  reader.user = &memory;
  reader.read = memory_read;
  reader.seek = memory_seek;

  decoder = NULL;
  assert_int_equal(cpkt_audio_decoder_open_reader(&decoder, &reader, NULL),
                   CPKT_AUDIO_OK);
  assert_non_null(decoder);
  assert_decoder_reads_f32_mono_16k(decoder, CPKT_AUDIO_FORMAT_WAV);
  decoder->destroy(decoder);
}

static void test_decoder_rejects_seekless_callback_reader_safely(void **state) {
  struct memory_reader memory;
  cpkt_audio_reader reader;
  cpkt_audio_decoder_config config;
  cpkt_audio_decoder *decoder;

  (void)state;
  memset(&memory, 0, sizeof(memory));
  memory.data = test_wav_mono_8000;
  memory.size = sizeof(test_wav_mono_8000);
  memory.max_chunk = 11;
  memset(&reader, 0, sizeof(reader));
  reader.user = &memory;
  reader.read = memory_read;
  reader.seek = NULL;
  memset(&config, 0, sizeof(config));
  config.encoding = CPKT_AUDIO_ENCODING_WAV;

  decoder = (cpkt_audio_decoder *)1;
  assert_int_equal(cpkt_audio_decoder_open_reader(&decoder, &reader, &config),
                   CPKT_AUDIO_ERR_IO);
  assert_null(decoder);
}

static void test_decoder_callback_failures(void **state) {
  struct memory_reader memory;
  cpkt_audio_reader reader;
  cpkt_audio_decoder *decoder;

  (void)state;
  memset(&memory, 0, sizeof(memory));
  memory.data = test_wav_mono_8000;
  memory.size = sizeof(test_wav_mono_8000);
  memset(&reader, 0, sizeof(reader));
  reader.user = &memory;
  reader.read = memory_read;
  reader.seek = failing_seek;

  decoder = (cpkt_audio_decoder *)1;
  assert_int_equal(cpkt_audio_decoder_open_reader(&decoder, &reader, NULL),
                   CPKT_AUDIO_ERR_IO);
  assert_null(decoder);

  memset(&reader, 0, sizeof(reader));
  reader.read = invalid_read;
  decoder = (cpkt_audio_decoder *)1;
  assert_int_equal(cpkt_audio_decoder_open_reader(&decoder, &reader, NULL),
                   CPKT_AUDIO_ERR_IO);
  assert_null(decoder);
}

static void test_decoder_reads_from_file(void **state) {
  (void)state;
  assert_true(cpkt_audio_format_can_decode(CPKT_AUDIO_FORMAT_WAV));
  assert_decoder_reads_fixture_file(
      "cpkt_audio_facade_test.wav", test_wav_mono_8000,
      sizeof(test_wav_mono_8000), NULL, CPKT_AUDIO_FORMAT_WAV);
}

static void test_decoder_reads_flac_file_when_supported(void **state) {
  (void)state;
  assert_true(cpkt_audio_format_can_decode(CPKT_AUDIO_FORMAT_FLAC));
  assert_decoder_reads_fixture_file(
      "cpkt_audio_facade_test.flac", test_flac_mono_8000,
      sizeof(test_flac_mono_8000), NULL, CPKT_AUDIO_FORMAT_FLAC);
}

static void test_decoder_reads_mp3_file_when_supported(void **state) {
  unsigned char mp3_data[1536];
  size_t mp3_size;

  (void)state;
  mp3_size = decode_base64_fixture(test_mp3_mono_44100_base64, mp3_data,
                                   sizeof(mp3_data));
  assert_true(mp3_size > 0);
  assert_true(cpkt_audio_format_can_decode(CPKT_AUDIO_FORMAT_MP3));
  assert_decoder_reads_fixture_file("cpkt_audio_facade_test.mp3", mp3_data,
                                    mp3_size, NULL, CPKT_AUDIO_FORMAT_MP3);
}

static void test_decoder_reads_mp3_file_url_when_supported(void **state) {
  unsigned char mp3_data[1536];
  char cwd[1024];
  char url[1200];
  const char *path;
  cpkt_audio_decoder *decoder;
  FILE *file;
  size_t mp3_size;

  (void)state;
  path = "cpkt_audio_facade_test_url.mp3";
  (void)remove(path);
  mp3_size = decode_base64_fixture(test_mp3_mono_44100_base64, mp3_data,
                                   sizeof(mp3_data));
  assert_true(mp3_size > 0);
  file = fopen(path, "wb");
  assert_non_null(file);
  assert_int_equal(fwrite(mp3_data, 1, mp3_size, file), mp3_size);
  assert_int_equal(fclose(file), 0);
  assert_non_null(getcwd(cwd, sizeof(cwd)));
  assert_true(strlen(cwd) + strlen(path) + 9U < sizeof(url));
  (void)sprintf(url, "file://%s/%s", cwd, path);

  decoder = NULL;
  assert_int_equal(cpkt_audio_decoder_open_url(&decoder, url, NULL),
                   CPKT_AUDIO_OK);
  assert_non_null(decoder);
  assert_decoder_reads_f32_mono_16k(decoder, CPKT_AUDIO_FORMAT_MP3);
  decoder->destroy(decoder);
  assert_int_equal(remove(path), 0);
}

static void test_decoder_http_url_streams_before_full_response(void **state) {
  unsigned char mp3_fixture[1536];
  unsigned char mp3_data[1536 * 16];
  char url[128];
  cpkt_audio_decoder *decoder;
  float frames[512];
  size_t frames_read;
  struct timespec start_time;
  struct timespec end_time;
  unsigned short port;
  pid_t server_pid;
  size_t mp3_size;

  (void)state;
  mp3_size = decode_base64_fixture(test_mp3_mono_44100_base64, mp3_fixture,
                                   sizeof(mp3_fixture));
  assert_true(mp3_size > 0);
  for (frames_read = 0; frames_read < 16U; ++frames_read) {
    memcpy(mp3_data + (frames_read * mp3_size), mp3_fixture, mp3_size);
  }
  mp3_size *= 16U;
  assert_true(cpkt_audio_format_can_decode(CPKT_AUDIO_FORMAT_MP3));
  assert_int_equal(cpkt_test_start_prefix_http_server(mp3_data, mp3_size, &port,
                                                      &server_pid),
                   0);
  (void)sprintf(url, "http://127.0.0.1:%u/fixture.mp3", (unsigned int)port);

  decoder = NULL;
  assert_int_equal(clock_gettime(CLOCK_MONOTONIC, &start_time), 0);
  assert_int_equal(cpkt_audio_decoder_open_url(&decoder, url, NULL),
                   CPKT_AUDIO_OK);
  assert_non_null(decoder);
  frames_read = 0;
  assert_int_equal(
      decoder->read_f32_mono_16k(decoder, frames, 512U, &frames_read),
      CPKT_AUDIO_OK);
  assert_int_equal(clock_gettime(CLOCK_MONOTONIC, &end_time), 0);
  assert_true(frames_read > 0U);
  assert_true(cpkt_test_elapsed_seconds(&start_time, &end_time) < 5.0);
  decoder->destroy(decoder);
  cpkt_test_stop_http_server(server_pid);
}

static void test_decoder_http_url_waits_for_large_id3_tag(void **state) {
  unsigned char mp3_fixture[1536];
  unsigned char mp3_data[11000];
  char url[128];
  cpkt_audio_decoder *decoder;
  unsigned short port;
  pid_t server_pid;
  size_t mp3_size;
  size_t tag_payload_size;
  size_t total_size;

  (void)state;
  mp3_size = decode_base64_fixture(test_mp3_mono_44100_base64, mp3_fixture,
                                   sizeof(mp3_fixture));
  tag_payload_size = 8192U;
  total_size = 10U + tag_payload_size + mp3_size;
  assert_true(mp3_size > 0);
  assert_true(total_size <= sizeof(mp3_data));
  memset(mp3_data, 0, total_size);
  mp3_data[0] = 'I';
  mp3_data[1] = 'D';
  mp3_data[2] = '3';
  mp3_data[3] = 4U;
  mp3_data[8] = (unsigned char)((tag_payload_size >> 7U) & 0x7fU);
  mp3_data[9] = (unsigned char)(tag_payload_size & 0x7fU);
  memcpy(mp3_data + 10U + tag_payload_size, mp3_fixture, mp3_size);

  assert_int_equal(cpkt_test_start_prefix_http_server(mp3_data, total_size,
                                                      &port, &server_pid),
                   0);
  (void)sprintf(url, "http://127.0.0.1:%u/large-id3.mp3", (unsigned int)port);

  decoder = NULL;
  assert_int_equal(cpkt_audio_decoder_open_url(&decoder, url, NULL),
                   CPKT_AUDIO_OK);
  assert_non_null(decoder);
  assert_decoder_reads_f32_mono_16k(decoder, CPKT_AUDIO_FORMAT_MP3);
  decoder->destroy(decoder);
  cpkt_test_stop_http_server(server_pid);
}

static void test_encoder_writes_wav_file(void **state) {
  const char *path;
  cpkt_audio_encoder *encoder;
  cpkt_audio_decoder *decoder;

  (void)state;
  path = "cpkt_audio_encoder_test.wav";
  (void)remove(path);

  encoder = NULL;
  assert_int_equal(cpkt_audio_encoder_open_file(&encoder, path, NULL),
                   CPKT_AUDIO_OK);
  assert_non_null(encoder);
  assert_non_null(encoder->write_f32);
  assert_non_null(encoder->close);
  assert_non_null(encoder->destroy);
  write_test_pcm(encoder);
  assert_int_equal(encoder->close(encoder), CPKT_AUDIO_OK);
  assert_int_equal(encoder->close(encoder), CPKT_AUDIO_OK);
  encoder->destroy(encoder);

  decoder = NULL;
  assert_int_equal(cpkt_audio_decoder_open_file(&decoder, path, NULL),
                   CPKT_AUDIO_OK);
  assert_non_null(decoder);
  assert_decoder_reads_f32_mono_16k(decoder, CPKT_AUDIO_FORMAT_WAV);
  decoder->destroy(decoder);
  assert_int_equal(remove(path), 0);
}

static void test_encoder_writes_wav_callback_writer(void **state) {
  struct memory_writer memory;
  struct memory_reader readback_memory;
  cpkt_audio_writer writer;
  cpkt_audio_reader reader;
  cpkt_audio_encoder *encoder;
  cpkt_audio_decoder *decoder;

  (void)state;
  memset(&memory, 0, sizeof(memory));
  memset(&writer, 0, sizeof(writer));
  writer.user = &memory;
  writer.write = memory_write;
  writer.seek = memory_writer_seek;

  encoder = NULL;
  assert_int_equal(cpkt_audio_encoder_open_writer(&encoder, &writer, NULL),
                   CPKT_AUDIO_OK);
  assert_non_null(encoder);
  write_test_pcm(encoder);
  assert_int_equal(encoder->close(encoder), CPKT_AUDIO_OK);
  encoder->destroy(encoder);
  assert_true(memory.size > 44);

  memset(&readback_memory, 0, sizeof(readback_memory));
  readback_memory.data = memory.data;
  readback_memory.size = memory.size;
  memset(&reader, 0, sizeof(reader));
  reader.user = &readback_memory;
  reader.read = memory_read;
  reader.seek = memory_seek;

  decoder = NULL;
  assert_int_equal(cpkt_audio_decoder_open_reader(&decoder, &reader, NULL),
                   CPKT_AUDIO_OK);
  assert_non_null(decoder);
  assert_decoder_reads_f32_mono_16k(decoder, CPKT_AUDIO_FORMAT_WAV);
  decoder->destroy(decoder);
}

static void test_encoder_callback_write_failure(void **state) {
  struct memory_writer memory;
  cpkt_audio_writer writer;
  cpkt_audio_encoder *encoder;
  float frames[16];
  size_t frames_written;
  size_t i;

  (void)state;
  memset(&memory, 0, sizeof(memory));
  memset(&writer, 0, sizeof(writer));
  writer.user = &memory;
  writer.write = memory_write;
  writer.seek = memory_writer_seek;

  encoder = NULL;
  assert_int_equal(cpkt_audio_encoder_open_writer(&encoder, &writer, NULL),
                   CPKT_AUDIO_OK);
  assert_non_null(encoder);
  memory.fail_after_size = memory.size + 1U;
  for (i = 0; i < 16; ++i) {
    frames[i] = (float)i / 16.0f;
  }
  frames_written = 99;
  assert_int_equal(encoder->write_f32(encoder, frames, 16, &frames_written),
                   CPKT_AUDIO_OK);
  assert_int_equal(encoder->close(encoder), CPKT_AUDIO_ERR_IO);
  encoder->destroy(encoder);
}

struct vox_capture {
  unsigned long count;
  unsigned long hard_count;
  unsigned long final_count;
  unsigned long state_count;
  int states[16];
  unsigned long state_segments[16];
  size_t frames[8];
  float first_frame[8];
  int fail;
};

static int capture_vox_state(const cpkt_audio_vox_state_event *event,
                             void *user) {
  struct vox_capture *capture;

  capture = (struct vox_capture *)user;
  assert_non_null(event);
  assert_true(capture->state_count < 16UL);
  capture->states[capture->state_count] = event->state;
  capture->state_segments[capture->state_count] = event->segment_index;
  ++capture->state_count;
  return capture->fail ? 1 : 0;
}

static int capture_vox_segment(cpkt_audio_vox_segment *segment, void *user) {
  struct vox_capture *capture;
  float frames[256];
  size_t frames_read;
  size_t total;
  cpkt_audio_result result;

  capture = (struct vox_capture *)user;
  assert_non_null(segment);
  assert_non_null(segment->read_f32_mono_16k);
  if (capture->fail) {
    return 1;
  }
  assert_true(capture->count < 8UL);
  capture->frames[capture->count] = segment->frame_count;
  capture->first_frame[capture->count] = 999.0f;
  if (segment->hard_cut) {
    ++capture->hard_count;
  }
  if (segment->is_final) {
    ++capture->final_count;
  }
  assert_int_equal(segment->segment_index, capture->count);
  total = 0U;
  do {
    frames_read = 99U;
    result = segment->read_f32_mono_16k(segment, frames, 256U, &frames_read);
    assert_true(result == CPKT_AUDIO_OK || result == CPKT_AUDIO_AT_END);
    if (total == 0U && frames_read > 0U) {
      capture->first_frame[capture->count] = frames[0];
    }
    total += frames_read;
  } while (result != CPKT_AUDIO_AT_END);
  assert_int_equal(total, segment->frame_count);
  ++capture->count;
  return 0;
}

static void test_vox_releases_on_silence(void **state) {
  struct vox_capture capture;
  cpkt_audio_vox_config config;
  cpkt_audio_vox *vox;
  float frames[360];
  size_t i;

  (void)state;
  memset(&capture, 0, sizeof(capture));
  memset(&config, 0, sizeof(config));
  config.threshold = 0.1f;
  config.release_silence_ms = 10UL;
  config.power_window_ms = 1UL;
  config.max_segment_ms = 100UL;
  config.min_segment_ms = 1UL;
  config.segment_sink = capture_vox_segment;
  config.segment_user = &capture;
  config.state_sink = capture_vox_state;
  config.state_user = &capture;

  for (i = 0U; i < 160U; ++i) {
    frames[i] = 0.2f;
  }
  for (; i < 360U; ++i) {
    frames[i] = 0.0f;
  }

  vox = NULL;
  assert_int_equal(cpkt_audio_vox_open(&vox, &config), CPKT_AUDIO_OK);
  assert_non_null(vox);
  assert_int_equal(vox->push_f32_mono_16k(vox, frames, 360U), CPKT_AUDIO_OK);
  assert_int_equal(capture.count, 1UL);
  assert_int_equal(capture.hard_count, 0UL);
  assert_int_equal(capture.final_count, 0UL);
  assert_int_equal(capture.frames[0], 331U);
  assert_int_equal(capture.state_count, 2UL);
  assert_int_equal(capture.states[0], CPKT_AUDIO_VOX_TX_ON);
  assert_int_equal(capture.state_segments[0], 0UL);
  assert_int_equal(capture.states[1], CPKT_AUDIO_VOX_TX_OFF);
  assert_int_equal(capture.state_segments[1], 0UL);
  assert_int_equal(vox->flush(vox), CPKT_AUDIO_OK);
  assert_int_equal(capture.count, 1UL);
  vox->destroy(vox);
}

static void test_vox_includes_prebuffer_before_threshold(void **state) {
  struct vox_capture capture;
  cpkt_audio_vox_config config;
  cpkt_audio_vox *vox;
  float frames[440];
  size_t i;

  (void)state;
  memset(&capture, 0, sizeof(capture));
  memset(&config, 0, sizeof(config));
  config.threshold = 0.1f;
  config.release_silence_ms = 10UL;
  config.prebuffer_ms = 5UL;
  config.power_window_ms = 1UL;
  config.min_segment_ms = 1UL;
  config.segment_sink = capture_vox_segment;
  config.segment_user = &capture;

  for (i = 0U; i < 120U; ++i) {
    frames[i] = 0.01f;
  }
  for (; i < 240U; ++i) {
    frames[i] = 0.2f;
  }
  for (; i < 440U; ++i) {
    frames[i] = 0.0f;
  }

  vox = NULL;
  assert_int_equal(cpkt_audio_vox_open(&vox, &config), CPKT_AUDIO_OK);
  assert_non_null(vox);
  assert_int_equal(vox->push_f32_mono_16k(vox, frames, 440U), CPKT_AUDIO_OK);
  assert_int_equal(capture.count, 1UL);
  assert_int_equal(capture.frames[0], 368U);
  assert_float_equal(capture.first_frame[0], 0.01f, 0.0001f);
  assert_int_equal(vox->flush(vox), CPKT_AUDIO_OK);
  vox->destroy(vox);
}

static void test_vox_hard_cuts_at_segment_budget(void **state) {
  struct vox_capture capture;
  cpkt_audio_vox_config config;
  cpkt_audio_vox *vox;
  float frames[2000];
  size_t i;

  (void)state;
  memset(&capture, 0, sizeof(capture));
  memset(&config, 0, sizeof(config));
  config.threshold = 0.1f;
  config.release_silence_ms = 100UL;
  config.power_window_ms = 1UL;
  config.max_segment_ms = 50UL;
  config.min_segment_ms = 1UL;
  config.segment_sink = capture_vox_segment;
  config.segment_user = &capture;
  config.state_sink = capture_vox_state;
  config.state_user = &capture;

  for (i = 0U; i < 1000U; ++i) {
    frames[i] = 0.2f;
  }

  vox = NULL;
  assert_int_equal(cpkt_audio_vox_open(&vox, &config), CPKT_AUDIO_OK);
  assert_non_null(vox);
  assert_int_equal(vox->push_f32_mono_16k(vox, frames, 1000U), CPKT_AUDIO_OK);
  assert_int_equal(capture.count, 1UL);
  assert_int_equal(capture.hard_count, 1UL);
  assert_int_equal(capture.frames[0], 800U);
  assert_int_equal(capture.state_count, 3UL);
  assert_int_equal(capture.states[0], CPKT_AUDIO_VOX_TX_ON);
  assert_int_equal(capture.state_segments[0], 0UL);
  assert_int_equal(capture.states[1], CPKT_AUDIO_VOX_HARD_CUT);
  assert_int_equal(capture.state_segments[1], 0UL);
  assert_int_equal(capture.states[2], CPKT_AUDIO_VOX_TX_ON);
  assert_int_equal(capture.state_segments[2], 1UL);
  assert_int_equal(vox->flush(vox), CPKT_AUDIO_OK);
  assert_int_equal(capture.count, 2UL);
  assert_int_equal(capture.final_count, 1UL);
  assert_int_equal(capture.frames[1], 200U);
  assert_int_equal(capture.state_count, 4UL);
  assert_int_equal(capture.states[3], CPKT_AUDIO_VOX_TX_OFF);
  assert_int_equal(capture.state_segments[3], 1UL);
  vox->destroy(vox);
}

static void
test_vox_releases_at_hang_time_even_when_more_audio_fits(void **state) {
  struct vox_capture capture;
  cpkt_audio_vox_config config;
  cpkt_audio_vox *vox;
  float frames[1380];
  size_t i;

  (void)state;
  memset(&capture, 0, sizeof(capture));
  memset(&config, 0, sizeof(config));
  config.threshold = 0.1f;
  config.release_silence_ms = 10UL;
  config.power_window_ms = 1UL;
  config.max_segment_ms = 100UL;
  config.min_segment_ms = 1UL;
  config.segment_sink = capture_vox_segment;
  config.segment_user = &capture;

  for (i = 0U; i < 600U; ++i) {
    frames[i] = 0.2f;
  }
  for (; i < 780U; ++i) {
    frames[i] = 0.0f;
  }
  for (; i < 1380U; ++i) {
    frames[i] = 0.2f;
  }

  vox = NULL;
  assert_int_equal(cpkt_audio_vox_open(&vox, &config), CPKT_AUDIO_OK);
  assert_non_null(vox);
  assert_int_equal(vox->push_f32_mono_16k(vox, frames, 1380U), CPKT_AUDIO_OK);
  assert_int_equal(capture.count, 1UL);
  assert_int_equal(capture.hard_count, 0UL);
  assert_int_equal(capture.frames[0], 771U);
  assert_int_equal(vox->flush(vox), CPKT_AUDIO_OK);
  assert_int_equal(capture.count, 2UL);
  assert_int_equal(capture.final_count, 1UL);
  assert_int_equal(capture.frames[1], 609U);
  vox->destroy(vox);
}

static void test_vox_flush_drops_subminimum_post_cut_tail(void **state) {
  struct vox_capture capture;
  cpkt_audio_vox_config config;
  cpkt_audio_vox *vox;
  float frames[1380];
  size_t i;

  (void)state;
  memset(&capture, 0, sizeof(capture));
  memset(&config, 0, sizeof(config));
  config.threshold = 0.1f;
  config.release_silence_ms = 1000UL;
  config.power_window_ms = 1UL;
  config.max_segment_ms = 50UL;
  config.min_segment_ms = 10UL;
  config.segment_sink = capture_vox_segment;
  config.segment_user = &capture;

  for (i = 0U; i < 800U; ++i) {
    frames[i] = 0.2f;
  }
  for (; i < 880U; ++i) {
    frames[i] = 0.2f;
  }
  for (; i < 1380U; ++i) {
    frames[i] = 0.0f;
  }

  vox = NULL;
  assert_int_equal(cpkt_audio_vox_open(&vox, &config), CPKT_AUDIO_OK);
  assert_non_null(vox);
  assert_int_equal(vox->push_f32_mono_16k(vox, frames, 1380U), CPKT_AUDIO_OK);
  assert_int_equal(capture.count, 1UL);
  assert_int_equal(capture.hard_count, 1UL);
  assert_int_equal(capture.frames[0], 800U);
  assert_int_equal(vox->flush(vox), CPKT_AUDIO_OK);
  assert_int_equal(capture.count, 1UL);
  assert_int_equal(capture.final_count, 0UL);
  vox->destroy(vox);
}

static void test_vox_spills_and_hard_cuts_at_storage_budget(void **state) {
  struct vox_capture capture;
  cpkt_audio_vox_config config;
  cpkt_audio_vox *vox;
  float frames[48];
  size_t i;

  (void)state;
  memset(&capture, 0, sizeof(capture));
  memset(&config, 0, sizeof(config));
  config.threshold = 0.1f;
  config.release_silence_ms = 100UL;
  config.power_window_ms = 1UL;
  config.max_segment_ms = 0UL;
  config.min_segment_ms = 1UL;
  config.memory_spool_bytes = 64UL;
  config.max_spool_bytes = 128UL;
  config.segment_sink = capture_vox_segment;
  config.segment_user = &capture;

  for (i = 0U; i < 48U; ++i) {
    frames[i] = 0.2f;
  }

  vox = NULL;
  assert_int_equal(cpkt_audio_vox_open(&vox, &config), CPKT_AUDIO_OK);
  assert_non_null(vox);
  assert_int_equal(vox->push_f32_mono_16k(vox, frames, 48U), CPKT_AUDIO_OK);
  assert_int_equal(capture.count, 1UL);
  assert_int_equal(capture.hard_count, 1UL);
  assert_int_equal(capture.frames[0], 32U);
  assert_int_equal(vox->flush(vox), CPKT_AUDIO_OK);
  assert_int_equal(capture.count, 2UL);
  assert_int_equal(capture.final_count, 1UL);
  assert_int_equal(capture.frames[1], 16U);
  vox->destroy(vox);
}

static void
test_vox_rejects_invalid_and_reports_callback_failure(void **state) {
  struct vox_capture capture;
  cpkt_audio_vox_config config;
  cpkt_audio_vox *vox;
  float frames[16];
  size_t i;

  (void)state;
  memset(&capture, 0, sizeof(capture));
  memset(&config, 0, sizeof(config));
  vox = (cpkt_audio_vox *)1;
  assert_int_equal(cpkt_audio_vox_open(NULL, &config), CPKT_AUDIO_ERR_ARG);
  assert_int_equal(cpkt_audio_vox_open(&vox, NULL), CPKT_AUDIO_ERR_ARG);
  assert_null(vox);
  assert_int_equal(cpkt_audio_vox_open(&vox, &config), CPKT_AUDIO_ERR_ARG);
  assert_null(vox);

  config.segment_sink = capture_vox_segment;
  config.segment_user = &capture;
  config.max_segment_ms = 1UL;
  config.min_segment_ms = 1UL;
  capture.fail = 1;
  for (i = 0U; i < 16U; ++i) {
    frames[i] = 1.0f;
  }
  assert_int_equal(cpkt_audio_vox_open(&vox, &config), CPKT_AUDIO_OK);
  assert_non_null(vox);
  assert_int_equal(vox->push_f32_mono_16k(vox, frames, 16U), CPKT_AUDIO_ERR_IO);
  assert_int_equal(vox->flush(vox), CPKT_AUDIO_ERR_IO);
  vox->destroy(vox);
}

static void test_ptt_toggles_and_emits_segments(void **state) {
  struct vox_capture capture;
  cpkt_audio_ptt_config config;
  cpkt_audio_ptt *ptt;
  float frames[1000];
  size_t i;

  (void)state;
  memset(&capture, 0, sizeof(capture));
  memset(&config, 0, sizeof(config));
  config.max_segment_ms = 50UL;
  config.min_segment_ms = 1UL;
  config.segment_sink = capture_vox_segment;
  config.segment_user = &capture;
  config.state_sink = capture_vox_state;
  config.state_user = &capture;

  for (i = 0U; i < 1000U; ++i) {
    frames[i] = 0.0f;
  }

  ptt = NULL;
  assert_int_equal(cpkt_audio_ptt_open(&ptt, &config), CPKT_AUDIO_OK);
  assert_non_null(ptt);
  assert_int_equal(ptt->push_f32_mono_16k(ptt, frames, 100U), CPKT_AUDIO_OK);
  assert_int_equal(capture.count, 0UL);
  assert_int_equal(ptt->press(ptt), CPKT_AUDIO_OK);
  assert_int_equal(ptt->push_f32_mono_16k(ptt, frames, 1000U), CPKT_AUDIO_OK);
  assert_int_equal(capture.count, 1UL);
  assert_int_equal(capture.hard_count, 1UL);
  assert_int_equal(capture.frames[0], 800U);
  assert_int_equal(ptt->release(ptt), CPKT_AUDIO_OK);
  assert_int_equal(capture.count, 2UL);
  assert_int_equal(capture.frames[1], 200U);
  assert_int_equal(capture.final_count, 0UL);
  assert_int_equal(capture.state_count, 4UL);
  assert_int_equal(capture.states[0], CPKT_AUDIO_VOX_TX_ON);
  assert_int_equal(capture.states[1], CPKT_AUDIO_VOX_HARD_CUT);
  assert_int_equal(capture.states[2], CPKT_AUDIO_VOX_TX_ON);
  assert_int_equal(capture.states[3], CPKT_AUDIO_VOX_TX_OFF);
  assert_int_equal(ptt->flush(ptt), CPKT_AUDIO_OK);
  assert_int_equal(capture.count, 2UL);
  ptt->destroy(ptt);
}

static void test_ptt_rejects_invalid_arguments(void **state) {
  struct vox_capture capture;
  cpkt_audio_ptt_config config;
  cpkt_audio_ptt *ptt;

  (void)state;
  memset(&capture, 0, sizeof(capture));
  memset(&config, 0, sizeof(config));
  ptt = (cpkt_audio_ptt *)1;
  assert_int_equal(cpkt_audio_ptt_open(NULL, &config), CPKT_AUDIO_ERR_ARG);
  assert_int_equal(cpkt_audio_ptt_open(&ptt, NULL), CPKT_AUDIO_ERR_ARG);
  assert_null(ptt);
  assert_int_equal(cpkt_audio_ptt_open(&ptt, &config), CPKT_AUDIO_ERR_ARG);
  assert_null(ptt);

  config.segment_sink = capture_vox_segment;
  config.segment_user = &capture;
  config.min_segment_ms = 100UL;
  config.max_segment_ms = 1UL;
  assert_int_equal(cpkt_audio_ptt_open(&ptt, &config), CPKT_AUDIO_ERR_ARG);
  assert_null(ptt);
}

static void test_invalid_arguments(void **state) {
  cpkt_audio_decoder *decoder;
  cpkt_audio_encoder *encoder;
  cpkt_audio_capture *capture;
  cpkt_audio_playback *playback;
  cpkt_audio_capture_config capture_config;
  cpkt_audio_playback_config playback_config;
  cpkt_audio_reader reader;
  cpkt_audio_writer writer;
  cpkt_audio_encoder_config encoder_config;
  float frame;
  size_t frames_read;

  (void)state;
  decoder = (cpkt_audio_decoder *)1;
  assert_int_equal(cpkt_audio_decoder_open_file(NULL, "x.wav", NULL),
                   CPKT_AUDIO_ERR_ARG);
  assert_int_equal(cpkt_audio_decoder_open_file(&decoder, NULL, NULL),
                   CPKT_AUDIO_ERR_ARG);
  assert_null(decoder);
  decoder = (cpkt_audio_decoder *)1;
  assert_int_equal(
      cpkt_audio_decoder_open_url(NULL, "https://example.invalid", NULL),
      CPKT_AUDIO_ERR_ARG);
  assert_int_equal(cpkt_audio_decoder_open_url(&decoder, "", NULL),
                   CPKT_AUDIO_ERR_ARG);
  assert_null(decoder);

  memset(&reader, 0, sizeof(reader));
  decoder = (cpkt_audio_decoder *)1;
  assert_int_equal(cpkt_audio_decoder_open_reader(&decoder, &reader, NULL),
                   CPKT_AUDIO_ERR_ARG);
  assert_null(decoder);

  frames_read = 1;
  assert_int_equal(cpkt_audio_decoder_open_reader(NULL, &reader, NULL),
                   CPKT_AUDIO_ERR_ARG);

  encoder = (cpkt_audio_encoder *)1;
  assert_int_equal(cpkt_audio_encoder_open_file(NULL, "x.wav", NULL),
                   CPKT_AUDIO_ERR_ARG);
  assert_int_equal(cpkt_audio_encoder_open_file(&encoder, NULL, NULL),
                   CPKT_AUDIO_ERR_ARG);
  assert_null(encoder);

  memset(&writer, 0, sizeof(writer));
  encoder = (cpkt_audio_encoder *)1;
  assert_int_equal(cpkt_audio_encoder_open_writer(&encoder, &writer, NULL),
                   CPKT_AUDIO_ERR_ARG);
  assert_null(encoder);

  memset(&encoder_config, 0, sizeof(encoder_config));
  encoder_config.format = CPKT_AUDIO_FORMAT_MP3;
  encoder = (cpkt_audio_encoder *)1;
  assert_int_equal(
      cpkt_audio_encoder_open_file(&encoder, "x.mp3", &encoder_config),
      CPKT_AUDIO_ERR_FORMAT);
  assert_null(encoder);

  capture = (cpkt_audio_capture *)1;
  assert_int_equal(cpkt_audio_capture_open_default(NULL, NULL),
                   CPKT_AUDIO_ERR_ARG);
  (void)capture;

  playback = (cpkt_audio_playback *)1;
  assert_int_equal(cpkt_audio_playback_open_default(NULL, NULL),
                   CPKT_AUDIO_ERR_ARG);
  (void)playback;

  memset(&capture_config, 0, sizeof(capture_config));
  capture_config.backend = CPKT_AUDIO_DEVICE_BACKEND_PULSEAUDIO;
  capture = (cpkt_audio_capture *)1;
  assert_int_equal(cpkt_audio_capture_open_default(&capture, &capture_config),
                   CPKT_AUDIO_ERR_ARG);
  assert_null(capture);

  memset(&playback_config, 0, sizeof(playback_config));
  playback_config.backend = CPKT_AUDIO_DEVICE_BACKEND_JACK;
  playback = (cpkt_audio_playback *)1;
  assert_int_equal(
      cpkt_audio_playback_open_default(&playback, &playback_config),
      CPKT_AUDIO_ERR_ARG);
  assert_null(playback);

  assert_true(cpkt_audio_format_can_decode(CPKT_AUDIO_FORMAT_WAV));
  assert_true(cpkt_audio_format_can_decode(CPKT_AUDIO_FORMAT_FLAC));
  assert_true(cpkt_audio_format_can_decode(CPKT_AUDIO_FORMAT_MP3));
  assert_true(cpkt_audio_format_can_encode(CPKT_AUDIO_FORMAT_WAV));
  assert_false(cpkt_audio_format_can_encode(CPKT_AUDIO_FORMAT_MP3));

  assert_int_equal(cpkt_audio_result_string(CPKT_AUDIO_OK)[0], 'o');
  assert_int_equal(cpkt_audio_result_string((cpkt_audio_result)999)[0], 'u');
  frame = 0.0f;
  (void)frame;
  (void)frames_read;
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_decoder_reads_from_callback_reader),
      cmocka_unit_test(test_decoder_reads_fragmented_callback_reader),
      cmocka_unit_test(test_decoder_rejects_seekless_callback_reader_safely),
      cmocka_unit_test(test_decoder_callback_failures),
      cmocka_unit_test(test_decoder_reads_from_file),
      cmocka_unit_test(test_decoder_reads_flac_file_when_supported),
      cmocka_unit_test(test_decoder_reads_mp3_file_when_supported),
      cmocka_unit_test(test_decoder_reads_mp3_file_url_when_supported),
      cmocka_unit_test(test_decoder_http_url_streams_before_full_response),
      cmocka_unit_test(test_decoder_http_url_waits_for_large_id3_tag),
      cmocka_unit_test(test_encoder_writes_wav_file),
      cmocka_unit_test(test_encoder_writes_wav_callback_writer),
      cmocka_unit_test(test_encoder_callback_write_failure),
      cmocka_unit_test(test_vox_releases_on_silence),
      cmocka_unit_test(test_vox_includes_prebuffer_before_threshold),
      cmocka_unit_test(test_vox_hard_cuts_at_segment_budget),
      cmocka_unit_test(
          test_vox_releases_at_hang_time_even_when_more_audio_fits),
      cmocka_unit_test(test_vox_flush_drops_subminimum_post_cut_tail),
      cmocka_unit_test(test_vox_spills_and_hard_cuts_at_storage_budget),
      cmocka_unit_test(test_vox_rejects_invalid_and_reports_callback_failure),
      cmocka_unit_test(test_ptt_toggles_and_emits_segments),
      cmocka_unit_test(test_ptt_rejects_invalid_arguments),
      cmocka_unit_test(test_invalid_arguments),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
