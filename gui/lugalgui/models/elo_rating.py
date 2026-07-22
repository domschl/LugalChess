"""Bradley-Terry Maximum Likelihood ELO Rating Calculator for LugalChess GUI."""

import math
from dataclasses import dataclass, field


@dataclass
class EngineEloStats:
    """Dataclass storing ELO rating, error margin, and W/D/L record for an engine."""

    name: str
    elo: float = 1500.0
    error: float = 0.0
    wins: int = 0
    draws: int = 0
    losses: int = 0

    @property
    def games_played(self) -> int:
        return self.wins + self.draws + self.losses

    @property
    def points(self) -> float:
        return self.wins * 1.0 + self.draws * 0.5

    @property
    def score_percentage(self) -> float:
        if self.games_played == 0:
            return 0.0
        return (self.points / self.games_played) * 100.0


class EloRatingCalculator:
    """Computes ELO ratings and confidence intervals using Bradley-Terry Maximum Likelihood Estimation."""

    @staticmethod
    def calculate_ratings(
        engine_names: list[str],
        match_results: list[tuple[str, str, float]],
        initial_elos: dict[str, float] | None = None,
        base_elo: float = 1500.0,
        max_iterations: int = 100,
        tolerance: float = 1e-4,
    ) -> dict[str, EngineEloStats]:
        """Compute relative ELO ratings from a list of match results (white_name, black_name, result).
        
        initial_elos can optionally map engine names to assumed benchmark ELO values (e.g. Stockfish 1800 -> 1800.0).
        """
        initial_map = initial_elos or {}
        stats: dict[str, EngineEloStats] = {
            name: EngineEloStats(name=name, elo=initial_map.get(name, base_elo)) for name in engine_names
        }

        # 1. Accumulate W/D/L records
        for white, black, score in match_results:
            if white not in stats:
                stats[white] = EngineEloStats(name=white, elo=initial_map.get(white, base_elo))
            if black not in stats:
                stats[black] = EngineEloStats(name=black, elo=initial_map.get(black, base_elo))

            if score == 1.0:
                stats[white].wins += 1
                stats[black].losses += 1
            elif score == 0.5:
                stats[white].draws += 1
                stats[black].draws += 1
            elif score == 0.0:
                stats[white].losses += 1
                stats[black].wins += 1

        active_engines = [name for name, s in stats.items() if s.games_played > 0]
        if not active_engines or len(active_engines) < 2:
            return stats

        # Initialize solver ratings with initial seed ELOs
        ratings = {name: initial_map.get(name, base_elo) for name in active_engines}
        anchored_engines = [n for n in active_engines if n in initial_map]

        # 2. Bradley-Terry Iterative Newton-Raphson Solver
        for _ in range(max_iterations):
            max_delta = 0.0
            new_ratings = dict(ratings)

            for name_i in active_engines:
                s_i = stats[name_i]
                actual_score = s_i.points
                expected_score = 0.0
                second_derivative = 0.0

                for white, black, score in match_results:
                    if white == name_i:
                        name_j = black
                    elif black == name_i:
                        name_j = white
                    else:
                        continue

                    r_i = ratings[name_i]
                    r_j = ratings[name_j]
                    p_ij = 1.0 / (1.0 + math.pow(10.0, (r_j - r_i) / 400.0))
                    expected_score += p_ij
                    second_derivative += p_ij * (1.0 - p_ij) * (math.log(10.0) / 400.0)

                if second_derivative > 1e-6:
                    delta = (actual_score - expected_score) / (second_derivative * (math.log(10.0) / 400.0) + 1e-6)
                    delta = max(-100.0, min(100.0, delta))
                    new_ratings[name_i] += delta
                    max_delta = max(max_delta, abs(delta))

            ratings = new_ratings

            # Normalize ratings relative to benchmark initial ELOs if present
            if anchored_engines:
                avg_assumed = sum(initial_map[n] for n in anchored_engines) / len(anchored_engines)
                avg_current = sum(ratings[n] for n in anchored_engines) / len(anchored_engines)
                offset = avg_assumed - avg_current
                for k in ratings:
                    ratings[k] += offset
            else:
                avg = sum(ratings.values()) / len(ratings)
                offset = base_elo - avg
                for k in ratings:
                    ratings[k] += offset

            if max_delta < tolerance:
                break

        # 3. Calculate 95% Confidence Interval Error Margins (± ELO)
        for name in active_engines:
            s_i = stats[name]
            ratings[name] = round(ratings[name], 1)
            stats[name].elo = ratings[name]

            # Approximate variance: sigma = 400 / (ln(10) * sqrt(N * P * (1-P)))
            n = s_i.games_played
            if n > 0:
                win_rate = max(0.01, min(0.99, s_i.score_percentage / 100.0))
                denom = (math.log(10.0) / 400.0) * math.sqrt(n * win_rate * (1.0 - win_rate))
                if denom > 1e-5:
                    stats[name].error = round(1.96 / denom, 1)

        return stats
