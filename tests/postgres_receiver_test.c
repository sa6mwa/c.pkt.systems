#include <cpkt/postgres.h>

static void notice_receiver(
    void *context, cpkt_postgres_connection *connection,
    const cpkt_postgres_result *result) {
  (void) context;
  (void) connection;
  (void) result;
}

static void notice_processor(
    void *context, cpkt_postgres_connection *connection, const char *message) {
  (void) context;
  (void) connection;
  (void) message;
}

static void thread_lock(int acquire) {
  (void) acquire;
}

static int ssl_key_password(
    char *buffer, int buffer_size, cpkt_postgres_connection *connection) {
  (void) buffer;
  (void) buffer_size;
  (void) connection;
  return 0;
}

static int auth_data(
    cpkt_postgres_auth_data_kind kind, cpkt_postgres_connection *connection,
    void *data, void *context) {
  (void) kind;
  (void) connection;
  (void) data;
  (void) context;
  return 0;
}

int main(void) {
  cpkt_postgres *pg;
  cpkt_postgres_notice_receiver old_receiver;
  cpkt_postgres_notice_processor old_processor;
  cpkt_postgres_auth_data_hook auth_callback;
  void *old_context;

  pg = cpkt_postgres_new("host=/tmp/cpkt-postgres-no-socket connect_timeout=1");
  if (pg == 0 || pg->tx == 0 || pg->send == 0 || pg->receive == 0 ||
      pg->reset == 0 || pg->close == 0) {
    return 1;
  }
  if (pg->status(pg) != CPKT_POSTGRES_CONNECTION_BAD) {
    pg->close(pg);
    return 2;
  }
  old_context = (void *) 1;
  cpkt_postgres_set_notice_receiver(pg->connection, notice_receiver, pg,
      &old_receiver, &old_context);
  if (old_receiver != 0 || old_context != 0) {
    pg->close(pg);
    return 3;
  }
  cpkt_postgres_set_notice_processor(pg->connection, notice_processor, pg,
      &old_processor, &old_context);
  if (old_processor != 0 || old_context != 0) {
    pg->close(pg);
    return 4;
  }
  if (cpkt_postgres_register_thread_lock(thread_lock) != 0) {
    pg->close(pg);
    return 5;
  }
  if (cpkt_postgres_register_thread_lock(0) != thread_lock) {
    pg->close(pg);
    return 6;
  }
  if (cpkt_postgres_set_ssl_key_password_hook(ssl_key_password) != 0 ||
      cpkt_postgres_get_ssl_key_password_hook() != ssl_key_password) {
    pg->close(pg);
    return 7;
  }
  if (cpkt_postgres_set_ssl_key_password_hook(0) != ssl_key_password ||
      cpkt_postgres_get_ssl_key_password_hook() != 0) {
    pg->close(pg);
    return 8;
  }
  cpkt_postgres_set_auth_data_hook(auth_data, pg);
  old_context = 0;
  auth_callback = cpkt_postgres_get_auth_data_hook(&old_context);
  if (auth_callback != auth_data || old_context != pg) {
    pg->close(pg);
    return 9;
  }
  cpkt_postgres_set_auth_data_hook(0, 0);
  pg->close(pg);
  return 0;
}
