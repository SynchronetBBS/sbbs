#define MAX_NDX_WORDS   13

const char *const papszNdxKeyWords[MAX_NDX_WORDS] = {
	"BBSName",
	"VillageName",
	"Address",
	"ConnectType",
	"MailType",
	"BBSId",
	"Status",
	"WorldName",
	"LeagueId",
	"Host",
	"NoMSG",
	"StrictMsgFile",
	"StrictRouting",
};

#define MAX_RT_WORDS    4

const char *const papszRouteKeyWords[MAX_RT_WORDS] = {
	"ROUTE",
	"CRASH",
	"HOLD",
	"NORMAL"
};

