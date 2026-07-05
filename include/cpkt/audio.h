#ifndef CPKT_AUDIO_H
#define CPKT_AUDIO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Handle for a single audio decoder instance. */
#ifndef CPKT_AUDIO_DECODER_TYPEDEF
#define CPKT_AUDIO_DECODER_TYPEDEF
typedef struct cpkt_audio_decoder cpkt_audio_decoder;
#endif
/** Handle for a single audio encoder instance. */
typedef struct cpkt_audio_encoder cpkt_audio_encoder;
/** Handle for a default-device or selected-backend audio capture instance. */
typedef struct cpkt_audio_capture cpkt_audio_capture;
/** Handle for a default-device or selected-backend audio playback instance. */
typedef struct cpkt_audio_playback cpkt_audio_playback;
/** Handle for a float32 mono 16 kHz voice-operated segmenter. */
typedef struct cpkt_audio_vox cpkt_audio_vox;
/** Handle for a float32 mono 16 kHz push-to-talk segmenter. */
typedef struct cpkt_audio_ptt cpkt_audio_ptt;

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
  CPKT_AUDIO_AT_END = 6,
  /** The requested wait operation reached its timeout. */
  CPKT_AUDIO_TIMEOUT = 7
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

/** Public audio container/codec format identifiers. */
typedef enum cpkt_audio_format {
  /** Unknown or caller-selected default format. */
  CPKT_AUDIO_FORMAT_UNKNOWN = 0,
  /** RIFF/WAVE format. Decoding and encoding are supported in this build. */
  CPKT_AUDIO_FORMAT_WAV = 1,
  /** FLAC format. Decoding is supported in this build. */
  CPKT_AUDIO_FORMAT_FLAC = 2,
  /** MPEG audio format. Decoding is supported in this build. */
  CPKT_AUDIO_FORMAT_MP3 = 3
} cpkt_audio_format;

/** Optional device backend selection for capture and playback. */
typedef enum cpkt_audio_device_backend {
  /** Let the facade select the best available backend for the platform. */
  CPKT_AUDIO_DEVICE_BACKEND_AUTO = 0,
  /** Spawn a platform audio command and stream raw PCM over pipes. */
  CPKT_AUDIO_DEVICE_BACKEND_PROCESS = 1,
  /** Darwin Core Audio backend. */
  CPKT_AUDIO_DEVICE_BACKEND_COREAUDIO = 2
} cpkt_audio_device_backend;

/** VOX state transition identifiers delivered to cpkt_audio_vox_state_sink. */
typedef enum cpkt_audio_vox_state {
  /** VOX opened because input crossed threshold. */
  CPKT_AUDIO_VOX_TX_ON = 1,
  /** VOX released because hang-time expired or the stream was flushed. */
  CPKT_AUDIO_VOX_TX_OFF = 2,
  /** VOX emitted a hard budget/spool cut without releasing TX. */
  CPKT_AUDIO_VOX_HARD_CUT = 3
} cpkt_audio_vox_state;

/** Capture state identifiers delivered to cpkt_audio_capture_state_sink. */
typedef enum cpkt_audio_capture_state {
  /** A capture read returned current-device frames and the handle is listening.
   */
  CPKT_AUDIO_CAPTURE_READY = 1
} cpkt_audio_capture_state;

/** Stream description for an opened decoder. */
typedef struct cpkt_audio_stream_info {
  /** Detected or configured cpkt_audio_format for the source stream. */
  int source_format;
  /** Output sample rate in hertz. */
  unsigned long output_sample_rate;
  /** Output channel count. The current speech-oriented output path uses mono.
   */
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
typedef size_t (*cpkt_audio_read_fn)(void *user, void *buffer,
                                     size_t bytes_to_read);

/**
 * Repositions a caller-owned input source.
 *
 * Return zero on success and non-zero on failure. origin is one of
 * cpkt_audio_seek_origin.
 */
typedef int (*cpkt_audio_seek_fn)(void *user, long offset, int origin);

/**
 * Writes up to bytes_to_write bytes to a caller-owned output sink.
 *
 * Return the number of bytes copied from buffer. Returning fewer bytes than
 * requested reports an I/O error to the encoder.
 */
typedef size_t (*cpkt_audio_write_fn)(void *user, const void *buffer,
                                      size_t bytes_to_write);

/** Capture state event emitted by a default-device capture handle. */
typedef struct cpkt_audio_capture_state_event {
  /** cpkt_audio_capture_state value. */
  int state;
  /** Number of mono 16 kHz frames made available by this read. */
  size_t frame_count;
} cpkt_audio_capture_state_event;

/**
 * Receives capture lifecycle events.
 *
 * Return zero to continue. Returning non-zero fails the read that emitted the
 * event with CPKT_AUDIO_ERR_IO.
 */
typedef int (*cpkt_audio_capture_state_sink)(
    const cpkt_audio_capture_state_event *event, void *user);

/** Callback reader used to decode from caller-owned storage or transport. */
typedef struct cpkt_audio_reader {
  /** Caller-owned value passed to read and seek callbacks. */
  void *user;
  /** Required byte reader callback. */
  cpkt_audio_read_fn read;
  /**
   * Optional seek callback. Sources may leave this NULL, but formats whose
   * backend decoder requires seeking can fail to open with CPKT_AUDIO_ERR_IO.
   */
  cpkt_audio_seek_fn seek;
} cpkt_audio_reader;

/** Callback writer used to encode into caller-owned storage or transport. */
typedef struct cpkt_audio_writer {
  /** Caller-owned value passed to write and seek callbacks. */
  void *user;
  /** Required byte writer callback. */
  cpkt_audio_write_fn write;
  /** Required seek callback for formats that patch headers during close. */
  cpkt_audio_seek_fn seek;
} cpkt_audio_writer;

/** Decoder construction options. */
typedef struct cpkt_audio_decoder_config {
  /** Optional cpkt_audio_encoding value used as an input format hint. */
  int encoding;
} cpkt_audio_decoder_config;

/** Encoder construction options. Zero initializes to WAV, mono, 16000 Hz. */
typedef struct cpkt_audio_encoder_config {
  /** cpkt_audio_format output format. Zero selects CPKT_AUDIO_FORMAT_WAV. */
  int format;
  /** Output sample rate in hertz. Zero selects 16000. */
  unsigned long sample_rate;
  /** Output channel count. Zero selects mono. */
  unsigned long channels;
} cpkt_audio_encoder_config;

/** Capture construction options. Zero initializes to default mic, mono 16 kHz.
 */
typedef struct cpkt_audio_capture_config {
  /** cpkt_audio_device_backend value. Zero selects automatic backend choice. */
  int backend;
  /** Internal ring buffer duration. Zero selects 2000 ms. */
  unsigned long buffer_ms;
  /** Requested device callback period. Zero selects 20 ms. */
  unsigned long period_ms;
  /** Optional capture event sink. NULL disables capture state callbacks. */
  cpkt_audio_capture_state_sink state_sink;
  /** Caller-owned value passed to state_sink. */
  void *state_user;
} cpkt_audio_capture_config;

/** Playback construction options. Zero initializes to default output, mono 16
 * kHz. */
typedef struct cpkt_audio_playback_config {
  /** cpkt_audio_device_backend value. Zero selects automatic backend choice. */
  int backend;
  /** Internal ring buffer duration. Zero selects 2000 ms. */
  unsigned long buffer_ms;
  /** Requested device callback period. Zero selects 20 ms. */
  unsigned long period_ms;
} cpkt_audio_playback_config;

#ifndef CPKT_AUDIO_VOX_SEGMENT_TYPEDEF
#define CPKT_AUDIO_VOX_SEGMENT_TYPEDEF
/** Pullable VOX segment delivered when speech releases or a budget is reached.
 */
typedef struct cpkt_audio_vox_segment cpkt_audio_vox_segment;
#endif

/** VOX state event delivered when TX/RX state changes or a hard cut occurs. */
typedef struct cpkt_audio_vox_state_event {
  /** cpkt_audio_vox_state value. */
  int state;
  /** Segment index active at the transition. */
  unsigned long segment_index;
  /** Configured threshold used for this VOX instance. */
  float threshold;
} cpkt_audio_vox_state_event;

/** Pullable VOX speech segment receiver shell. */
struct cpkt_audio_vox_segment {
  /** Private implementation pointer. Callers must not inspect or modify it. */
  void *impl;
  /** Number of mono 16000 Hz PCM frames in frames. */
  size_t frame_count;
  /** Segment start time in 10 ms units on the input PCM timeline. */
  long t0;
  /** Segment end time in 10 ms units on the input PCM timeline. */
  long t1;
  /** Zero-based segment number emitted by this VOX instance. */
  unsigned long segment_index;
  /** Non-zero when the segment was closed by max_segment_ms, not silence. */
  int hard_cut;
  /** Non-zero when this segment was emitted during flush/end-of-stream. */
  int is_final;
  /**
   * Reads segment PCM as float32 mono 16000 Hz frames.
   *
   * The segment is valid only during the VOX callback. frames_read is set
   * before return when arguments are valid. CPKT_AUDIO_AT_END means the
   * segment source has been fully consumed.
   */
  cpkt_audio_result (*read_f32_mono_16k)(cpkt_audio_vox_segment *self,
                                         float *frames, size_t frame_capacity,
                                         size_t *frames_read);
};

/**
 * Receives a bounded VOX speech segment.
 *
 * Return zero to continue. Returning non-zero makes the active push or flush
 * call return CPKT_AUDIO_ERR_IO.
 */
typedef int (*cpkt_audio_vox_segment_sink)(cpkt_audio_vox_segment *segment,
                                           void *user);

/**
 * Receives VOX TX/RX state events.
 *
 * Return zero to continue. Returning non-zero makes the active push or flush
 * call return CPKT_AUDIO_ERR_IO.
 */
typedef int (*cpkt_audio_vox_state_sink)(
    const cpkt_audio_vox_state_event *event, void *user);

/** VOX construction options. Zero initializes to speech-friendly defaults. */
typedef struct cpkt_audio_vox_config {
  /** RMS power threshold that opens or keeps VOX active. Zero selects 0.01. */
  float threshold;
  /** Silence duration that releases VOX. Zero selects 1500 ms. */
  unsigned long release_silence_ms;
  /**
   * Audio retained before threshold opens VOX. Zero selects 10 ms. This keeps
   * the leading attack of speech in the emitted segment.
   */
  unsigned long prebuffer_ms;
  /** Maximum segment duration before a budget split. Zero disables the time
   * cap. */
  unsigned long max_segment_ms;
  /** Minimum segment duration to emit. Zero selects 100 ms. */
  unsigned long min_segment_ms;
  /**
   * Bytes kept in memory before spilling an open segment to an anonymous
   * temporary file. Zero selects 1 MiB.
   */
  unsigned long memory_spool_bytes;
  /**
   * Maximum bytes for one open VOX segment before forced hard cut. Zero
   * selects 1 GiB. The cap applies to RAM plus disk-backed spool bytes.
   */
  unsigned long max_spool_bytes;
  /** Required sink for emitted speech segments. */
  cpkt_audio_vox_segment_sink segment_sink;
  /** User value passed to segment_sink. */
  void *segment_user;
  /** Optional sink for TX/RX state transitions and hard cuts. */
  cpkt_audio_vox_state_sink state_sink;
  /** User value passed to state_sink. */
  void *state_user;
} cpkt_audio_vox_config;

/** PTT construction options. Zero initializes to capture-friendly defaults. */
typedef struct cpkt_audio_ptt_config {
  /** Maximum TX segment duration before a budget split. Zero disables the time
   * cap. */
  unsigned long max_segment_ms;
  /** Minimum segment duration to emit. Zero selects 100 ms. */
  unsigned long min_segment_ms;
  /**
   * Bytes kept in memory before spilling an open segment to an anonymous
   * temporary file. Zero selects 1 MiB.
   */
  unsigned long memory_spool_bytes;
  /**
   * Maximum bytes for one open PTT segment before forced hard cut. Zero selects
   * 1 GiB. The cap applies to RAM plus disk-backed spool bytes.
   */
  unsigned long max_spool_bytes;
  /** Required sink for emitted PTT speech segments. */
  cpkt_audio_vox_segment_sink segment_sink;
  /** User value passed to segment_sink. */
  void *segment_user;
  /** Optional sink for TX/RX state transitions and hard cuts. */
  cpkt_audio_vox_state_sink state_sink;
  /** User value passed to state_sink. */
  void *state_user;
} cpkt_audio_ptt_config;

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
  cpkt_audio_result (*read_f32_mono_16k)(cpkt_audio_decoder *self,
                                         float *frames, size_t frame_capacity,
                                         size_t *frames_read);
  /** Fills info with the decoder output format and known frame count. */
  cpkt_audio_result (*info)(const cpkt_audio_decoder *self,
                            cpkt_audio_stream_info *info);
  /** Releases the decoder and all resources owned by the handle. */
  void (*destroy)(cpkt_audio_decoder *self);
};

/** Receiver shell for encoder operations. */
struct cpkt_audio_encoder {
  /** Private implementation pointer. Callers must not inspect or modify it. */
  void *impl;
  /**
   * Writes 32-bit float PCM frames using the configured channel count.
   *
   * frames_written is set before return when arguments are valid. The input
   * frame buffer must contain frame_count * channels samples.
   */
  cpkt_audio_result (*write_f32)(cpkt_audio_encoder *self, const float *frames,
                                 size_t frame_count, size_t *frames_written);
  /** Finalizes the encoded stream. Safe to call once; repeated calls are OK. */
  cpkt_audio_result (*close)(cpkt_audio_encoder *self);
  /** Finalizes and releases the encoder and all resources owned by the handle.
   */
  void (*destroy)(cpkt_audio_encoder *self);
};

/** Receiver shell for default-device audio capture. */
struct cpkt_audio_capture {
  /** Private implementation pointer. Callers must not inspect or modify it. */
  void *impl;
  /** Starts capture from the configured input device. */
  cpkt_audio_result (*start)(cpkt_audio_capture *self);
  /**
   * Reads captured audio as 32-bit float, mono, 16000 Hz PCM frames.
   *
   * frames_read is set before return when arguments are valid. If no frames are
   * currently buffered, the call returns CPKT_AUDIO_OK with frames_read set to
   * zero. The handle uses a bounded ring buffer and may drop oldest frames when
   * a live producer outruns the consumer.
   */
  cpkt_audio_result (*read_f32_mono_16k)(cpkt_audio_capture *self,
                                         float *frames, size_t frame_capacity,
                                         size_t *frames_read);
  /**
   * Blocks until capture has readable current-device audio.
   *
   * timeout_ms is a maximum wait in milliseconds. Zero waits indefinitely. The
   * call does not consume frames; call read_f32_mono_16k afterwards to receive
   * audio. CPKT_AUDIO_TIMEOUT means no frames became ready before the timeout.
   */
  cpkt_audio_result (*wait_ready)(cpkt_audio_capture *self,
                                  unsigned long timeout_ms);
  /** Stops capture. Repeated calls are OK. */
  cpkt_audio_result (*stop)(cpkt_audio_capture *self);
  /** Releases the capture handle and all resources owned by it. */
  void (*destroy)(cpkt_audio_capture *self);
};

/** Receiver shell for default-device audio playback. */
struct cpkt_audio_playback {
  /** Private implementation pointer. Callers must not inspect or modify it. */
  void *impl;
  /** Starts playback to the configured output device. */
  cpkt_audio_result (*start)(cpkt_audio_playback *self);
  /**
   * Queues 32-bit float, mono, 16000 Hz PCM frames for playback.
   *
   * frames_written is set before return when arguments are valid. The call
   * waits for bounded ring-buffer space instead of materializing the full
   * stream.
   */
  cpkt_audio_result (*write_f32_mono_16k)(cpkt_audio_playback *self,
                                          const float *frames,
                                          size_t frame_count,
                                          size_t *frames_written);
  /** Waits until queued playback frames have been consumed. */
  cpkt_audio_result (*drain)(cpkt_audio_playback *self);
  /** Stops playback. Repeated calls are OK. */
  cpkt_audio_result (*stop)(cpkt_audio_playback *self);
  /** Releases the playback handle and all resources owned by it. */
  void (*destroy)(cpkt_audio_playback *self);
};

/** Receiver shell for float32 mono 16 kHz VOX segmenting. */
struct cpkt_audio_vox {
  /** Private implementation pointer. Callers must not inspect or modify it. */
  void *impl;
  /**
   * Pushes decoded mono 16 kHz PCM through VOX.
   *
   * The facade may synchronously invoke segment_sink zero or more times before
   * returning. Input frames are not retained after closed segments are emitted.
   */
  cpkt_audio_result (*push_f32_mono_16k)(cpkt_audio_vox *self,
                                         const float *frames,
                                         size_t frame_count);
  /**
   * Ends the stream and emits any open speech segment as a final segment.
   */
  cpkt_audio_result (*flush)(cpkt_audio_vox *self);
  /** Releases the VOX handle and its bounded in-memory segment buffer. */
  void (*destroy)(cpkt_audio_vox *self);
};

/** Receiver shell for float32 mono 16 kHz push-to-talk segmenting. */
struct cpkt_audio_ptt {
  /** Private implementation pointer. Callers must not inspect or modify it. */
  void *impl;
  /** Opens TX. Repeated calls while TX is open are OK. */
  cpkt_audio_result (*press)(cpkt_audio_ptt *self);
  /**
   * Pushes captured mono 16 kHz PCM into the current PTT segment.
   *
   * Frames pushed while TX is closed are ignored. The facade may synchronously
   * invoke segment_sink when max_segment_ms or max_spool_bytes is reached.
   */
  cpkt_audio_result (*push_f32_mono_16k)(cpkt_audio_ptt *self,
                                         const float *frames,
                                         size_t frame_count);
  /** Closes TX and emits the current segment when it meets min_segment_ms. */
  cpkt_audio_result (*release)(cpkt_audio_ptt *self);
  /** Ends the stream and emits any open PTT segment as final. */
  cpkt_audio_result (*flush)(cpkt_audio_ptt *self);
  /** Releases the PTT handle and its bounded segment buffer. */
  void (*destroy)(cpkt_audio_ptt *self);
};

/**
 * Opens an audio decoder from a filesystem path.
 *
 * On success, *out receives a decoder handle that must be destroyed with its
 * destroy receiver. On failure, *out is set to NULL when out is non-NULL.
 */
cpkt_audio_result
cpkt_audio_decoder_open_file(cpkt_audio_decoder **out, const char *path,
                             const cpkt_audio_decoder_config *config);

/**
 * Opens an audio decoder from a libcurl-supported URL.
 *
 * The URL is streamed through libcurl as the backend decoder pulls bytes; the
 * facade does not download the full response before decoding. Explicit config
 * encoding takes precedence. Otherwise response Content-Type is used when it
 * maps to a supported audio format, then early stream signatures/backend
 * detection are used. URL sources are not seekable, so formats that require
 * seeking can fail to open with CPKT_AUDIO_ERR_IO or CPKT_AUDIO_ERR_FORMAT. On
 * failure, *out is set to NULL when out is non-NULL.
 */
cpkt_audio_result
cpkt_audio_decoder_open_url(cpkt_audio_decoder **out, const char *url,
                            const cpkt_audio_decoder_config *config);

/**
 * Opens an audio decoder from callback-based input.
 *
 * The reader structure is copied during construction. The caller-owned user
 * value and backing source must remain valid until the decoder is destroyed.
 */
cpkt_audio_result
cpkt_audio_decoder_open_reader(cpkt_audio_decoder **out,
                               const cpkt_audio_reader *reader,
                               const cpkt_audio_decoder_config *config);

/**
 * Opens an audio encoder for a filesystem path.
 *
 * WAV is the initial supported encoding format. On success, *out receives an
 * encoder handle that must be closed or destroyed.
 */
cpkt_audio_result
cpkt_audio_encoder_open_file(cpkt_audio_encoder **out, const char *path,
                             const cpkt_audio_encoder_config *config);

/**
 * Opens an audio encoder for callback-based output.
 *
 * The writer structure is copied during construction. The caller-owned user
 * value and backing sink must remain valid until the encoder is destroyed.
 */
cpkt_audio_result
cpkt_audio_encoder_open_writer(cpkt_audio_encoder **out,
                               const cpkt_audio_writer *writer,
                               const cpkt_audio_encoder_config *config);

/**
 * Opens capture from the platform default input device.
 *
 * The handle captures normalized float32 mono 16 kHz PCM. Device backends are
 * loaded by the facade at runtime; missing host audio libraries cause open or
 * start to fail without adding link requirements for ordinary decoder users.
 */
cpkt_audio_result
cpkt_audio_capture_open_default(cpkt_audio_capture **out,
                                const cpkt_audio_capture_config *config);

/**
 * Opens playback to the platform default output device.
 *
 * The handle plays normalized float32 mono 16 kHz PCM. Device backends are
 * loaded by the facade at runtime; missing host audio libraries cause open or
 * start to fail without adding link requirements for ordinary decoder users.
 */
cpkt_audio_result
cpkt_audio_playback_open_default(cpkt_audio_playback **out,
                                 const cpkt_audio_playback_config *config);

/**
 * Opens a float32 mono 16 kHz voice-operated segmenter.
 *
 * The VOX handle never materializes a whole stream. It retains at most the
 * configured memory_spool_bytes in RAM before spilling to disk, and emits a
 * hard-cut segment when max_segment_ms or max_spool_bytes is reached.
 */
cpkt_audio_result cpkt_audio_vox_open(cpkt_audio_vox **out,
                                      const cpkt_audio_vox_config *config);

/**
 * Opens a float32 mono 16 kHz push-to-talk segmenter.
 *
 * PTT uses caller-driven press/release control and the same pullable segment
 * sink shape as VOX. It never materializes a whole stream; open TX segments
 * spill to disk and hard-cut at the configured storage or duration caps.
 */
cpkt_audio_result cpkt_audio_ptt_open(cpkt_audio_ptt **out,
                                      const cpkt_audio_ptt_config *config);

/**
 * Returns non-zero when format is supported for decoding by this facade build.
 */
int cpkt_audio_format_can_decode(int format);

/**
 * Returns non-zero when format is supported for encoding by this facade build.
 */
int cpkt_audio_format_can_encode(int format);

/** Returns a stable human-readable string for an audio result code. */
const char *cpkt_audio_result_string(cpkt_audio_result result);

#ifdef __cplusplus
}
#endif

#endif
