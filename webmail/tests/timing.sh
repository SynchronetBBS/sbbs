#!/usr/bin/env bash
# timing.sh: times the web mail client's main requests against a running
# web server, the way the browser sends them.  Not run by the test suite.
#
# usage: timing.sh [-k] [-y] [-n runs] [-s word] [-w] [-S address [-N sends]]
#                  [base-url [user]]
#
#   base-url  the web mail address, e.g. https://bbs.example.com/webmail/
#             (or WEBMAIL_URL); requests go to <base-url>api.ssjs
#   user      the account to sign in as (or WEBMAIL_USER)
#   -n runs   times each request is sent (default 20)
#   -s word   the search term for the search row (default "the", a word
#             likely to be in some subject or name; the cost hardly depends
#             on the number of hits, since every message is looked at)
#   -k        passed to curl: accept a self-signed TLS certificate
#   -w        also time the requests that change mail (see "Writes" below);
#             what they change is put back
#   -S address
#             also time sending: SENDS MAIL.  address must be the signed-in
#             user's own address (the script checks it against the
#             addresses the account may send from and refuses any other);
#             one small message is sent to it per send, and THESE MESSAGES
#             ARRIVE in that mailbox (and a copy of each in Sent): they
#             are not removed, and they count toward the account's daily
#             e-mail limit
#   -N sends  how many messages -S sends (default 3)
#   -y        do not ask before changing mail (-w, -S)
#
# The password: let the script prompt for it (not echoed).  It can also
# come from WEBMAIL_PASSWORD, but a WEBMAIL_PASSWORD=... typed on a command
# line is kept in the shell's history; prefer the prompt.  The script never
# puts the password on a command line (curl reads the request from its
# input) and never prints it.  With the require_tls setting on (the
# default), use an https:// address.
#
# Runs under bash 3.2 or later; needs curl, awk, mktemp, tr, cut, sed, od
# and sort.
#
# Rows (each a POST to api.ssjs, timed with curl's time_total):
#   folders+inbox  folder.list and message.list of the Inbox's first 50
#                  in one request: what opening the client does
#   message.get    one message with mark_read false: the message of the
#                  first Inbox page with the largest size at or under
#                  64 KB (its size is printed, nothing else about it)
#   changes        the poll, with the state token of the reply before
#   search         message.list of All mail (the search box's default
#                  scope) for the search term, first 50
# With -w:
#   star           message.set: the star of one Inbox message turned over
#                  (then turned back, not timed)
#   move           message.move of that message from the Inbox to a folder
#                  (then moved back, not timed)
#   move+folders   the same move, then folder.list as a second request,
#                  as the client refreshes its counts: the two times added
#   draft.save     a draft's text saved (a body of 2,048 bytes)
#   draft.attach   one piece of an attachment, 1,000,000 bytes (the size
#                  the client sends; the attachment is removed again, not
#                  timed)
# With -S:
#   draft.send     a message of about 200 bytes, no attachment, sent to the
#                  -S address (the draft is made and filled first, not
#                  timed)
#
# Each row prints the median, fastest and slowest in milliseconds beside
# its budget for the median, and PASS or OVER.  The exit status is 0 when
# every median is within budget, 1 when one is over, 2 when signing in or
# a request fails (no reply within 60 seconds, an HTTP status other than
# 200, a reply that is not JSON, or an error result).
#
# Without -w and -S the script reads only: message.get is sent with
# mark_read false and nothing is moved, flagged or deleted.  As when the
# client is opened in a browser, signing in records a session, folder.list
# may run the once-a-day removal of messages that have been in Trash longer
# than allowed, and the server's caches of list data are brought up to
# date.  It signs out at the end.
#
# Writes (-w), on the account's real mailbox.  Before starting, the script
# says what it will change and, when run from a terminal, asks (-y skips
# the question).  It uses:
# - one existing Inbox message: the OLDEST message already read (at least
#   two days short of the Inbox's age limit, so moving it back cannot be
#   refused as too old).  Its star is turned over and back, and it is
#   moved to the test folder and back, keeping its read and star state.
#   Moving it out of and back into the Inbox gives it a new number in the
#   mail base each time, so a mail program that tracks messages by number
#   (IMAP) may show it as removed and added again.  The moves are skipped
#   when the mailbox has a storage limit it is within 1 MB of (a move into
#   a folder could otherwise remove old Trash to make room).
# - a folder it creates, named "timing.sh test <random> (safe to delete)",
#   and a draft it creates, "timing.sh test draft (safe to delete)"; both
#   are removed at the end.
# - with -S, one draft per message sent; sending uses it up.
# On an error, or when interrupted, it stops, puts back what it changed
# and removes what it created, and prints anything it could not.

set -u

RUNS=20
TERM_WORD="the"
INSECURE=()
WRITES=0
SEND_TO=""
SENDS=3
ASK=1
while getopts "kn:s:wS:N:y" opt; do
	case "$opt" in
	k) INSECURE=(-k) ;;
	n) RUNS=$OPTARG ;;
	s) TERM_WORD=$OPTARG ;;
	w) WRITES=1 ;;
	S) SEND_TO=$OPTARG ;;
	N) SENDS=$OPTARG ;;
	y) ASK=0 ;;
	*) sed -n '2,/^$/s/^# \{0,1\}//p' "$0"; exit 2 ;;
	esac
done
shift $((OPTIND - 1))
BASE=${1:-${WEBMAIL_URL:-}}
USERNAME=${2:-${WEBMAIL_USER:-}}
if [ -z "$BASE" ] || [ -z "$USERNAME" ]; then
	echo "usage: $0 [-k] [-y] [-n runs] [-s word] [-w] [-S address [-N sends]] base-url user" >&2
	exit 2
fi
case "$RUNS" in
'' | *[!0-9]* | 0) echo "runs must be a positive number" >&2; exit 2 ;;
esac
case "$SENDS" in
'' | *[!0-9]* | 0) echo "sends must be a positive number" >&2; exit 2 ;;
esac
case "$SEND_TO" in
'' | *@*.*) ;;
*) echo "-S needs an Internet address (name@domain)" >&2; exit 2 ;;
esac
case "$BASE" in
*/) ;;
*) BASE="$BASE/" ;;
esac
URL="${BASE}api.ssjs"

if [ -n "${WEBMAIL_PASSWORD+set}" ]; then
	PASSWORD=$WEBMAIL_PASSWORD
else
	read -r -s -p "Password for $USERNAME: " PASSWORD
	echo >&2
fi

TMP=$(mktemp -d "${TMPDIR:-/tmp}/webmail-timing.XXXXXX") || exit 2
chmod 700 "$TMP"
JAR="$TMP/cookies"
SIGNED_IN=0
AS=0

# What the writes changed, for cleanup()
MSG_ID=""		# the Inbox message the writes use, by its current id
MSG_STARRED=""		# its star before (true or false)
MSG_IN_FOLDER=0		# 1 while it is in the test folder
STAR_TURNED=0		# 1 while its star is turned over
FOLDER_NAME=""		# the test folder's name, once it is being made
FOLDER_ID=""		# the test folder, while it exists
MAKING_DRAFT=0		# 1 while a draft is being made
DRAFT_ID=""		# the test draft, while it exists
ATTACHMENT=""		# an attachment of the test draft, while it exists
SEND_DRAFT=""		# a draft made for sending, while it exists
SENT=0			# messages sent

# A JSON string literal for a value (backslash, quote and control
# characters escaped)
json_str() {
	local s=$1
	s=${s//\\/\\\\}
	s=${s//\"/\\\"}
	s=${s//$'\t'/\\t}
	s=${s//$'\r'/\\r}
	s=${s//$'\n'/\\n}
	printf '"%s"' "$s"
}

# A random token of 24 hexadecimal digits
token() {
	od -An -N12 -tx1 /dev/urandom | tr -d ' \n'
}

# POST a request body (from stdin) the way the browser does; the reply
# goes to the file $1, "<HTTP status> <total seconds>" to stdout.  The
# exit status is curl's.  (The array test keeps bash 3.2 from calling an
# empty INSECURE unbound under set -u.)  The empty Expect header: for a
# large body (draft.attach) curl would otherwise send "Expect:
# 100-continue", which browsers do not, and wait a second for an answer
# the web server does not give.
post() {
	curl -s ${INSECURE[@]+"${INSECURE[@]}"} --connect-timeout 10 --max-time 60 \
		-b "$JAR" -c "$JAR" -o "$1" -w '%{http_code} %{time_total}' \
		-H 'Content-Type: application/json' -H 'X-Webmail: 1' -H 'Expect:' \
		--data-binary @- "$URL" 2> /dev/null
}

# Send a request (body on stdin) for the step named $1, reply to file $2;
# prints the total time in seconds.  Exits 2 with the reason when there is
# no reply, the status is not 200 or the reply is not JSON.
request() {
	local name=$1 out=$2 res rc code
	res=$(post "$out")
	rc=$?
	code=${res%% *}
	if [ $rc -ne 0 ]; then
		case $rc in
		28) echo "$name: no reply from $URL within the time limit" >&2 ;;
		6 | 7) echo "$name: cannot connect to $URL" >&2 ;;
		35 | 51 | 53 | 58 | 59 | 60 | 77 | 80 | 82 | 83 | 90 | 91)
			echo "$name: TLS failed for $URL (a self-signed certificate needs -k)" >&2 ;;
		*) echo "$name: the request to $URL failed (curl exit status $rc)" >&2 ;;
		esac
		exit 2
	fi
	if [ "$code" != "200" ]; then
		echo "$name: the server answered HTTP status $code (is $BASE the web mail address?)" >&2
		exit 2
	fi
	if [ "$(tr -d ' \t\r\n' < "$out" | cut -c1)" != "{" ]; then
		echo "$name: the reply is not JSON (is $BASE the web mail address?)" >&2
		exit 2
	fi
	echo "${res#* }"
}

# Flatten a JSON document into "path<TAB>value" lines, one per scalar
# (strings left escaped, without their quotes), e.g.
#   .results[0][1].items[3].size	20480
flatten() {
	awk 'BEGIN { RS = "\001" }
	function emit(v) { print path() "\t" v; after_value() }
	function path(   p, i) {
		p = ""
		for (i = 1; i <= depth; i++)
			p = p (kind[i] == "a" ? "[" idx[i] "]" : "." key[i])
		return p
	}
	function after_value() { if (depth > 0 && kind[depth] == "o") want_key[depth] = 1 }
	{
		s = $0; n = length(s); depth = 0
		for (i = 1; i <= n; i++) {
			c = substr(s, i, 1)
			if (c == "\"") {
				j = i + 1; v = ""
				while (j <= n) {
					d = substr(s, j, 1)
					if (d == "\\") { v = v d substr(s, j + 1, 1); j += 2; continue }
					if (d == "\"") break
					v = v d; j++
				}
				i = j
				if (depth > 0 && kind[depth] == "o" && want_key[depth]) {
					key[depth] = v; want_key[depth] = 0
				} else
					emit(v)
			} else if (c == "{" || c == "[") {
				depth++; kind[depth] = (c == "{") ? "o" : "a"; idx[depth] = 0
				want_key[depth] = (c == "{")
			} else if (c == "}" || c == "]") {
				depth--; after_value()
			} else if (c == ",") {
				if (kind[depth] == "a") idx[depth]++
				else want_key[depth] = 1
			} else if (c ~ /[-0-9tfn]/) {
				j = i
				while (j <= n && substr(s, j, 1) ~ /[-+.0-9a-zE]/) j++
				emit(substr(s, i, j - i)); i = j - 1
			}
		}
	}' "$1"
}

# The value at a path in a reply file
value_at() {
	flatten "$1" | awk -F '\t' -v p="$2" '$1 == p { print $2; exit }'
}

# The error of the first call of a reply (its code, and reason if any)
error_of() {
	local e r
	e=$(value_at "$1" '.results[0][1].error')
	[ -z "$e" ] && e=$(value_at "$1" '.error')
	r=$(value_at "$1" '.results[0][1].reason')
	[ -n "$r" ] && e="$e, $r"
	echo "${e:-an unexpected reply}"
}

# One write call ($2: method, $3: arguments as JSON) for the step named $1,
# reply to $TMP/w; its time goes to LAST_T.  Exits 2 when the call fails.
# (Called directly, never in $(...), so the exit runs the cleanup.)
LAST_T=0
write_call() {
	LAST_T=$(printf '{"calls":[["%s",%s,1]],"as":%s}' "$2" "$3" "$AS" | request "$1" "$TMP/w") \
		|| exit 2
	if [ "$(value_at "$TMP/w" '.results[0][1].ok')" != "true" ]; then
		echo "$1: the server answered with an error ($(error_of "$TMP/w"))" >&2
		exit 2
	fi
}

# The new id of the one message of a write call's reply in $TMP/w, or
# nothing when that message failed
moved_id() {
	if [ "$(value_at "$TMP/w" '.results[0][1].results[0].ok')" = "true" ]; then
		value_at "$TMP/w" '.results[0][1].results[0].new_id'
	fi
}

# A write call made while cleaning up: never exits, returns 1 on failure
# (reply in $TMP/c)
quiet_call() {
	local res
	res=$(printf '{"calls":[["%s",%s,1]],"as":%s}' "$1" "$2" "$AS" | post "$TMP/c") || return 1
	[ "${res%% *}" = "200" ] || return 1
	[ "$(value_at "$TMP/c" '.results[0][1].ok')" = "true" ]
}

# Put back what the writes changed and remove what they created; print
# what is left.  The test folder is looked at rather than trusted: a run
# stopped during a move may not know where the message went or its new id,
# but whatever is in the test folder goes back to the Inbox.
undo_writes() {
	local left="" ids n
	[ "$AS" = 0 ] && return
	if [ -z "$FOLDER_ID" ] && [ -n "$FOLDER_NAME" ] \
		&& quiet_call folder.list null; then
		FOLDER_ID=$(flatten "$TMP/c" | awk -F '\t' -v name="$FOLDER_NAME" '
			match($1, /^\.results\[0\]\[1\]\.folders\[[0-9]+\]\./) {
				k = substr($1, 1, RLENGTH); f = substr($1, RLENGTH + 1)
				if (f == "id") id[k] = $2
				else if (f == "name") nm[k] = $2
			}
			END { for (k in id) if (nm[k] == name) { print id[k]; exit } }')
	fi
	if [ -n "$FOLDER_ID" ]; then
		if quiet_call message.list "{\"folder\":\"$FOLDER_ID\",\"offset\":0,\"limit\":100}"; then
			ids=$(flatten "$TMP/c" | awk -F '\t' '
				$1 ~ /^\.results\[0\]\[1\]\.items\[[0-9]+\]\.id$/ {
					printf "%s\"%s\"", (n++ ? "," : ""), $2
				}')
			if [ -n "$ids" ] && quiet_call message.move "{\"ids\":[$ids],\"folder\":\"inbox\"}"; then
				n=$(flatten "$TMP/c" | awk -F '\t' '
					$1 ~ /^\.results\[0\]\[1\]\.results\[[0-9]+\]\.ok$/ && $2 != "true" { bad++ }
					$1 ~ /^\.results\[0\]\[1\]\.results\[[0-9]+\]\.error$/ { bad++ }
					END { print bad + 0 }')
				if [ "$n" = 0 ]; then
					MSG_ID=$(value_at "$TMP/c" '.results[0][1].results[0].new_id')
					ids=""
				fi
			fi
			if [ -z "$ids" ]; then
				MSG_IN_FOLDER=0
				# Removing a folder moves what it holds to Trash: only
				# an empty one
				quiet_call folder.delete "{\"id\":\"$FOLDER_ID\"}" && FOLDER_ID=""
			else
				left="$left
  the message $MSG_ID is in the folder \"$FOLDER_NAME\": move it back to the Inbox"
			fi
		fi
		[ -n "$FOLDER_ID" ] && left="$left
  the folder \"$FOLDER_NAME\""
	elif [ -n "$FOLDER_NAME" ]; then
		left="$left
  maybe the folder \"$FOLDER_NAME\" (the run stopped while making it)"
	fi
	if [ "$STAR_TURNED" = 1 ]; then
		if quiet_call message.set "{\"ids\":[\"$MSG_ID\"],\"starred\":$MSG_STARRED}" \
			&& [ "$(value_at "$TMP/c" '.results[0][1].results[0].ok')" = "true" ]; then
			STAR_TURNED=0
		else
			left="$left
  the star of the oldest read Inbox message is turned over (it was starred: $MSG_STARRED)"
		fi
	fi
	if [ -n "$DRAFT_ID" ]; then
		if quiet_call draft.delete "{\"id\":\"$DRAFT_ID\"}"; then
			DRAFT_ID=""
		else
			left="$left
  a draft \"timing.sh test draft (safe to delete)\""
		fi
	fi
	if [ -n "$SEND_DRAFT" ] && quiet_call draft.delete "{\"id\":\"$SEND_DRAFT\"}"; then
		SEND_DRAFT=""
	fi
	[ -n "$SEND_DRAFT" ] && left="$left
  a draft \"timing.sh test message\", or the message itself if the run stopped while
  sending it"
	[ "$MAKING_DRAFT" = 1 ] && left="$left
  maybe an empty draft (the run stopped while making it)"
	if [ -n "$left" ]; then
		echo "NOT PUT BACK (the cleanup failed or was stopped; do this by hand):$left" >&2
	fi
	if [ "$SENT" -gt 0 ]; then
		echo "$SENT test message(s) were sent to $SEND_TO: they are in that mailbox" \
			"and in Sent (\"timing.sh test message\")"
		SENT=0
	fi
}

sign_out() {
	if [ "$SIGNED_IN" = 1 ]; then
		echo '{"calls":[["session.logout",null,1]]}' | post "$TMP/logout" > /dev/null || :
		SIGNED_IN=0
	fi
}
cleanup() {
	undo_writes
	sign_out
	rm -rf "$TMP"
}
trap cleanup EXIT
trap 'exit 2' INT TERM HUP

# ---- Sign in
{
	printf '{"calls":[["session.login",{"user":'
	json_str "$USERNAME"
	printf ',"password":'
	json_str "$PASSWORD"
	printf ',"keep":false},1]]}'
} | request "sign-in" "$TMP/login" > /dev/null || exit 2
PASSWORD=
if [ "$(value_at "$TMP/login" '.results[0][1].ok')" != "true" ]; then
	err=$(value_at "$TMP/login" '.results[0][1].error')
	[ -z "$err" ] && err=$(value_at "$TMP/login" '.error')
	echo "sign-in failed (${err:-an unexpected reply})" >&2
	exit 2
fi
SIGNED_IN=1
echo "signed in to $URL"
AS=$(value_at "$TMP/login" '.usernum')
case "$AS" in
'' | *[!0-9]* | 0) echo "sign-in: the reply did not name the user" >&2; exit 2 ;;
esac

# ---- Choose the message and take a state token
REQ_INBOX='{"calls":[["folder.list",null,1],["message.list",{"folder":"inbox","offset":0,"limit":50},2]]}'
echo "$REQ_INBOX" | request "the Inbox" "$TMP/inbox" > /dev/null || exit 2
if [ "$(value_at "$TMP/inbox" '.results[1][1].ok')" != "true" ]; then
	echo "the Inbox could not be listed" >&2
	exit 2
fi
pick=$(flatten "$TMP/inbox" | awk -F '\t' '
	match($1, /^\.results\[1\]\[1\]\.items\[[0-9]+\]\./) {
		n = substr($1, 1, RLENGTH); f = substr($1, RLENGTH + 1)
		if (f == "id") id[n] = $2
		else if (f == "size") size[n] = $2 + 0
	}
	END {
		best = -1
		for (n in id)
			if (size[n] <= 65536 && size[n] > best) { best = size[n]; pick = id[n] }
		if (best >= 0) print pick "\t" best
	}')
if [ -z "$pick" ]; then
	echo "no message of 64 KB or less on the first Inbox page" >&2
	exit 2
fi
GET_ID=${pick%%$'\t'*}
GET_SIZE=${pick#*$'\t'}
STATE=$(value_at "$TMP/inbox" '.state')
INBOX_TOTAL=$(value_at "$TMP/inbox" '.results[1][1].total')
INBOX_DAYS=$(value_at "$TMP/inbox" '.results[0][1].inbox_days')
USED=$(value_at "$TMP/inbox" '.results[0][1].usage.bytes')
QUOTA=$(value_at "$TMP/inbox" '.results[0][1].usage.quota')

# ---- What the writes will use, and the question
MOVES=1
if [ "$WRITES" = 1 ]; then
	# The oldest read message: the Inbox is newest first, so from its
	# last page back (at most 20 pages)
	now=$(date +%s)
	cutoff=0
	if [ "${INBOX_DAYS:-0}" -gt 0 ]; then
		cutoff=$((now - (INBOX_DAYS - 2) * 86400))
	fi
	off=$((${INBOX_TOTAL:-0} - 50))
	[ $off -lt 0 ] && off=0
	pages=0
	while [ -z "$MSG_ID" ] && [ $pages -lt 20 ] && [ "${INBOX_TOTAL:-0}" -gt 0 ]; do
		echo "{\"calls\":[[\"message.list\",{\"folder\":\"inbox\",\"offset\":$off,\"limit\":50},1]]}" \
			| request "the Inbox" "$TMP/page" > /dev/null || exit 2
		found=$(flatten "$TMP/page" | awk -F '\t' -v cutoff="$cutoff" '
			match($1, /^\.results\[0\]\[1\]\.items\[[0-9]+\]\./) {
				n = substr($1, 1, RLENGTH); f = substr($1, RLENGTH + 1)
				i = substr(n, 22) + 0
				if (f == "id") id[i] = $2
				else if (f == "read") rd[i] = $2
				else if (f == "starred") st[i] = $2
				else if (f == "arrived") ar[i] = $2 + 0
				if (i > last) last = i
			}
			END {
				for (i = last; i >= 0; i--)
					if ((i in id) && rd[i] == "true" && ar[i] > cutoff) {
						print id[i] "\t" st[i]
						exit
					}
			}')
		if [ -n "$found" ]; then
			MSG_ID=${found%%$'\t'*}
			MSG_STARRED=${found#*$'\t'}
		fi
		[ $off -eq 0 ] && break
		off=$((off - 50))
		[ $off -lt 0 ] && off=0
		pages=$((pages + 1))
	done
	if [ -z "$MSG_ID" ]; then
		echo "no read message in the Inbox (at least two days short of its age limit)" \
			"to use for the writes" >&2
		exit 2
	fi
	case "$MSG_STARRED" in
	true | false) ;;
	*) echo "the Inbox list did not say whether $MSG_ID is starred" >&2; exit 2 ;;
	esac
	if [ "${QUOTA:-0}" -gt 0 ] && [ $((${USED:-0} + 1048576)) -gt "$QUOTA" ]; then
		MOVES=0
	fi
fi
OWN=""
if [ -n "$SEND_TO" ]; then
	echo '{"calls":[["settings.get",null,1]]}' | request "settings" "$TMP/settings" > /dev/null || exit 2
	OWN=$(flatten "$TMP/settings" | awk -F '\t' -v to="$SEND_TO" '
		$1 == ".results[0][1].from_address_local" { local = $2 }
		$1 ~ /^\.results\[0\]\[1\]\.from_domains\[[0-9]+\]$/ { d[++n] = $2 }
		END {
			to = tolower(to)
			for (i = 1; i <= n; i++)
				if (local != "" && tolower(local "@" d[i]) == to) { print to; exit }
		}')
	if [ -z "$OWN" ]; then
		echo "-S $SEND_TO: not an address of this account; the script only sends to" \
			"the signed-in user's own address" >&2
		exit 2
	fi
fi
if [ "$WRITES" = 1 ] || [ -n "$SEND_TO" ]; then
	echo
	echo "This run changes mail in this account:"
	if [ "$WRITES" = 1 ]; then
		if [ "$MOVES" = 1 ]; then
			echo "- the oldest read Inbox message ($MSG_ID) has its star turned over and back" \
				"$RUNS times and is moved to a test folder and back $((RUNS * 2)) times;" \
				"its read and star state and folder are put back, but it gets a new number" \
				"each time it returns to the Inbox"
		else
			echo "- the oldest read Inbox message ($MSG_ID) has its star turned over and back" \
				"$RUNS times (no moves: the mailbox is within 1 MB of its storage limit)"
		fi
		echo "- a folder \"timing.sh test ... (safe to delete)\" and a draft are made and" \
			"removed at the end"
	fi
	if [ -n "$SEND_TO" ]; then
		echo "- $SENDS message(s) \"timing.sh test message\" are SENT to $SEND_TO;" \
			"they will arrive there and a copy of each is kept in Sent"
	fi
	if [ "$ASK" = 1 ] && [ -t 0 ]; then
		read -r -p "Continue? [y/N] " answer
		case "$answer" in
		y | Y | yes | YES) ;;
		*) echo "nothing changed"; exit 2 ;;
		esac
	fi
	echo
fi

echo "message.get: a message of $GET_SIZE bytes; search term: \"$TERM_WORD\"; $RUNS runs each"
echo

REQ_GET='{"calls":[["message.get",{"id":"'"$GET_ID"'","mark_read":false},1]]}'
REQ_CHANGES='{"calls":[["changes",{"state":'"$(json_str "$STATE")"'},1]]}'
REQ_SEARCH='{"calls":[["message.list",{"folder":"all","offset":0,"limit":50,"search":'"$(json_str "$TERM_WORD")"'},1]]}'

OVER=0
printf '%-15s %8s %8s %8s %8s  %s\n' "request" "median" "fastest" "slowest" "budget" "result"
# summary <name> <budget ms> <times in seconds, one per line>
summary() {
	printf '%s' "$3" | sort -n | awk -v name="$1" -v budget="$2" '
		{ v[NR] = $1 * 1000 }
		END {
			med = (NR % 2) ? v[(NR + 1) / 2] : (v[NR / 2] + v[NR / 2 + 1]) / 2
			printf "%-15s %8.1f %8.1f %8.1f %8d  %s\n", name, med, v[1], v[NR], budget,
				(med <= budget) ? "PASS" : "OVER"
			exit (med <= budget) ? 0 : 1
		}' || OVER=1
}
# row <name> <budget ms> <request body> <path of the result's ok>
row() {
	local name=$1 budget=$2 body=$3 okpath=$4 i t times=""
	for ((i = 0; i < RUNS; i++)); do
		t=$(printf '%s' "$body" | request "$name" "$TMP/reply") || exit 2
		if [ "$(value_at "$TMP/reply" "$okpath")" != "true" ]; then
			echo "$name: the server answered with an error" \
				"($(value_at "$TMP/reply" "${okpath%.ok}.error"))" >&2
			exit 2
		fi
		times="$times$t"$'\n'
	done
	summary "$name" "$budget" "$times"
}
row "folders+inbox" 300 "$REQ_INBOX" '.results[1][1].ok'
row "message.get" 200 "$REQ_GET" '.results[0][1].ok'
row "changes" 100 "$REQ_CHANGES" '.results[0][1].ok'
row "search" 500 "$REQ_SEARCH" '.results[0][1].ok'

# ---- Writes
# Move the message ($1: the folder id as JSON) and keep its new id; the
# time goes to LAST_T
move_to() {
	local nid
	write_call "move" message.move "{\"ids\":[\"$MSG_ID\"],\"folder\":$1}"
	nid=$(moved_id)
	if [ -z "$nid" ]; then
		echo "move: the message $MSG_ID was not moved" \
			"($(value_at "$TMP/w" '.results[0][1].results[0].error'))" >&2
		exit 2
	fi
	MSG_ID=$nid
}
if [ "$WRITES" = 1 ]; then
	not=true
	[ "$MSG_STARRED" = true ] && not=false
	times=""
	for ((i = 0; i < RUNS; i++)); do
		STAR_TURNED=1
		write_call "star" message.set "{\"ids\":[\"$MSG_ID\"],\"starred\":$not}"
		[ -n "$(moved_id)" ] || { echo "star: the message $MSG_ID was not changed" >&2; exit 2; }
		times="$times$LAST_T"$'\n'
		write_call "star" message.set "{\"ids\":[\"$MSG_ID\"],\"starred\":$MSG_STARRED}"
		[ -n "$(moved_id)" ] || { echo "star: the message $MSG_ID was not changed back" >&2; exit 2; }
		STAR_TURNED=0
	done
	summary "star" 200 "$times"

	FOLDER_NAME="timing.sh test $(token | cut -c1-8) (safe to delete)"
	write_call "folder" folder.create "{\"name\":$(json_str "$FOLDER_NAME")}"
	FOLDER_ID=$(value_at "$TMP/w" '.results[0][1].folder.id')
	if [ -z "$FOLDER_ID" ]; then
		echo "folder: no folder id in the reply" >&2
		exit 2
	fi
	if [ "$MOVES" = 1 ]; then
		times=""
		for ((i = 0; i < RUNS; i++)); do
			MSG_IN_FOLDER=1
			move_to "\"$FOLDER_ID\""
			times="$times$LAST_T"$'\n'
			move_to '"inbox"'
			MSG_IN_FOLDER=0
		done
		summary "move" 300 "$times"
		times=""
		for ((i = 0; i < RUNS; i++)); do
			MSG_IN_FOLDER=1
			move_to "\"$FOLDER_ID\""
			t=$LAST_T
			t2=$(echo '{"calls":[["folder.list",null,1]]}' | request "move+folders" "$TMP/reply") || exit 2
			if [ "$(value_at "$TMP/reply" '.results[0][1].ok')" != "true" ]; then
				echo "move+folders: the server answered with an error ($(error_of "$TMP/reply"))" >&2
				exit 2
			fi
			move_to '"inbox"'
			MSG_IN_FOLDER=0
			times="$times$(awk -v a="$t" -v b="$t2" 'BEGIN { printf "%.6f", a + b }')"$'\n'
		done
		summary "move+folders" 400 "$times"
	fi
	write_call "folder" folder.delete "{\"id\":\"$FOLDER_ID\"}"
	FOLDER_ID=""
	FOLDER_NAME=""

	MAKING_DRAFT=1
	write_call "draft" draft.create '{"mode":"new"}'
	DRAFT_ID=$(value_at "$TMP/w" '.results[0][1].draft.id')
	MAKING_DRAFT=0
	REV=$(value_at "$TMP/w" '.results[0][1].draft.rev')
	if [ -z "$DRAFT_ID" ] || [ -z "$REV" ]; then
		echo "draft: no draft id in the reply" >&2
		exit 2
	fi
	# 2,048 bytes of text: a first line, then lines of 64 characters
	body=$(awk 'BEGIN {
		s = "A test draft made by timing.sh; safe to delete.\n"
		line = "The quick brown fox jumps over the lazy dog, again and again..\n"
		while (length(s) + length(line) <= 2048) s = s line
		while (length(s) < 2048) s = s "."
		printf "%s", s
	}')
	BODY_JSON=$(json_str "$body")
	SUBJ_JSON=$(json_str "timing.sh test draft (safe to delete)")
	times=""
	for ((i = 0; i < RUNS; i++)); do
		write_call "draft.save" draft.save \
			"{\"id\":\"$DRAFT_ID\",\"rev\":$REV,\"subject\":$SUBJ_JSON,\"body\":$BODY_JSON}"
		times="$times$LAST_T"$'\n'
		REV=$(value_at "$TMP/w" '.results[0][1].draft.rev')
	done
	summary "draft.save" 150 "$times"

	# 1,000,000 bytes of zeros: "AAAA" for every 3 bytes, "AA==" for the
	# last one
	awk 'BEGIN { n = 333333; u = "AAAA"; s = ""
		while (n > 0) { if (n % 2) s = s u; u = u u; n = int(n / 2) }
		printf "%sAA==", s }' > "$TMP/b64"
	times=""
	for ((i = 0; i < RUNS; i++)); do
		upload=$(token)
		t=$({
			printf '{"calls":[["draft.attach",{"id":"%s","upload":"%s",' "$DRAFT_ID" "$upload"
			printf '"name":"timing-test.bin","type":"application/octet-stream",'
			printf '"offset":0,"total":1000000,"b64":"'
			cat "$TMP/b64"
			printf '"},1]],"as":%s}' "$AS"
		} | request "draft.attach" "$TMP/w") || exit 2
		if [ "$(value_at "$TMP/w" '.results[0][1].ok')" != "true" ]; then
			echo "draft.attach: the server answered with an error ($(error_of "$TMP/w"))" >&2
			exit 2
		fi
		ATTACHMENT=$(value_at "$TMP/w" '.results[0][1].attachment.id')
		if [ "$(value_at "$TMP/w" '.results[0][1].attachment.complete')" != "true" ]; then
			echo "draft.attach: the attachment was not complete after its one piece" >&2
			exit 2
		fi
		write_call "draft.attach" draft.detach \
			"{\"id\":\"$DRAFT_ID\",\"attachment\":\"$ATTACHMENT\"}"
		ATTACHMENT=""
		times="$times$t"$'\n'
	done
	summary "draft.attach" 400 "$times"
	write_call "draft" draft.delete "{\"id\":\"$DRAFT_ID\"}"
	DRAFT_ID=""
fi

# ---- Sending
if [ -n "$SEND_TO" ]; then
	TO_JSON=$(json_str "$OWN")
	times=""
	MSG_TEXT=$(json_str "A message sent by timing.sh to time sending; safe to delete.
")
	for ((i = 1; i <= SENDS; i++)); do
		MAKING_DRAFT=1
		write_call "draft.send" draft.create '{"mode":"new"}'
		SEND_DRAFT=$(value_at "$TMP/w" '.results[0][1].draft.id')
		MAKING_DRAFT=0
		rev=$(value_at "$TMP/w" '.results[0][1].draft.rev')
		subj=$(json_str "timing.sh test message $i of $SENDS (safe to delete)")
		write_call "draft.send" draft.save \
			"{\"id\":\"$SEND_DRAFT\",\"rev\":$rev,\"to\":$TO_JSON,\"subject\":$subj,\"body\":$MSG_TEXT}"
		write_call "draft.send" draft.send "{\"id\":\"$SEND_DRAFT\",\"key\":\"$(token)\"}"
		SEND_DRAFT=""
		SENT=$((SENT + 1))
		times="$times$LAST_T"$'\n'
	done
	summary "draft.send" 500 "$times"
fi

undo_writes
sign_out
exit $OVER
