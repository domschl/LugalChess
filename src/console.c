#include "console.h"
#include "defs.h"
#include "bitboard.h"
#include "position.h"

#include "movegen.h"
#include "move.h"
#include "search.h"
#include "evaluation.h"
#include "tt.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int search_depth = 5; // Default search depth level

// Print commands help
static void print_help(void) {
    printf("\nLugalChess Interactive Console Commands:\n");
    printf("  help            - Show this help message\n");
    printf("  new             - Start a new game from the standard starting position\n");
    printf("  board (or d)    - Display the current board state\n");
    printf("  level <depth>   - Set the engine search depth (current: %d)\n", search_depth);
    printf("  fen <FEN>       - Set the board to a custom FEN position\n");
    printf("  go              - Force the engine to think and play a move\n");
    printf("  undo            - Take back the last moves (your move + computer's move)\n");
    printf("  eval            - Print the static evaluation score of the current position\n");
    printf("  moves           - List all legal moves in the current position\n");
    printf("  quit            - Exit the program\n");
    printf("  <move>          - Type a move in UCI format (e.g. e2e4, g1f3, e7e8q) to play it\n\n");
}

// Helper to parse and execute a player move
static bool execute_player_move(Position *pos, const char *move_str) {
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

// Make engine search and play a move
static void make_engine_move(Position *pos) {
    printf("Engine is thinking (depth %d)...\n", search_depth);
    fflush(stdout);

    // Call search
    // We redirect the stdout of search temporarily or let it print UCI style info.
    // To make it clean, we search and retrieve the best move from the TT.
    increment_tt_age();
    
    // Perform search
    int score = pv_search(pos, search_depth, 0, -INFINITY_VALUE, INFINITY_VALUE, true);
    
    // Retrieve move from TT
    Move best_move = 0;
    int dummy_score;
    read_tt(pos->hash_key, search_depth, -INFINITY_VALUE, INFINITY_VALUE, &dummy_score, &best_move);
    
    if (best_move != 0) {
        int from = MOVE_FROM(best_move);
        int to = MOVE_TO(best_move);
        printf("Engine plays: %c%d%c%d", 
               'a' + (from % 8), (from / 8) + 1,
               'a' + (to % 8), (to / 8) + 1);
        if (move_is_promo(best_move)) {
            int promo = move_promo_piece(best_move);
            const char promo_chars[] = "  pnbrqk";
            printf("%c", promo_chars[promo]);
        }
        printf(" (Score: %+d)\n", score);
        
        make_move(pos, best_move);
    } else {
        printf("Engine resigned or found no legal moves.\n");
    }
}

// Portable line reader that echos characters, handles backspace/delete, and handles \r/\n
static void get_line_custom(char *buffer, int max_len) {
    int len = 0;
    while (len < max_len - 1) {
        int c = getchar();
        if (c == EOF || c == 0) {
            continue;
        }
        
        // Handle newline / carriage return
        if (c == '\r' || c == '\n') {
            buffer[len] = '\0';
            printf("\n");
            fflush(stdout);
            break;
        }
        // Handle Backspace (ASCII 8 or 127)
        else if (c == '\b' || c == 127) {
            if (len > 0) {
                len--;
                // Erase char on terminal (backspace, space, backspace)
                printf("\b \b");
                fflush(stdout);
            }
        }
        // Handle printable characters
        else if (c >= 32 && c <= 126) {
            buffer[len++] = (char)c;
            putchar(c);
            fflush(stdout);
        }
    }
}

void console_loop(void) {
    // Initialize Transposition Table (safely allocate 32KB on microcontrollers, 16MB on host)
#if defined(__arm__) || defined(PICO_BOARD)
    init_tt(0);
#else
    init_tt(16);
#endif


    Position pos;
    parse_fen(&pos, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    printf("=========================================\n");
    printf("   LugalChess Interactive Console Mode   \n");
    printf("=========================================\n");
    printf("Type 'help' for a list of commands.\n\n");

    print_board(&pos);

    char line[512];
    while (1) {
        printf("\nLugalChess> ");
        fflush(stdout);

        get_line_custom(line, sizeof(line));

        // Clean any leftover carriage return/line feed characters
        line[strcspn(line, "\n")] = '\0';
        line[strcspn(line, "\r")] = '\0';


        // Skip empty input
        if (strlen(line) == 0) {
            continue;
        }

        if (strcmp(line, "help") == 0) {
            print_help();
        } 
        else if (strcmp(line, "new") == 0) {
            parse_fen(&pos, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
            printf("New game started.\n");
            print_board(&pos);
        } 
        else if (strcmp(line, "board") == 0 || strcmp(line, "d") == 0) {
            print_board(&pos);
            print_position_info(&pos);
        } 
        else if (strncmp(line, "level", 5) == 0) {
            int val = atoi(line + 5);
            if (val >= 1 && val <= 64) {
                search_depth = val;
                printf("Search depth set to %d.\n", search_depth);
            } else {
                printf("Invalid level. Please specify depth between 1 and 64.\n");
            }
        } 
        else if (strncmp(line, "fen", 3) == 0) {
            char *fen_str = line + 3;
            while (*fen_str == ' ') fen_str++;
            parse_fen(&pos, fen_str);
            printf("Position loaded.\n");
            print_board(&pos);
        } 
        else if (strcmp(line, "go") == 0) {
            make_engine_move(&pos);
            print_board(&pos);
        } 
        else if (strcmp(line, "undo") == 0) {
            // Undo 2 ply (player move + engine move) if possible, or 1 ply if only 1 move has been made.
            if (pos.history_ply >= 2) {
                unmake_move(&pos);
                unmake_move(&pos);
                printf("Took back last turn (2 moves).\n");
                print_board(&pos);
            } else if (pos.history_ply == 1) {
                unmake_move(&pos);
                printf("Took back 1 move.\n");
                print_board(&pos);
            } else {
                printf("Nothing to undo.\n");
            }
        } 
        else if (strcmp(line, "eval") == 0) {
            int score = evaluate(&pos);
            printf("Static Evaluation Score: %+d centipawns (from current side's perspective)\n", score);
        } 
        else if (strcmp(line, "moves") == 0) {
            MoveList list;
            generate_moves(&pos, &list);
            printf("Legal moves in this position:\n");
            int legal_cnt = 0;
            for (int i = 0; i < list.count; i++) {
                Move m = list.moves[i];
                if (make_move(&pos, m)) {
                    int from = MOVE_FROM(m);
                    int to = MOVE_TO(m);
                    printf("  %c%d%c%d", 
                           'a' + (from % 8), (from / 8) + 1,
                           'a' + (to % 8), (to / 8) + 1);
                    if (move_is_promo(m)) {
                        int promo = move_promo_piece(m);
                        const char promo_chars[] = "  pnbrqk";
                        printf("%c", promo_chars[promo]);
                    }
                    printf("\n");
                    legal_cnt++;
                    unmake_move(&pos);
                }
            }
            printf("Total legal moves: %d\n", legal_cnt);
        } 
        else if (strcmp(line, "quit") == 0) {
            break;
        } 
        else {
            // Try to parse input as a player move
            if (execute_player_move(&pos, line)) {
                print_board(&pos);
                
                // Check if game is over
                MoveList list;
                generate_moves(&pos, &list);
                int legal_cnt = 0;
                for (int i = 0; i < list.count; i++) {
                    if (make_move(&pos, list.moves[i])) {
                        legal_cnt++;
                        unmake_move(&pos);
                        break;
                    }
                }
                
                if (legal_cnt == 0) {
                    // Check if in check
                    int in_check = is_square_attacked(&pos, get_lsb(pos.piece_bbs[KING] & pos.color_bbs[pos.side]), pos.side ^ 1);
                    if (in_check) {
                        printf("\nCheckmate! You win!\n");
                    } else {
                        printf("\nStalemate! Game is a draw.\n");
                    }
                } else {
                    // Trigger engine move
                    make_engine_move(&pos);
                    print_board(&pos);
                }
            } else {
                printf("Unknown command or invalid move: '%s'. Type 'help' for instructions.\n", line);
            }
        }
    }

    free_tt();
}
