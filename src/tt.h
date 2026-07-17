#ifndef TT_H
#define TT_H

#include "defs.h"

#define TT_EXACT 0
#define TT_ALPHA 1
#define TT_BETA  2

typedef struct {
    uint64_t hash_key;
    Move best_move;
    int16_t score;
    int8_t depth;
    uint8_t flags;
    uint8_t age;
} TTEntry;

extern TTEntry *tt_table;
extern int tt_size_entries;
extern uint8_t tt_current_age;

void init_tt(int size_mb);
void free_tt(void);
void clear_tt(void);
void increment_tt_age(void);

// Read from TT
bool read_tt(uint64_t hash_key, int depth, int alpha, int beta, int *score, Move *best_move);

// Write to TT
void write_tt(uint64_t hash_key, Move best_move, int score, int depth, uint8_t flags);

#endif // TT_H
