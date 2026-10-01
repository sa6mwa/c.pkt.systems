#define _POSIX_C_SOURCE 200809L
#include <open62541/plugin/log_stdout.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define THREADS 4
#define RECORDS 32
#define CHECK(e)                                                               \
  do {                                                                         \
    if (!(e)) {                                                                \
      fprintf(stderr, "stdout contention line %d: %s\n", __LINE__, #e);        \
      abort();                                                                 \
    }                                                                          \
  } while (0)

#ifdef CPKT_TEST_STDOUT_LOCK_WRAP
static pthread_key_t worker_key;
static pthread_mutex_t gate = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ready = PTHREAD_COND_INITIALIZER;
static int waiting;

/* Observe the actual logger's blocking boundary. The old logger owns its
 * spinlock when it reaches printf; the fixed logger reaches flockfile first.
 * Neither wrapper changes the write or lock operation. */
static void announce(void) {
  if (!pthread_getspecific(worker_key))
    return;
  CHECK(!pthread_mutex_lock(&gate));
  waiting = 1;
  CHECK(!pthread_cond_signal(&ready));
  CHECK(!pthread_mutex_unlock(&gate));
}
void __real_flockfile(FILE *file);
void __wrap_flockfile(FILE *file) {
  announce();
  __real_flockfile(file);
}
int __wrap_printf(const char *format, ...) {
  int result;
  va_list args;
  announce();
  va_start(args, format);
  result = vprintf(format, args);
  va_end(args);
  return result;
}
static void *blocked_logger(void *unused) {
  (void)unused;
  CHECK(!pthread_setspecific(worker_key, &waiting));
  UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_CLIENT, "cpkt blocked worker");
  CHECK(!pthread_setspecific(worker_key, NULL));
  return NULL;
}
#endif

static void *writer(void *context) {
  int id = *(const int *)context;
  for (int i = 0; i < RECORDS; ++i)
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_CLIENT,
                "cpkt contention thread=%d record=%d", id, i);
  return NULL;
}

int main(int argc, char **argv) {
  pthread_t threads[THREADS];
  int ids[THREADS], seen[THREADS][RECORDS] = {{0}};
  int worker = 0, owner = 0, warning = 0, records = 0;
  char line[1024];
  UA_Logger filtered = UA_Log_Stdout_withLevel(UA_LOGLEVEL_WARNING);
  CHECK(argc == 2);
#ifdef CPKT_TEST_STDOUT_LOCK_WRAP
  CHECK(!pthread_key_create(&worker_key, NULL));
#endif
  CHECK(freopen(argv[1], "w+", stdout) != NULL);
  flockfile(stdout);
#ifdef CPKT_TEST_STDOUT_LOCK_WRAP
  CHECK(!pthread_create(&threads[0], NULL, blocked_logger, NULL));
  CHECK(!pthread_mutex_lock(&gate));
  while (!waiting)
    CHECK(!pthread_cond_wait(&ready, &gate));
  CHECK(!pthread_mutex_unlock(&gate));
#endif
  /* FILE locks are recursive: a caller holding stdout must be able to log
   * while another logger waits for that stream. A second global spinlock
   * reverses the lock order and deadlocks this exact interleaving. */
  UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_CLIENT, "cpkt stdout owner");
  UA_LOG_INFO(&filtered, UA_LOGCATEGORY_CLIENT, "cpkt filtered info");
  UA_LOG_WARNING(&filtered, UA_LOGCATEGORY_CLIENT, "cpkt retained warning");
  funlockfile(stdout);
#ifdef CPKT_TEST_STDOUT_LOCK_WRAP
  CHECK(!pthread_join(threads[0], NULL));
#endif
  for (int i = 0; i < THREADS; ++i) {
    ids[i] = i;
    CHECK(!pthread_create(&threads[i], NULL, writer, &ids[i]));
  }
  for (int i = 0; i < THREADS; ++i)
    CHECK(!pthread_join(threads[i], NULL));
  CHECK(!fflush(stdout));
  CHECK(!fseek(stdout, 0, SEEK_SET));
  while (fgets(line, sizeof(line), stdout)) {
    char *record = strstr(line, "cpkt contention thread=");
    CHECK(line[0] == '[' && strchr(line, '\n') != NULL);
    CHECK(strstr(line, "cpkt filtered info") == NULL);
    if (record) {
      int id, index, consumed = 0;
      CHECK(sscanf(record, "cpkt contention thread=%d record=%d%n", &id, &index,
                   &consumed) == 2);
      CHECK(record[consumed] == '\n' && id >= 0 && id < THREADS && index >= 0 &&
            index < RECORDS && !seen[id][index]);
      seen[id][index] = 1;
      ++records;
    } else if (strstr(line, "cpkt blocked worker\n"))
      ++worker;
    else if (strstr(line, "cpkt stdout owner\n"))
      ++owner;
    else if (strstr(line, "cpkt retained warning\n"))
      ++warning;
    else
      CHECK(0);
  }
  CHECK(!ferror(stdout) && owner == 1 && warning == 1 &&
        records == THREADS * RECORDS);
#ifdef CPKT_TEST_STDOUT_LOCK_WRAP
  CHECK(worker == 1);
  CHECK(!pthread_key_delete(worker_key));
#else
  CHECK(worker == 0);
#endif
  CHECK(!fclose(stdout));
  fprintf(stderr, "stdout locking, filtering and all %d records passed\n",
          records);
  return 0;
}
