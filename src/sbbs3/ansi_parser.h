#ifndef ANSI_PARSE_H
#define ANSI_PARSE_H

#include <string>

enum ansiState {
	 ansiState_none         // No sequence
	,ansiState_esc          // Escape
	,ansiState_csi          // CSI
	,ansiState_intermediate // Intermediate byte
	,ansiState_final        // Final byte
	,ansiState_string       // APS, DCS, PM, or OSC
	,ansiState_sos          // SOS
	,ansiState_sos_esc      // ESC inside SOS
	,ansiState_broken	// Invalid ANSI
};

class ANSI_Parser {
public:
	enum ansiState parse(unsigned char ch);
	enum ansiState current_state();
	void reset();
	unsigned count_params();
	unsigned get_pval(unsigned pnum, unsigned dflt);

	std::string ansi_sequence{""};
	std::string ansi_params{""};
	std::string ansi_ibs{""};
	char ansi_final_byte{0};
	bool ansi_was_cc{false};
	bool ansi_was_string{false};
	bool ansi_was_private{false};

private:
	enum ansiState outchar_esc{ansiState_none}; // track ANSI escape seq output
};

#endif
