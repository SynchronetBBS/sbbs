#!/usr/bin/env bash
# timing.sh: times the web mail client's main requests against a running
# web server, the way the browser sends them.  Not run by the test suite.
#
# usage: timing.sh [-k] [-n runs] [-s word] [base-url [user]]
#
#   base-url  the web mail address, e.g. https://bbs.example.com/webmail/
#             (or WEBMAIL_URL); requests go to <base-url>api.ssjs
#   user      the account to sign in as (or WEBMAIL_USER)
#   -n runs   times each request is sent (default 20)
#   -s word   the search term for the search row (default "the", a word
#             likely to be in some subject or name; the cost hardly depends
#             on the number of hits, since every message is looked at)
#   -k        passed to curl: accept a self-signed TLS certificate
#
# The password: let the script prompt for it (not echoed).  It can also
# come from WEBMAIL_PASSWORD, but a WEBMAIL_PASSWORD=... typed on a command
# line is kept in the shell's history; prefer the prompt.  The script never
# puts the password on a command line (curl reads the request from its
# input) and never prints it.  With the require_tls setting on (the
# default), use an https:// address.
#
# Runs under bash 3.2 or later; needs curl, awk, mktemp, tr, cut, sed and
# sort.
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
#
# Each row prints the median, fastest and slowest in milliseconds beside
# its budget for the median, and PASS or OVER.  The exit status is 0 when
# every median is within budget, 1 when one is over, 2 when signing in or
# a request fails (no reply within 60 seconds, an HTTP status other than
# 200, a reply that is not JSON, or an error result).
#
# The script reads only: message.get is sent with mark_read false and
# nothing is moved, flagged or deleted.  As when the client is opened in a
# browser, signing in records a session, folder.list may run the once-a-day
# removal of messages that have been in Trash longer than allowed, and the
# server's caches of list data are brought up to date.  It signs out at
# the end.

set -u

RUNS=20
TERM_WORD="the"
INSECURE=()
while getopts "kn:s:" opt; do
	case "$opt" in
	k) INSECURE=(-k) ;;
	n) RUNS=$OPTARG ;;
	s) TERM_WORD=$OPTARG ;;
	*) sed -n '2,/^$/s/^# \{0,1\}//p' "$0"; exit 2 ;;
	esac
done
shift $((OPTIND - 1))
BASE=${1:-${WEBMAIL_URL:-}}
USERNAME=${2:-${WEBMAIL_USER:-}}
if [ -z "$BASE" ] || [ -z "$USERNAME" ]; then
	echo "usage: $0 [-k] [-n runs] [-s word] base-url user" >&2
	exit 2
fi
case "$RUNS" in
'' | *[!0-9]* | 0) echo "runs must be a positive number" >&2; exit 2 ;;
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

# POST a request body (from stdin) the way the browser does; the reply
# goes to the file $1, "<HTTP status> <total seconds>" to stdout.  The
# exit status is curl's.  (The array test keeps bash 3.2 from calling an
# empty INSECURE unbound under set -u.)
post() {
	curl -s ${INSECURE[@]+"${INSECURE[@]}"} --connect-timeout 10 --max-time 60 \
		-b "$JAR" -c "$JAR" -o "$1" -w '%{http_code} %{time_total}' \
		-H 'Content-Type: application/json' -H 'X-Webmail: 1' \
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

sign_out() {
	if [ "$SIGNED_IN" = 1 ]; then
		echo '{"calls":[["session.logout",null,1]]}' | post "$TMP/logout" > /dev/null || :
		SIGNED_IN=0
	fi
}
cleanup() {
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
MSG_ID=${pick%%$'\t'*}
MSG_SIZE=${pick#*$'\t'}
STATE=$(value_at "$TMP/inbox" '.state')
echo "message.get: a message of $MSG_SIZE bytes; search term: \"$TERM_WORD\"; $RUNS runs each"
echo

REQ_GET='{"calls":[["message.get",{"id":"'"$MSG_ID"'","mark_read":false},1]]}'
REQ_CHANGES='{"calls":[["changes",{"state":'"$(json_str "$STATE")"'},1]]}'
REQ_SEARCH='{"calls":[["message.list",{"folder":"all","offset":0,"limit":50,"search":'"$(json_str "$TERM_WORD")"'},1]]}'

OVER=0
printf '%-15s %8s %8s %8s %8s  %s\n' "request" "median" "fastest" "slowest" "budget" "result"
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
	printf '%s' "$times" | sort -n | awk -v name="$name" -v budget="$budget" '
		{ v[NR] = $1 * 1000 }
		END {
			med = (NR % 2) ? v[(NR + 1) / 2] : (v[NR / 2] + v[NR / 2 + 1]) / 2
			printf "%-15s %8.1f %8.1f %8.1f %8d  %s\n", name, med, v[1], v[NR], budget,
				(med <= budget) ? "PASS" : "OVER"
			exit (med <= budget) ? 0 : 1
		}' || OVER=1
}
row "folders+inbox" 300 "$REQ_INBOX" '.results[1][1].ok'
row "message.get" 200 "$REQ_GET" '.results[0][1].ok'
row "changes" 100 "$REQ_CHANGES" '.results[0][1].ok'
row "search" 500 "$REQ_SEARCH" '.results[0][1].ok'

sign_out
exit $OVER
