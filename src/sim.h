#ifndef SIM_INC
#define SIM_INC

/* Minimal configuration for the Teensy IMSAI 8080 standalone firmware. */
#define DEF_CPU I8080
#define CPU_SPEED 2
#define UNDOC_INST

/* Keep modem declarations in imsai-sio2.h available for SIO2B ports. */
#define HAS_MODEM
#define HAS_HAL

#define IMSAISIM
#define MACHINE "imsai"

#endif
