#include <cpkt/postgres.h>

#include <stdio.h>
#include <string.h>

static int require_value(const cpkt_postgres_result *result,
                         const char *expected) {
  char *value;

  if (result == 0 ||
      cpkt_postgres_result_status_get(result) !=
          CPKT_POSTGRES_RESULT_TUPLES_OK ||
      cpkt_postgres_result_row_count(result) != 1 ||
      cpkt_postgres_result_field_count(result) != 1) {
    return 0;
  }
  value = cpkt_postgres_result_value(result, 0, 0);
  return value != 0 && strcmp(value, expected) == 0;
}

static int require_command(const cpkt_postgres_result *result) {
  return result != 0 && cpkt_postgres_result_status_get(result) ==
                            CPKT_POSTGRES_RESULT_COMMAND_OK;
}

static int run_integration(const char *server_name,
                           const char *connection_info) {
  const char *parameters[2];
  cpkt_postgres *pg;
  cpkt_postgres_result *result;
  int ok;

  pg = cpkt_postgres_new(connection_info);
  if (pg == 0 || pg->status(pg) != CPKT_POSTGRES_CONNECTION_OK) {
    fprintf(stderr, "%s: connection failed: %s\n", server_name,
            pg == 0 ? "allocation failed" : pg->error(pg));
    if (pg != 0) {
      pg->close(pg);
    }
    return 0;
  }

  ok = 1;
  result = pg->tx(pg, "SELECT 1");
  if (!require_value(result, "1")) {
    ok = 0;
  }
  cpkt_postgres_result_free(result);

  parameters[0] = "cpkt";
  parameters[1] = "-facade";
  result = pg->tx_params(pg, "SELECT $1::TEXT || $2::TEXT", 2, 0, parameters, 0,
                         0, 0);
  if (!require_value(result, "cpkt-facade")) {
    ok = 0;
  }
  cpkt_postgres_result_free(result);

  if (!pg->send(pg, "SELECT 9223372036854775807::INT8")) {
    ok = 0;
  } else {
    result = pg->receive(pg);
    if (!require_value(result, "9223372036854775807")) {
      ok = 0;
    }
    cpkt_postgres_result_free(result);
  }

  result = cpkt_postgres_prepare(pg->connection, "cpkt_receiver_statement",
                                 "SELECT $1::INT4 + 1", 1, 0);
  if (!require_command(result)) {
    ok = 0;
  }
  cpkt_postgres_result_free(result);
  parameters[0] = "41";
  result = cpkt_postgres_execute_prepared(
      pg->connection, "cpkt_receiver_statement", 1, parameters, 0, 0, 0);
  if (!require_value(result, "42")) {
    ok = 0;
  }
  cpkt_postgres_result_free(result);

  result = pg->tx(pg, "BEGIN");
  if (!require_command(result) ||
      cpkt_postgres_transaction_status_get(pg->connection) !=
          CPKT_POSTGRES_TRANSACTION_IN_TRANSACTION) {
    ok = 0;
  }
  cpkt_postgres_result_free(result);
  result = pg->tx(pg, "ROLLBACK");
  if (!require_command(result) ||
      cpkt_postgres_transaction_status_get(pg->connection) !=
          CPKT_POSTGRES_TRANSACTION_IDLE) {
    ok = 0;
  }
  cpkt_postgres_result_free(result);

  pg->close(pg);
  if (!ok) {
    fprintf(stderr, "%s: PostgreSQL-wire integration assertion failed\n",
            server_name);
  }
  return ok;
}

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: %s SERVER-NAME CONNECTION-INFO\n", argv[0]);
    return 2;
  }
  return run_integration(argv[1], argv[2]) ? 0 : 1;
}
