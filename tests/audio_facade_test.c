#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    "SUQzBAAAAAAAIlRTU0UAAAAOAAADTGF2ZjYxLjcuMTAwAAAAAAAAAAAAAAD/+1DAAAAAAAAAAAAA"
    "AAAAAAAAAABJbmZvAAAADwAAAAMAAANCAH9/f39/f39/f39/f39/f39/f39/f39/f39/f39/f39/"
    "f7+/v7+/v7+/v7+/v7+/v7+/v7+/v7+/v7+/v7+/v7+/v///////////////////////////////"
    "/////////////wAAAABMYXZjNjEuMTkAAAAAAAAAAAAAAAAkAqMAAAAAAAADQlOBWkoAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA//tQxAAACixDLlWUgAGBFWYjOtAAB6rDjjOF02TxocEI"
    "HFgfXh8YHEYEHonlsy5aD6g7E2vw+GBQFAQDAoJEDE5o0aNsH3iAEAQxOD/BB050+c5fznL+7p93"
    "Lg+D4Ph8EAxKAMH9IAEIAAEEABhqOhh7cQgAQxTkZGgxtMo1uIUAgcb7B0p2czA4omYlhMYRhIt7"
    "r9bBHQHT4DlEmC1fhah2jCiW/47hhhLiRHr/5ImReLxiXf/y6ZF4vIl0u/xEFQVER7/gqgBSsYA4"
    "AdmAPP/7UsQEggpgIRC98QABWQSh4r4gAAHZgTgFyYAgBPGAjgcBltKwGZHsKjmFcA3JgpAHOYEm"
    "BeGBUgUhgXIEUYBiAVQNRw6CztWZp1+qijh78q3/rXb9NWjq/enG///vAgngcAMmAKgJZgVIFGYD"
    "ECiGCfBRRnWSrqZkaM9mG9hMZg6YGyYEOCKmB5gbZgdYGeYBiAfprvPDFiwco4UZ1nFaqN+S+/D9"
    "3bQmxVm9Fn7l/oM+iuYqMAIAp1B2B3CHH43AwFAAA9mFYgg9oFI2fyYX//tSxA2ADeTJe/mJhJEw"
    "jaMDsvAAU8oeWf0puguE1Ir4jcugdxAHR4uMey6CMwHEwASfFLjTLo4wt6AGEFoIlL5XY3K7BcMH"
    "oizg+IV39N7vGeEJhqj4GOH3//lApFMwOGZc/6gcEZwCBcg14DjgNtAWCL6mMeai5pFrDmQqbTJo"
    "DqhXMyZYEtKSYnouKEwFdHL6PSqm1WxYT67Chr7MXVrZesMbNfa3q9PWFQW6////fy3numpMQU1F"
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
  cpkt_audio_result result;

  memset(&info, 0, sizeof(info));
  assert_int_equal(decoder->info(decoder, &info), CPKT_AUDIO_OK);
  assert_int_equal(info.source_format, expected_source_format);
  assert_int_equal(info.output_channels, 1);
  assert_int_equal(info.output_sample_rate, 16000);

  total = 0;
  do {
    frames_read = 0;
    result = decoder->read_f32_mono_16k(decoder, frames, 64, &frames_read);
    assert_true(result == CPKT_AUDIO_OK || result == CPKT_AUDIO_AT_END);
    total += frames_read;
  } while (result != CPKT_AUDIO_AT_END);

  assert_true(total >= 8);
  frames_read = 1;
  assert_int_equal(decoder->read_f32_mono_16k(decoder, frames, 64,
                                              &frames_read),
                   CPKT_AUDIO_AT_END);
  assert_int_equal(frames_read, 0);
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
  assert_decoder_reads_fixture_file("cpkt_audio_facade_test.wav",
                                    test_wav_mono_8000,
                                    sizeof(test_wav_mono_8000), NULL,
                                    CPKT_AUDIO_FORMAT_WAV);
}

static void test_decoder_reads_flac_file_when_supported(void **state) {
  (void)state;
  assert_true(cpkt_audio_format_can_decode(CPKT_AUDIO_FORMAT_FLAC));
  assert_decoder_reads_fixture_file("cpkt_audio_facade_test.flac",
                                    test_flac_mono_8000,
                                    sizeof(test_flac_mono_8000), NULL,
                                    CPKT_AUDIO_FORMAT_FLAC);
}

static void test_decoder_reads_mp3_file_when_supported(void **state) {
  unsigned char mp3_data[1536];
  cpkt_audio_decoder_config config;
  size_t mp3_size;

  (void)state;
  memset(&config, 0, sizeof(config));
  config.encoding = CPKT_AUDIO_ENCODING_MP3;
  mp3_size = decode_base64_fixture(test_mp3_mono_44100_base64, mp3_data,
                                   sizeof(mp3_data));
  assert_true(mp3_size > 0);
  assert_true(cpkt_audio_format_can_decode(CPKT_AUDIO_FORMAT_MP3));
  assert_decoder_reads_fixture_file("cpkt_audio_facade_test.mp3", mp3_data,
                                    mp3_size, &config,
                                    CPKT_AUDIO_FORMAT_MP3);
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

static void test_invalid_arguments(void **state) {
  cpkt_audio_decoder *decoder;
  cpkt_audio_encoder *encoder;
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
      cmocka_unit_test(test_encoder_writes_wav_file),
      cmocka_unit_test(test_encoder_writes_wav_callback_writer),
      cmocka_unit_test(test_encoder_callback_write_failure),
      cmocka_unit_test(test_invalid_arguments),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
