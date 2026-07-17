#ifndef PERFT_H
#define PERFT_H

#include "defs.h"
#include "position.h"

uint64_t run_perft(Position *pos, int depth);
int run_perft_tests(void);

#endif // PERFT_H
