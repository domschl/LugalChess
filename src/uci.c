#include "uci.h"
#include "defs.h"
#include "position.h"
#include "movegen.h"
#include "move.h"
#include "search.h"
#include "tt.h"

// Parse a UCI format move string (e.g. "e2e4", "e7e8q") and make it on the board
static bool parse_and_make_move(Position *pos, const char *move_str) {
    if (move_str[0] < 'a' || move_str[0] > 'h' || move_str[1] < '1' || move_str[1] > '8' ||
        move_str[2] < 'a' || move_str[2] > 'h' || move_str[3] < '1' || move_str[3] > '8') {
        return false;
    }

    int from = (move_str[0] - 'a') + (move_str[1] - '1') * 8;
    int to = (move_str[2] - 'a') + (move_str[3] - '1') * 8;
    int promo_piece = NO_PIECE;

    if (move_str[4] != '\0' && move_str[4] != ' ' && move_str[4] != '\n' && move_str[4] != '\r') {
        switch (move_str[4]) {
            case 'q': promo_piece = QUEEN; break;
            case 'r': promo_piece = ROOK; break;
            case 'b': promo_piece = BISHOP; break;
            case 'n': promo_piece = KNIGHT; break;
        }
    }

    MoveList list;
    generate_moves(pos, &list);

    for (int i = 0; i < list.count; i++) {
        Move move = list.moves[i];
        if (MOVE_FROM(move) == from && MOVE_TO(move) == to) {
            if (promo_piece != NO_PIECE) {
                if (move_is_promo(move) && move_promo_piece(move) == promo_piece) {
                    if (make_move(pos, move)) return true;
                }
            } else {
                if (!move_is_promo(move)) {
                    if (make_move(pos, move)) return true;
                }
            }
        }
    }

    return false;
}

// UCI protocol loop
void uci_loop(void) {
    // Default Transposition Table size (safely allocate 32KB on microcontrollers, 16MB on host)
#if defined(__arm__) || defined(PICO_BOARD)
    init_tt(0);
#else
    init_tt(16);
#endif


    char line[2048];
    Position pos;
    parse_fen(&pos, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    printf("LugalChess Engine initialized. Waiting for UCI commands...\n");
    fflush(stdout);

    while (1) {
        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }

        // Clean newline chars
        line[strcspn(line, "\n")] = '\0';
        line[strcspn(line, "\r")] = '\0';

        if (strcmp(line, "uci") == 0) {
            printf("id name LugalChess 1.0\n");
            printf("id author Antigravity\n");
            printf("option name Hash type spin default 16 min 1 max 256\n");
            printf("uciok\n");
            fflush(stdout);
        } else if (strcmp(line, "isready") == 0) {
            printf("readyok\n");
            fflush(stdout);
        } else if (strncmp(line, "setoption name Hash value", 25) == 0) {
            int val = atoi(line + 25);
            init_tt(val);
            printf("info string Hash table set to %d MB\n", val);
            fflush(stdout);
        } else if (strcmp(line, "ucinewgame") == 0) {
            clear_tt();
        } else if (strncmp(line, "position", 8) == 0) {
            char *ptr = line + 8;
            while (*ptr == ' ') ptr++;

            if (strncmp(ptr, "startpos", 8) == 0) {
                parse_fen(&pos, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
                ptr += 8;
            } else if (strncmp(ptr, "fen", 3) == 0) {
                ptr += 3;
                while (*ptr == ' ') ptr++;
                
                // Extract FEN string (all text until " moves " or end of line)
                char fen[512];
                int fen_len = 0;
                while (*ptr != '\0' && strncmp(ptr, "moves", 5) != 0) {
                    fen[fen_len++] = *ptr++;
                }
                while (fen_len > 0 && fen[fen_len - 1] == ' ') fen_len--;
                fen[fen_len] = '\0';
                
                parse_fen(&pos, fen);
            }

            // Parse subsequent moves if present
            char *moves_ptr = strstr(ptr, "moves");
            if (moves_ptr != NULL) {
                moves_ptr += 5; // skip "moves"
                while (*moves_ptr != '\0') {
                    while (*moves_ptr == ' ') moves_ptr++;
                    if (*moves_ptr == '\0') break;

                    char move_str[10];
                    int len = 0;
                    while (*moves_ptr != ' ' && *moves_ptr != '\0' && len < 9) {
                        move_str[len++] = *moves_ptr++;
                    }
                    move_str[len] = '\0';

                    parse_and_make_move(&pos, move_str);
                }
            }
        } else if (strncmp(line, "go", 2) == 0) {
            int depth = 8; // Default search depth
            int time_ms = -1; // Infinite time by default

            char *depth_ptr = strstr(line, "depth");
            if (depth_ptr != NULL) {
                depth = atoi(depth_ptr + 5);
            }

            char *movetime_ptr = strstr(line, "movetime");
            if (movetime_ptr != NULL) {
                time_ms = atoi(movetime_ptr + 8);
            } else {
                // Parse wtime/btime for active color time limits
                if (pos.side == WHITE) {
                    char *wtime_ptr = strstr(line, "wtime");
                    if (wtime_ptr != NULL) {
                        int wtime = atoi(wtime_ptr + 5);
                        // Allocate 1/20th of remaining time
                        time_ms = wtime / 20;
                    }
                } else {
                    char *btime_ptr = strstr(line, "btime");
                    if (btime_ptr != NULL) {
                        int btime = atoi(btime_ptr + 5);
                        time_ms = btime / 20;
                    }
                }
            }

            search_position(&pos, depth, time_ms);
        } else if (strcmp(line, "quit") == 0) {
            break;
        }
    }

    free_tt();
}
