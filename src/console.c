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

#if defined(__arm__) || defined(PICO_BOARD)
#include "pico/stdlib.h"
#include "tm1638.h"
#include <ctype.h>

static char last_player_move[5] = "    ";
static char last_engine_move[5] = "    ";

typedef enum {
    MODE_NORMAL,
    MODE_BOARD_VIEW,
    MODE_LEVEL_SELECT,
    MODE_OPTION_MENU
} BoardMode;

static BoardMode current_board_mode = MODE_NORMAL;
static int current_rank = 0;
static int current_option_idx = 0;

#define OPTION_COUNT 8
static const char *option_names[OPTION_COUNT] = {
    "nEU gAnE",
    "PLAy bL ",
    "PLAy UH ",
    "ScOrE   ",
    "LEuEL   ",
    "SIdES   ",
    "HAlF    ",
    "MOuES   "
};

static void update_tm1638_display(void) {
    char buf[9];
    // Copy player move (uppercase)
    for (int i = 0; i < 4; i++) {
        buf[i] = last_player_move[i] ? toupper((unsigned char)last_player_move[i]) : ' ';
    }
    // Copy engine move (uppercase)
    for (int i = 0; i < 4; i++) {
        buf[i + 4] = last_engine_move[i] ? toupper((unsigned char)last_engine_move[i]) : ' ';
    }
    buf[8] = '\0';
    tm1638_display_string(buf);
}

static void format_score_str(int score, char *score_str) {
    if (score > MATE_VALUE - 100) {
        int moves = (INFINITY_VALUE - score + 1) / 2;
        score_str[0] = 't';
        score_str[1] = ' ';
        score_str[2] = '0' + (moves / 10);
        score_str[3] = '0' + (moves % 10);
        score_str[4] = '\0';
    } else if (score < -MATE_VALUE + 100) {
        int moves = (INFINITY_VALUE + score + 1) / 2;
        score_str[0] = '-';
        score_str[1] = 't';
        score_str[2] = '0' + (moves / 10);
        score_str[3] = '0' + (moves % 10);
        score_str[4] = '\0';
    } else {
        int abs_score = score >= 0 ? score : -score;
        score_str[0] = score >= 0 ? '+' : '-';
        score_str[1] = '0' + ((abs_score / 100) % 10);
        score_str[2] = '0' + ((abs_score / 10) % 10);
        score_str[3] = '0' + (abs_score % 10);
        score_str[4] = '\0';
    }
}

static void format_move_str(Move move, char *move_str) {
    int from = MOVE_FROM(move);
    int to = MOVE_TO(move);
    move_str[0] = 'a' + (from % 8);
    move_str[1] = '1' + (from / 8);
    move_str[2] = 'a' + (to % 8);
    move_str[3] = '1' + (to / 8);
    move_str[4] = '\0';
}

static void show_board_rank(const Position *pos, int rank) {
    // 1. Flash rank number first
    char rank_name[9];
    snprintf(rank_name, sizeof(rank_name), "rAnK  %d ", rank + 1);
    tm1638_display_string(rank_name);
    sleep_ms(350);

    // 2. Display pieces
    char formatted[17];
    int f_idx = 0;
    for (int file = 0; file < 8; file++) {
        int sq = file + rank * 8;
        int piece = pos->board[sq];
        if (piece == NO_PIECE) {
            formatted[f_idx++] = '_';
        } else {
            // White pieces have the decimal point lit, Black pieces do not.
            // Pawn=P, Knight=n, Bishop=b, Rook=r, Queen=q, King=k (mapped to H in font)
            const char piece_chars[] = "Pnbrqk";
            formatted[f_idx++] = piece_chars[piece];
            int color = pos->color_bbs[WHITE] & (1ULL << sq) ? WHITE : BLACK;
            if (color == WHITE) {
                formatted[f_idx++] = '.'; // Add decimal point for White
            }
        }
    }
    formatted[f_idx] = '\0';
    tm1638_display_string(formatted);
}

static Move current_search_best_move = 0;
static int current_search_score = 0;
static int current_search_depth = 0;
static uint32_t last_display_toggle_ms = 0;
static bool display_show_score = true;

static void update_thinking_display(void) {
    char move_str[5];
    if (current_search_best_move != 0) {
        format_move_str(current_search_best_move, move_str);
    } else {
        strcpy(move_str, "tHIn");
    }
    
    char right_str[5];
    if (display_show_score) {
        if (current_search_best_move != 0) {
            format_score_str(current_search_score, right_str);
        } else {
            strcpy(right_str, "K   ");
        }
    } else {
        snprintf(right_str, sizeof(right_str), "L-%02d", current_search_depth);
    }
    
    char buf[9];
    for (int i = 0; i < 4; i++) {
        buf[i] = toupper((unsigned char)move_str[i]);
        buf[i + 4] = toupper((unsigned char)right_str[i]);
    }
    buf[8] = '\0';
    tm1638_display_string(buf);
}

// Progress callback for updating the screen during engine search
void search_progress_callback(Move move, int score, int depth) {
    if (depth == 1 && move == 0) {
        // Reset search states on start
        current_search_best_move = 0;
        current_search_score = 0;
        current_search_depth = 1;
        display_show_score = true;
        last_display_toggle_ms = time_us_32() / 1000;
    }
    
    if (move != 0) {
        current_search_best_move = move;
        current_search_score = score;
    }
    current_search_depth = depth;
    update_thinking_display();
}

// Poll key to abort search and toggle thinking display every second
void search_poll_stop_callback(void) {
    // Check if Stop button (key 11) is pressed
    int key = tm1638_get_key();
    if (key == 11) {
        stop_search = true;
    }
    
    // Alternating display toggle every 1000 ms
    uint32_t now_ms = time_us_32() / 1000;
    if (now_ms - last_display_toggle_ms >= 1000) {
        display_show_score = !display_show_score;
        last_display_toggle_ms = now_ms;
        update_thinking_display();
    }
}

// Current Position pointer for menu evaluations
static Position *current_pos_ptr = NULL;
#else
// Dummy search callbacks for host build
void search_progress_callback(Move move, int score, int depth) {}
void search_poll_stop_callback(void) {}
#endif


// Default search depth: lower on firmware to reduce stack usage
#if defined(__arm__) || defined(PICO_BOARD)
static int search_level = 2;
static const int level_depths[8] = { 1, 2, 3, 5, 7, 9, 11, 13 };
static int search_depth = 2; // dynamically set: search_depth = level_depths[search_level - 1]
#else
static int search_depth = 5;
#endif

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

    // If user didn't specify a promotion piece, check if the only legal moves for this from/to are promotions.
    // If so, default to QUEEN.
    if (promo_piece == NO_PIECE) {
        bool only_promo = false;
        for (int i = 0; i < list.count; i++) {
            Move move = list.moves[i];
            if (MOVE_FROM(move) == from && MOVE_TO(move) == to) {
                if (move_is_promo(move)) {
                    only_promo = true;
                    break;
                }
            }
        }
        if (only_promo) {
            promo_piece = QUEEN;
        }
    }

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

    // Perform iterative deepening search
    search_position(pos, search_depth, -1);
    
    // Retrieve best move and score from TT (scanning downwards from search_depth in case search was aborted)
    Move best_move = 0;
    int score = 0;
    for (int d = search_depth; d >= 1; d--) {
        int dummy_score = 0;
        read_tt(pos->hash_key, d, -INFINITY_VALUE, INFINITY_VALUE, &dummy_score, &best_move);
        if (best_move != 0) {
            score = dummy_score;
            break;
        }
    }
    
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
#if defined(__arm__) || defined(PICO_BOARD)
        last_engine_move[0] = 'a' + (from % 8);
        last_engine_move[1] = '1' + (from / 8);
        last_engine_move[2] = 'a' + (to % 8);
        last_engine_move[3] = '1' + (to / 8);
        last_engine_move[4] = '\0';
        update_tm1638_display();
#endif

    } else {
        printf("Engine resigned or found no legal moves.\n");
    }
}

// Portable line reader that echos characters, handles backspace/delete, and handles \r/\n
static void get_line_custom(char *buffer, int max_len) {
    int len = 0;
#if defined(__arm__) || defined(PICO_BOARD)
    static char keypad_input[5] = "";
    static int keypad_len = 0;
#endif

    while (len < max_len - 1) {
        int c;
#if defined(__arm__) || defined(PICO_BOARD)
        // 1. Poll TM1638 Keypad directly on Core 0
        int key = tm1638_get_key();
        if (key != -1) {
            // Key press debouncing
            sleep_ms(250);

            if (current_board_mode == MODE_NORMAL) {
                // S1..S8: input keys A/1 to H/8
                if (key >= 0 && key <= 7) {
                    if (keypad_len == 0 || keypad_len == 2) {
                        char file_char = 'a' + key;
                        keypad_input[keypad_len++] = file_char;
                        keypad_input[keypad_len] = '\0';
                    } else if (keypad_len == 1 || keypad_len == 3) {
                        char rank_char = '1' + key;
                        keypad_input[keypad_len++] = rank_char;
                        keypad_input[keypad_len] = '\0';
                    }

                    // Sync with display state
                    for (int i = 0; i < 4; i++) {
                        last_player_move[i] = (i < keypad_len) ? keypad_input[i] : ' ';
                    }
                    strcpy(last_engine_move, "    ");
                    update_tm1638_display();

                    if (keypad_len == 4) {
                        strcpy(buffer, keypad_input);
                        keypad_len = 0;
                        keypad_input[0] = '\0';
                        printf("%s\n", buffer);
                        fflush(stdout);
                        return;
                    }
                }
                // S9: Back key -> Undo
                else if (key == 8) {
                    strcpy(buffer, "undo");
                    printf("undo\n");
                    fflush(stdout);
                    return;
                }
                // S11: Board key -> Enter board view
                else if (key == 10) {
                    current_board_mode = MODE_BOARD_VIEW;
                    current_rank = 0;
                    if (current_pos_ptr) {
                        show_board_rank(current_pos_ptr, current_rank);
                    }
                }
                // S12: Stop key -> Clear typing progress
                else if (key == 11) {
                    keypad_len = 0;
                    keypad_input[0] = '\0';
                    strcpy(last_player_move, "    ");
                    update_tm1638_display();
                }
                // S13: Lvl key -> Choose depth
                else if (key == 12) {
                    current_board_mode = MODE_LEVEL_SELECT;
                    char buf[9];
                    snprintf(buf, sizeof(buf), "L-%02d    ", search_level);
                    tm1638_display_string(buf);
                }
                // S14: Anal key -> Trigger search
                else if (key == 13) {
                    strcpy(buffer, "go");
                    printf("go\n");
                    fflush(stdout);
                    return;
                }
                // S15: Opt key -> Options menu
                else if (key == 14) {
                    current_board_mode = MODE_OPTION_MENU;
                    current_option_idx = 0;
                    tm1638_display_string(option_names[current_option_idx]);
                }
            }
            else if (current_board_mode == MODE_BOARD_VIEW) {
                // Back key (8) -> Scroll rank up
                if (key == 8) {
                    if (current_rank < 7) {
                        current_rank++;
                        if (current_pos_ptr) show_board_rank(current_pos_ptr, current_rank);
                    }
                }
                // Fwrd key (9) -> Scroll rank down
                else if (key == 9) {
                    if (current_rank > 0) {
                        current_rank--;
                        if (current_pos_ptr) show_board_rank(current_pos_ptr, current_rank);
                    }
                }
                // Stop key (11) -> Exit board view
                else if (key == 11) {
                    current_board_mode = MODE_NORMAL;
                    update_tm1638_display();
                }
            }
            else if (current_board_mode == MODE_LEVEL_SELECT) {
                // S1..S8: select level 1-8
                if (key >= 0 && key <= 7) {
                    search_level = key + 1;
                    search_depth = level_depths[search_level - 1];
                    char buf[9];
                    snprintf(buf, sizeof(buf), "L-%02d    ", search_level);
                    tm1638_display_string(buf);
                }
                // Stop key (11) -> Cancel
                else if (key == 11) {
                    current_board_mode = MODE_NORMAL;
                    update_tm1638_display();
                }
                // Enter key (15) -> Confirm
                else if (key == 15) {
                    current_board_mode = MODE_NORMAL;
                    update_tm1638_display();
                }
            }
            else if (current_board_mode == MODE_OPTION_MENU) {
                // Back key (8) -> Previous option
                if (key == 8) {
                    current_option_idx = (current_option_idx - 1 + OPTION_COUNT) % OPTION_COUNT;
                    tm1638_display_string(option_names[current_option_idx]);
                }
                // Fwrd key (9) -> Next option
                else if (key == 9) {
                    current_option_idx = (current_option_idx + 1) % OPTION_COUNT;
                    tm1638_display_string(option_names[current_option_idx]);
                }
                // Stop key (11) -> Cancel
                else if (key == 11) {
                    current_board_mode = MODE_NORMAL;
                    update_tm1638_display();
                }
                // Enter key (15) -> Confirm selection
                else if (key == 15) {
                    if (current_option_idx == 0) { // New game
                        current_board_mode = MODE_NORMAL;
                        strcpy(buffer, "new");
                        printf("new\n");
                        fflush(stdout);
                        return;
                    } else if (current_option_idx == 1) { // Play Black
                        current_board_mode = MODE_NORMAL;
                        update_tm1638_display();
                        // No immediate engine move required (player is White)
                    } else if (current_option_idx == 2) { // Play White
                        current_board_mode = MODE_NORMAL;
                        strcpy(buffer, "go");
                        printf("go\n");
                        fflush(stdout);
                        return;
                    } else if (current_option_idx == 3) { // Score
                        if (current_pos_ptr) {
                            int score = evaluate(current_pos_ptr);
                            char score_str[5];
                            format_score_str(score, score_str);
                            char buf[9];
                            snprintf(buf, sizeof(buf), "ScO %s", score_str);
                            tm1638_display_string(buf);
                            sleep_ms(2000);
                        }
                        tm1638_display_string(option_names[current_option_idx]);
                    } else if (current_option_idx == 4) { // Level
                        current_board_mode = MODE_LEVEL_SELECT;
                        char buf[9];
                        snprintf(buf, sizeof(buf), "L-%02d    ", search_level);
                        tm1638_display_string(buf);
                    } else if (current_option_idx == 5) { // Side
                        if (current_pos_ptr) {
                            char buf[9];
                            snprintf(buf, sizeof(buf), "SIdE %s", current_pos_ptr->side == WHITE ? "WH" : "bL");
                            tm1638_display_string(buf);
                            sleep_ms(2000);
                        }
                        tm1638_display_string(option_names[current_option_idx]);
                    } else if (current_option_idx == 6) { // Halfmove
                        if (current_pos_ptr) {
                            char buf[9];
                            snprintf(buf, sizeof(buf), "H- %02d   ", current_pos_ptr->halfmove);
                            tm1638_display_string(buf);
                            sleep_ms(2000);
                        }
                        tm1638_display_string(option_names[current_option_idx]);
                    } else if (current_option_idx == 7) { // Move count
                        if (current_pos_ptr) {
                            char buf[9];
                            snprintf(buf, sizeof(buf), "n- %02d   ", current_pos_ptr->history_ply);
                            tm1638_display_string(buf);
                            sleep_ms(2000);
                        }
                        tm1638_display_string(option_names[current_option_idx]);
                    }
                }
            }
        }

        // 2. Poll USB serial with 10ms timeout
        c = getchar_timeout_us(10000);
        if (c == PICO_ERROR_TIMEOUT) {
            continue;
        }
#else
        c = getchar();
#endif

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

#if defined(__arm__) || defined(PICO_BOARD)
static void sync_moves_from_history(const Position *pos) {
    if (pos->history_ply >= 2) {
        format_move_str(pos->history[pos->history_ply - 2].move, last_player_move);
        format_move_str(pos->history[pos->history_ply - 1].move, last_engine_move);
    } else if (pos->history_ply == 1) {
        format_move_str(pos->history[pos->history_ply - 1].move, last_player_move);
        strcpy(last_engine_move, "    ");
    } else {
        strcpy(last_player_move, "    ");
        strcpy(last_engine_move, "    ");
    }
    update_tm1638_display();
}
#endif

static bool check_and_display_game_over(Position *pos) {
    MoveList list;
    generate_moves(pos, &list);
    int legal_cnt = 0;
    for (int i = 0; i < list.count; i++) {
        if (make_move(pos, list.moves[i])) {
            legal_cnt++;
            unmake_move(pos);
            break;
        }
    }
    
    if (legal_cnt == 0) {
        int in_check = is_square_attacked(pos, get_lsb(pos->piece_bbs[KING] & pos->color_bbs[pos->side]), pos->side ^ 1);
        if (in_check) {
            if (pos->side == WHITE) {
                printf("\nCheckmate! Black wins!\n");
#if defined(__arm__) || defined(PICO_BOARD)
                tm1638_display_string("nAtE bL "); // "mate bl"
#endif
            } else {
                printf("\nCheckmate! White wins!\n");
#if defined(__arm__) || defined(PICO_BOARD)
                tm1638_display_string("nAtE UH "); // "mate wh"
#endif
            }
        } else {
            printf("\nStalemate! Game is a draw.\n");
#if defined(__arm__) || defined(PICO_BOARD)
            tm1638_display_string("drAU    "); // "draw"
#endif
        }
        return true;
    }
    
    if (pos->halfmove >= 100) {
        printf("\nDraw by 50-move rule.\n");
#if defined(__arm__) || defined(PICO_BOARD)
        tm1638_display_string("drAU    ");
#endif
        return true;
    }

    return false;
}

void console_loop(void) {
    // Initialize Transposition Table (safely allocate 32KB on microcontrollers, 16MB on host)
#if defined(__arm__) || defined(PICO_BOARD)
    init_tt(0);
#else
    init_tt(16);
#endif


    Position pos;
#if defined(__arm__) || defined(PICO_BOARD)
    current_pos_ptr = &pos;
#endif
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
#if defined(__arm__) || defined(PICO_BOARD)
            sync_moves_from_history(&pos);
#endif
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
#if defined(__arm__) || defined(PICO_BOARD)
                // Try to find corresponding level 1-8
                search_level = 0;
                for (int i = 0; i < 8; i++) {
                    if (level_depths[i] == val) {
                        search_level = i + 1;
                        break;
                    }
                }
#endif
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
#if defined(__arm__) || defined(PICO_BOARD)
            sync_moves_from_history(&pos);
#endif
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
#if defined(__arm__) || defined(PICO_BOARD)
            sync_moves_from_history(&pos);
#endif
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
#if defined(__arm__) || defined(PICO_BOARD)
                strncpy(last_player_move, line, 4);
                last_player_move[4] = '\0';
                strcpy(last_engine_move, "    ");
                update_tm1638_display();
#endif
                print_board(&pos);

                
                // Check if game is over
                if (!check_and_display_game_over(&pos)) {
                    // Trigger engine move
                    make_engine_move(&pos);
                    print_board(&pos);
                    // Check if engine's move ended the game
                    check_and_display_game_over(&pos);
                }
            } else {
                printf("Unknown command or invalid move: '%s'. Type 'help' for instructions.\n", line);
            }
        }
    }

    free_tt();
}
