/* packet types */
#define PT_CLANMOVE     0
#define PT_DATAOK       1
#define PT_RESET        2
#define PT_GOTRESET     3
#define PT_RECON        4
#define PT_GOTRECON     5
#define PT_NEWUSER      6
#define PT_DELUSER      7
#define PT_ADDUSER      8
#define PT_SUBUSER      9           // subtract user, opposite of add user :)
#define PT_COMEBACK     10          // come back option
#define PT_MSJ          11          // message posted
#define PT_ATTACK       12          // InterBBS attack -- yeah!
#define PT_ATTACKRESULT 13
#define PT_SPY          14
#define PT_SPYRESULT    15
#define PT_SCOREDATA    16
#define PT_SCORELIST    17
#define PT_NEWNDX       18
#define PT_ULIST        19
