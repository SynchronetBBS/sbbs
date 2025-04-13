#include "ansi_parser.h"

#include <stdio.h>
enum ansiState
ANSI_Parser::parse(unsigned char ch)
{
	switch (state) {
		case ansiState_none:
			if (ch == '\x1b') {
				state = ansiState_esc;
				ansi_sequence += ch;
			}
			break;
		case ansiState_esc:
			ansi_sequence += ch;
			if (ch == '[') {
				state = ansiState_csi;
				ansi_params = "";
			}
			else if (ch == '_' || ch == 'P' || ch == '^' || ch == ']') {
				state = ansiState_string;
				ansi_was_string = true;
			}
			else if (ch == 'X') {
				state = ansiState_sos;
				ansi_was_string = true;
			}
			else if (ch >= ' ' && ch <= '/') {
				ansi_ibs += ch;
				state = ansiState_intermediate;
			}
			else if (ch >= '0' && ch <= '~') {
				state = ansiState_final;
				ansi_was_cc = true;
				ansi_final_byte = ch;
			}
			else {
				state = ansiState_broken;
			}
			break;
		case ansiState_csi:
			ansi_sequence += ch;
			if (ch >= '0' && ch <= '?') {
				if (ansi_params == "" && ch >= '<' && ch <= '?')
					ansi_was_private = true;
				ansi_params += ch;
			}
			else if (ch >= ' ' && ch <= '/') {
				ansi_ibs += ch;
				state = ansiState_intermediate;
			}
			else if (ch >= '@' && ch <= '~') {
				state = ansiState_final;
				ansi_final_byte = ch;
			}
			else {
				state = ansiState_broken;
			}
			break;
		case ansiState_intermediate:
			ansi_sequence += ch;
			if (ch >= ' ' && ch <= '/') {
				ansi_ibs += ch;
				state = ansiState_intermediate;
			}
			else if (ch >= '@' && ch <= '~') {
				state = ansiState_final;
				ansi_final_byte = ch;
			}
			else {
				state = ansiState_broken;
			}
			break;
		case ansiState_string: // APS, DCS, PM, or OSC
			ansi_sequence += ch;
			if (ch == '\x1b')
				state = ansiState_esc;
			else if (!((ch >= '\b' && ch <= '\r') || (ch >= ' ' && ch <= '~')))
				state = ansiState_broken;
			break;
		case ansiState_sos: // SOS
			ansi_sequence += ch;
			if (ch == '\x1b')
				state = ansiState_sos_esc;
			break;
		case ansiState_sos_esc: // ESC inside SOS
			ansi_sequence += ch;
			if (ch == '\\')
				state = ansiState_esc;
			else if (ch == 'X')
				state = ansiState_broken;
			else
				state = ansiState_sos;
			break;
		case ansiState_broken:
			// Stay in broken state.
			break;
		case ansiState_final:
			// Stay in final state.
			break;
	}
	return state;
}

enum ansiState
ANSI_Parser::current_state()
{
	return state;
}

void
ANSI_Parser::reset()
{
	ansi_params.clear();
	ansi_ibs.clear();
	ansi_sequence.clear();
	state = ansiState_none;
	ansi_final_byte = 0;
	ansi_was_cc = false;
	ansi_was_string = false;
	ansi_was_private = false;
}

unsigned
ANSI_Parser::count_params()
{
	std::string tp = ansi_params;
	unsigned ret = 1;

	try {
		for (;;) {
			size_t sc = tp.find(";");
			if (sc == std::string::npos)
				return ret;
			ret++;
			tp.erase(0, sc + 1);
		}
	}
	catch (...) {
		return 0;
	}
}

unsigned
ANSI_Parser::get_pval(unsigned pnum, unsigned dflt)
{
	try {
		if (ansi_params == "")
			return dflt;
		unsigned p = 0;
		std::string tp = ansi_params;
		switch (tp.at(0)) {
			case '<':
			case '=':
			case '>':
			case '?':
				tp.erase(0, 1);
				break;
		}
		while (p < pnum) {
			size_t sc = tp.find(";");
			if (sc == std::string::npos)
				return dflt;
			tp.erase(0, sc + 1);
			p++;
		}
		size_t sc = tp.find(";");
		if (sc != std::string::npos)
			tp.erase(sc);
		sc = tp.find(":");
		if (sc != std::string::npos)
			tp.erase(sc);
		sc = tp.find("<");
		if (sc != std::string::npos)
			tp.erase(sc);
		sc = tp.find("=");
		if (sc != std::string::npos)
			tp.erase(sc);
		sc = tp.find(">");
		if (sc != std::string::npos)
			tp.erase(sc);
		sc = tp.find("?");
		if (sc != std::string::npos)
			tp.erase(sc);
		if (tp == "")
			return dflt;
		return std::stoul(tp);
	}
	catch (...) {
		return dflt;
	}
}
