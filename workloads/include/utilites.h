#ifndef UTILITIES_H
#define UTILITIES_H
#include <stdint.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdlib.h>

uint64_t now_usec (void);
uint64_t xorshift64star(uint64_t *state);
#endif