#include "perft.h"
#include "movegen.h"
#include "move.h"
#include <time.h>

typedef struct {
    const char *name;
    const char *fen;
    uint64_t perftcnt[10];
    int depth_count;
} PerftTestCase;

static const PerftTestCase perft_cases[] = {
    // Reference: https://www.chessprogramming.org/Perft_Results
    {"end games", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -", {14, 191, 2812, 43238, 674624, 11030083, 178633661}, 7},
    {"strange bugs", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", {44, 1486, 62379, 2103487, 89941194}, 5},
    {"start pos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", {20, 400, 8902, 197281, 4865609, 119060324, 3195901860ULL}, 7},
    {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -", {48, 2039, 97862, 4085603, 193690690, 8031647685ULL}, 6},

    {"position-4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", {6, 264, 9467, 422333, 15833292, 706045033}, 6},
    {"position-4-mirror", "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1", {6, 264, 9467, 422333, 15833292, 706045033}, 6},
    {"position-6", "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", {46, 2079, 89890, 3894594, 164075551, 6923051137ULL}, 6},
    
    // Reference: https://gist.github.com/peterellisjones/8c46c28141c162d1d8a0f0badbc9cff9
    {"pej-1", "r6r/1b2k1bq/8/8/7B/8/8/R3K2R b QK - 3 2", {8}, 1},
    {"pej-2", "r1bqkbnr/pppppppp/n7/8/8/P7/1PPPPPPP/RNBQKBNR w QqKk - 2 2", {19}, 1},
    {"pej-3", "r3k2r/p1pp1pb1/bn2Qnp1/2qPN3/1p2P3/2N5/PPPBBPPP/R3K2R b QqKk - 3 2", {5}, 1},
    {"pej-4", "2kr3r/p1ppqpb1/bn2Qnp1/3PN3/1p2P3/2N5/PPPBBPPP/R3K2R b QK - 3 2", {44}, 1},
    {"pej-5", "rnb2k1r/pp1Pbppp/2p5/q7/2B5/8/PPPQNnPP/RNB1K2R w QK - 3 9", {39}, 1},
    {"pej-6", "2r5/3pk3/8/2P5/8/2K5/8/8 w - - 5 4", {9}, 1},
    {"pej-7", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", {44, 1486, 62379}, 3},
    {"pej-8", "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", {46, 2079, 89890}, 3},
    {"pej-9", "3k4/3p4/8/K1P4r/8/8/8/8 b - - 0 1", {18, 92, 1670, 10138, 185429, 1134888}, 6},
    {"pej-10", "8/8/4k3/8/2p5/8/B2P2K1/8 w - - 0 1", {13, 102, 1266, 10276, 135655, 1015133}, 6},
    {"pej-11", "8/8/1k6/2b5/2pP4/8/5K2/8 b - d3 0 1", {15, 126, 1928, 13931, 206379, 1440467}, 6},
    {"pej-12", "5k2/8/8/8/8/8/8/4K2R w K - 0 1", {15, 66, 1198, 6399, 120330, 661072}, 6},
    {"pej-13", "3k4/8/8/8/8/8/8/R3K3 w Q - 0 1", {16, 71, 1286, 7418, 141077, 803711}, 6},
    {"pej-14", "r3k2r/1b4bq/8/8/8/8/7B/R3K2R w KQkq - 0 1", {26, 1141, 27826, 1274206}, 4},
    {"pej-15", "r3k2r/8/3Q4/8/8/5q2/8/R3K2R b KQkq - 0 1", {44, 1494, 50509, 1720476}, 4},
    {"pej-16", "2K2r2/4P3/8/8/8/8/8/3k4 w - - 0 1", {11, 133, 1442, 19174, 266199, 3821001}, 6},
    {"pej-17", "8/8/1P2K3/8/2n5/1q6/8/5k2 b - - 0 1", {29, 165, 5160, 31961, 1004658}, 5},
    {"pej-18", "4k3/1P6/8/8/8/8/K7/8 w - - 0 1", {9, 40, 472, 2661, 38983, 217342}, 6},
    {"pej-19", "8/P1k5/K7/8/8/8/8/8 w - - 0 1", {6, 27, 273, 1329, 18135, 92683}, 6},
    {"pej-20", "K1k5/8/P7/8/8/8/8/8 w - - 0 1", {2, 6, 13, 63, 382, 2217}, 6},
    {"pej-21", "8/k1P5/8/1K6/8/8/8/8 w - - 0 1", {10, 25, 268, 926, 10857, 43261, 567584}, 7},
    {"pej-22", "8/8/2k5/5q2/5n2/8/5K2/8 b - - 0 1", {37, 183, 6559, 23527}, 4}
};

static const int perft_cases_count = sizeof(perft_cases) / sizeof(perft_cases[0]);

// Recursive perft runner
uint64_t run_perft(Position *pos, int depth) {
    if (depth == 0) {
        return 1ULL;
    }

    MoveList list;
    generate_moves(pos, &list);
    uint64_t nodes = 0ULL;

    for (int i = 0; i < list.count; i++) {
        if (!make_move(pos, list.moves[i])) {
            continue;
        }
        nodes += run_perft(pos, depth - 1);
        unmake_move(pos);
    }

    return nodes;
}

int run_perft_tests(void) {
    int errors = 0;
    int passed = 0;
    
    printf("Starting PERFT Verification Suite...\n");
    printf("========================================================================\n");

    for (int i = 0; i < perft_cases_count; i++) {
        const PerftTestCase *tc = &perft_cases[i];
        printf("Test Case: %-18s FEN: %s\n", tc->name, tc->fen);
        
        Position pos;
        parse_fen(&pos, tc->fen);
        
        // We test up to depth 5 or the maximum defined in the test case to avoid excessive runtimes
        int max_d = tc->depth_count;
        if (max_d > 5) max_d = 5; // Cap at 5 for quick verify, but startpos goes to 4/5 fast.
        
        printf("Depth: ");
        for (int d = 1; d <= max_d; d++) {
            // Check if test case has expected node count for this depth
            uint64_t expected = tc->perftcnt[d - 1];
            if (expected == 0) break;

            printf("%d ", d);
            fflush(stdout);

            clock_t start = clock();
            uint64_t actual = run_perft(&pos, d);
            clock_t end = clock();

            double time_spent = (double)(end - start) / CLOCKS_PER_SEC;
            double nps = time_spent > 0.0 ? (double)actual / time_spent : 0.0;
            double knps = nps / 1000.0;

            if (actual != expected) {
                printf("\n  -> ERROR at Depth %d: Expected %llu, Got %llu\n", d, (unsigned long long)expected, (unsigned long long)actual);
                errors++;
                break;
            } else {
                passed++;
                if (actual > 10000) {
                    printf("(%llu nodes, %.1f KNPS) ", (unsigned long long)actual, knps);
                }
            }
        }
        printf("\n------------------------------------------------------------------------\n");
    }

    printf("PERFT Results: %d passed depths, %d errors.\n", passed, errors);
    return errors;
}
