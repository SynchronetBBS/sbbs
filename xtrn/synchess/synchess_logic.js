/**
 * synchess_logic.js — Full chess rules engine for SyncChess
 */
"use strict";

this.ChessGame = function(userNum) {
    this.userNum    = userNum || "0";
    this.board      = [];
    this.turn       = 'w';
    this.playerColor = 'w';
    this.level      = 3;
    this.castling   = "KQkq";
    this.enPassant  = "-";
    this.halfMoveClock  = 0;
    this.fullMoveNumber = 1;
    this.history    = [];   // [{alg, color, uci}]
    this.captured   = { w: [], b: [] };
    this.positionHistory = {};  // fenPos -> count, for 3-fold repetition
    this.stateStack = [];       // for undo

    // ------------------------------------------------------------------ //
    this.init = function() {
        this.board = [
            ['r','n','b','q','k','b','n','r'],
            ['p','p','p','p','p','p','p','p'],
            [' ',' ',' ',' ',' ',' ',' ',' '],
            [' ',' ',' ',' ',' ',' ',' ',' '],
            [' ',' ',' ',' ',' ',' ',' ',' '],
            [' ',' ',' ',' ',' ',' ',' ',' '],
            ['P','P','P','P','P','P','P','P'],
            ['R','N','B','Q','K','B','N','R']
        ];
        this.turn           = 'w';
        this.castling       = "KQkq";
        this.enPassant      = "-";
        this.halfMoveClock  = 0;
        this.fullMoveNumber = 1;
        this.history        = [];
        this.captured       = { w: [], b: [] };
        this.positionHistory = {};
        this.stateStack     = [];
        this.recordPosition();
    };

    // ------------------------------------------------------------------ //
    this.toAlgebraic = function(r, c) {
        return String.fromCharCode(97 + c) + (8 - r);
    };
    this.toCoords = function(s) {
        return { r: 8 - parseInt(s[1], 10), c: s.charCodeAt(0) - 97 };
    };

    // ------------------------------------------------------------------ //
    this.getFENPosition = function() {
        // First 4 FEN fields: pieces + active color + castling + en passant
        var fen = "";
        for (var r = 0; r < 8; r++) {
            var empty = 0;
            for (var c = 0; c < 8; c++) {
                var p = this.board[r][c];
                if (p === ' ') { empty++; }
                else { if (empty > 0) { fen += empty; empty = 0; } fen += p; }
            }
            if (empty > 0) fen += empty;
            if (r < 7) fen += "/";
        }
        return fen + " " + this.turn + " " + (this.castling || '-') + " " + this.enPassant;
    };

    this.getFEN = function() {
        return this.getFENPosition() + " " + this.halfMoveClock + " " + this.fullMoveNumber;
    };

    this.recordPosition = function() {
        var pos = this.getFENPosition();
        this.positionHistory[pos] = (this.positionHistory[pos] || 0) + 1;
    };

    // ------------------------------------------------------------------ //
    // Is square (r,c) attacked by any piece of byColor?
    this.isAttacked = function(r, c, byColor) {
        var isW = (byColor === 'w');
        var pawn   = isW ? 'P' : 'p';
        var knight = isW ? 'N' : 'n';
        var bishop = isW ? 'B' : 'b';
        var rook   = isW ? 'R' : 'r';
        var queen  = isW ? 'Q' : 'q';
        var king   = isW ? 'K' : 'k';
        var i, dr, dc, p;

        // Pawns attack diagonally
        var pr = r + (isW ? 1 : -1);
        if (pr >= 0 && pr < 8) {
            if (c > 0 && this.board[pr][c-1] === pawn) return true;
            if (c < 7 && this.board[pr][c+1] === pawn) return true;
        }

        // Knights
        var kd = [[-2,-1],[-2,1],[-1,-2],[-1,2],[1,-2],[1,2],[2,-1],[2,1]];
        for (i = 0; i < kd.length; i++) {
            var kr = r + kd[i][0], kc = c + kd[i][1];
            if (kr >= 0 && kr < 8 && kc >= 0 && kc < 8 && this.board[kr][kc] === knight) return true;
        }

        // Diagonals: bishop + queen
        var dd = [[-1,-1],[-1,1],[1,-1],[1,1]];
        for (i = 0; i < dd.length; i++) {
            dr = r + dd[i][0]; dc = c + dd[i][1];
            while (dr >= 0 && dr < 8 && dc >= 0 && dc < 8) {
                p = this.board[dr][dc];
                if (p !== ' ') { if (p === bishop || p === queen) return true; break; }
                dr += dd[i][0]; dc += dd[i][1];
            }
        }

        // Straights: rook + queen
        var sd = [[-1,0],[1,0],[0,-1],[0,1]];
        for (i = 0; i < sd.length; i++) {
            dr = r + sd[i][0]; dc = c + sd[i][1];
            while (dr >= 0 && dr < 8 && dc >= 0 && dc < 8) {
                p = this.board[dr][dc];
                if (p !== ' ') { if (p === rook || p === queen) return true; break; }
                dr += sd[i][0]; dc += sd[i][1];
            }
        }

        // King
        for (var kr2 = r-1; kr2 <= r+1; kr2++) {
            for (var kc2 = c-1; kc2 <= c+1; kc2++) {
                if (kr2 === r && kc2 === c) continue;
                if (kr2 >= 0 && kr2 < 8 && kc2 >= 0 && kc2 < 8 && this.board[kr2][kc2] === king) return true;
            }
        }
        return false;
    };

    this.isInCheck = function(color) {
        var king = (color === 'w') ? 'K' : 'k';
        var opp  = (color === 'w') ? 'b' : 'w';
        for (var r = 0; r < 8; r++) {
            for (var c = 0; c < 8; c++) {
                if (this.board[r][c] === king) return this.isAttacked(r, c, opp);
            }
        }
        return false;
    };

    // ------------------------------------------------------------------ //
    this.isPathClear = function(fr, fc, tr, tc) {
        var rS = tr > fr ? 1 : (tr < fr ? -1 : 0);
        var cS = tc > fc ? 1 : (tc < fc ? -1 : 0);
        var r = fr + rS, c = fc + cS;
        while (r !== tr || c !== tc) {
            if (this.board[r][c] !== ' ') return false;
            r += rS; c += cS;
        }
        return true;
    };

    // Pseudo-legal: piece mechanics only, no check detection
    this.isPseudoLegal = function(fr, fc, tr, tc) {
        var piece = this.board[fr][fc];
        if (piece === ' ') return false;
        if (fr === tr && fc === tc) return false;
        var p    = piece.toUpperCase();
        var isW  = (piece === piece.toUpperCase());
        var target = this.board[tr][tc];
        var dr   = Math.abs(tr - fr);
        var dc   = Math.abs(tc - fc);

        // Can't capture own piece
        if (target !== ' ') {
            if (isW === (target === target.toUpperCase())) return false;
        }

        if (p === 'N') return (dr === 2 && dc === 1) || (dr === 1 && dc === 2);
        if (p === 'B') return (dr === dc) && this.isPathClear(fr, fc, tr, tc);
        if (p === 'R') return (fr === tr || fc === tc) && this.isPathClear(fr, fc, tr, tc);
        if (p === 'Q') return (dr === dc || fr === tr || fc === tc) && this.isPathClear(fr, fc, tr, tc);

        if (p === 'K') {
            if (dr <= 1 && dc <= 1) return true;
            // Castling
            if (dr === 0 && dc === 2 && fr === (isW ? 7 : 0) && fc === 4) {
                var kRight = isW ? 'K' : 'k';
                var qRight = isW ? 'Q' : 'q';
                if (tc === 6 && this.castling.indexOf(kRight) !== -1) {
                    return this.board[fr][5] === ' ' && this.board[fr][6] === ' ';
                }
                if (tc === 2 && this.castling.indexOf(qRight) !== -1) {
                    return this.board[fr][1] === ' ' && this.board[fr][2] === ' ' && this.board[fr][3] === ' ';
                }
            }
            return false;
        }

        if (p === 'P') {
            var dir      = isW ? -1 : 1;
            var startRow = isW ? 6 : 1;
            // Forward
            if (fc === tc) {
                if (target !== ' ') return false;
                if (tr - fr === dir) return true;
                if (tr - fr === 2 * dir && fr === startRow && this.board[fr + dir][fc] === ' ') return true;
            }
            // Diagonal capture (normal or en passant)
            if (dr === 1 && dc === 1 && (tr - fr) === dir) {
                if (target !== ' ') return true;
                if (this.toAlgebraic(tr, tc) === this.enPassant) return true;
            }
        }
        return false;
    };

    // Full legal: pseudo-legal + doesn't leave own king in check
    this.isLegalMove = function(fr, fc, tr, tc) {
        if (!this.isPseudoLegal(fr, fc, tr, tc)) return false;
        var piece = this.board[fr][fc];
        var isW   = (piece === piece.toUpperCase());
        var color = isW ? 'w' : 'b';
        var opp   = isW ? 'b' : 'w';

        // Castling: king must not be in check now, or pass through check
        if (piece.toUpperCase() === 'K' && Math.abs(tc - fc) === 2) {
            if (this.isAttacked(fr, fc, opp)) return false;
            if (this.isAttacked(fr, (fc + tc) / 2, opp)) return false;
        }

        // Temporarily apply move
        var savedBoard = [];
        for (var r = 0; r < 8; r++) savedBoard.push(this.board[r].slice());

        this.board[tr][tc] = piece;
        this.board[fr][fc] = ' ';

        // En passant: remove captured pawn
        if (piece.toUpperCase() === 'P' && fc !== tc && savedBoard[tr][tc] === ' ' &&
            this.toAlgebraic(tr, tc) === this.enPassant) {
            this.board[fr][tc] = ' ';
        }

        // Castling: move rook for check detection
        if      (piece === 'K' && fc===4 && tc===6) { this.board[7][5]='R'; this.board[7][7]=' '; }
        else if (piece === 'K' && fc===4 && tc===2) { this.board[7][3]='R'; this.board[7][0]=' '; }
        else if (piece === 'k' && fc===4 && tc===6) { this.board[0][5]='r'; this.board[0][7]=' '; }
        else if (piece === 'k' && fc===4 && tc===2) { this.board[0][3]='r'; this.board[0][0]=' '; }

        var inCheck = this.isInCheck(color);
        for (var r2 = 0; r2 < 8; r2++) this.board[r2] = savedBoard[r2];
        return !inCheck;
    };

    this.getLegalMovesFrom = function(fr, fc) {
        var moves = [];
        for (var tr = 0; tr < 8; tr++)
            for (var tc = 0; tc < 8; tc++)
                if (this.isLegalMove(fr, fc, tr, tc)) moves.push({ r: tr, c: tc });
        return moves;
    };

    this.hasLegalMoves = function(color) {
        for (var r = 0; r < 8; r++) {
            for (var c = 0; c < 8; c++) {
                var p = this.board[r][c];
                if (p === ' ') continue;
                if ((p === p.toUpperCase()) !== (color === 'w')) continue;
                if (this.getLegalMovesFrom(r, c).length > 0) return true;
            }
        }
        return false;
    };

    this.isCheckmate = function() {
        return this.isInCheck(this.turn) && !this.hasLegalMoves(this.turn);
    };
    this.isStalemate = function() {
        return !this.isInCheck(this.turn) && !this.hasLegalMoves(this.turn);
    };

    this.isThreefoldRepetition = function() {
        for (var pos in this.positionHistory)
            if (this.positionHistory[pos] >= 3) return true;
        return false;
    };

    this.isInsufficientMaterial = function() {
        var pieces = [];
        for (var r = 0; r < 8; r++) {
            for (var c = 0; c < 8; c++) {
                var p = this.board[r][c];
                if (p === ' ' || p === 'K' || p === 'k') continue;
                pieces.push({ p: p.toUpperCase(), r: r, c: c,
                              color: (p === p.toUpperCase()) ? 'w' : 'b' });
            }
        }
        if (pieces.length === 0) return true;  // K vs K
        if (pieces.length === 1 && (pieces[0].p === 'N' || pieces[0].p === 'B')) return true;  // K+minor vs K
        if (pieces.length === 2 && pieces[0].p === 'B' && pieces[1].p === 'B' &&
            pieces[0].color !== pieces[1].color) {
            // K+B vs K+B same square color
            if ((pieces[0].r + pieces[0].c) % 2 === (pieces[1].r + pieces[1].c) % 2) return true;
        }
        return false;
    };

    // ------------------------------------------------------------------ //
    // Generate algebraic notation for a move (call BEFORE applyMove)
    this.moveToAlgebraic = function(from, to, promoteTo) {
        var piece = this.board[from.r][from.c];
        var pUp   = piece.toUpperCase();
        var captured = this.board[to.r][to.c];
        var isCapture = (captured !== ' ');

        // En passant is also a capture
        if (pUp === 'P' && from.c !== to.c && captured === ' ' &&
            this.toAlgebraic(to.r, to.c) === this.enPassant) isCapture = true;

        // Castling
        if (pUp === 'K' && Math.abs(to.c - from.c) === 2)
            return to.c === 6 ? 'O-O' : 'O-O-O';

        var alg = '';
        if (pUp !== 'P') {
            alg += pUp;
            // Disambiguation
            var others = [];
            for (var r = 0; r < 8; r++) {
                for (var c = 0; c < 8; c++) {
                    if (r === from.r && c === from.c) continue;
                    if (this.board[r][c] === piece && this.isLegalMove(r, c, to.r, to.c))
                        others.push({ r: r, c: c });
                }
            }
            if (others.length > 0) {
                var sameFile = others.filter(function(p) { return p.c === from.c; });
                var sameRank = others.filter(function(p) { return p.r === from.r; });
                if (sameFile.length === 0)
                    alg += String.fromCharCode(97 + from.c);
                else if (sameRank.length === 0)
                    alg += (8 - from.r);
                else
                    alg += String.fromCharCode(97 + from.c) + (8 - from.r);
            }
        }

        if (isCapture) {
            if (pUp === 'P') alg += String.fromCharCode(97 + from.c);
            alg += 'x';
        }
        alg += this.toAlgebraic(to.r, to.c);
        if (promoteTo) alg += '=' + promoteTo.toUpperCase();
        return alg;
    };

    // ------------------------------------------------------------------ //
    // Parse user input → { f, t, promoteTo } | { ambiguous, candidates, t, type } | null
    this.parseMove = function(input) {
        // Normalise: trim, collapse spaces, lowercase for all matching
        var lower = input.trim().replace(/\s+/g, '').toLowerCase();

        // Castling (o-o / 0-0 / o-o-o / 0-0-0)
        if (lower === 'o-o' || lower === '0-0') {
            var r = (this.turn === 'w') ? 7 : 0;
            var move = { f: {r:r,c:4}, t: {r:r,c:6} };
            return this.isLegalMove(r, 4, r, 6) ? move : null;
        }
        if (lower === 'o-o-o' || lower === '0-0-0') {
            var r = (this.turn === 'w') ? 7 : 0;
            var move = { f: {r:r,c:4}, t: {r:r,c:2} };
            return this.isLegalMove(r, 4, r, 2) ? move : null;
        }

        // Promotion suffix: e8=q, e8q
        var promoChar = null;
        var promoMatch = lower.match(/=?([qrbn])$/);
        if (promoMatch) {
            promoChar = promoMatch[1].toUpperCase();
            lower = lower.substring(0, lower.length - promoMatch[0].length);
        }

        // Strip check/capture decoration
        var clean = lower.replace(/[+#x]/g, '');

        // Coordinate: e2e4 / e2-e4 / b1c3  (now all lowercase, works after input uppercasing)
        var coordMatch = clean.match(/^([a-h][1-8])[-]?([a-h][1-8])$/);
        if (coordMatch) {
            var from = this.toCoords(coordMatch[1]);
            var to   = this.toCoords(coordMatch[2]);
            if (this.isLegalMove(from.r, from.c, to.r, to.c))
                return { f: from, t: to, promoteTo: promoChar };
            return null;
        }

        // Pawn capture by file: "bc3" or "bxc3" (x already stripped) — file + dest
        // Must be tried before piece-move matching since 'b' would be mistaken for Bishop
        var pawnCapMatch = clean.match(/^([a-h])([a-h][1-8])$/);
        if (pawnCapMatch && pawnCapMatch[1] !== pawnCapMatch[2][0]) {
            var pcFile = pawnCapMatch[1].charCodeAt(0) - 97;
            var pcDest = this.toCoords(pawnCapMatch[2]);
            var pawnPiece = (this.turn === 'w') ? 'P' : 'p';
            for (var r = 0; r < 8; r++) {
                if (this.board[r][pcFile] === pawnPiece &&
                    this.isLegalMove(r, pcFile, pcDest.r, pcDest.c))
                    return { f: {r:r, c:pcFile}, t: pcDest, promoteTo: promoChar };
            }
            // No legal pawn capture found — fall through to piece-move interpretation
        }

        // Algebraic: [Piece][ambig][dest]   e.g. e4, nc3, rae1, r1e5, exd5, kg1
        var algMatch = clean.match(/^([nbrqk])?([a-h]|[1-8])?([a-h][1-8])$/);
        if (!algMatch) return null;

        var pieceType   = algMatch[1] ? algMatch[1].toUpperCase() : 'P';
        var ambig       = algMatch[2] || null;
        var dest        = this.toCoords(algMatch[3]);
        var pieceLetter = (this.turn === 'w') ? pieceType : pieceType.toLowerCase();

        var candidates = [];
        for (var r = 0; r < 8; r++) {
            for (var c = 0; c < 8; c++) {
                if (this.board[r][c] !== pieceLetter) continue;
                if (!this.isLegalMove(r, c, dest.r, dest.c)) continue;
                if (ambig) {
                    if (isNaN(ambig)) {
                        if (String.fromCharCode(97 + c) !== ambig) continue;
                    } else {
                        if ((8 - r) !== parseInt(ambig, 10)) continue;
                    }
                }
                candidates.push({ r: r, c: c });
            }
        }

        if (candidates.length === 1)
            return { f: candidates[0], t: dest, promoteTo: promoChar };
        if (candidates.length > 1)
            return { ambiguous: true, candidates: candidates, t: dest, type: pieceType };
        return null;
    };

    // ------------------------------------------------------------------ //
    this.applyMove = function(from, to, promoteTo) {
        this.saveState();  // snapshot before move (for undo)

        var piece   = this.board[from.r][from.c];
        var isW     = (piece === piece.toUpperCase());
        var color   = isW ? 'w' : 'b';

        var uciStr = this.toAlgebraic(from.r, from.c) + this.toAlgebraic(to.r, to.c) +
                     (promoteTo ? promoteTo.toLowerCase() : '');
        var alg = this.moveToAlgebraic(from, to, promoteTo);

        var captured = this.board[to.r][to.c];
        var epCapSquare = null;

        // En passant capture
        if (piece.toUpperCase() === 'P' && from.c !== to.c && captured === ' ' &&
            this.toAlgebraic(to.r, to.c) === this.enPassant) {
            epCapSquare = { r: from.r, c: to.c };
            captured    = this.board[epCapSquare.r][epCapSquare.c];
            this.board[epCapSquare.r][epCapSquare.c] = ' ';
        }

        // Move piece
        this.board[to.r][to.c]   = piece;
        this.board[from.r][from.c] = ' ';

        // Castling: also move the rook
        if      (piece === 'K' && from.c===4 && to.c===6) { this.board[7][5]='R'; this.board[7][7]=' '; }
        else if (piece === 'K' && from.c===4 && to.c===2) { this.board[7][3]='R'; this.board[7][0]=' '; }
        else if (piece === 'k' && from.c===4 && to.c===6) { this.board[0][5]='r'; this.board[0][7]=' '; }
        else if (piece === 'k' && from.c===4 && to.c===2) { this.board[0][3]='r'; this.board[0][0]=' '; }

        // Pawn promotion
        if (piece === 'P' && to.r === 0) {
            this.board[to.r][to.c] = promoteTo ? promoteTo.toUpperCase() : 'Q';
        } else if (piece === 'p' && to.r === 7) {
            this.board[to.r][to.c] = promoteTo ? promoteTo.toLowerCase() : 'q';
        }

        // Update castling rights
        if (piece === 'K') { this.castling = this.castling.replace('K','').replace('Q',''); }
        if (piece === 'k') { this.castling = this.castling.replace('k','').replace('q',''); }
        if (piece === 'R') {
            if (from.r===7 && from.c===7) this.castling = this.castling.replace('K','');
            if (from.r===7 && from.c===0) this.castling = this.castling.replace('Q','');
        }
        if (piece === 'r') {
            if (from.r===0 && from.c===7) this.castling = this.castling.replace('k','');
            if (from.r===0 && from.c===0) this.castling = this.castling.replace('q','');
        }
        if (!this.castling) this.castling = '-';

        // En passant square for next move
        if (piece.toUpperCase() === 'P' && Math.abs(to.r - from.r) === 2) {
            this.enPassant = this.toAlgebraic((from.r + to.r) / 2, from.c);
        } else {
            this.enPassant = '-';
        }

        // Record capture
        if (captured !== ' ') this.captured[color].push(captured);

        // Clocks
        this.halfMoveClock = (piece.toUpperCase() === 'P' || captured !== ' ') ? 0 : this.halfMoveClock + 1;
        if (!isW) this.fullMoveNumber++;

        // Switch turn
        this.turn = isW ? 'b' : 'w';

        // Check/checkmate suffix
        if      (this.isCheckmate())        alg += '#';
        else if (this.isInCheck(this.turn)) alg += '+';

        this.history.push({ alg: alg, color: color, uci: uciStr });

        // Record position for 3-fold repetition
        this.recordPosition();

        return alg;
    };

    // ------------------------------------------------------------------ //
    // Undo support — save/restore complete game state
    this.saveState = function() {
        var snap = {
            board: [], turn: this.turn, castling: this.castling,
            enPassant: this.enPassant, halfMoveClock: this.halfMoveClock,
            fullMoveNumber: this.fullMoveNumber,
            history: this.history.slice(),
            captured: { w: this.captured.w.slice(), b: this.captured.b.slice() },
            positionHistory: {}
        };
        for (var r = 0; r < 8; r++) snap.board.push(this.board[r].slice());
        for (var pos in this.positionHistory) snap.positionHistory[pos] = this.positionHistory[pos];
        this.stateStack.push(snap);
    };

    this.undoMove = function() {
        if (this.stateStack.length === 0) return false;
        var snap = this.stateStack.pop();
        this.board = snap.board; this.turn = snap.turn;
        this.castling = snap.castling; this.enPassant = snap.enPassant;
        this.halfMoveClock = snap.halfMoveClock;
        this.fullMoveNumber = snap.fullMoveNumber;
        this.history = snap.history; this.captured = snap.captured;
        this.positionHistory = snap.positionHistory;
        return true;
    };

    // ------------------------------------------------------------------ //
    this.pieceValue = function(p) {
        return { 'P':1, 'N':3, 'B':3, 'R':5, 'Q':9, 'K':0 }[p.toUpperCase()] || 0;
    };

    // Positive = white ahead (white captured more), negative = black ahead
    this.materialBalance = function() {
        var w = 0, b = 0;
        for (var i = 0; i < this.captured.w.length; i++) w += this.pieceValue(this.captured.w[i]);
        for (var i = 0; i < this.captured.b.length; i++) b += this.pieceValue(this.captured.b[i]);
        return w - b;
    };
};
