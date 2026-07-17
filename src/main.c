#include "defs.h"
#include "bitboard.h"
#include "zobrist.h"
#include "position.h"
#include "perft.h"
#include "movegen.h"
#include "uci.h"
#include "console.h"


int main(int argc, char *argv[]) {
    // Initialize engine systems
    init_bitboards();
    init_zobrist();

    // Check command line arguments
    bool run_perft_flag = false;
    bool run_console_flag = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--perft") == 0) {
            run_perft_flag = true;
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--console") == 0) {
            run_console_flag = true;
        }
    }

    if (run_perft_flag) {
        return run_perft_tests();
    }

    if (run_console_flag) {
        console_loop();
        return 0;
    }

    // Default mode: run UCI loop
    uci_loop();

    return 0;
}



