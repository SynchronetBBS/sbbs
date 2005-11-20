#ifndef _ALLFONTS_H_
#define _ALLFONTS_H_

struct conio_font_data_struct {
        char *eight_by_sixteen;
        char *eight_by_fourteen;
        char *eight_by_eight;
        char *desc;
};

extern struct conio_font_data_struct conio_fontdata[257];

#endif
