#include <cpkt/audio.h>

int main(void) {
  cpkt_audio_decoder *decoder;

  if (cpkt_audio_format_can_decode(CPKT_AUDIO_FORMAT_MP3) == 0) {
    return 1;
  }
  if (cpkt_audio_format_can_encode(CPKT_AUDIO_FORMAT_WAV) == 0) {
    return 2;
  }
  decoder = (cpkt_audio_decoder *)1;
  if (cpkt_audio_decoder_open_url(&decoder, "", 0) != CPKT_AUDIO_ERR_ARG) {
    return 3;
  }
  if (decoder != 0) {
    return 4;
  }
  return 0;
}
