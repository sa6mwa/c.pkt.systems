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
};

static const unsigned char test_wav_mono_8000[] = {
    'R', 'I', 'F', 'F', 52, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ',
    16, 0, 0, 0, 1, 0, 1, 0, 0x40, 0x1f, 0, 0, 0x80, 0x3e, 0, 0, 2, 0, 16, 0,
    'd', 'a', 't', 'a', 16, 0, 0, 0, 0x00, 0x00, 0xff, 0x1f, 0x00, 0x40, 0xff, 0x1f,
    0x00, 0x00, 0x01, 0xe0, 0x00, 0xc0, 0x01, 0xe0};

static size_t memory_read(void *user, void *buffer, size_t bytes_to_read) {
  struct memory_reader *reader;
  size_t remaining;
  size_t to_copy;

  reader = (struct memory_reader *)user;
  remaining = reader->size - reader->cursor;
  to_copy = remaining < bytes_to_read ? remaining : bytes_to_read;
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

static void assert_decoder_reads_f32_mono_16k(cpkt_audio_decoder *decoder) {
  cpkt_audio_stream_info info;
  float frames[64];
  size_t frames_read;
  size_t total;
  cpkt_audio_result result;

  memset(&info, 0, sizeof(info));
  assert_int_equal(decoder->info(decoder, &info), CPKT_AUDIO_OK);
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
  assert_int_equal(cpkt_audio_decoder_open_reader(&decoder, &reader, NULL), CPKT_AUDIO_OK);
  assert_non_null(decoder);
  assert_non_null(decoder->read_f32_mono_16k);
  assert_non_null(decoder->info);
  assert_non_null(decoder->destroy);
  assert_decoder_reads_f32_mono_16k(decoder);
  decoder->destroy(decoder);
}

static void test_decoder_reads_from_file(void **state) {
  const char *path;
  FILE *file;
  cpkt_audio_decoder *decoder;

  (void)state;
  path = "cpkt_audio_facade_test.wav";
  (void)remove(path);
  file = fopen(path, "wb");
  assert_non_null(file);
  assert_int_equal(fwrite(test_wav_mono_8000, 1, sizeof(test_wav_mono_8000), file), sizeof(test_wav_mono_8000));
  assert_int_equal(fclose(file), 0);

  decoder = NULL;
  assert_int_equal(cpkt_audio_decoder_open_file(&decoder, path, NULL), CPKT_AUDIO_OK);
  assert_non_null(decoder);
  assert_decoder_reads_f32_mono_16k(decoder);
  decoder->destroy(decoder);
  assert_int_equal(remove(path), 0);
}

static void test_invalid_arguments(void **state) {
  cpkt_audio_decoder *decoder;
  cpkt_audio_reader reader;
  float frame;
  size_t frames_read;

  (void)state;
  decoder = (cpkt_audio_decoder *)1;
  assert_int_equal(cpkt_audio_decoder_open_file(NULL, "x.wav", NULL), CPKT_AUDIO_ERR_ARG);
  assert_int_equal(cpkt_audio_decoder_open_file(&decoder, NULL, NULL), CPKT_AUDIO_ERR_ARG);
  assert_null(decoder);

  memset(&reader, 0, sizeof(reader));
  decoder = (cpkt_audio_decoder *)1;
  assert_int_equal(cpkt_audio_decoder_open_reader(&decoder, &reader, NULL), CPKT_AUDIO_ERR_ARG);
  assert_null(decoder);

  frames_read = 1;
  assert_int_equal(cpkt_audio_decoder_open_reader(NULL, &reader, NULL), CPKT_AUDIO_ERR_ARG);
  assert_int_equal(cpkt_audio_result_string(CPKT_AUDIO_OK)[0], 'o');
  assert_int_equal(cpkt_audio_result_string((cpkt_audio_result)999)[0], 'u');
  frame = 0.0f;
  (void)frame;
  (void)frames_read;
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_decoder_reads_from_callback_reader),
      cmocka_unit_test(test_decoder_reads_from_file),
      cmocka_unit_test(test_invalid_arguments),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
