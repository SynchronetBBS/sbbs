#ifndef INC_1OOM_COMP_H
#define INC_1OOM_COMP_H

#ifndef MIN
#define MIN(a, b) (((a) <= (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) (((a) >= (b)) ? (a) : (b))
#endif
#ifndef SETMIN
#define SETMIN(a, b) do { if ((b) < (a)) { (a) = (b); }} while (0)
#endif
#ifndef SETMAX
#define SETMAX(a, b) do { if ((b) > (a)) { (a) = (b); }} while (0)
#endif
#ifndef SETRANGE
#define SETRANGE(a, b, c) do { if (((c) <= (b)) || ((b) > (a))) { (a) = (b); } else if ((c) < (a)) { (a) = (c); } } while (0)
#endif
#define ADDSATT(_v_, _n_, _top_) do { int _t_; _t_ = (_v_) + (_n_); SETMIN(_t_, (_top_)); (_v_) = _t_; } while (0)
#define SUBSATT(_v_, _n_, _bot_) do { int _t_; _t_ = (_v_) - (_n_); SETMAX(_t_, (_bot_)); (_v_) = _t_; } while (0)
#define SUBSAT0(_v_, _n_) SUBSATT(_v_, _n_, 0)
#define TBLLEN(_t_) (sizeof((_t_)) / sizeof((_t_)[0]))

#endif
