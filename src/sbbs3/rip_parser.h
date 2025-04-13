#ifndef RIP_PARSE_H
#define RIP_PARSE_H

#include <string>
#include <vector>

enum ripState {
	 ripState_none         // No sequence (waiting for start)
	,ripState_pipe         // RIP line (waiting for pipe)
	,ripState_level        // Parsing level/sublevel
	,ripState_cmd          // RIP command byte
	,ripState_args         // Arguments
	,ripState_backslash    // Continuation or escape
	,ripState_cont         // Continuation
	,ripState_done         // Done
	,ripState_next         // Starting another RIP command
	,ripState_broken       // Invalid rip
};

class RIP_Parser {
public:
	~RIP_Parser();
	enum ripState parse(unsigned char ch, unsigned col);
	enum ripState current_state();
	void reset(enum ripState);

	std::vector<unsigned> rip_args{};
	std::string rip_text_arg{""};
	unsigned rip_level{};
	unsigned char rip_cmd{};
	const char *cmd_template{};
	unsigned char lastch{};

	static const char * params(unsigned level, unsigned char cmd);

private:
	enum ripState state{ripState_none}; // track RIP escape seq output
	unsigned tpos{};
	unsigned apos{};
	unsigned alen{};
	unsigned carg{};
	bool gotcr{};
	bool gotlf{};
	bool allocated{};
};

#endif
