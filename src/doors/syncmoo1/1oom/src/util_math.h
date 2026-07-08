#ifndef INC_1OOM_UTIL_MATH_H
#define INC_1OOM_UTIL_MATH_H

extern int util_math_calc_angle(int dx, int dy);
extern int util_math_angle_dist_cos(int angle, int dist);
extern int util_math_angle_dist_sin(int angle, int dist);
extern void util_math_go_line_dist(int *x0ptr, int *y0ptr, int x1, int y1, int dist);
extern int util_math_dist_steps(int x0, int y0, int x1, int y1);
extern int util_math_dist_fast(int x0, int y0, int x1, int y1);
extern int util_math_dist_maxabs(int x0, int y0, int x1, int y1);
extern int util_math_line_plot(int x0, int y0, int x1, int y1, int *tblx, int *tbly);
extern int util_math_get_route_len(int x0, int y0, const int *tblx, const int *tbly, int len);

#endif
