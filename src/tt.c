#include "tt.h"

TTEntry *tt_table = NULL;
int tt_size_entries = 0;
uint8_t tt_current_age = 0;

void init_tt(int size_mb) {
    if (tt_table != NULL) {
        free_tt();
    }

    uint64_t bytes;
#if defined(__arm__) || defined(PICO_BOARD)
    // On microcontrollers, ignore size_mb and allocate a safe 32KB table (2048 entries)
    bytes = 32 * 1024ULL;
#else
    // Default to 16MB if size is 0 or negative
    if (size_mb <= 0) {
        size_mb = 16;
    }
    bytes = (uint64_t)size_mb * 1024ULL * 1024ULL;
#endif

    tt_size_entries = (int)(bytes / sizeof(TTEntry));

    // Allocate memory
    tt_table = (TTEntry *)malloc(tt_size_entries * sizeof(TTEntry));
    if (tt_table == NULL) {
        tt_size_entries = 0;
        return;
    }

    clear_tt();
}


void free_tt(void) {
    if (tt_table != NULL) {
        free(tt_table);
        tt_table = NULL;
    }
    tt_size_entries = 0;
}

void clear_tt(void) {
    if (tt_table != NULL) {
        memset(tt_table, 0, tt_size_entries * sizeof(TTEntry));
    }
    tt_current_age = 0;
}

void increment_tt_age(void) {
    tt_current_age++;
}

bool read_tt(uint64_t hash_key, int depth, int alpha, int beta, int *score, Move *best_move) {
    if (tt_table == NULL || tt_size_entries == 0) {
        return false;
    }

    int idx = (int)(hash_key % (uint64_t)tt_size_entries);
    TTEntry *entry = &tt_table[idx];

    if (entry->hash_key == hash_key) {
        *best_move = entry->best_move;

        // Only use the score if the depth is sufficient
        if (entry->depth >= depth) {
            int tt_score = entry->score;

            if (entry->flags == TT_EXACT) {
                *score = tt_score;
                return true;
            }
            if (entry->flags == TT_ALPHA && tt_score <= alpha) {
                *score = alpha;
                return true;
            }
            if (entry->flags == TT_BETA && tt_score >= beta) {
                *score = beta;
                return true;
            }
        }
    } else {
        *best_move = 0;
    }

    return false;
}

void write_tt(uint64_t hash_key, Move best_move, int score, int depth, uint8_t flags) {
    if (tt_table == NULL || tt_size_entries == 0) {
        return;
    }

    int idx = (int)(hash_key % (uint64_t)tt_size_entries);
    TTEntry *entry = &tt_table[idx];

    // Replacement strategy: Replace if empty, if it's the same position,
    // if new depth >= old depth, or if old entry belongs to a previous search age.
    bool replace = (entry->hash_key == 0) ||
                   (entry->hash_key == hash_key) ||
                   (depth >= entry->depth) ||
                   (entry->age != tt_current_age);

    if (replace) {
        entry->hash_key = hash_key;
        // Keep the old best move if the new one is null (e.g. during fails-low)
        entry->best_move = (best_move != 0) ? best_move : entry->best_move;
        entry->score = (int16_t)score;
        entry->depth = (int8_t)depth;
        entry->flags = flags;
        entry->age = tt_current_age;
    }
}
