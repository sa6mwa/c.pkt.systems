#include "postgres_events_private.h"

#include <stdlib.h>

/** Allocate zeroed event bookkeeping; separated for fault injection. */
void *cpkt_postgres_event_allocate(size_t size) { return calloc(1, size); }
