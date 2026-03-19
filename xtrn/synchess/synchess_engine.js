"use strict";

// ── Built-in alpha-beta chess engine for SynChess ────────────────────────────
// Plugs in as a drop-in replacement for Stockfish via getMove(depth).
// Uses ChessGame (synchess_logic.js) for move generation / make / unmake.

// ── Material values (centipawns) ──────────────────────────────────────────────
var CE_VAL = { 'P':100, 'N':320, 'B':330, 'R':500, 'Q':900, 'K':20000 };

// ── Piece-square tables (white perspective, row 0 = rank 8, row 7 = rank 1) ──
var CE_PST = {
    'P': [
        [  0,  0,  0,  0,  0,  0,  0,  0],
        [ 50, 50, 50, 50, 50, 50, 50, 50],
        [ 10, 10, 20, 30, 30, 20, 10, 10],
        [  5,  5, 10, 25, 25, 10,  5,  5],
        [  0,  0,  0, 20, 20,  0,  0,  0],
        [  5, -5,-10,  0,  0,-10, -5,  5],
        [  5, 10, 10,-20,-20, 10, 10,  5],
        [  0,  0,  0,  0,  0,  0,  0,  0]
    ],
    'N': [
        [-50,-40,-30,-30,-30,-30,-40,-50],
        [-40,-20,  0,  0,  0,  0,-20,-40],
        [-30,  0, 10, 15, 15, 10,  0,-30],
        [-30,  5, 15, 20, 20, 15,  5,-30],
        [-30,  0, 15, 20, 20, 15,  0,-30],
        [-30,  5, 10, 15, 15, 10,  5,-30],
        [-40,-20,  0,  5,  5,  0,-20,-40],
        [-50,-40,-30,-30,-30,-30,-40,-50]
    ],
    'B': [
        [-20,-10,-10,-10,-10,-10,-10,-20],
        [-10,  0,  0,  0,  0,  0,  0,-10],
        [-10,  0,  5, 10, 10,  5,  0,-10],
        [-10,  5,  5, 10, 10,  5,  5,-10],
        [-10,  0, 10, 10, 10, 10,  0,-10],
        [-10, 10, 10, 10, 10, 10, 10,-10],
        [-10,  5,  0,  0,  0,  0,  5,-10],
        [-20,-10,-10,-10,-10,-10,-10,-20]
    ],
    'R': [
        [  0,  0,  0,  0,  0,  0,  0,  0],
        [  5, 10, 10, 10, 10, 10, 10,  5],
        [ -5,  0,  0,  0,  0,  0,  0, -5],
        [ -5,  0,  0,  0,  0,  0,  0, -5],
        [ -5,  0,  0,  0,  0,  0,  0, -5],
        [ -5,  0,  0,  0,  0,  0,  0, -5],
        [ -5,  0,  0,  0,  0,  0,  0, -5],
        [  0,  0,  0,  5,  5,  0,  0,  0]
    ],
    'Q': [
        [-20,-10,-10, -5, -5,-10,-10,-20],
        [-10,  0,  0,  0,  0,  0,  0,-10],
        [-10,  0,  5,  5,  5,  5,  0,-10],
        [ -5,  0,  5,  5,  5,  5,  0, -5],
        [  0,  0,  5,  5,  5,  5,  0, -5],
        [-10,  5,  5,  5,  5,  5,  0,-10],
        [-10,  0,  5,  0,  0,  0,  0,-10],
        [-20,-10,-10, -5, -5,-10,-10,-20]
    ],
    // King — middlegame: shelter behind pawns; endgame: centralise
    'K': [
        [-30,-40,-40,-50,-50,-40,-40,-30],
        [-30,-40,-40,-50,-50,-40,-40,-30],
        [-30,-40,-40,-50,-50,-40,-40,-30],
        [-30,-40,-40,-50,-50,-40,-40,-30],
        [-20,-30,-30,-40,-40,-30,-30,-20],
        [-10,-20,-20,-20,-20,-20,-20,-10],
        [ 20, 20,  0,  0,  0,  0, 20, 20],
        [ 20, 30, 10,  0,  0, 10, 30, 20]
    ]
};

var CE_KING_EG = [
    [-50,-40,-30,-20,-20,-30,-40,-50],
    [-30,-20,-10,  0,  0,-10,-20,-30],
    [-30,-10, 20, 30, 30, 20,-10,-30],
    [-30,-10, 30, 40, 40, 30,-10,-30],
    [-30,-10, 30, 40, 40, 30,-10,-30],
    [-30,-10, 20, 30, 30, 20,-10,-30],
    [-30,-30,  0,  0,  0,  0,-30,-30],
    [-50,-30,-30,-30,-30,-30,-30,-50]
];

// ── Engine object ─────────────────────────────────────────────────────────────
function ChessEngine(gameObj) {
    this.g = gameObj;
}

ChessEngine.prototype.isEndgame = function() {
    var q = 0, m = 0;
    for (var r = 0; r < 8; r++)
        for (var c = 0; c < 8; c++) {
            var t = this.g.board[r][c].toUpperCase();
            if (t === 'Q') q++;
            if (t === 'N' || t === 'B') m++;
        }
    return q === 0 || (q <= 1 && m <= 1);
};

// Static evaluation — positive = good for side to move
ChessEngine.prototype.evaluate = function() {
    var g = this.g, score = 0, eg = this.isEndgame();
    for (var r = 0; r < 8; r++) {
        for (var c = 0; c < 8; c++) {
            var p = g.board[r][c];
            if (p === ' ') continue;
            var isW = (p === p.toUpperCase());
            var t   = p.toUpperCase();
            var pst = (t === 'K' && eg) ? CE_KING_EG : CE_PST[t];
            var tb  = isW ? pst[r][c] : pst[7 - r][c];
            if (isW) score += CE_VAL[t] + tb;
            else     score -= CE_VAL[t] + tb;
        }
    }
    return (g.turn === 'w') ? score : -score;
};

// Generate moves for the side to move, sorted captures-first by MVV-LVA
ChessEngine.prototype.getMoves = function(capturesOnly) {
    var g = this.g, moves = [];
    for (var r = 0; r < 8; r++) {
        for (var c = 0; c < 8; c++) {
            var p = g.board[r][c];
            if (p === ' ') continue;
            if ((p === p.toUpperCase()) !== (g.turn === 'w')) continue;
            var pt      = p.toUpperCase();
            var targets = g.getLegalMovesFrom(r, c);
            for (var ti = 0; ti < targets.length; ti++) {
                var tg  = targets[ti];
                var cap = g.board[tg.r][tg.c];
                var isC = cap !== ' ';
                if (capturesOnly && !isC) continue;
                var promo = (pt === 'P' && (tg.r === 0 || tg.r === 7)) ? 'Q' : null;
                moves.push({
                    f:        { r: r, c: c },
                    t:        tg,
                    promoteTo: promo,
                    score:    isC ? (CE_VAL[cap.toUpperCase()] * 10 - CE_VAL[pt]) : 0
                });
            }
        }
    }
    moves.sort(function(a, b) { return b.score - a.score; });
    return moves;
};

// Quiescence search — only captures, prevents horizon blunders
ChessEngine.prototype.quiesce = function(alpha, beta) {
    var stand = this.evaluate();
    if (stand >= beta) return beta;
    if (stand > alpha) alpha = stand;
    var caps = this.getMoves(true);
    for (var i = 0; i < caps.length; i++) {
        var m = caps[i];
        this.g.applyMove(m.f, m.t, m.promoteTo);
        var sc = -this.quiesce(-beta, -alpha);
        this.g.undoMove();
        if (sc >= beta) return beta;
        if (sc > alpha) alpha = sc;
    }
    return alpha;
};

// Alpha-beta negamax
ChessEngine.prototype.search = function(depth, alpha, beta) {
    if (depth === 0) return this.quiesce(alpha, beta);
    var g = this.g;

    // Check time limit
    if (this.deadline && (new Date().getTime()) > this.deadline) {
        this.timeUp = true;
        return alpha;
    }

    var moves = this.getMoves(false);
    if (moves.length === 0)
        return g.isInCheck(g.turn) ? (-19000 - depth) : 0;

    for (var i = 0; i < moves.length; i++) {
        if (this.timeUp) break;
        var m = moves[i];
        g.applyMove(m.f, m.t, m.promoteTo);
        var sc = -this.search(depth - 1, -beta, -alpha);
        g.undoMove();
        if (sc > alpha) alpha = sc;
        if (alpha >= beta) break;
    }
    return alpha;
};

// Public entry point — iterative deepening up to maxDepth or timeLimitMs
// Returns {f, t, promoteTo} or null
ChessEngine.prototype.getMove = function(maxDepth, timeLimitMs) {
    this.deadline = timeLimitMs ? (new Date().getTime() + timeLimitMs) : null;
    this.timeUp   = false;

    var best = null;
    for (var d = 1; d <= maxDepth; d++) {
        if (this.timeUp) break;
        var moves = this.getMoves(false);
        if (moves.length === 0) break;
        var alpha = -999999, iterBest = null;
        for (var i = 0; i < moves.length; i++) {
            if (this.timeUp) break;
            var m = moves[i];
            this.g.applyMove(m.f, m.t, m.promoteTo);
            var sc = -this.search(d - 1, -999999, -alpha);
            this.g.undoMove();
            if (sc > alpha || iterBest === null) {
                alpha = sc;
                iterBest = m;
            }
        }
        if (iterBest) best = iterBest;
    }
    if (!best) return null;
    return { f: best.f, t: best.t, promoteTo: best.promoteTo };
};
