#ifndef CPKT_SUS_H
#define CPKT_SUS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cpkt_sus cpkt_sus;

typedef enum cpkt_sus_result {
  CPKT_SUS_OK = 0,
  CPKT_SUS_ERR_ARG = 1,
  CPKT_SUS_ERR_ALLOC = 2,
  CPKT_SUS_ERR_MODEL = 3,
  CPKT_SUS_ERR_UPSTREAM = 4
} cpkt_sus_result;

typedef struct cpkt_sus_config {
  const char *model_path;
  int cpu_only;
} cpkt_sus_config;

typedef struct cpkt_sus_info {
  const char *backend_version;
  const char *backend_system_info;
  int cpu_only;
} cpkt_sus_info;

struct cpkt_sus {
  void *impl;
  cpkt_sus_result (*info)(const cpkt_sus *self, cpkt_sus_info *info);
  void (*destroy)(cpkt_sus *self);
};

cpkt_sus_result cpkt_sus_open_model(
    cpkt_sus **out,
    const cpkt_sus_config *config);

const char *cpkt_sus_backend_version(void);
const char *cpkt_sus_backend_system_info(void);
const char *cpkt_sus_facade_version(void);
const char *cpkt_sus_result_string(cpkt_sus_result result);

#ifdef __cplusplus
}
#endif

#endif
