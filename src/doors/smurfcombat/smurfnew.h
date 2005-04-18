FILE           *stream;
int             turnsaday, defrounds;
int             inuser, cyc, bbsexit, thisuserno, noplayers, displayreal,
                cyc2, scrnlen, cyc3;

char            smurfname[10][41] = {"GRANDPA SMURF", "PAPA SMURF", "SHADOW SMURF", "REBEL SMURF"};
char            realname[10][41] = {"GRANDPA SMURF", "PAPA SMURF", "SHADOW SMURF", "REBEL SMURF"};
int             realnumb[10] = {0, 0, 0, 0};
char            smurfweap[10][41] = {"Burning Cross", "Fire Water", "Shadow Lance", "H2O Uzi"};
char            smurfarmr[10][41] = {"Inverted Star", "Armor", "Shadow Shield", "No Fear Clothes"};
char            smurftext[10][81] = {"The forces of evil will prevail!",
    "Didn't anyone teach you to respect your dad!?!?",
    "Fool, now the S.S. must make you pay . . .",
"Traitor! You are one of PAPA SMURF'S henchmen!"};

char            smurfettename[10][41] = {"Female Fetale", "Fire Eyes", "Shadow Girl", "Cassie"};
int             smurfettelevel[10] = {10, 9, 7, 5};

int             smurfweapno[10] = {20, 19, 3, 8};
int             smurfarmrno[10] = {20, 19, 3, 8};

char            smurfconf[10][41] = {"Underworld", "Fortress", "Basement", "Infirmary"};
int             smurfconfno[10] = {11, 10, 2, 3};

int             smurfhost[10][10] = {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};

int             smurflevel[10] = {41, 30, 3, 3};
float           smurfmoney[10] = {8458, 3245, 3491, 745};
float           smurfbankd[10] = {9127231, 788735, 497, 2446};
float           smurfexper[10] = {553, 513, 154, 634};
int             smurffights[10] = {3, 3, 3, 0};
int             smurfwin[10] = {231, 117, 23, 34};
int             smurflose[10] = {0, 1, 3, 2};
int             smurfhp[10] = {413, 329, 36, 34};
int             smurfhpm[10] = {413, 329, 36, 34};
int             smurfstr[10] = {42, 37, 12, 16};
int             smurfspd[10] = {45, 32, 22, 13};
int             smurfint[10] = {26, 23, 13, 11};
int             smurfcon[10] = {31, 24, 14, 12};
int             smurfbra[10] = {36, 42, 12, 12};
int             smurfchr[10] = {13, 27, 8, 17};
int             smurfturns[10] = {3, 3, 3, 3};
int             smurfspcweapon[10][6] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int             smurfqtyweapon[10][6] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
char            smurfsex[10] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
int             __morale[41] = {0, 1, 2, 2};
int             __ettemorale[41] = {999, 999, 999, 999};
