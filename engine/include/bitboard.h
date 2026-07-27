#ifndef BITBOARD_H
#define BITBOARD_H

#include "defs.h"

// Macro/Inline function definitions for bit manipulations
#define get_bit(bb, square) (((bb) >> (square)) & 1ULL)
#define set_bit(bb, square) ((bb) |= (1ULL << (square)))
#define clear_bit(bb, square) ((bb) &= ~(1ULL << (square)))

// Cross-platform compiler built-ins for trailing zero count (LSB) and population count
#if defined(__GNUC__) || defined(__clang__)
    static inline int count_bits(uint64_t bb) {
        return __builtin_popcountll(bb);
    }
    static inline int get_lsb(uint64_t bb) {
        if (bb == 0) return NO_SQ;
        return __builtin_ctzll(bb);
    }
#elif defined(_MSC_VER)
    #include <intrin.h>
    static inline int count_bits(uint64_t bb) {
        return (int)__popcnt64(bb);
    }
    static inline int get_lsb(uint64_t bb) {
        if (bb == 0) return NO_SQ;
        unsigned long index;
        _BitScanForward64(&index, bb);
        return (int)index;
    }
#else
    // Software fallback
    static inline int count_bits(uint64_t bb) {
        int count = 0;
        while (bb) {
            count++;
            bb &= bb - 1; // clear the least significant bit
        }
        return count;
    }
    static inline int get_lsb(uint64_t bb) {
        if (bb == 0) return 64;
        int index = 0;
        if ((bb & 0xFFFFFFFFULL) == 0) { bb >>= 32; index += 32; }
        if ((bb & 0xFFFFULL) == 0) { bb >>= 16; index += 16; }
        if ((bb & 0xFFULL) == 0) { bb >>= 8; index += 8; }
        if ((bb & 0xFULL) == 0) { bb >>= 4; index += 4; }
        if ((bb & 0x3ULL) == 0) { bb >>= 2; index += 2; }
        if ((bb & 0x1ULL) == 0) { index += 1; }
        return index;
    }
#endif

// Pop least significant bit and return its index
static inline int pop_lsb(uint64_t *bb) {
    int lsb = get_lsb(*bb);
    *bb &= *bb - 1;
    return lsb;
}

// Global precomputed attack tables
extern uint64_t pawn_attacks[2][64];
extern uint64_t knight_attacks[64];
extern uint64_t king_attacks[64];

// Board file & rank masks
extern uint64_t file_masks[8];
extern uint64_t rank_masks[8];

// Clear file masks (for pawn/knight jumps and moves)
extern const uint64_t not_a_file;
extern const uint64_t not_h_file;
extern const uint64_t not_ab_file;
extern const uint64_t not_gh_file;

// Initialization
void init_bitboards(void);

// Attack getters
uint64_t get_pawn_attacks(int color, int square);
uint64_t get_knight_attacks(int square);
uint64_t get_king_attacks(int square);
uint64_t get_bishop_attacks(int square, uint64_t occupied);
uint64_t get_rook_attacks(int square, uint64_t occupied);
uint64_t get_queen_attacks(int square, uint64_t occupied);

// Print utilities
void print_bitboard(uint64_t bb);

// Reference on-the-fly sliding attacks (used directly if USE_MAGIC_BITBOARDS is 0, or during Magic verification)
uint64_t bishop_attacks_on_the_fly(int square, uint64_t occupied);
uint64_t rook_attacks_on_the_fly(int square, uint64_t occupied);

#endif // BITBOARD_H
