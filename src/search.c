#include "search.h"
#include "defs.h"
#include "bitboard.h"
#include "movegen.h"
#include "move.h"
#include "evaluation.h"
#include "tt.h"
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Global search variables
int max_search_depth = 64;
long max_search_time_ms = -1;
long start_search_time_ms = 0;
bool stop_search = false;
long nodes_searched = 0;

// Move ordering heuristic tables
static int history_table[6][64]; // [moved_piece][destination]
static Move killer_moves[2][MAX_PLYS]; // [killer_index][ply]

// Approximate piece values for MVV-LVA sorting
static const int sorting_values[6] = { 100, 300, 300, 500, 900, 10000 };

static const char *book_lines[] = {
    // 1. e4 lines
    "e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 e1g1 f8e7", // Ruy Lopez
    "e2e4 e7e5 g1f3 b8c6 f1c4 f8c5 c2c3 g8f6 d2d4", // Italian
    "e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 a7a6", // Sicilian Najdorf
    "e2e4 c7c5 g1f3 e7e6 d2d4 c5d4 f3d4 b8c6 b1c3 d7d6", // Sicilian Taimanov
    "e2e4 c7c5 c2c3 d7d5 e4d5 d8d5 d2d4 g8f6", // Sicilian Alapin
    "e2e4 e7e6 d2d4 d7d5 b1c3 g8f6 c1g5 f8e7", // French Classical
    "e2e4 c7c6 d2d4 d7d5 b1c3 d5e4 c3e4 c8f5", // Caro-Kann
    "e2e4 d7d6 d2d4 g8f6 b1c3 g7g6 f2f4 f8g7", // Pirc
    // 2. d4 lines
    "d2d4 d7d5 c2c4 e7e6 b1c3 g8f6 c1g5 f8e7", // QGD
    "d2d4 d7d5 c2c4 c7c6 g1f3 g8f6 b1c3 e7e6", // Slav
    "d2d4 g8f6 c2c4 g7g6 b1c3 f8g7 e4e5 d6d6", // King's Indian
    "d2d4 g8f6 c2c4 e7e6 g1f3 b7b6 g2g3 c8b7", // Queen's Indian
    "d2d4 g8f6 c2c4 e7e6 b1c3 f8b4 e2e3 e1g1", // Nimzo-Indian
    // 3. Flank openings
    "g1f3 d7d5 g2g3 g8f6 f1g2 c7c6 e1g1 c8f5", // KIA
    "c2c4 e7e5 b1c3 g8f6 g1f3 b8c6 g2g3 f8b4", // English
    "f2f4 d7d5 g1f3 g8f6 e2e3 c7c5 f1e2 b8c6" // Bird's
};

static Move get_book_move(Position *pos) {
    char history_str[512] = "";
    int offset = 0;
    for (int i = 0; i < pos->history_ply; i++) {
        Move m = pos->history[i].move;
        int from = MOVE_FROM(m);
        int to = MOVE_TO(m);
        char m_str[6];
        m_str[0] = 'a' + (from % 8);
        m_str[1] = '1' + (from / 8);
        m_str[2] = 'a' + (to % 8);
        m_str[3] = '1' + (to / 8);
        m_str[4] = '\0';
        if (move_is_promo(m)) {
            int promo = move_promo_piece(m);
            const char promo_chars[] = "pnbrqk";
            m_str[4] = promo_chars[promo];
            m_str[5] = '\0';
        }
        
        int len = snprintf(history_str + offset, sizeof(history_str) - offset, "%s%s", i > 0 ? " " : "", m_str);
        if (len < 0 || offset + len >= (int)sizeof(history_str)) {
            return 0;
        }
        offset += len;
    }

    const char *candidates[64];
    int candidate_count = 0;
    int history_len = strlen(history_str);

    int book_size = sizeof(book_lines) / sizeof(book_lines[0]);
    for (int i = 0; i < book_size; i++) {
        const char *line = book_lines[i];
        if (history_len == 0) {
            candidates[candidate_count++] = line;
        } else {
            if (strncmp(line, history_str, history_len) == 0 && (line[history_len] == ' ' || line[history_len] == '\0')) {
                if (line[history_len] == ' ') {
                    candidates[candidate_count++] = line + history_len + 1;
                }
            }
        }
        if (candidate_count >= 64) break;
    }

    if (candidate_count == 0) {
        return 0;
    }

    char next_move_strs[64][6];
    int unique_count = 0;
    for (int i = 0; i < candidate_count; i++) {
        const char *cand = candidates[i];
        char next_m[6];
        int c_idx = 0;
        while (cand[c_idx] != ' ' && cand[c_idx] != '\0' && c_idx < 5) {
            next_m[c_idx] = cand[c_idx];
            c_idx++;
        }
        next_m[c_idx] = '\0';

        bool is_unique = true;
        for (int j = 0; j < unique_count; j++) {
            if (strcmp(next_move_strs[j], next_m) == 0) {
                is_unique = false;
                break;
            }
        }
        if (is_unique) {
            strcpy(next_move_strs[unique_count++], next_m);
        }
    }

    if (unique_count == 0) return 0;

    int choice = rand() % unique_count;
    const char *chosen_move_str = next_move_strs[choice];

    MoveList list;
    generate_moves(pos, &list);
    for (int i = 0; i < list.count; i++) {
        Move m = list.moves[i];
        if (make_move(pos, m)) {
            unmake_move(pos);
            int from = MOVE_FROM(m);
            int to = MOVE_TO(m);
            char m_str[6];
            m_str[0] = 'a' + (from % 8);
            m_str[1] = '1' + (from / 8);
            m_str[2] = 'a' + (to % 8);
            m_str[3] = '1' + (to / 8);
            m_str[4] = '\0';
            if (move_is_promo(m)) {
                int promo = move_promo_piece(m);
                const char promo_chars[] = "pnbrqk";
                m_str[4] = promo_chars[promo];
                m_str[5] = '\0';
            }

            if (strcmp(m_str, chosen_move_str) == 0) {
                return m;
            }
        }
    }

    return 0;
}

// Time check helper
static long get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void check_up_time(void) {
    if ((nodes_searched & 2047) == 0) {
        extern void search_poll_stop_callback(void);
        search_poll_stop_callback();
        if (max_search_time_ms != -1) {
            if (get_time_ms() - start_search_time_ms >= max_search_time_ms) {
                stop_search = true;
            }
        }
    }
}

// Convert mate scores to/from transposition table representation
static inline int score_to_tt(int score, int ply) {
    if (score > MATE_VALUE) return score + ply;
    if (score < -MATE_VALUE) return score - ply;
    return score;
}

static inline int score_from_tt(int score, int ply) {
    if (score > MATE_VALUE) return score - ply;
    if (score < -MATE_VALUE) return score + ply;
    return score;
}

// MVV-LVA capture scoring helper
static int score_capture(const Position *pos, Move move) {
    int from = MOVE_FROM(move);
    int to = MOVE_TO(move);
    int attacker = pos->board[from];
    int victim;
    
    if (MOVE_FLAGS(move) == MOVE_FLAG_EN_PASSANT) {
        victim = PAWN;
    } else {
        victim = pos->board[to];
    }
    
    // Most valuable victim, least valuable attacker
    return 1000000 + (sorting_values[victim] * 10) - sorting_values[attacker];
}

// Score a move for ordering
static int score_move(const Position *pos, Move move, Move tt_move, int ply) {
    if (move == tt_move) {
        return 2000000; // Search best hash move first
    }
    
    if (move_is_capture(move)) {
        return score_capture(pos, move);
    }
    
    if (move_is_promo(move)) {
        int promo = move_promo_piece(move);
        return 900000 + sorting_values[promo];
    }
    
    // Killer moves
    if (killer_moves[0][ply] == move) return 800000;
    if (killer_moves[1][ply] == move) return 700000;
    
    // History heuristic
    int piece = pos->board[MOVE_FROM(move)];
    int to = MOVE_TO(move);
    return history_table[piece][to];
}

// Insertion sort for move list (fast for small arrays)
static void sort_moves(const Position *pos, MoveList *list, Move tt_move, int ply) {
    int scores[MAX_MOVES];
    for (int i = 0; i < list->count; i++) {
        scores[i] = score_move(pos, list->moves[i], tt_move, ply);
    }
    
    for (int i = 1; i < list->count; i++) {
        Move temp_m = list->moves[i];
        int temp_s = scores[i];
        int j = i - 1;
        while (j >= 0 && scores[j] < temp_s) {
            list->moves[j + 1] = list->moves[j];
            scores[j + 1] = scores[j];
            j--;
        }
        list->moves[j + 1] = temp_m;
        scores[j + 1] = temp_s;
    }
}

// Quiescence Search (tactical search only)
int quiescence(Position *pos, int alpha, int beta) {
    nodes_searched++;
    check_up_time();
    if (stop_search) return 0;
    
    // Safety guard to prevent stack overflow/out-of-bounds history plies
    if (pos->history_ply >= MAX_PLYS - 1) {
        return evaluate(pos);
    }
    
    // Standing pat score (assume we can stand pat and do no more moves)
    int stand_pat = evaluate(pos);
    if (stand_pat >= beta) {
        return beta;
    }
    if (stand_pat > alpha) {
        alpha = stand_pat;
    }
    
    MoveList list;
    generate_captures(pos, &list);
    sort_moves(pos, &list, 0, 0);
    
    for (int i = 0; i < list.count; i++) {
        if (!make_move(pos, list.moves[i])) {
            continue;
        }
        int score = -quiescence(pos, -beta, -alpha);
        unmake_move(pos);
        
        if (stop_search) return 0;
        
        if (score >= beta) {
            return beta;
        }
        if (score > alpha) {
            alpha = score;
        }
    }
    
    return alpha;
}

// Principal Variation Search (PVS) with Pruning
int pv_search(Position *pos, int depth, int ply, int alpha, int beta, bool null_move_allowed) {
    nodes_searched++;
    check_up_time();
    if (stop_search) return 0;

    // Safety guard to prevent stack overflow/out-of-bounds history plies
    if (ply >= MAX_PLYS - 1 || pos->history_ply >= MAX_PLYS - 1) {
        return evaluate(pos);
    }

    // Draw by repetition or 50-move rule
    if (ply > 0) {
        // Simple repetition check (check if current position hash matches any parent hash)
        for (int i = pos->history_ply - 2; i >= pos->history_ply - pos->halfmove; i -= 2) {
            if (i >= 0 && pos->history[i].hash_key == pos->hash_key) {
                return 0; // Draw score
            }
        }
        if (pos->halfmove >= 100) return 0; // 50-move rule draw
    }

    // Mate distance pruning
    int mate_val = INFINITY_VALUE - ply;
    if (alpha >= mate_val) return alpha;
    if (beta <= -mate_val) return beta;

    // TT Lookup
    Move tt_move = 0;
    int tt_score = 0;
    if (read_tt(pos->hash_key, depth, alpha, beta, &tt_score, &tt_move)) {
        return score_from_tt(tt_score, ply);
    }

    // Leaf nodes
    if (depth <= 0) {
        return quiescence(pos, alpha, beta);
    }

    int in_check = is_square_attacked(pos, get_lsb(pos->piece_bbs[KING] & pos->color_bbs[pos->side]), pos->side ^ 1);
    if (in_check) {
        depth++; // Check extension
    }

    // Null Move Pruning (NMP)
    if (null_move_allowed && !in_check && depth >= 3) {
        // Verify we have major pieces left (avoid zugzwang)
        uint64_t majors = pos->piece_bbs[KNIGHT] | pos->piece_bbs[BISHOP] | pos->piece_bbs[ROOK] | pos->piece_bbs[QUEEN];
        if (majors & pos->color_bbs[pos->side]) {
            make_null_move(pos);
            int score = -pv_search(pos, depth - 1 - 2, ply + 1, -beta, -beta + 1, false);
            unmake_null_move(pos);
            
            if (stop_search) return 0;
            if (score >= beta) {
                return beta; // Prune!
            }
        }
    }

    // Move generation
    MoveList list;
    generate_moves(pos, &list);
    sort_moves(pos, &list, tt_move, ply);

    int legal_moves = 0;
    int best_score = -INFINITY_VALUE;
    Move best_move = 0;
    uint8_t tt_flag = TT_ALPHA;

    for (int i = 0; i < list.count; i++) {
        Move move = list.moves[i];
        if (!make_move(pos, move)) {
            continue;
        }
        legal_moves++;

        int score;
        if (legal_moves == 1) {
            // PV move: search with full window
            score = -pv_search(pos, depth - 1, ply + 1, -beta, -alpha, true);
        } else {
            // Quiet moves LMR (Late Move Reduction)
            if (depth >= 3 && !in_check && !move_is_capture(move) && !move_is_promo(move) && legal_moves > 4) {
                // Reduce depth
                score = -pv_search(pos, depth - 2, ply + 1, -alpha - 1, -alpha, true);
                if (score > alpha) {
                    // Re-search at full depth with null window
                    score = -pv_search(pos, depth - 1, ply + 1, -alpha - 1, -alpha, true);
                }
            } else {
                // Regular null window search
                score = -pv_search(pos, depth - 1, ply + 1, -alpha - 1, -alpha, true);
            }

            // Re-search with full window if fail-high
            if (score > alpha && score < beta) {
                score = -pv_search(pos, depth - 1, ply + 1, -beta, -alpha, true);
            }
        }

        unmake_move(pos);
        if (stop_search) return 0;

        if (score > best_score) {
            best_score = score;
            best_move = move;
            if (ply == 0) {
                extern void search_progress_callback(Move move, int score, int depth);
                search_progress_callback(best_move, best_score, depth);
            }
        }

        if (score >= beta) {
            // Store killer moves and history heuristic for quiet moves
            if (!move_is_capture(move) && !move_is_promo(move)) {
                killer_moves[1][ply] = killer_moves[0][ply];
                killer_moves[0][ply] = move;
                
                int piece = pos->board[MOVE_FROM(move)];
                int to = MOVE_TO(move);
                history_table[piece][to] += depth * depth;
                if (history_table[piece][to] > 400000) {
                    // Scale down to prevent overflow
                    for (int p = 0; p < 6; p++) {
                        for (int sq = 0; sq < 64; sq++) {
                            history_table[p][sq] /= 2;
                        }
                    }
                }
            }
            write_tt(pos->hash_key, move, score_to_tt(beta, ply), depth, TT_BETA);
            return beta;
        }

        if (score > alpha) {
            alpha = score;
            tt_flag = TT_EXACT;
        }
    }

    // Checkmate/Stalemate detection
    if (legal_moves == 0) {
        if (in_check) {
            return -INFINITY_VALUE + ply; // Mate in ply
        } else {
            return 0; // Stalemate
        }
    }

    write_tt(pos->hash_key, best_move, score_to_tt(best_score, ply), depth, tt_flag);
    return best_score;
}

// Iterative deepening entry point
void search_position(Position *pos, int depth, int time_limit_ms) {
    // Check if there is an opening book move
    Move book_move = get_book_move(pos);
    if (book_move != 0) {
        int from = MOVE_FROM(book_move);
        int to = MOVE_TO(book_move);
        printf("bestmove %c%d%c%d", 'a' + (from % 8), (from / 8) + 1, 'a' + (to % 8), (to / 8) + 1);
        if (move_is_promo(book_move)) {
            int promo = move_promo_piece(book_move);
            const char promo_chars[] = "pnbrqk";
            printf("%c", promo_chars[promo]);
        }
        printf("\n");
        fflush(stdout);

        // Store in TT so that caller (e.g. make_engine_move) can retrieve it
        write_tt(pos->hash_key, book_move, 0, depth, TT_EXACT);
        return;
    }

    // Dynamically adjust search depth in endgame when there are fewer pieces
    int piece_count = count_bits(pos->color_bbs[WHITE] | pos->color_bbs[BLACK]);
    if (piece_count <= 6) {
        depth += 6;
    } else if (piece_count <= 10) {
        depth += 4;
    } else if (piece_count <= 16) {
        depth += 2;
    }

    start_search_time_ms = get_time_ms();
    max_search_time_ms = time_limit_ms;
    stop_search = false;
    nodes_searched = 0;
    increment_tt_age();

    // Reset move ordering heuristics
    memset(killer_moves, 0, sizeof(killer_moves));
    
    Move best_move = 0;
    int best_score = -INFINITY_VALUE;

    // Iterative Deepening
    for (int d = 1; d <= depth; d++) {
        extern void search_progress_callback(Move move, int score, int depth);
        search_progress_callback(0, 0, d);
        int score = pv_search(pos, d, 0, -INFINITY_VALUE, INFINITY_VALUE, true);
        
        if (stop_search) {
            break;
        }

        // Retrieve best move from Transposition Table
        Move temp_move = 0;
        int temp_score;
        read_tt(pos->hash_key, d, -INFINITY_VALUE, INFINITY_VALUE, &temp_score, &temp_move);
        if (temp_move != 0) {
            best_move = temp_move;
            best_score = score;
        }

        long time_spent = get_time_ms() - start_search_time_ms;
        double nps = time_spent > 0 ? (double)nodes_searched / ((double)time_spent / 1000.0) : 0.0;

        // Print UCI info block
        printf("info depth %d score cp %d nodes %ld nps %.0f time %ld pv ", 
               d, best_score, nodes_searched, nps, time_spent);
        
        // Print Principal Variation (PV) path
        Position temp_pos = *pos;
        int pv_ply = 0;
        Move pv_move = best_move;
        
        while (pv_move != 0 && pv_ply < d) {
            int from = MOVE_FROM(pv_move);
            int to = MOVE_TO(pv_move);
            printf("%c%d%c%d", 'a' + (from % 8), (from / 8) + 1, 'a' + (to % 8), (to / 8) + 1);
            if (move_is_promo(pv_move)) {
                int promo = move_promo_piece(pv_move);
                const char promo_chars[] = "pnbrqk";
                printf("%c", promo_chars[promo]);
            }
            printf(" ");
            
            if (!make_move(&temp_pos, pv_move)) break;
            pv_ply++;
            
            // Look up next move in PV
            int dummy_score;
            read_tt(temp_pos.hash_key, d - pv_ply, -INFINITY_VALUE, INFINITY_VALUE, &dummy_score, &pv_move);
        }
        printf("\n");
        fflush(stdout);

        // Terminate search early if we have run out of 50% of the allocated search time window
        if (max_search_time_ms != -1 && time_spent > max_search_time_ms / 2) {
            break;
        }
    }

    // Output UCI bestmove
    int from = MOVE_FROM(best_move);
    int to = MOVE_TO(best_move);
    printf("bestmove %c%d%c%d", 'a' + (from % 8), (from / 8) + 1, 'a' + (to % 8), (to / 8) + 1);
    if (move_is_promo(best_move)) {
        int promo = move_promo_piece(best_move);
        const char promo_chars[] = "pnbrqk";
        printf("%c", promo_chars[promo]);
    }
    printf("\n");
    fflush(stdout);
    stop_search = false;
}
