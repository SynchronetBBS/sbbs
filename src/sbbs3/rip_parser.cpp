#include "rip_parser.h"

#include <stdio.h>
#include <stdlib.h>

RIP_Parser::~RIP_Parser()
{
	if (allocated)
		free((void*)cmd_template);
}

enum ripState
RIP_Parser::parse(unsigned char ch, unsigned col)
{
	switch (state) {
		case ripState_none:
			if (ch == '!' && col == 0)
				state = ripState_pipe;
			else if (ch == 1 || ch == 2)
				state = ripState_pipe;
			break;
		case ripState_pipe:
			if (ch == '|')
				state = ripState_level;
			else if (ch == '\r') {
				// Ignore
			}
			else
				state = ripState_done;
			break;
		case ripState_level:
			if (ch >= '0' && ch <= '9') {
				rip_level *= 10;
				rip_level += (ch - '0');
				break;
			}
			// Fall-through
		case ripState_cmd:
			cmd_template = RIP_Parser::params(rip_level, ch);
			rip_cmd = ch;
			if (cmd_template) {
				if (cmd_template[0])
					state = ripState_args;
				else {
					lastch = 0;
					state = ripState_done;
				}
			}
			else {
				state = ripState_broken;
			}
			break;
		case ripState_args:
			switch (cmd_template[tpos]) {
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					if (alen == 0)
						alen = cmd_template[tpos] - '0';
					if (ch >= '0' && ch <= '9') {
						carg *= 36;
						carg += (ch - '0');
						alen--;
					}
					else if (ch >= 'A' && ch <= 'Z') {
						carg *= 36;
						carg += (ch - 'A' + 10);
						alen--;
					}
					else if (ch >= 'a' && ch <= 'z') {
						carg *= 36;
						carg += (ch - 'a' + 10);
						alen--;
					}
					else if (lastch == '\\' && ch == '\r') {
						state = ripState_cont;
						gotcr = false;
						gotlf = true;
					}
					else if (lastch == '\\' && ch == '\n') {
						state = ripState_cont;
						gotcr = true;
						gotlf = false;
					}
					else if (ch == '\\') {
						if (lastch == '\\') {
							state = ripState_broken;
						}
					}
					else {
						if (cmd_template[tpos + 1] != 0) {
							state = ripState_broken;
						}
						else if (ch == '|') {
							rip_args.emplace_back(carg);
							carg = 0;
							state = ripState_next;
						}
						else {
							rip_args.emplace_back(carg);
							carg = 0;
							lastch = ch;
							state = ripState_done;
						}
					}
					if (alen == 0) {
						rip_args.emplace_back(carg);
						carg = 0;
						tpos++;
						if (cmd_template[tpos] == 0) {
							lastch = 0;
							state = ripState_done;
						}
					}
					break;
				case 't':
					if (lastch == '<' && ch == '>') {
						alen++;
						if (alen + '0' == cmd_template[tpos + 1]) {
							lastch = 0;
							state = ripState_done;
						}
					}
					else if (lastch == '\\') {
						if (ch == '\r') {
							state = ripState_cont;
							gotcr = false;
							gotlf = true;
						}
						else if (ch == '\n') {
							state = ripState_cont;
							gotcr = true;
							gotlf = false;
						}
						else
							rip_text_arg += ch;
					}
					else if (ch == '\\') {
						// Escape
					}
					else if (ch == '\r' || ch == '\n') {
						lastch = ch;
						state = ripState_done;
					}
					else if (ch == '|') {
						state = ripState_next;
					}
					else {
						if (lastch == '<')
							rip_text_arg += lastch;
						if (ch != '<')
							rip_text_arg += ch;
					}
					break;
				case 'T':
					if (lastch == '\\') {
						if (ch == '\r' || ch == '\n')
							state = ripState_cont;
						else
							rip_text_arg += ch;
					}
					else if (ch == '\\') {
						// Escape
					}
					else if (ch == '\r' || ch == '\n') {
						lastch = ch;
						state = ripState_done;
					}
					else if (ch == '|') {
						state = ripState_next;
					}
					break;
				case '*':
					tpos++;
					size_t ntsz = (strlen(cmd_template) - tpos) * rip_args[tpos - 2] + tpos + 1;
					char *new_template = (char*)malloc(ntsz);
					strcpy(new_template, cmd_template);
					for (unsigned i = 1; i < rip_args[tpos - 2]; i++) {
						strcat(new_template, &cmd_template[tpos]);
					}
					if (allocated)
						free((void*)cmd_template);
					cmd_template = new_template;
					allocated = true;
					parse(ch, col);
					break;
			}
			break;
		case ripState_cont:
			if (gotcr == false && ch == '\r') {
				ch = 0;
				gotcr = true;
			}
			else if (gotlf == false && ch == '\n') {
				ch = 0;
				gotlf = true;
			}
			if (ch == 0 && gotcr && gotlf) {
				state = ripState_args;
			}
			else {
				state = ripState_args;
				parse(ch, col);
			}
			break;
		default:
			break;
	}
	if (state != ripState_done)
		lastch = ch;
	return state;
}

enum ripState
RIP_Parser::current_state()
{
	return state;
}

void
RIP_Parser::reset(enum ripState rstate)
{
	rip_args.clear();
	rip_text_arg.clear();
	rip_level = 0;
	rip_cmd = 0;
	if (allocated)
		free((void*)cmd_template);
	cmd_template = nullptr;
	state = rstate;
	tpos = 0;
	apos = 0;
	alen = 0;
	carg = 0;
	lastch = 0;
	gotcr = false;
	gotlf = false;
	allocated = false;
}

const char *
RIP_Parser::params(unsigned level, unsigned char cmd)
{
	switch (level) {
		case 0:
			switch (cmd) {
				case '*':
				case 'e':
				case 'E':
				case 'H':
				case '>':
				case '#':
					return "";
				case 'c':
				case 'W':
					return "2";
				case 'm':
				case 'g':
				case 'a':
				case 'X':
				case 'S':
					return "22";
				case 'F':
				case 'C':
					return "222";
				case '=':
					return "242";
				case 'v':
				case 'Y':
				case 'L':
				case 'R':
				case 'B':
				case 'o':
					return "2222";
				case 'A':
				case 'I':
					return "22222";
				case 'i':
				case 'V':
				case 'O':
					return "222222";
				case 'w':
					return "222211";
				case 's':
				case 'Z':
					return "222222222";
				case 'Q':
					return "2222222222222222";
				case 'T':
					return "T";
				case '@':
					return "22T";
				case 'P':
				case 'p':
				case 'l':
					return "2*22";
			}
			break;
		case 1:
			switch(cmd) {
				case 'K':
				case 'E':
					return "";
				case 't':
				case 'W':
					return "1T";
				case 'R':
					return "8T";
				case '\x1b':
					return "13T";
				case 'F':
					return "24t1";
				case 'D':
					return "32T";
				case 'P':
					return "2221";
				case 'T':
					return "22222";
				case 'C':
					return "22221";
				case 'G':
					return "222222";
				case 'I':
					return "22212T";
				case 'M':
					return "22222115T";
				case 'U':
					return "2222211t3";
				case 'B':
					return "222422222222226";
			}
			break;
		case 9:
			switch (cmd) {
				case '\x1b':
					return "1124T";
			}
			break;
	}
	return NULL;
}
