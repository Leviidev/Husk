#ifndef HUSK_TL_LOCATION_H
#define HUSK_TL_LOCATION_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum { HUSK_LOCATION_PENDING, HUSK_LOCATION_AUTHORIZED, HUSK_LOCATION_DENIED, HUSK_LOCATION_DISABLED, HUSK_LOCATION_UNAVAILABLE };
#define HUSK_LOCATION_SAMPLE_SIZE 12
/* No coordinates are manufactured. Reads return 0 until Core Location supplies an authorized fix.
 * Sample: latitude, longitude, altitude, horizontal/vertical accuracy, speed, course, Unix milliseconds,
 * monotonic nanoseconds, speed accuracy, course accuracy, simulated-source flag. Negative accuracies/speed/course mean absent. */
int husk_location_status(void);
void husk_location_enable(bool enabled);
uint64_t husk_location_read(double *sample, size_t count);
void husk_location_reset(void);
#endif
