#ifndef SIMIO_INC
#define SIMIO_INC

#include "simdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IO_DATA_UNUSED 0xff

extern in_func_t *const port_in[256];
extern out_func_t *const port_out[256];

void init_io(void);
void exit_io(void);
void reset_io(void);

#ifdef __cplusplus
}
#endif

#endif
