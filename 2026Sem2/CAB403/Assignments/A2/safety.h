#ifndef SAFETY_H
#define SAFETY_H

#include "shared.h"

#define SHM_NAME_PREFIX "/car"

int is_valid_flr(const char *flr);
int is_valid_status(const char *status);
void check_safety_conditions(car_shared_mem *shared_mem);

#endif