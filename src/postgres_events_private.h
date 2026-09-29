#ifndef CPKT_POSTGRES_EVENTS_PRIVATE_H
#define CPKT_POSTGRES_EVENTS_PRIVATE_H

#include <libpq-fe.h>
#include <stddef.h>

/* Private allocation boundary used by native-peer fault regressions. */
void *cpkt_postgres_event_allocate(size_t size);

/* Prepare result state without firing RESULTCREATE for application-created
 * results. These helpers are private to the PostgreSQL facade. */
int cpkt_postgres_event_prepare_result(PGconn *connection, PGresult *result);
/* A borrowed notice has native events but no initial RESULTCREATE. Hold a
 * source role during its callback; release it afterward unless an explicit
 * RESULTCREATE initialized native destruction for that result. */
int cpkt_postgres_event_prepare_notice(PGconn *connection,
                                       const PGresult *result);
void cpkt_postgres_event_release_notice(const PGresult *result);
int cpkt_postgres_event_prepare_copy(const PGresult *source, PGresult *dest,
                                     int flags);
void cpkt_postgres_event_result_release(const PGresult *result);

#endif
