if(NOT DEFINED CPKT_BINARY)
  message(FATAL_ERROR "CPKT_BINARY is required")
endif()
if(NOT EXISTS "${CPKT_BINARY}")
  message(FATAL_ERROR "binary does not exist: ${CPKT_BINARY}")
endif()

file(STRINGS "${CPKT_BINARY}" _cpkt_matches
  REGEX "libasound|asound\\.so|snd_pcm_|libpulse|libjack|CoreAudio\\.framework|AudioUnit\\.framework")

if(_cpkt_matches)
  string(REPLACE ";" "\n  " _cpkt_match_text "${_cpkt_matches}")
  message(FATAL_ERROR
    "binary contains forbidden audio device backend strings:\n  ${_cpkt_match_text}")
endif()
