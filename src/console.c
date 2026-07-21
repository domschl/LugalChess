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

static Position ram_save_pos;
static int ram_save_level = 0;
static bool ram_save_valid = false;
static int max_history_ply = 0;

static bool level_mode_time = true; // true = Time-Based, false = Ply-Based
static int search_level = 2;
static const int level_depths[8] = { 1, 2, 3, 5, 7, 9, 11, 13 };
static const int level_times_ms[8] = { 1000, 2000, 5000, 10000, 15000, 30000, 60000, -1 };
static int search_depth = 2;

static bool check_and_display_game_over(Position *pos);
static void check_and_update_check_status(Position *pos);

#if defined(__arm__) || defined(PICO_BOARD)
static void format_move_str(Move move, char *move_str);
static void sync_moves_from_history(const Position *pos);
#endif

#if defined(__arm__) || defined(PICO_BOARD)
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "tm1638.h"
#include "st7735.h"
#include <ctype.h>

static char last_player_move[6] = "     ";
static char last_engine_move[6] = "     ";
static Position *current_pos_ptr = NULL;
static char game_status_msg[9] = "";
static bool display_show_moves = true;
static uint32_t last_normal_toggle_ms = 0;
static int game_side_to_move = WHITE;
static bool show_thinking_board = false;
static uint32_t last_thinking_board_update_ms = 0;

typedef enum {
    MODE_NORMAL,
    MODE_BOARD_VIEW,
    MODE_LEVEL_SELECT,
    MODE_OPTION_MENU
} BoardMode;

static BoardMode current_board_mode = MODE_NORMAL;
static int current_rank = 0;
static int current_option_idx = 0;

#define FLASH_SAVE_OFFSET (1536 * 1024)
typedef struct {
    uint32_t magic;
    char fen[220];
    int level;
} SaveData;

#define OPTION_COUNT 11
static const char *option_names[OPTION_COUNT] = {
    "nEU gAnE",
    "PLAy bL ",
    "PLAy UH ",
    "ScOrE   ",
    "LEuEL   ",
    "tInE PLy",
    "SIdES   ",
    "HAlF    ",
    "MOuES   ",
    "SAuE    ",
    "LOAd    "
};

static void update_tm1638_display(void) {
    char buf[9];
    // Copy player move
    if (last_player_move[4] != '\0' && last_player_move[4] != ' ') {
        buf[0] = toupper((unsigned char)last_player_move[0]);
        buf[1] = toupper((unsigned char)last_player_move[1]);
        buf[2] = toupper((unsigned char)last_player_move[2]);
        buf[3] = toupper((unsigned char)last_player_move[4]);
    } else {
        for (int i = 0; i < 4; i++) {
            buf[i] = last_player_move[i] ? toupper((unsigned char)last_player_move[i]) : ' ';
        }
    }
    // Copy engine move
    if (last_engine_move[4] != '\0' && last_engine_move[4] != ' ') {
        buf[4] = toupper((unsigned char)last_engine_move[0]);
        buf[5] = toupper((unsigned char)last_engine_move[1]);
        buf[6] = toupper((unsigned char)last_engine_move[2]);
        buf[7] = toupper((unsigned char)last_engine_move[4]);
    } else {
        for (int i = 0; i < 4; i++) {
            buf[i + 4] = last_engine_move[i] ? toupper((unsigned char)last_engine_move[i]) : ' ';
        }
    }
    buf[8] = '\0';
    tm1638_display_string(buf);

    if (current_pos_ptr) {
        game_side_to_move = current_pos_ptr->side;
        int score = evaluate(current_pos_ptr);
        char last_move_buf[6] = "";
        const char *last_move = "";
        if (current_pos_ptr->history_ply > 0) {
            format_move_str(current_pos_ptr->history[current_pos_ptr->history_ply - 1].move, last_move_buf);
            last_move = last_move_buf;
        }
        st7735_draw_board(current_pos_ptr);
        st7735_draw_status(current_pos_ptr, search_level, level_mode_time, game_side_to_move, search_depth, score, 0, last_move, false, game_status_msg);
    }
}

static void format_score_str(int score, char *score_str) {
    if (score > MATE_VALUE - 100) {
        int moves = (INFINITY_VALUE - score + 1) / 2;
        snprintf(score_str, 8, "+t%02d", moves > 99 ? 99 : moves);
    } else if (score < -MATE_VALUE + 100) {
        int moves = (score - INFINITY_VALUE) / 2;
        snprintf(score_str, 8, "-t%02d", (-moves) > 99 ? 99 : -moves);
    } else {
        char sign = (score >= 0) ? '+' : '-';
        int abs_score = (score >= 0) ? score : -score;
        
        if (abs_score < 1000) {
            // < 10.00 pawns: format as sign + d.dd (e.g. +1.50, +0.60)
            int pawns = abs_score / 100;
            int remainder = abs_score % 100;
            snprintf(score_str, 8, "%c%d.%02d", sign, pawns, remainder);
        } else if (abs_score < 10000) {
            // 10.00 to 99.99 pawns: format as sign + dd.d (e.g. +11.5, +25.4)
            int pawns = abs_score / 100;
            int tenths = (abs_score % 100) / 10;
            snprintf(score_str, 8, "%c%d.%d", sign, pawns, tenths);
        } else {
            // >= 100.0 pawns: format as sign + ddd (e.g. +125)
            int pawns = abs_score / 100;
            if (pawns > 999) pawns = 999;
            snprintf(score_str, 8, "%c%d", sign, pawns);
        }
    }
}

static void format_move_str(Move move, char *move_str) {
    int from = MOVE_FROM(move);
    int to = MOVE_TO(move);
    move_str[0] = 'a' + (from % 8);
    move_str[1] = '1' + (from / 8);
    move_str[2] = 'a' + (to % 8);
    move_str[3] = '1' + (to / 8);
    if (move_is_promo(move)) {
        int promo = move_promo_piece(move);
        const char promo_chars[] = "pnbrqk";
        move_str[4] = promo_chars[promo];
        move_str[5] = '\0';
    } else {
        move_str[4] = '\0';
    }
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
    char move_str[6];
    if (current_search_best_move != 0) {
        format_move_str(current_search_best_move, move_str);
    } else {
        strcpy(move_str, "tHIn");
    }
    
    char right_str[8];
    if (display_show_score) {
        if (current_search_best_move != 0) {
            int w_score = (game_side_to_move == WHITE) ? current_search_score : -current_search_score;
            format_score_str(w_score, right_str);
        } else {
            strcpy(right_str, "K   ");
        }
    } else {
        snprintf(right_str, sizeof(right_str), "L-%02d", current_search_depth);
    }
    
    char uppercase_move[5];
    for (int i = 0; i < 4; i++) {
        uppercase_move[i] = toupper((unsigned char)move_str[i]);
    }
    uppercase_move[4] = '\0';
    
    char display_buf[16];
    snprintf(display_buf, sizeof(display_buf), "%s%s", uppercase_move, right_str);
    tm1638_display_string(display_buf);

#if defined(__arm__) || defined(PICO_BOARD)
    if (current_pos_ptr) {
        st7735_draw_status(current_pos_ptr, search_level, level_mode_time, game_side_to_move, current_search_depth, current_search_score, current_search_best_move, "", true, "");
    }
#endif
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
        show_thinking_board = false;
        last_thinking_board_update_ms = 0;
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
        while (tm1638_get_key() == 11) {
            sleep_ms(10);
        }
    }
    else if (key == 10) { // Board key
        show_thinking_board = !show_thinking_board;
        sleep_ms(250); // Debounce board key
        if (current_pos_ptr) {
            st7735_draw_board(current_pos_ptr);
        }
    }
    
    uint32_t now_ms = time_us_32() / 1000;
    
    // If show_thinking_board is active, update the board rendering every 500 ms
    if (show_thinking_board && current_pos_ptr) {
        if (now_ms - last_thinking_board_update_ms >= 500) {
            st7735_draw_board(current_pos_ptr);
            last_thinking_board_update_ms = now_ms;
        }
    }

    // Alternating display toggle every 1000 ms
    if (now_ms - last_display_toggle_ms >= 1000) {
        display_show_score = !display_show_score;
        last_display_toggle_ms = now_ms;
        update_thinking_display();
    }
}

// Current Position pointer for menu evaluations
#else
// Dummy search callbacks for host build
void search_progress_callback(Move move, int score, int depth) {}
void search_poll_stop_callback(void) {}
#endif

// Print commands help
static void print_help(void) {
    printf("\nLugalChess Interactive Console Commands:\n");
    printf("  help            - Show this help message\n");
    printf("  new             - Start a new game from the standard starting position\n");
    printf("  board (or d)    - Display the current board state\n");
    printf("  level <depth>   - Set the engine search depth (current: %d)\n", search_depth);
    printf("  fen [FEN]       - Show current FEN or set to a custom FEN position\n");
    printf("  save            - Save the current position and settings\n");
    printf("  load            - Load the saved position and settings\n");
    printf("  go              - Force the engine to think and play a move\n");
    printf("  undo            - Take back 1 half-move\n");
    printf("  redo            - Re-apply 1 half-move\n");
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
                    if (make_move(pos, move)) {
                        max_history_ply = pos->history_ply;
                        return true;
                    }
                }
            } else {
                if (!move_is_promo(move)) {
                    if (make_move(pos, move)) {
                        max_history_ply = pos->history_ply;
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

static Move get_tt_best_move(uint64_t hash_key) {
    if (tt_table == NULL || tt_size_entries == 0) return 0;
    int idx = (int)(hash_key % (uint64_t)tt_size_entries);
    if (tt_table[idx].hash_key == hash_key) {
        return tt_table[idx].best_move;
    }
    return 0;
}

// Make engine search and play a move
static void make_engine_move(Position *pos) {
    printf("Engine is thinking (%s level %d)...\n", level_mode_time ? "time" : "depth", search_level);
    fflush(stdout);

    stop_search = false;
#if defined(__arm__) || defined(PICO_BOARD)
    current_search_best_move = 0;
    current_search_score = 0;
    current_search_depth = 1;
#endif

    int max_depth = 64;
    int time_limit = -1;

    if (level_mode_time) {
        time_limit = level_times_ms[search_level - 1];
        max_depth = 64;
    } else {
        time_limit = -1;
        max_depth = level_depths[search_level - 1];
    }

    // Perform iterative deepening search
    search_position(pos, max_depth, time_limit);
    
    // Retrieve best move and score from TT (scanning downwards from max depth in case search was boosted or aborted)
    Move best_move = 0;
    int score = 0;
    for (int d = 64; d >= 1; d--) {
        int dummy_score = 0;
        read_tt(pos->hash_key, d, -INFINITY_VALUE, INFINITY_VALUE, &dummy_score, &best_move);
        if (best_move != 0) {
            score = dummy_score;
            break;
        }
    }

#if defined(__arm__) || defined(PICO_BOARD)
    if (best_move == 0 && current_search_best_move != 0) {
        best_move = current_search_best_move;
        score = current_search_score;
    }
#endif
    if (best_move == 0) {
        best_move = get_tt_best_move(pos->hash_key);
    }
    if (best_move == 0) {
        MoveList list;
        generate_moves(pos, &list);
        for (int i = 0; i < list.count; i++) {
            if (make_move(pos, list.moves[i])) {
                unmake_move(pos);
                best_move = list.moves[i];
                break;
            }
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
            const char promo_chars[] = "pnbrqk";
            printf("%c", promo_chars[promo]);
        }
        printf(" (Score: %+d)\n", score);
        
        make_move(pos, best_move);
        max_history_ply = pos->history_ply;
#if defined(__arm__) || defined(PICO_BOARD)
        last_engine_move[0] = 'a' + (from % 8);
        last_engine_move[1] = '1' + (from / 8);
        last_engine_move[2] = 'a' + (to % 8);
        last_engine_move[3] = '1' + (to / 8);
        if (move_is_promo(best_move)) {
            int promo = move_promo_piece(best_move);
            const char promo_chars[] = "pnbrqk";
            last_engine_move[4] = promo_chars[promo];
            last_engine_move[5] = '\0';
        } else {
            last_engine_move[4] = '\0';
        }
        update_tm1638_display();
        if (!check_and_display_game_over(pos)) {
            check_and_update_check_status(pos);
        }
#endif

    } else {
        printf("Engine resigned or found no legal moves.\n");
#if defined(__arm__) || defined(PICO_BOARD)
        if (!check_and_display_game_over(pos)) {
            check_and_update_check_status(pos);
        }
#endif
    }
}

#if defined(__arm__) || defined(PICO_BOARD)

static bool is_player_move_promotion(const char *move_str) {
    if (!current_pos_ptr) return false;
    if (move_str[0] < 'a' || move_str[0] > 'h' || move_str[1] < '1' || move_str[1] > '8' ||
        move_str[2] < 'a' || move_str[2] > 'h' || move_str[3] < '1' || move_str[3] > '8') {
        return false;
    }
    int from = (move_str[0] - 'a') + (move_str[1] - '1') * 8;
    int to = (move_str[2] - 'a') + (move_str[3] - '1') * 8;
    
    if (current_pos_ptr->board[from] != PAWN) return false;
    
    int us = current_pos_ptr->color_bbs[WHITE] & (1ULL << from) ? WHITE : BLACK;
    if (us == WHITE && to / 8 == 7) return true;
    if (us == BLACK && to / 8 == 0) return true;
    
    return false;
}
#endif

// Portable line reader that echos characters, handles backspace/delete, and handles \r/\n
static void get_line_custom(char *buffer, int max_len) {
    int len = 0;
#if defined(__arm__) || defined(PICO_BOARD)
    static char keypad_input[6] = "";
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

            if (game_status_msg[0] != '\0' && key != 11) {
                strcpy(game_status_msg, "");
                display_show_moves = true;
                update_tm1638_display();
            }

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
                        if (is_player_move_promotion(keypad_input)) {
                            tm1638_display_string("1n2b3r4q");
                            // Wait for key release first
                            while (tm1638_get_key() != -1) {
                                sleep_ms(10);
                            }
                            // Wait for key press
                            int choice_key = -1;
                            while (choice_key == -1) {
                                choice_key = tm1638_get_key();
                                sleep_ms(10);
                            }
                            char promo_char = 'q'; // Default to Queen
                            if (choice_key == 0) promo_char = 'n';
                            else if (choice_key == 1) promo_char = 'b';
                            else if (choice_key == 2) promo_char = 'r';
                            else if (choice_key == 3) promo_char = 'q';
                            
                            keypad_input[4] = promo_char;
                            keypad_input[5] = '\0';
                            keypad_len = 5;
                            
                            // Wait for key release again
                            while (tm1638_get_key() != -1) {
                                sleep_ms(10);
                            }
                        } else {
                            keypad_input[4] = '\0';
                            keypad_len = 4;
                        }

                        // Copy full moves (including promotion suffix)
                        for (int i = 0; i < 5; i++) {
                            last_player_move[i] = (i < keypad_len) ? keypad_input[i] : '\0';
                        }
                        strcpy(last_engine_move, "     ");
                        update_tm1638_display();

                        strcpy(buffer, keypad_input);
                        keypad_len = 0;
                        keypad_input[0] = '\0';
                        printf("%s\n", buffer);
                        fflush(stdout);
                        return;
                    }
                }
                // S9: Back key -> Undo 1 half-move
                else if (key == 8) {
                    strcpy(buffer, "undo");
                    printf("undo\n");
                    fflush(stdout);
                    return;
                }
                // S10: Fwrd key -> Redo 1 half-move
                else if (key == 9) {
                    strcpy(buffer, "redo");
                    printf("redo\n");
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
                    if (level_mode_time) {
                        const char *time_names[8] = { "t-1s    ", "t-2s    ", "t-5s    ", "t10s    ", "t15s    ", "t30s    ", "t60s    ", "t-In    " };
                        snprintf(buf, sizeof(buf), "%s", time_names[search_level - 1]);
                    } else {
                        snprintf(buf, sizeof(buf), "L-%02d    ", search_level);
                    }
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
                        if (level_mode_time) {
                            const char *time_names[8] = { "t-1s    ", "t-2s    ", "t-5s    ", "t10s    ", "t15s    ", "t30s    ", "t60s    ", "t-In    " };
                            snprintf(buf, sizeof(buf), "%s", time_names[search_level - 1]);
                        } else {
                            snprintf(buf, sizeof(buf), "L-%02d    ", search_level);
                        }
                        tm1638_display_string(buf);
                    } else if (current_option_idx == 5) { // Mode: Time vs Ply
                        level_mode_time = !level_mode_time;
                        char buf[9];
                        snprintf(buf, sizeof(buf), "%s", level_mode_time ? "tInE LEu" : "PLy  LEu");
                        tm1638_display_string(buf);
                        sleep_ms(1500);
                        tm1638_display_string(option_names[current_option_idx]);
                    } else if (current_option_idx == 6) { // Side
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
                    } else if (current_option_idx == 8) { // Save
                        if (current_pos_ptr) {
                            ram_save_pos = *current_pos_ptr;
                            ram_save_level = search_level;
                            ram_save_valid = true;

                            SaveData data;
                            data.magic = 0xDECADE02;
                            generate_fen(current_pos_ptr, data.fen);
                            data.level = search_level;
                            
                            uint32_t ints = save_and_disable_interrupts();
                            flash_range_erase(FLASH_SAVE_OFFSET, FLASH_SECTOR_SIZE);
                            flash_range_program(FLASH_SAVE_OFFSET, (const uint8_t*)&data, 256);
                            restore_interrupts(ints);
                            
                            tm1638_display_string("SAuEd   "); // "SAVEd"
                            sleep_ms(1500);
                        }
                        current_board_mode = MODE_NORMAL;
                        update_tm1638_display();
                    } else if (current_option_idx == 9) { // Load
                        if (current_pos_ptr) {
                            const SaveData *flash_data = (const SaveData *)(0x10000000 + FLASH_SAVE_OFFSET);
                            if (flash_data->magic == 0xDECADE02) {
                                parse_fen(current_pos_ptr, flash_data->fen);
                                search_level = flash_data->level;
                                if (search_level >= 1 && search_level <= 8) {
                                    search_depth = level_depths[search_level - 1];
                                } else {
                                    search_level = 2;
                                    search_depth = level_depths[search_level - 1];
                                }
                                sync_moves_from_history(current_pos_ptr);
                                tm1638_display_string("LOAdEd  "); // "LOAdEd"
                                sleep_ms(1500);
                            } else if (ram_save_valid) {
                                *current_pos_ptr = ram_save_pos;
                                search_level = ram_save_level;
                                search_depth = level_depths[search_level - 1];
                                sync_moves_from_history(current_pos_ptr);
                                tm1638_display_string("LOAdEd  "); // "LOAdEd"
                                sleep_ms(1500);
                            } else {
                                tm1638_display_string("nO SAvE "); // "no save"
                                sleep_ms(1500);
                            }
                        }
                        current_board_mode = MODE_NORMAL;
                        update_tm1638_display();
                    }
                }
            }
        }

        // 2. Poll USB serial with 10ms timeout
        c = getchar_timeout_us(10000);
        if (c == PICO_ERROR_TIMEOUT) {
            if (current_board_mode == MODE_NORMAL && game_status_msg[0] != '\0') {
                uint32_t now_ms = time_us_32() / 1000;
                if (now_ms - last_normal_toggle_ms >= 1000) {
                    display_show_moves = !display_show_moves;
                    last_normal_toggle_ms = now_ms;
                    if (display_show_moves) {
                        update_tm1638_display();
                    } else {
                        tm1638_display_string(game_status_msg);
                    }
                }
            }
            continue;
        }
#else
        c = getchar();
#endif

        if (c == EOF || c == 0) {
            continue;
        }

#if defined(__arm__) || defined(PICO_BOARD)
        if (game_status_msg[0] != '\0') {
            strcpy(game_status_msg, "");
            display_show_moves = true;
            update_tm1638_display();
        }
#endif

        
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

static void check_and_update_check_status(Position *pos) {
#if defined(__arm__) || defined(PICO_BOARD)
    int in_check = is_square_attacked(pos, get_lsb(pos->piece_bbs[KING] & pos->color_bbs[pos->side]), pos->side ^ 1);
    if (in_check) {
        strcpy(game_status_msg, "CHk     ");
        display_show_moves = false;
        last_normal_toggle_ms = time_us_32() / 1000;
        update_tm1638_display();
    } else {
        strcpy(game_status_msg, "");
        display_show_moves = true;
        update_tm1638_display();
    }
#else
    (void)pos;
#endif
}

static bool is_threefold_repetition(const Position *pos) {
    if (pos->history_ply < 4) return false;
    
    int count = 1; // Current position counts as 1st occurrence
    int start_ply = pos->history_ply - 1;
    int end_ply = pos->history_ply - pos->halfmove;
    if (end_ply < 0) end_ply = 0;

    for (int i = start_ply; i >= end_ply; i--) {
        if (pos->history[i].hash_key == pos->hash_key) {
            count++;
            if (count >= 3) {
                return true;
            }
        }
    }
    
    return false;
}

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
                strcpy(game_status_msg, "nAtE bL ");
                display_show_moves = false;
                last_normal_toggle_ms = time_us_32() / 1000;
                tm1638_display_string(game_status_msg);
#endif
            } else {
                printf("\nCheckmate! White wins!\n");
#if defined(__arm__) || defined(PICO_BOARD)
                strcpy(game_status_msg, "nAtE UH ");
                display_show_moves = false;
                last_normal_toggle_ms = time_us_32() / 1000;
                tm1638_display_string(game_status_msg);
#endif
            }
        } else {
            printf("\nStalemate! Game is a draw.\n");
#if defined(__arm__) || defined(PICO_BOARD)
            strcpy(game_status_msg, "drAU    ");
            display_show_moves = false;
            last_normal_toggle_ms = time_us_32() / 1000;
            tm1638_display_string(game_status_msg);
#endif
        }
        return true;
    }
    
    if (is_threefold_repetition(pos)) {
        printf("\nDraw by 3-fold repetition.\n");
#if defined(__arm__) || defined(PICO_BOARD)
        strcpy(game_status_msg, "drAU    ");
        display_show_moves = false;
        last_normal_toggle_ms = time_us_32() / 1000;
        update_tm1638_display();
#endif
        return true;
    }

    if (pos->halfmove >= 100) {
        printf("\nDraw by 50-move rule.\n");
#if defined(__arm__) || defined(PICO_BOARD)
        strcpy(game_status_msg, "drAU    ");
        display_show_moves = false;
        last_normal_toggle_ms = time_us_32() / 1000;
        update_tm1638_display();
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

#if defined(__arm__) || defined(PICO_BOARD)
    update_tm1638_display();
#endif

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
            strcpy(game_status_msg, "");
            display_show_moves = true;
            sync_moves_from_history(&pos);
            update_tm1638_display();
#endif
            max_history_ply = pos.history_ply;
        } 
        else if (strcmp(line, "board") == 0 || strcmp(line, "d") == 0) {
            print_board(&pos);
            print_position_info(&pos);
        } 
        else if (strncmp(line, "level", 5) == 0) {
            char *ptr = line + 5;
            while (*ptr == ' ') ptr++;
            int val = atoi(ptr);
            if (val >= 1 && val <= 8) {
                search_level = val;
                if (strstr(ptr, "d") != NULL || strstr(ptr, "p") != NULL) {
                    level_mode_time = false;
                } else if (strstr(ptr, "s") != NULL || strstr(ptr, "t") != NULL) {
                    level_mode_time = true;
                }
                search_depth = level_depths[search_level - 1];
                if (level_mode_time) {
                    int t_ms = level_times_ms[search_level - 1];
                    if (t_ms != -1) {
                        printf("Search level set to %d (Time: %d ms per move).\n", search_level, t_ms);
                    } else {
                        printf("Search level set to %d (Time: Infinite / manual stop).\n", search_level);
                    }
                } else {
                    printf("Search level set to %d (Depth: %d plies).\n", search_level, search_depth);
                }
#if defined(__arm__) || defined(PICO_BOARD)
                update_tm1638_display();
#endif
            } else if (val >= 9 && val <= 64) {
                level_mode_time = false;
                search_depth = val;
                printf("Custom depth search set to %d plies.\n", search_depth);
            } else {
                printf("Invalid level. Specify level 1-8 (e.g. 'level 3', 'level 3s', 'level 5d').\n");
            }
        } 
        else if (strncmp(line, "fen", 3) == 0) {
            char *fen_str = line + 3;
            while (*fen_str == ' ') fen_str++;
            if (*fen_str == '\0') {
                char fen_buf[256];
                generate_fen(&pos, fen_buf);
                printf("Current FEN: %s\n", fen_buf);
            } else {
                parse_fen(&pos, fen_str);
                max_history_ply = pos.history_ply;
                printf("Position loaded.\n");
                print_board(&pos);
#if defined(__arm__) || defined(PICO_BOARD)
                sync_moves_from_history(&pos);
#endif
            }
        } 
        else if (strcmp(line, "save") == 0) {
            ram_save_pos = pos;
#if defined(__arm__) || defined(PICO_BOARD)
            ram_save_level = search_level;
#else
            ram_save_level = search_depth;
#endif
            ram_save_valid = true;
            printf("Position and settings saved to RAM quicksave slot.\n");

#if !defined(__arm__) && !defined(PICO_BOARD)
            FILE *f = fopen("lugalchess.save", "w");
            if (f) {
                char fen_buf[256];
                generate_fen(&pos, fen_buf);
                fprintf(f, "%s\n%d\n", fen_buf, search_depth);
                fclose(f);
                printf("Saved persistently to 'lugalchess.save'.\n");
            } else {
                printf("Error: Could not write save file.\n");
            }
#else
            SaveData data;
            data.magic = 0xDECADE02;
            generate_fen(&pos, data.fen);
            data.level = search_level;
            
            uint32_t ints = save_and_disable_interrupts();
            flash_range_erase(FLASH_SAVE_OFFSET, FLASH_SECTOR_SIZE);
            flash_range_program(FLASH_SAVE_OFFSET, (const uint8_t*)&data, 256);
            restore_interrupts(ints);
            printf("Saved persistently to QSPI flash.\n");
#endif
        }
        else if (strcmp(line, "load") == 0) {
            bool loaded = false;
#if !defined(__arm__) && !defined(PICO_BOARD)
            FILE *f = fopen("lugalchess.save", "r");
            if (f) {
                char fen_buf[256];
                int level_val;
                if (fgets(fen_buf, sizeof(fen_buf), f)) {
                    fen_buf[strcspn(fen_buf, "\r\n")] = '\0';
                    parse_fen(&pos, fen_buf);
                    max_history_ply = pos.history_ply;
                    if (fscanf(f, "%d", &level_val) == 1) {
                        search_depth = level_val;
                    }
                    printf("Game loaded from 'lugalchess.save'. Level set to %d.\n", search_depth);
                    print_board(&pos);
                    loaded = true;
                }
                fclose(f);
            }
#else
            const SaveData *flash_data = (const SaveData *)(0x10000000 + FLASH_SAVE_OFFSET);
            if (flash_data->magic == 0xDECADE02) {
                parse_fen(&pos, flash_data->fen);
                max_history_ply = pos.history_ply;
                search_level = flash_data->level;
                if (search_level >= 1 && search_level <= 8) {
                    search_depth = level_depths[search_level - 1];
                } else {
                    search_level = 2;
                    search_depth = level_depths[search_level - 1];
                }
                sync_moves_from_history(&pos);
                if (!check_and_display_game_over(&pos)) {
                    check_and_update_check_status(&pos);
                }
                printf("Game loaded persistently from QSPI flash. Level set to %d.\n", search_level);
                print_board(&pos);
                loaded = true;
            }
#endif
            if (!loaded) {
                if (ram_save_valid) {
                    pos = ram_save_pos;
                    max_history_ply = pos.history_ply;
#if defined(__arm__) || defined(PICO_BOARD)
                    search_level = ram_save_level;
                    search_depth = level_depths[search_level - 1];
                    sync_moves_from_history(&pos);
                    if (!check_and_display_game_over(&pos)) {
                        check_and_update_check_status(&pos);
                    }
                    printf("Game loaded from RAM quicksave slot. Level set to %d.\n", search_level);
#else
                    search_depth = ram_save_level;
                    printf("Game loaded from RAM quicksave slot. Level set to %d.\n", search_depth);
#endif
                    print_board(&pos);
                } else {
                    printf("No saved game found.\n");
                }
            }
        } 
        else if (strcmp(line, "go") == 0) {
            make_engine_move(&pos);
            print_board(&pos);
            if (!check_and_display_game_over(&pos)) {
                check_and_update_check_status(&pos);
            }
        } 
        else if (strcmp(line, "undo") == 0) {
            if (pos.history_ply > 0) {
                unmake_move(&pos);
                printf("Took back 1 half-move.\n");
                print_board(&pos);
            } else {
                printf("Nothing to undo.\n");
            }
#if defined(__arm__) || defined(PICO_BOARD)
            sync_moves_from_history(&pos);
            if (!check_and_display_game_over(&pos)) {
                check_and_update_check_status(&pos);
            }
#endif
        } 
        else if (strcmp(line, "redo") == 0) {
            if (pos.history_ply < max_history_ply) {
                Move move_to_redo = pos.history[pos.history_ply].move;
                if (make_move(&pos, move_to_redo)) {
                    printf("Re-applied 1 half-move.\n");
                    print_board(&pos);
                } else {
                    printf("Cannot redo move.\n");
                }
            } else {
                printf("Nothing to redo.\n");
            }
#if defined(__arm__) || defined(PICO_BOARD)
            sync_moves_from_history(&pos);
            if (!check_and_display_game_over(&pos)) {
                check_and_update_check_status(&pos);
            }
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
                        const char promo_chars[] = "pnbrqk";
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
                strncpy(last_player_move, line, 5);
                last_player_move[5] = '\0';
                strcpy(last_engine_move, "     ");
                update_tm1638_display();
#endif
                print_board(&pos);

                // Check if game is over
                if (!check_and_display_game_over(&pos)) {
                    // Trigger engine move
                    make_engine_move(&pos);
                    print_board(&pos);
                    // Check if engine's move ended the game
                    if (!check_and_display_game_over(&pos)) {
                        check_and_update_check_status(&pos);
                    }
                }
            } else {
                printf("Unknown command or invalid move: '%s'. Type 'help' for instructions.\n", line);
            }
        }
    }

    free_tt();
}
