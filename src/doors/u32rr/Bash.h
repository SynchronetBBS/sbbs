#ifndef _BASH_H_
#define _BASH_H_

enum bash {
	Tackle,
	DropKick,
	Uppercut,
	Bite,
	LegSweep,
	JointBreak,
	Knifehand,
	NervePunch,
	Chokehold,
	Headbash,
	PullHair,
	Kick,
	StraightPunch,
	Ram,
	BASH_COUNT
};

extern const char *UCBashName[BASH_COUNT];

enum bash_rank {
	Rotten,
	Awful,
	Lousy,
	Pathetic,
	Bad,
	Poor,
	Incompetent,
	BelowAverage,
	Average,
	AboveAverage,
	PrettyGood,
	Competent,
	Good,
	VeryGood,
	Extraordinary,
	Excellent,
	Superb,
	COMPLETE,
	RANK_COUNT
};

extern const char *BashRank[RANK_COUNT];

#endif
