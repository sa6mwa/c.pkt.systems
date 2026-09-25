#include <iodbcext.h>
#include <iodbcinst.h>
#include <iodbcunix.h>
#include <isql.h>
#include <isqlext.h>
#include <isqltypes.h>
#include <odbcinst.h>
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>
#include <sqlucode.h>

typedef char
    cpkt_iodbc_bigint_must_be_eight_bytes[sizeof(SQLBIGINT) == 8 ? 1 : -1];
typedef char
    cpkt_iodbc_ubigint_must_be_eight_bytes[sizeof(SQLUBIGINT) == 8 ? 1 : -1];

int main(void) {
  SQLHENV environment;
  SQLHDBC connection;
  UWORD config_mode;
  SQLRETURN status;

  environment = SQL_NULL_HENV;
  connection = SQL_NULL_HDBC;
  config_mode = ODBC_BOTH_DSN;
  if (!SQLGetConfigMode(&config_mode))
    return 1;
  if (config_mode != ODBC_BOTH_DSN && config_mode != ODBC_USER_DSN &&
      config_mode != ODBC_SYSTEM_DSN)
    return 2;
  status = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &environment);
  if (!SQL_SUCCEEDED(status) || environment == SQL_NULL_HENV)
    return 3;
  status = SQLSetEnvAttr(environment, SQL_ATTR_ODBC_VERSION,
                         (SQLPOINTER)SQL_OV_ODBC3, 0);
  if (!SQL_SUCCEEDED(status))
    return 4;
  status = SQLAllocHandle(SQL_HANDLE_DBC, environment, &connection);
  if (!SQL_SUCCEEDED(status) || connection == SQL_NULL_HDBC)
    return 5;
  if (!SQL_SUCCEEDED(SQLFreeHandle(SQL_HANDLE_DBC, connection)))
    return 6;
  if (!SQL_SUCCEEDED(SQLFreeHandle(SQL_HANDLE_ENV, environment)))
    return 7;
  return 0;
}
