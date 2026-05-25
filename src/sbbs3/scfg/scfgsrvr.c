/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "scfg.h"
#include "sbbs_ini.h"
#include "netwrap.h"

const char* strReadingIniFile = "Reading sbbs.ini ...";
const char* strDisabled = "<disabled>";
const char* strDefault = "<default>";
const char* strInfinite = "Infinite";

static const char* threshold(uint val)
{
	static char str[128];
	if (val == 0)
		return strDisabled;
	SAFEPRINTF(str, "%u", val);
	return str;
}

static const char* duration(uint val, bool verbose, const char* zero)
{
	static char str[128];
	if (val == 0 && zero != NULL)
		return zero;
	return verbose ? duration_to_vstr(val, str, sizeof(str)) : duration_to_str(val, str, sizeof(str));;
}

static const char* vduration(uint val, const char* zero)
{
	return duration(val, true, zero);
}

static const char* maximum(uint val)
{
	static char str[128];
	if (val == 0)
		return "Unlimited";
	SAFEPRINTF(str, "%u", val);
	return str;
}

static void login_attempt_cfg(struct login_attempt_settings* login_attempt)
{
	static int cur, bar;
	char       str[256];
	bool       changes = uifc.changes;

	while (1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-30s%u ms", "Delay", login_attempt->delay);
		snprintf(opt[i++], MAX_OPLN, "%-30s%u ms", "Throttle", login_attempt->throttle);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Hack Log Threshold", threshold(login_attempt->hack_threshold));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Temporary Ban Threshold", threshold(login_attempt->tempban_threshold));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Temporary Ban Duration"
		         , vduration(login_attempt->tempban_duration, NULL));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Automatic Filter Threshold", threshold(login_attempt->filter_threshold));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Automatic Filter Duration"
		         , vduration(login_attempt->filter_duration, strInfinite));
		opt[i][0] = '\0';

		uifc.helpbuf =
			"`Failed Login Attempt Settings:`\n"
			"\n"
			"Settings that control the throttling, logging, and subsequent filtering\n"
			"of clients (based on IP address) that have failed login attempts.\n"
			"\n"
			"Temporary Bans of IP addresses (stored in memory) are `not` persistent\n"
			"across reset or recycles of SBBS.\n"
			"\n"
			"Filters of IP Addresses (stored in text/ip.can file) `are` persistent\n"
			"across resets of SBBS but may be configured to have a limited lifetime."
		;
		switch (uifc.list(WIN_ACT | WIN_BOT | WIN_SAV, 0, 0, 0, &cur, &bar
		                  , "Failed Login Attempts", opt)) {
			case 0:
				SAFEPRINTF(str, "%u", login_attempt->delay);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Millisecond Delay After Each Failed Login Attempt", str, 6, K_NUMBER | K_EDIT) > 0)
					login_attempt->delay = atoi(str);
				break;
			case 1:
				SAFEPRINTF(str, "%u", login_attempt->throttle);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Throttle multiplier (in milliseconds) for Failed Logins", str, 6, K_NUMBER | K_EDIT) > 0)
					login_attempt->throttle = atoi(str);
				break;
			case 2:
				SAFEPRINTF(str, "%u", login_attempt->hack_threshold);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Threshold for Logging Failed Logins to hack.log", str, 4, K_NUMBER | K_EDIT) > 0)
					login_attempt->hack_threshold = atoi(str);
				break;
			case 3:
				SAFEPRINTF(str, "%u", login_attempt->tempban_threshold);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Threshold for Temp-Ban of IPs for Failed Logins", str, 4, K_NUMBER | K_EDIT) > 0)
					login_attempt->tempban_threshold = atoi(str);
				break;
			case 4:
				SAFECOPY(str, duration(login_attempt->tempban_duration, false, NULL));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Lifetime of Temporary-Ban of IP", str, 10, K_EDIT) > 0)
					login_attempt->tempban_duration = (uint)parse_duration(str);
				break;
			case 5:
				SAFEPRINTF(str, "%u", login_attempt->filter_threshold);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Threshold for Auto-Filtering of IPs for Failed Logins", str, 3, K_NUMBER | K_EDIT) > 0)
					login_attempt->filter_threshold = atoi(str);
				break;
			case 6:
				SAFECOPY(str, duration(login_attempt->filter_duration, false, strInfinite));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Lifetime of Auto-Filter of IPs", str, 8, K_EDIT) > 0)
					login_attempt->filter_duration = (uint)parse_duration(str);
				break;
			default:
				uifc.changes = changes;
				return;
		}
	}
}

static void max_concurrent_cfg(uint* max, struct max_concurrent_settings* settings)
{
	static int cur, bar;
	char       str[256];
	bool       changes = uifc.changes;

	while (1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Maximum (Unauthenticated)", maximum(*max));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Auto-Filter Threshold", threshold(settings->filter_threshold));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Auto-Filter Duration"
		         , vduration(settings->filter_duration, strInfinite));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Auto-Filter Silently"
		         , settings->filter_silent ? "Yes" : "No");
		opt[i][0] = '\0';

		uifc.helpbuf =
			"`Max Concurrent Connections Settings:`\n"
			"\n"
			"Limit the number of simultaneous unauthenticated connections from a\n"
			"single IP address.  Optionally, IP addresses that hit this limit\n"
			"repeatedly are automatically added to the IP filter (`text/ip.can`).\n"
			"\n"
			"`Maximum`: maximum number of concurrent unauthenticated connections\n"
			"allowed from a single client IP address.\n"
			"\n"
			"`Auto-Filter Threshold`: number of times a client IP may hit the\n"
			"`Maximum` connection limit before being automatically added to the\n"
			"IP filter file.  Set to `0` to disable auto-filtering.\n"
			"\n"
			"`Auto-Filter Duration`: lifetime of an automatically-added entry in\n"
			"`text/ip.can`.  Set to `0` to filter the IP indefinitely.\n"
			"\n"
			"`Auto-Filter Silently`: if `Yes`, abuser IPs are added to\n"
			"`text/ip-silent.can` instead of `text/ip.can`, so subsequent\n"
			"connections from those IPs are dropped without logging a\n"
			"`blocked` notice.\n"
			"\n"
			"The strike counter for an IP is held in memory and is cleared on\n"
			"any of: a successful login from that IP, terminal server recycle\n"
			"or restart, the `clear*.term` semaphore file, or the MQTT `clear`\n"
			"topic.\n"
		;
		switch (uifc.list(WIN_ACT | WIN_BOT | WIN_SAV, 0, 0, 0, &cur, &bar
		                  , "Max Concurrent Connections", opt)) {
			case 0:
				SAFECOPY(str, maximum(*max));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Maximum Concurrent (Unauthenticated) Connections", str, 10, K_EDIT) > 0)
					*max = atoi(str);
				break;
			case 1:
				SAFEPRINTF(str, "%u", settings->filter_threshold);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Threshold for Auto-Filtering of IPs Exceeding Connection Limit", str, 4, K_NUMBER | K_EDIT) > 0)
					settings->filter_threshold = atoi(str);
				break;
			case 2:
				SAFECOPY(str, duration(settings->filter_duration, false, strInfinite));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Lifetime of Auto-Filter of IPs", str, 10, K_EDIT) > 0)
					settings->filter_duration = (uint)parse_duration(str);
				break;
			case 3:
				i = settings->filter_silent ? 0 : 1;
				i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0
				              , "Add Abuser IPs to ip-silent.can (instead of ip.can)", uifcYesNoOpts);
				if (i == 0)
					settings->filter_silent = true;
				else if (i == 1)
					settings->filter_silent = false;
				break;
			default:
				uifc.changes = changes;
				return;
		}
	}
}

/* View struct passed to rate_limit_cfg() so a single SCFG menu function can
 * serve servers with different combinations of rate-limit knobs.  Set any
 * pointer to NULL to hide the corresponding menu item; e.g. FTP/Mail don't
 * have connect-rate limiting, Services doesn't have request-rate limiting. */
struct rate_limit_cfg_view {
	const char*                 server_name;     /* e.g. "Web Server", "FTP Server" */
	uint*                       max_connects;    /* connect-rate limit; NULL = not supported */
	uint*                       connect_period;
	uint*                       max_requests;    /* request-rate limit; NULL = not supported */
	uint*                       request_period;
	struct rate_limit_settings* rl;              /* common subnet + auto-filter knobs */
};

enum rate_limit_cfg_action {
	RLA_NONE,
	RLA_CONNECTS,
	RLA_REQUESTS,
	RLA_PREFIX4,
	RLA_PREFIX6,
	RLA_FILTER_THRESHOLD,
	RLA_FILTER_DURATION,
	RLA_FILTER_SILENT,
	RLA_SUBNET_THRESHOLD,
};

static void rate_limit_cfg(struct rate_limit_cfg_view* view)
{
	static int cur, bar;
	char       title[64];
	char       str[256];
	char       tmp[128];
	bool       changes = uifc.changes;
	int        action[16];  /* parallel to opt[]: maps menu index -> RLA_* */

	snprintf(title, sizeof title, "%s Rate Limiting", view->server_name);

	while (1) {
		int i = 0;
		if (view->max_connects != NULL && view->connect_period != NULL) {
			if (*view->max_connects < 1 || *view->connect_period < 1)
				SAFECOPY(str, strDisabled);
			else
				snprintf(str, sizeof str, "%u per %s", *view->max_connects
				         , duration_to_vstr(*view->connect_period, tmp, sizeof tmp));
			action[i] = RLA_CONNECTS;
			snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Limit Rate of Connections", str);
		}
		if (view->max_requests != NULL && view->request_period != NULL) {
			if (*view->max_requests < 1 || *view->request_period < 1)
				SAFECOPY(str, strDisabled);
			else
				snprintf(str, sizeof str, "%u per %s", *view->max_requests
				         , duration_to_vstr(*view->request_period, tmp, sizeof tmp));
			action[i] = RLA_REQUESTS;
			snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Limit Rate of Requests", str);
		}
		/* Prefix + auto-filter knobs apply on top of an active rate limit; hide
		 * them entirely when no rate limit is enabled, since there is nothing
		 * for them to act on. */
		bool connects_on = view->max_connects != NULL && view->connect_period != NULL
		                   && *view->max_connects > 0 && *view->connect_period > 0;
		bool requests_on = view->max_requests != NULL && view->request_period != NULL
		                   && *view->max_requests > 0 && *view->request_period > 0;
		bool any_rate_limit = connects_on || requests_on;
		if (any_rate_limit) {
			if (view->rl->prefix4 == 0)
				SAFECOPY(str, "Per-host IP address");
			else
				snprintf(str, sizeof str, "/%u subnet", view->rl->prefix4);
			action[i] = RLA_PREFIX4;
			snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Count IPv4 Clients By", str);

			if (view->rl->prefix6 == 0)
				SAFECOPY(str, "Per-host IP address");
			else
				snprintf(str, sizeof str, "/%u subnet", view->rl->prefix6);
			action[i] = RLA_PREFIX6;
			snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Count IPv6 Clients By", str);

			action[i] = RLA_FILTER_THRESHOLD;
			snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Auto-Filter Threshold", threshold(view->rl->filter));
		}
		/* The remaining knobs only configure the auto-filter; hide them when
		 * it is disabled (filter == 0) since they have no effect. */
		bool autofilter_on = any_rate_limit && view->rl->filter > 0;
		if (autofilter_on) {
			action[i] = RLA_FILTER_DURATION;
			snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Auto-Filter Duration"
			         , vduration(view->rl->filter_duration, strInfinite));

			action[i] = RLA_FILTER_SILENT;
			snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Auto-Filter Silently"
			         , view->rl->filter_silent ? "Yes" : "No");

			action[i] = RLA_SUBNET_THRESHOLD;
			snprintf(opt[i++], MAX_OPLN, "%-30s%u", "Subnet Filter Threshold"
			         , view->rl->filter_subnet_threshold);
		}
		opt[i][0] = '\0';

		uifc.helpbuf =
			"`Rate Limiting Settings:`\n"
			"\n"
			"Limit the rate of incoming requests and/or connections from a client,\n"
			"optionally counting all clients within a subnet together, and optionally\n"
			"auto-filtering (blocking) abusers.\n"
			"\n"
			"`Limit Rate of Connections`: maximum connections allowed from a client in\n"
			"the specified period (enforced at accept, before a thread or TLS\n"
			"handshake is spawned - the cheapest point to reject a flood).\n"
			"\n"
			"`Limit Rate of Requests`: maximum requests allowed from a client in the\n"
			"specified period (enforced after the request is parsed).\n"
			"\n"
			"`Count IPv4/IPv6 Clients By`: when set to a subnet prefix length (e.g.\n"
			"`24` for IPv4 or `64` for IPv6), all clients within that subnet are\n"
			"counted together against the limits above and filtered as a block (in\n"
			"CIDR notation).  Use this to defeat distributed abuse spread thinly\n"
			"across many addresses in a hosting provider's range.  `0` counts each\n"
			"host IP address separately.  The IPv6 default is `64` because a typical\n"
			"IPv6 subscriber gets a /64 (or larger) allocation -- counting per-host\n"
			"would let a single attacker cycle freely through addresses they own.\n"
			"The IPv4 default is `0` (per-host).\n"
			"\n"
			"`Auto-Filter Threshold`: number of times a client (or subnet) may exceed\n"
			"a rate limit before being automatically added to the IP filter file.\n"
			"Set to `0` to disable auto-filtering.\n"
			"\n"
			"`Auto-Filter Duration`: lifetime of an automatically-added filter entry.\n"
			"Set to `0` to filter indefinitely.\n"
			"\n"
			"`Auto-Filter Silently`: if `Yes`, abusers are added to\n"
			"`text/ip-silent.can` (dropped at accept, no log notice) instead of\n"
			"`text/ip.can`.\n"
			"\n"
			"`Subnet Filter Threshold`: when subnet aggregation is enabled (above),\n"
			"this is the minimum number of distinct host IPs within the subnet that\n"
			"must trip the rate limit before the entire subnet is auto-filtered;\n"
			"if fewer distinct hosts are responsible, only the offending host IP\n"
			"is filtered, sparing innocent neighbors in the subnet.  Default `2`\n"
			"(one neighbor required); set to `1` to filter the subnet on the first\n"
			"abuser (no neighbor required); raise it for wider prefixes where\n"
			"collateral risk is higher.  Has no effect when both subnet prefixes\n"
			"are `0`.\n"
		;
		int sel = uifc.list(WIN_ACT | WIN_BOT | WIN_SAV, 0, 0, 0, &cur, &bar, title, opt);
		if (sel < 0 || sel >= i) {
			uifc.changes = changes;
			return;
		}
		switch (action[sel]) {
			case RLA_CONNECTS:
				SAFECOPY(str, maximum(*view->max_connects));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Maximum Connections (0=unlimited)", str, 10, K_EDIT | K_NUMBER) > 0)
					*view->max_connects = atoi(str);
				if (*view->max_connects < 1)
					break;
				duration_to_vstr(*view->connect_period, str, sizeof str);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Connection Rate Limit Period", str, 10, K_EDIT) > 0)
					*view->connect_period = (uint)parse_duration(str);
				break;
			case RLA_REQUESTS:
				SAFECOPY(str, maximum(*view->max_requests));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Maximum Requests (0=unlimited)", str, 10, K_EDIT | K_NUMBER) > 0)
					*view->max_requests = atoi(str);
				if (*view->max_requests < 1)
					break;
				duration_to_vstr(*view->request_period, str, sizeof str);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Request Rate Limit Period", str, 10, K_EDIT) > 0)
					*view->request_period = (uint)parse_duration(str);
				break;
			case RLA_PREFIX4:
				SAFEPRINTF(str, "%u", view->rl->prefix4);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "IPv4 Subnet Prefix Bits (0=per-host-IP, e.g. 24)", str, 2, K_NUMBER | K_EDIT) > 0) {
					view->rl->prefix4 = atoi(str);
					if (view->rl->prefix4 > 32)
						view->rl->prefix4 = 32;
				}
				break;
			case RLA_PREFIX6:
				SAFEPRINTF(str, "%u", view->rl->prefix6);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "IPv6 Subnet Prefix Bits (0=per-host-IP, e.g. 64)", str, 3, K_NUMBER | K_EDIT) > 0) {
					view->rl->prefix6 = atoi(str);
					if (view->rl->prefix6 > 128)
						view->rl->prefix6 = 128;
				}
				break;
			case RLA_FILTER_THRESHOLD:
				SAFEPRINTF(str, "%u", view->rl->filter);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Threshold for Auto-Filtering of Rate-Limited Clients", str, 4, K_NUMBER | K_EDIT) > 0)
					view->rl->filter = atoi(str);
				break;
			case RLA_FILTER_DURATION:
				SAFECOPY(str, duration(view->rl->filter_duration, false, strInfinite));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Lifetime of Auto-Filter of Clients", str, 10, K_EDIT) > 0)
					view->rl->filter_duration = (uint)parse_duration(str);
				break;
			case RLA_FILTER_SILENT: {
				int yn = view->rl->filter_silent ? 0 : 1;
				yn = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &yn, 0
				               , "Add Abuser IPs to ip-silent.can (instead of ip.can)", uifcYesNoOpts);
				if (yn == 0)
					view->rl->filter_silent = true;
				else if (yn == 1)
					view->rl->filter_silent = false;
				break;
			}
			case RLA_SUBNET_THRESHOLD:
				SAFEPRINTF(str, "%u", view->rl->filter_subnet_threshold);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0
				               , "Minimum Distinct Abusers in Subnet Before Filtering Whole Subnet"
				               , str, 4, K_NUMBER | K_EDIT) > 0) {
					view->rl->filter_subnet_threshold = atoi(str);
					if (view->rl->filter_subnet_threshold < 1)
						view->rl->filter_subnet_threshold = 1;
				}
				break;
		}
	}
}

static void web_rate_limit_cfg(web_startup_t* startup)
{
	struct rate_limit_cfg_view view = {
		.server_name    = "Web Server",
		.max_connects   = &startup->max_connects_per_period,
		.connect_period = &startup->connect_rate_limit_period,
		.max_requests   = &startup->max_requests_per_period,
		.request_period = &startup->request_rate_limit_period,
		.rl             = &startup->rate_limit,
	};
	rate_limit_cfg(&view);
}

static void ftp_rate_limit_cfg(ftp_startup_t* startup)
{
	struct rate_limit_cfg_view view = {
		.server_name    = "FTP Server",
		.max_requests   = &startup->max_requests_per_period,
		.request_period = &startup->request_rate_limit_period,
		.rl             = &startup->rate_limit,
	};
	rate_limit_cfg(&view);
}

static void mail_rate_limit_cfg(mail_startup_t* startup)
{
	struct rate_limit_cfg_view view = {
		.server_name    = "Mail Server",
		.max_requests   = &startup->max_requests_per_period,
		.request_period = &startup->request_rate_limit_period,
		.rl             = &startup->rate_limit,
	};
	rate_limit_cfg(&view);
}

static void services_rate_limit_cfg(services_startup_t* startup)
{
	struct rate_limit_cfg_view view = {
		.server_name    = "Services",
		.max_connects   = &startup->max_connects_per_period,
		.connect_period = &startup->connect_rate_limit_period,
		.rl             = &startup->rate_limit,
	};
	rate_limit_cfg(&view);
}

static void js_startup_cfg(js_startup_t* js)
{
	static int cur, bar;
	char       str[256];
	bool       changes = uifc.changes;

	while (1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-20s%s bytes", "Heap Size", byte_count_to_str(js->max_bytes, str, sizeof(str)));
		snprintf(opt[i++], MAX_OPLN, "%-20s%u ticks", "Time Limit", js->time_limit);
		snprintf(opt[i++], MAX_OPLN, "%-20s%u ticks", "GC Interval ", js->gc_interval);
		snprintf(opt[i++], MAX_OPLN, "%-20s%u ticks", "Yield Interval", js->yield_interval);
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Load Path", js->load_path);
		opt[i][0] = '\0';

		uifc.helpbuf =
			"`JavaScript Settings:`\n"
			"\n"
			"Limits and tuning knobs for the SpiderMonkey JavaScript runtime that\n"
			"executes server-side scripts (logon/logoff, services, web SSJS, timed\n"
			"events, external programs, etc.).  These values may be set globally\n"
			"and overridden per-server; changes take effect after a server recycle.\n"
			"\n"
			"`Heap Size`: maximum bytes the JavaScript runtime may allocate for one\n"
			"script invocation before a forced garbage collection.  Raise if\n"
			"scripts run out of memory; lower if many concurrent scripts run\n"
			"together and total memory matters more than per-script ceiling.\n"
			"\n"
			"`Time Limit`: maximum number of operation callbacks before a script\n"
			"is forcibly terminated (infinite-loop guard).  Counts JavaScript\n"
			"engine branch operations, NOT wall-clock time, so the actual run-time\n"
			"depends on the script's workload.  `0` disables the limit -- required\n"
			"for long-running service scripts.\n"
			"\n"
			"`GC Interval`: number of operation callbacks between periodic garbage-\n"
			"collection attempts.  Lower = more frequent GC = lower peak memory\n"
			"but more CPU overhead.  `0` disables periodic GC (heap-size forced\n"
			"GC still runs).\n"
			"\n"
			"`Yield Interval`: number of operation callbacks between voluntary CPU\n"
			"yields.  Lets other threads run while a long script executes.\n"
			"`0` disables voluntary yielding.\n"
			"\n"
			"`Load Path`: comma-separated additional directories searched by JS\n"
			"`load()` when a script imports a library (relative to `exec/` and\n"
			"`mods/`).  The built-in `load` directory is always searched."
		;
		switch (uifc.list(WIN_ACT | WIN_BOT | WIN_SAV, 0, 0, 0, &cur, &bar
		                  , "JavaScript Settings", opt)) {
			case 0:
				byte_count_to_str(js->max_bytes, str, sizeof(str));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "JavaScript Heap Size (Maximum Allocated Bytes)", str, 6, K_UPPER | K_EDIT) > 0)
					js->max_bytes = (uint)parse_byte_count(str, 1);
				break;
			case 1:
				SAFEPRINTF(str, "%u", js->time_limit);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "JavaScript Execution Time Limit (in ticks)", str, 6, K_NUMBER | K_EDIT) > 0)
					js->time_limit = atoi(str);
				break;
			case 2:
				SAFEPRINTF(str, "%u", js->gc_interval);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "JavaScript Garbage Collection Interval (in ticks)", str, 6, K_NUMBER | K_EDIT) > 0)
					js->gc_interval = atoi(str);
				break;
			case 3:
				SAFEPRINTF(str, "%u", js->yield_interval);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "JavaScript Yield Interval (in ticks)", str, 6, K_NUMBER | K_EDIT) > 0)
					js->yield_interval = atoi(str);
				break;
			case 4:
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "JavaScript Load Library Path", js->load_path, sizeof(js->load_path) - 1, K_EDIT);
				break;
			default:
				uifc.changes = changes;
				return;
		}
	}
}

static void global_cfg(void)
{
	static int       cur, bar;
	char             str[256];
	char             tmp[256];
	global_startup_t startup = { .size = sizeof startup };

	FILE*            fp = iniOpenFile(cfg.filename, /* for_modify? */ false);
	if (fp == NULL) {
		uifc.msgf("Error opening %s", cfg.filename);
		return;
	}
	uifc.pop(strReadingIniFile);
	bool result = sbbs_read_ini(
		fp
		, cfg.filename
		, &startup
		, NULL
		, NULL //&bbs_startup
		, NULL
		, NULL //&ftp_startup
		, NULL
		, NULL //&web_startup
		, NULL
		, NULL //&mail_startup
		, NULL
		, NULL //&services_startup
		);
	iniCloseFile(fp);
	uifc.pop(NULL);
	if (!result) {
		uifc.msgf("Error reading %s", cfg.filename);
		return;
	}
	global_startup_t saved_startup = startup;

	while (1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Log Level", iniLogLevelStringList()[startup.log_level]);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "TLS Error Level", iniLogLevelStringList()[startup.tls_error_level]);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Network Interfaces (IPv4/6)", strListCombine(startup.interfaces, tmp, sizeof(tmp), ", "));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Outbound Interface (IPv4)", IPv4AddressToStr(startup.outgoing4.s_addr, tmp, sizeof(tmp)));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Bind Retry Count", threshold(startup.bind_retry_count));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Bind Retry Delay", vduration(startup.bind_retry_delay, strDisabled));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Sem File Check Interval", vduration(startup.sem_chk_freq, strDefault));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Login Requirements", startup.login_ars);
		strcpy(opt[i++], "JavaScript Settings...");
		strcpy(opt[i++], "Failed Login Attempts...");
		opt[i][0] = '\0';

		uifc.helpbuf =
			"`Global Server Settings:`\n"
			"\n"
			"Settings that are shared among multiple Synchronet servers.\n"
			"\n"
			"These settings can be over-ridden on a per-server basis via other SCFG\n"
			"Server Configuration menus or by editing the corresponding keys in each\n"
			"`[server]` section of the `ctrl/sbbs.ini` file.\n"
		;
		switch (uifc.list(WIN_ACT | WIN_RHT | WIN_SAV | WIN_ESC, 0, 0, 0, &cur, &bar
		                  , "Global Server Settings", opt)) {
			case 0:
				uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &startup.log_level, 0, "Log Level", iniLogLevelStringList());
				break;
			case 1:
				uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &startup.tls_error_level, 0, "TLS Error Log Level", iniLogLevelStringList());
				break;
			case 2:
				strListCombine(startup.interfaces, str, sizeof(str), ", ");
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Inbound Network Interfaces (IPv4/6)", str, sizeof(str) - 1, K_EDIT) >= 0) {
					strListFree(&startup.interfaces);
					strListSplitCopy(&startup.interfaces, str, ", ");
					uifc.changes = true;
				}
				break;
			case 3:
				IPv4AddressToStr(startup.outgoing4.s_addr, str, sizeof(str));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Outbound Network Interface (IPv4)", str, sizeof(str) - 1, K_EDIT) > 0)
					startup.outgoing4.s_addr = parseIPv4Address(str);
				break;
			case 4:
				SAFECOPY(str, threshold(startup.bind_retry_count));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Port Bind Retry Count", str, 6, K_EDIT) > 0)
					startup.bind_retry_count = atoi(str);
				break;
			case 5:
				SAFECOPY(str, duration(startup.bind_retry_delay, false, strDisabled));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Port Bind Retry Delay", str, 10, K_EDIT) > 0)
					startup.bind_retry_delay = (uint)parse_duration(str);
				break;
			case 6:
				SAFECOPY(str, duration(startup.sem_chk_freq, false, strDefault));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Semaphore File Check Interval", str, 10, K_EDIT) > 0)
					startup.sem_chk_freq = (uint16_t)parse_duration(str);
				break;
			case 7:
				getar("Global Server Login", startup.login_ars, /* helpbuf: */ NULL);
				break;
			case 8:
				js_startup_cfg(&startup.js);
				break;
			case 9:
				login_attempt_cfg(&startup.login_attempt);
				break;
			default:
				if (memcmp(&saved_startup, &startup, sizeof(startup)) != 0)
					uifc.changes = true;
				i = save_changes(WIN_MID);
				if (i < 0)
					continue;
				if (i == 0) {
					FILE* fp = iniOpenFile(cfg.filename, /* for_modify? */ true);
					if (fp == NULL)
						uifc.msgf("Error opening %s", cfg.filename);
					else {
						if (!sbbs_write_ini(
								fp
								, &cfg
								, &startup
								, false
								, NULL
								, false
								, NULL
								, false
								, NULL
								, false
								, NULL
								, false
								, NULL
								))
							uifc.msgf("Error writing %s", cfg.filename);
						iniCloseFile(fp);
					}
				}
				sbbs_free_ini(&startup
				              , NULL //&bbs_startup
				              , NULL //&ftp_startup
				              , NULL //&web_startup
				              , NULL //&mail_startup
				              , NULL //&services_startup
				              );
				return;
		}
	}
}

static void telnet_srvr_cfg(bbs_startup_t* startup)
{
	static int cur, bar;
	char       str[256];
	char       tmp[256];

	while (1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Enabled", startup->options & BBS_OPT_NO_TELNET ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Interfaces"
		         , startup->options & BBS_OPT_NO_TELNET ? "N/A" : strListCombine(startup->telnet_interfaces, tmp, sizeof(tmp), ", "));
		snprintf(opt[i++], MAX_OPLN, "%-20s%u", "Port", startup->telnet_port);
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Command Debug"
		         , startup->options & BBS_OPT_NO_TELNET ? "N/A" : startup->options & BBS_OPT_DEBUG_TELNET ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Send Go-Aheads"
		         , startup->options & BBS_OPT_NO_TELNET ? "N/A" : startup->options & BBS_OPT_NO_TELNET_GA ? "No" : "Yes");
		opt[i][0] = '\0';

		switch (uifc.list(WIN_ACT | WIN_ESC | WIN_MID | WIN_SAV, 0, 0, 0, &cur, &bar
		                  , "Telnet Support", opt)) {
			case 0:
				startup->options ^= BBS_OPT_NO_TELNET;
				break;
			case 1:
				if (startup->options & BBS_OPT_NO_TELNET)
					break;
				strListCombine(startup->telnet_interfaces, str, sizeof(str), ", ");
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Telnet Network Interfaces (IPv4/6)", str, sizeof(str) - 1, K_EDIT) >= 0) {
					strListFree(&startup->telnet_interfaces);
					strListSplitCopy(&startup->telnet_interfaces, str, ", ");
					uifc.changes = true;
				}
				break;
			case 2:
				if (startup->options & BBS_OPT_NO_TELNET)
					break;
				SAFEPRINTF(str, "%u", startup->telnet_port);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Telnet TCP Port", str, 5, K_NUMBER | K_EDIT) > 0)
					startup->telnet_port = atoi(str);
				break;
			case 3:
				if (startup->options & BBS_OPT_NO_TELNET)
					break;
				startup->options ^= BBS_OPT_DEBUG_TELNET;
				break;
			case 4:
				if (startup->options & BBS_OPT_NO_TELNET)
					break;
				startup->options ^= BBS_OPT_NO_TELNET_GA;
				break;
			default:
				return;
		}
	}
}

static void ssh_srvr_cfg(bbs_startup_t* startup)
{
	static int cur, bar;
	char       str[256];
	char       tmp[256];

	while (1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Enabled", startup->options & BBS_OPT_ALLOW_SSH ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Interfaces"
		         , startup->options & BBS_OPT_ALLOW_SSH ? strListCombine(startup->ssh_interfaces, tmp, sizeof(tmp), ", ") : "N/A");
		snprintf(opt[i++], MAX_OPLN, "%-30s%u", "Port", startup->ssh_port);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Connect Timeout"
		         , startup->options & BBS_OPT_ALLOW_SSH ? vduration(startup->ssh_connect_timeout, NULL) : "N/A");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Error Level"
		         , startup->options & BBS_OPT_ALLOW_SSH ? iniLogLevelStringList()[startup->ssh_error_level] : "N/A");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "User Authentication Type"
		         , startup->options & BBS_OPT_ALLOW_SSH ? (startup->options & BBS_OPT_SSH_ANYAUTH ? "Any" : "Valid Key or Username") : "N/A");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "File Transfer (SFTP) Support"
		         , startup->options & BBS_OPT_ALLOW_SSH ? (startup->options & BBS_OPT_ALLOW_SFTP ? "Yes" : "No") : "N/A");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max SFTP Inactivity"
		         , (startup->options & BBS_OPT_ALLOW_SSH) && (startup->options & BBS_OPT_ALLOW_SFTP) ? vduration(startup->max_sftp_inactivity, strDisabled) : "N/A");

		opt[i][0] = '\0';

		switch (uifc.list(WIN_ACT | WIN_ESC | WIN_MID | WIN_SAV, 0, 0, 0, &cur, &bar
		                  , "SSH Support", opt)) {
			case 0:
				startup->options ^= BBS_OPT_ALLOW_SSH;
				break;
			case 1:
				if (!(startup->options & BBS_OPT_ALLOW_SSH))
					break;
				strListCombine(startup->ssh_interfaces, str, sizeof(str), ", ");
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "SSH Network Interfaces (IPv4/6)", str, sizeof(str) - 1, K_EDIT) >= 0) {
					strListFree(&startup->ssh_interfaces);
					strListSplitCopy(&startup->ssh_interfaces, str, ", ");
					uifc.changes = true;
				}
				break;
			case 2:
				if (!(startup->options & BBS_OPT_ALLOW_SSH))
					break;
				SAFEPRINTF(str, "%u", startup->ssh_port);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "SSH TCP Port", str, 5, K_NUMBER | K_EDIT) > 0)
					startup->ssh_port = atoi(str);
				break;
			case 3:
				if (!(startup->options & BBS_OPT_ALLOW_SSH))
					break;
				SAFECOPY(str, duration(startup->ssh_connect_timeout, false, NULL));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "SSH Connect Timeout", str, 6, K_EDIT) > 0)
					startup->ssh_connect_timeout = (uint16_t)parse_duration(str);
				break;
			case 4:
				if (!(startup->options & BBS_OPT_ALLOW_SSH))
					break;
				uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &startup->ssh_error_level, 0, "SSH Error Log Level", iniLogLevelStringList());
				break;
			case 5:
				if (!(startup->options & BBS_OPT_ALLOW_SSH))
					break;
				startup->options ^= BBS_OPT_SSH_ANYAUTH;
				break;
			case 6:
				if (!(startup->options & BBS_OPT_ALLOW_SSH))
					break;
				startup->options ^= BBS_OPT_ALLOW_SFTP;
				break;
			case 7:
				if (!(startup->options & BBS_OPT_ALLOW_SSH))
					break;
				if (!(startup->options & BBS_OPT_ALLOW_SFTP))
					break;
				SAFECOPY(str, duration(startup->max_sftp_inactivity, false, strDisabled));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Maximum Socket Inactivity during SFTP Session", str, 10, K_EDIT) > 0)
					startup->max_sftp_inactivity = (uint16_t)parse_duration(str);
				break;
			default:
				return;
		}
	}
}

static void rlogin_srvr_cfg(bbs_startup_t* startup)
{
	static int cur, bar;
	char       str[256];
	char       tmp[256];

	while (1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Enabled", startup->options & BBS_OPT_ALLOW_RLOGIN ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Interfaces"
		         , startup->options & BBS_OPT_ALLOW_RLOGIN ? strListCombine(startup->rlogin_interfaces, tmp, sizeof(tmp), ", ") : "N/A");
		snprintf(opt[i++], MAX_OPLN, "%-20s%u", "Port", startup->rlogin_port);
		opt[i][0] = '\0';

		switch (uifc.list(WIN_ACT | WIN_ESC | WIN_MID | WIN_SAV, 0, 0, 0, &cur, &bar
		                  , "RLogin Support", opt)) {
			case 0:
				startup->options ^= BBS_OPT_ALLOW_RLOGIN;
				break;
			case 1:
				if (!(startup->options & BBS_OPT_ALLOW_RLOGIN))
					break;
				strListCombine(startup->rlogin_interfaces, str, sizeof(str), ", ");
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "RLogin Network Interfaces (IPv4/6)", str, sizeof(str) - 1, K_EDIT) >= 0) {
					strListFree(&startup->rlogin_interfaces);
					strListSplitCopy(&startup->rlogin_interfaces, str, ", ");
					uifc.changes = true;
				}
				break;
			case 2:
				if (!(startup->options & BBS_OPT_ALLOW_RLOGIN))
					break;
				SAFEPRINTF(str, "%u", startup->rlogin_port);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "RLogin TCP Port", str, 5, K_NUMBER | K_EDIT) > 0)
					startup->rlogin_port = atoi(str);
				break;
			default:
				return;
		}
	}
}

static void termsrvr_cfg(void)
{
	static int    cur, bar;
	char          str[256];
	bool          enabled = false;
	bbs_startup_t startup = { .size = sizeof startup };

	FILE*         fp = iniOpenFile(cfg.filename, /* for_modify? */ false);
	if (fp == NULL) {
		uifc.msgf("Error opening %s", cfg.filename);
		return;
	}
	uifc.pop(strReadingIniFile);
	bool result = sbbs_read_ini(
		fp
		, cfg.filename
		, NULL
		, &enabled
		, &startup
		, NULL
		, NULL //&ftp_startup
		, NULL
		, NULL //&web_startup
		, NULL
		, NULL //&mail_startup
		, NULL
		, NULL //&services_startup
		);
	iniCloseFile(fp);
	uifc.pop(NULL);
	if (!result) {
		uifc.msgf("Error reading %s", cfg.filename);
		return;
	}
	bbs_startup_t saved_startup = startup;

	while (1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Enabled", enabled ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Log Level", iniLogLevelStringList()[startup.log_level]);
		snprintf(str, sizeof str, "%u-%u", startup.first_node, startup.last_node);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Serving Nodes", str);
		snprintf(str, sizeof str, "Port %u", startup.ssh_port);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "SSH Support...", startup.options & BBS_OPT_ALLOW_SSH ? str : strDisabled);
		snprintf(str, sizeof str, "Port %u", startup.telnet_port);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Telnet Support...", startup.options & BBS_OPT_NO_TELNET ? strDisabled : str);
		snprintf(str, sizeof str, "Port %u", startup.rlogin_port);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "RLogin Support...", startup.options & BBS_OPT_ALLOW_RLOGIN ? str : strDisabled);
		snprintf(str, sizeof str, "Port %u", startup.pet40_port);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "40 Column PETSCII Support", startup.pet40_port ? str : strDisabled );
		snprintf(str, sizeof str, "Port %u", startup.pet80_port);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "80 Column PETSCII Support", startup.pet80_port  ? str : strDisabled);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "DOS Program Support", startup.options & BBS_OPT_NO_DOS ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max Concurrent Connections", maximum(startup.max_concurrent_connections));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max Dumb Login Inactivity", vduration(startup.max_dumbterm_inactivity, strDisabled));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max User Login Inactivity", vduration(startup.max_login_inactivity, strDisabled));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max New User Inactivity", vduration(startup.max_newuser_inactivity, strDisabled));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max User Inactivity", vduration(startup.max_session_inactivity, strDisabled));
		snprintf(opt[i++], MAX_OPLN, "%-30s%u ms", "Output Buffer Drain Timeout", startup.outbuf_drain_timeout);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Execute Timed Events", startup.options & BBS_OPT_NO_EVENTS ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Execute QWK-related Events"
		         , startup.options & BBS_OPT_NO_EVENTS ? "N/A" : startup.options & BBS_OPT_NO_QWK_EVENTS ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Event Log Level"
		         , startup.options & BBS_OPT_NO_EVENTS ? "N/A" : iniLogLevelStringList()[startup.event_log_level]);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Lookup Client Hostname", startup.options & BBS_OPT_NO_HOST_LOOKUP ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Login Requirements", startup.login_ars);
		strcpy(opt[i++], "JavaScript Settings...");
		strcpy(opt[i++], "Failed Login Attempts...");
		opt[i][0] = '\0';

		uifc.helpbuf =
			"`Terminal Server Settings:`\n"
			"\n"
			"The initialization settings of the Synchronet server that provides the\n"
			"traditional BBS experience over `Telnet`, `SSH`, `RLogin`, or `Raw TCP`\n"
			"protocols.\n"
			"\n"
			"For full documentation, see `http://wiki.synchro.net/server:terminal`\n"
		;
		switch (uifc.list(WIN_ACT | WIN_ESC | WIN_RHT | WIN_SAV, 0, 0, 0, &cur, &bar
		                  , "Terminal Server Settings", opt)) {
			case 0:
				enabled = !enabled;
				uifc.changes = true;
				break;
			case 1:
				uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &startup.log_level, 0, "Log Level", iniLogLevelStringList());
				break;
			case 2:
				SAFEPRINTF(str, "%u", startup.first_node);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "First Node Available For Terminal Logins", str, 3, K_NUMBER | K_EDIT) < 1)
					break;
				startup.first_node = atoi(str);
				SAFEPRINTF(str, "%u", startup.last_node);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Last Node Available For Terminal Logins", str, 3, K_NUMBER | K_EDIT) > 0)
					startup.last_node = atoi(str);
				break;
			case 3:
				ssh_srvr_cfg(&startup);
				break;
			case 4:
				telnet_srvr_cfg(&startup);
				break;
			case 5:
				rlogin_srvr_cfg(&startup);
				break;
			case 6:
				SAFEPRINTF(str, "%u", startup.pet40_port);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "40 Column CBM/PETSCII TCP Port", str, 5, K_NUMBER | K_EDIT) > 0)
					startup.pet40_port = atoi(str);
				break;
			case 7:
				SAFEPRINTF(str, "%u", startup.pet80_port);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "80 Column CBM/PETSCII TCP Port", str, 5, K_NUMBER | K_EDIT) > 0)
					startup.pet80_port = atoi(str);
				break;
			case 8:
				startup.options ^= BBS_OPT_NO_DOS;
				break;
			case 9:
				max_concurrent_cfg(&startup.max_concurrent_connections, &startup.max_concurrent);
				break;
#define SOCKET_INACTIVITY_HELP  "\n" \
		"An `Inactivity Alert` (by default, 3 BELLs) can be sent to the client\n" \
		"before socket disconnection by setting `User Inactivity Warning` in\n" \
		"`System->Advanced Options` (by default, `75 percent` of maximum inactivity).\n"
#define EXTRA_INACTIVITY_HELP  "\n" \
		"For higher-level inactive user warning/detection/disconnection, see the\n" \
		"`Maximum User Inactivity` setting in `System->Advanced Options`.\n" \
		"Normally, if enabled, this socket inactivity duration should be `longer`\n" \
		"than the `Maximum User Inactivity` setting in `System->Advanced Options`.\n"
			case 10:
				uifc.helpbuf =
					"`Maximum Socket Inactivity for Dumb Terminal Login:`\n"
					"\n"
					"This is the duration of time the socket must be inactive before the\n"
					"client will be automatically disconnected while a dumb terminal is\n"
					"attempting to login.  Since dumb terminal connections are usually\n"
					"from door knocking or brute force attack bots, it is best to have\n"
					"this duration set low to free up Terminal Server Nodes for \"real users\"\n"
					"more quickly.\n"
					"\n"
					"A setting of `0` will disable the socket inactivity detection feature\n"
					"during dumb terminal logins.\n"
					"\n"
					"The default setting is `1 minute`.\n"
					SOCKET_INACTIVITY_HELP
				;
				SAFECOPY(str, duration(startup.max_dumbterm_inactivity, false, strDisabled));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Maximum Socket Inactivity for Dumb Terminal Login", str, 10, K_EDIT) > 0)
					startup.max_dumbterm_inactivity = (uint16_t)parse_duration(str);
				break;
			case 11:
				uifc.helpbuf =
					"`Maximum Socket Inactivity for User Terminal Login:`\n"
					"\n"
					"This is the duration of time the socket must be inactive before the\n"
					"client will be automatically disconnected while an apparent \"real user\"\n"
					"is attempting to login via an auto-detected terminal type (e.g. ANSI).\n"
					"\n"
					"A setting of `0` will disable this socket inactivity detection feature\n"
					"during user terminal logins.\n"
					"\n"
					"The default setting is `10 minutes`.\n"
					SOCKET_INACTIVITY_HELP
					EXTRA_INACTIVITY_HELP
				;
				SAFECOPY(str, duration(startup.max_login_inactivity, false, strDisabled));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Maximum Socket Inactivity for User Terminal Login", str, 10, K_EDIT) > 0)
					startup.max_login_inactivity = (uint16_t)parse_duration(str);
				break;
			case 12:
				uifc.helpbuf =
					"`Maximum Socket Inactivity at New User Registration:`\n"
					"\n"
					"This is the duration of time the socket must be inactive before the\n"
					"client will be automatically disconnected while a new user is\n"
					"creating a new user account (registering).\n"
					"\n"
					"A setting of `0` will disable the socket inactivity detection feature\n"
					"during new user registration.\n"
					"\n"
					"The default setting is `60 minutes`.\n"
					SOCKET_INACTIVITY_HELP
					EXTRA_INACTIVITY_HELP
				;
				SAFECOPY(str, duration(startup.max_newuser_inactivity, false, strDisabled));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Maximum Socket Inactivity at New User Registration", str, 10, K_EDIT) > 0)
					startup.max_newuser_inactivity = (uint16_t)parse_duration(str);
				break;
			case 13:
				uifc.helpbuf =
					"`Maximum Socket Inactivity during User Session:`\n"
					"\n"
					"This is the duration of time the socket must be inactive before the\n"
					"client will be automatically disconnected after a user has authenticated\n"
					"and successfully logged-in.\n"
					"\n"
					"A setting of `0` will disable the socket inactivity detection feature\n"
					"during user sessions.  The default setting is `10 minutes`.\n"
					SOCKET_INACTIVITY_HELP
					EXTRA_INACTIVITY_HELP
					"\n"
					"`H`-exempt users will not be disconnected due to socket inactivity."
				;
				SAFECOPY(str, duration(startup.max_session_inactivity, false, strDisabled));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Maximum Socket Inactivity during User Session", str, 10, K_EDIT) > 0)
					startup.max_session_inactivity = (uint16_t)parse_duration(str);
				break;
			case 14:
				SAFEPRINTF(str, "%u", startup.outbuf_drain_timeout);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Output Buffer Drain Timeout (milliseconds)", str, 5, K_NUMBER | K_EDIT) > 0)
					startup.outbuf_drain_timeout = atoi(str);
				break;
			case 15:
				startup.options ^= BBS_OPT_NO_EVENTS;
				break;
			case 16:
				if (startup.options & BBS_OPT_NO_EVENTS)
					break;
				startup.options ^= BBS_OPT_NO_QWK_EVENTS;
				break;
			case 17:
				if (startup.options & BBS_OPT_NO_EVENTS)
					break;
				uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &startup.event_log_level, 0, "Event Log Level", iniLogLevelStringList());
				break;
			case 18:
				startup.options ^= BBS_OPT_NO_HOST_LOOKUP;
				break;
			case 19:
				getar("Terminal Server Login", startup.login_ars, /* helpbuf: */ NULL);
				break;
			case 20:
				js_startup_cfg(&startup.js);
				break;
			case 21:
				login_attempt_cfg(&startup.login_attempt);
				break;
			default:
				if (memcmp(&saved_startup, &startup, sizeof(startup)) != 0)
					uifc.changes = true;
				i = save_changes(WIN_MID);
				if (i < 0)
					continue;
				if (i == 0) {
					FILE* fp = iniOpenFile(cfg.filename, /* for_modify? */ true);
					if (fp == NULL)
						uifc.msgf("Error opening %s", cfg.filename);
					else {
						if (!sbbs_write_ini(
								fp
								, &cfg
								, NULL
								, enabled
								, &startup
								, false
								, NULL
								, false
								, NULL
								, false
								, NULL
								, false
								, NULL
								))
							uifc.msgf("Error writing %s", cfg.filename);
						iniCloseFile(fp);
					}
				}
				sbbs_free_ini(NULL
				              , &startup
				              , NULL //&ftp_startup
				              , NULL //&web_startup
				              , NULL //&mail_startup
				              , NULL //&services_startup
				              );
				return;
		}
	}
}

static void http_srvr_cfg(web_startup_t* startup)
{
	static int cur, bar;
	char       tmp[256];
	char       str[256];

	while (1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Enabled", startup->options & WEB_OPT_NO_HTTP ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Interfaces"
		         , startup->options & WEB_OPT_NO_HTTP ? "N/A" : strListCombine(startup->interfaces, tmp, sizeof(tmp), ", "));
		snprintf(opt[i++], MAX_OPLN, "%-20s%u", "Port", startup->port);
		opt[i][0] = '\0';

		switch (uifc.list(WIN_ACT | WIN_ESC | WIN_MID | WIN_SAV, 0, 0, 0, &cur, &bar
		                  , "HTTP Support", opt)) {
			case 0:
				startup->options ^= WEB_OPT_NO_HTTP;
				break;
			case 1:
				if (startup->options & WEB_OPT_NO_HTTP)
					break;
				strListCombine(startup->interfaces, str, sizeof(str), ", ");
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "HTTP Network Interfaces (IPv4/6)", str, sizeof(str) - 1, K_EDIT) >= 0) {
					strListFree(&startup->interfaces);
					strListSplitCopy(&startup->interfaces, str, ", ");
					uifc.changes = true;
				}
				break;
			case 2:
				if (startup->options & WEB_OPT_NO_HTTP)
					break;
				SAFEPRINTF(str, "%u", startup->port);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "HTTP TCP Port", str, 5, K_NUMBER | K_EDIT) > 0)
					startup->port = atoi(str);
				break;
			default:
				return;
		}
	}
}

static void https_srvr_cfg(web_startup_t* startup)
{
	static int cur, bar;
	char       tmp[256];
	char       str[256];

	while (1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Enabled", startup->options & WEB_OPT_ALLOW_TLS  ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Interfaces"
		         , startup->options & WEB_OPT_ALLOW_TLS ? strListCombine(startup->tls_interfaces, tmp, sizeof(tmp), ", ") : "N/A");
		snprintf(opt[i++], MAX_OPLN, "%-30s%u", "Port", startup->tls_port);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Strict Transport Security"
		         , startup->options & WEB_OPT_ALLOW_TLS ? (startup->options & WEB_OPT_HSTS_SAFE ? "Yes" : "No") : "N/A");
		opt[i][0] = '\0';

		switch (uifc.list(WIN_ACT | WIN_ESC | WIN_MID | WIN_SAV, 0, 0, 0, &cur, &bar
		                  , "HTTPS Support", opt)) {
			case 0:
				startup->options ^= WEB_OPT_ALLOW_TLS;
				break;
			case 1:
				if (!(startup->options & WEB_OPT_ALLOW_TLS))
					break;
				strListCombine(startup->tls_interfaces, str, sizeof(str), ", ");
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "HTTP/TLS (HTTPS) Network Interfaces (IPv4/6)", str, sizeof(str) - 1, K_EDIT) >= 0) {
					strListFree(&startup->tls_interfaces);
					strListSplitCopy(&startup->tls_interfaces, str, ", ");
					uifc.changes = true;
				}
				break;
			case 2:
				if (!(startup->options & WEB_OPT_ALLOW_TLS))
					break;
				SAFEPRINTF(str, "%u", startup->tls_port);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "HTTP/TLS (HTTPS) TCP Port", str, 5, K_NUMBER | K_EDIT) > 0)
					startup->tls_port = atoi(str);
				break;
			case 3:
				if (!(startup->options & WEB_OPT_ALLOW_TLS))
					break;
				startup->options ^= WEB_OPT_HSTS_SAFE;
				break;
			default:
				return;
		}
	}
}

static void websrvr_cgi_cfg(web_startup_t* startup)
{
	static int cur, bar;
	char       tmp[256];
	char       str[256];

	while (1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-25s%s", "Enabled",  startup->options & WEB_OPT_NO_CGI ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-25s%s", "Directory", startup->cgi_dir);
		snprintf(opt[i++], MAX_OPLN, "%-25s%s", "File Extensions", strListCombine(startup->cgi_ext, tmp, sizeof(tmp), ", "));
		snprintf(opt[i++], MAX_OPLN, "%-25s%s", "Default Content-Type", startup->default_cgi_content);
		snprintf(opt[i++], MAX_OPLN, "%-25s%s", "Max Inactivity", vduration(startup->max_cgi_inactivity, strDefault));
		opt[i][0] = '\0';

		switch (uifc.list(WIN_ACT | WIN_ESC | WIN_MID | WIN_SAV, 0, 0, 0, &cur, &bar
		                  , "Web Server CGI Support", opt)) {
			case 0:
				startup->options ^= WEB_OPT_NO_CGI;
				break;
			case 1:
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "CGI Directory"
				           , startup->cgi_dir, sizeof(startup->cgi_dir) - 1, K_EDIT);
				break;
			case 2:
				strListCombine(startup->cgi_ext, str, sizeof(str), ", ");
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "CGI File Extensions", str, sizeof(str) - 1, K_EDIT) >= 0) {
					strListFree(&startup->cgi_ext);
					strListSplitCopy(&startup->cgi_ext, str, ", ");
					uifc.changes = true;
				}
				break;
			case 3:
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Default CGI MIME Content-Type"
				           , startup->default_cgi_content, sizeof(startup->default_cgi_content) - 1, K_EDIT);
				break;
			case 4:
				SAFECOPY(str, duration(startup->max_cgi_inactivity, false, strDefault));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Maximum CGI Inactivity", str, 10, K_EDIT) > 0)
					startup->max_cgi_inactivity = (uint16_t)parse_duration(str);
				break;
			default:
				return;
		}
	}
}

static void websrvr_filebase_cfg(web_startup_t* startup)
{
	static int cur, bar;

	while (1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Enabled",  startup->options & WEB_OPT_NO_FILEBASE ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Index Script", startup->file_index_script);
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "VPath Prefix", startup->file_vpath_prefix);
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "VPath for VHosts"
		         , startup->options & WEB_OPT_VIRTUAL_HOSTS ? startup->file_vpath_for_vhosts ? "Yes" : "No" : "N/A");
		opt[i][0] = '\0';

		switch (uifc.list(WIN_ACT | WIN_ESC | WIN_MID | WIN_SAV, 0, 0, 0, &cur, &bar
		                  , "Web Server Filebase Support", opt)) {
			case 0:
				startup->options ^= WEB_OPT_NO_FILEBASE;
				break;
			case 1:
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Filebase Index Script"
				           , startup->file_index_script, sizeof(startup->file_index_script) - 1, K_EDIT);
				break;
			case 2:
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Filebase Virtual Path Prefix"
				           , startup->file_vpath_prefix, sizeof(startup->file_vpath_prefix) - 1, K_EDIT);
				break;
			case 3:
				startup->file_vpath_for_vhosts = !startup->file_vpath_for_vhosts;
				break;
			default:
				return;
		}
	}
}

static void websrvr_cfg(void)
{
	static int    cur, bar;
	char          tmp[256];
	char          str[256];
	bool          enabled = false;
	web_startup_t startup = { .size = sizeof startup };

	FILE*         fp = iniOpenFile(cfg.filename, /* for_modify? */ false);
	if (fp == NULL) {
		uifc.msgf("Error opening %s", cfg.filename);
		return;
	}
	uifc.pop(strReadingIniFile);
	bool result = sbbs_read_ini(
		fp
		, cfg.filename
		, NULL
		, NULL
		, NULL
		, NULL
		, NULL //&ftp_startup
		, &enabled
		, &startup
		, NULL
		, NULL //&mail_startup
		, NULL
		, NULL //&services_startup
		);
	iniCloseFile(fp);
	uifc.pop(NULL);
	if (!result) {
		uifc.msgf("Error reading %s", cfg.filename);
		return;
	}
	web_startup_t saved_startup = startup;

	while (1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Enabled", enabled ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Log Level", iniLogLevelStringList()[startup.log_level]);
		snprintf(str, sizeof str, "Port %u", startup.port);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "HTTP Support...", startup.options & WEB_OPT_NO_HTTP ? strDisabled : str);
		snprintf(str, sizeof str, "Port %u", startup.tls_port);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "HTTPS Support...",  startup.options & WEB_OPT_ALLOW_TLS ? str : strDisabled);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "SSJS File Extension", startup.ssjs_ext);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Index Filenames", strListCombine(startup.index_file_name, tmp, sizeof(tmp), ", "));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Content Root Directory", startup.root_dir);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Error Sub-directory", startup.error_dir);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Virtual Host Support", startup.options & WEB_OPT_VIRTUAL_HOSTS ? "Yes" : "No");
		const char* host = "";
		if ((startup.options & WEB_OPT_VIRTUAL_HOSTS) && !(startup.options & WEB_OPT_ONE_HTTP_LOG))
			host = "<host>-";
		if (*startup.logfile_base == '\0')
			snprintf(str, sizeof str, "[%slogs/http-%s<date>.log]", cfg.logs_dir, host);
		else
			snprintf(str, sizeof str, "%s-%s<date>.log", startup.logfile_base, host);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Access Logging", startup.options & WEB_OPT_HTTP_LOGGING ? str : strDisabled);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Access Log Format"
			, startup.options & WEB_OPT_HTTP_LOGGING ? (*startup.custom_log_fmt ? startup.custom_log_fmt : "<Combined Log Format>") : "N/A");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max Clients", maximum(startup.max_clients));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max Inactivity", vduration(startup.max_inactivity, strDefault));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max Concurrent Connections", maximum(startup.max_concurrent_connections));
		str[0] = '\0';
		if (startup.max_connects_per_period > 0 && startup.connect_rate_limit_period > 0)
			SAFECOPY(str, "Connections");
		if (startup.max_requests_per_period > 0 && startup.request_rate_limit_period > 0) {
			if (str[0] != '\0')
				SAFECAT(str, " + Requests");
			else
				SAFECOPY(str, "Requests");
		}
		if (str[0] == '\0')
			SAFECOPY(str, strDisabled);
		else if (startup.rate_limit.filter > 0)
			SAFECAT(str, ", Auto-Filter");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Rate Limiting...", str);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Authentication Methods", startup.default_auth_list);
		snprintf(opt[i++], MAX_OPLN, "%-30s%u ms", "Output Buffer Drain Timeout", startup.outbuf_drain_timeout);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Lookup Client Hostname", startup.options & BBS_OPT_NO_HOST_LOOKUP ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "CGI Support...",  startup.options & WEB_OPT_NO_CGI ? strDisabled : startup.cgi_dir);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Filebase Support...", startup.options & WEB_OPT_NO_FILEBASE ? strDisabled: startup.file_vpath_prefix);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Login Requirements", startup.login_ars);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Login Info Save", startup.login_info_save);
		strcpy(opt[i++], "JavaScript Settings...");
		strcpy(opt[i++], "Failed Login Attempts...");
		opt[i][0] = '\0';

		uifc.helpbuf =
			"`Web Server Settings:`\n"
			"\n"
			"The initialization settings of the Synchronet Web Server that provides\n"
			"support for web browser access to the BBS over `HTTP` and `HTTPS` (TLS)\n"
			"protocols.\n"
			"\n"
			"Static content (e.g. HTML and graphics files) can be served up as well\n"
			"as dynamic content via Server-side JavaScript (SSJS) and external CGI\n"
			"processes (traditional and fast-CGI).\n"
			"\n"
			"Advanced features such as Virtual Hosts, Filebase access, and Strict\n"
			"Transport Security are also supported.\n"
			"\n"
			"For full documentation, see `http://wiki.synchro.net/server:web`\n"
		;
		switch (uifc.list(WIN_ACT | WIN_ESC | WIN_RHT | WIN_SAV, 0, 0, 0, &cur, &bar
		                  , "Web Server Settings", opt)) {
			case 0:
				enabled = !enabled;
				uifc.changes = true;
				break;
			case 1:
				uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &startup.log_level, 0, "Log Level", iniLogLevelStringList());
				break;
			case 2:
				http_srvr_cfg(&startup);
				break;
			case 3:
				https_srvr_cfg(&startup);
				break;
			case 4:
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Server-side JavaScript File Extension"
				           , startup.ssjs_ext, sizeof(startup.ssjs_ext) - 1, K_EDIT);
				break;
			case 5:
				strListCombine(startup.index_file_name, str, sizeof(str), ", ");
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Index Filenames", str, sizeof(str) - 1, K_EDIT) >= 0) {
					strListFree(&startup.index_file_name);
					strListSplitCopy(&startup.index_file_name, str, ", ");
					uifc.changes = true;
				}
				break;
			case 6:
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Content Root Directory"
				           , startup.root_dir, sizeof(startup.root_dir) - 1, K_EDIT);
				break;
			case 7:
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Error Sub-directory"
				           , startup.error_dir, sizeof(startup.error_dir) - 1, K_EDIT);
				break;
			case 8:
				startup.options ^= WEB_OPT_VIRTUAL_HOSTS;
				break;
			case 9:
				i = startup.options & WEB_OPT_HTTP_LOGGING ? 0 : 1;
				i = uifc.list(WIN_SAV | WIN_MID, 0, 0, 0, &i, 0, "Log Requests to HTTP Access Log", uifcYesNoOpts);
				if (i == 0) {
					startup.options |= WEB_OPT_HTTP_LOGGING;
					uifc.input(WIN_MID | WIN_SAV, 0, 0, "Base path/filename (blank = default)"
					           , startup.logfile_base, sizeof(startup.logfile_base) - 1, K_EDIT);
					if (startup.options & WEB_OPT_VIRTUAL_HOSTS) {
						i = (startup.options & WEB_OPT_ONE_HTTP_LOG) ? 0 : 1;
						i = uifc.list(WIN_SAV | WIN_MID, 0, 0, 0, &i, 0, "Use Single Log File for All Virtual Hosts", uifcYesNoOpts);
						if (i == 0)
							startup.options |= WEB_OPT_ONE_HTTP_LOG;
						else if (i == 1)
							startup.options &= ~WEB_OPT_ONE_HTTP_LOG;
					}
				} else if (i == 1)
					startup.options &= ~WEB_OPT_HTTP_LOGGING;
				break;
			case 10:
				if (startup.options & WEB_OPT_HTTP_LOGGING)
					uifc.input(WIN_MID | WIN_SAV, 0, 0, "Log Format (blank for Combined Log Format)"
						, startup.custom_log_fmt, sizeof(startup.custom_log_fmt) - 1, K_EDIT);
				break;
			case 11:
				SAFECOPY(str, maximum(startup.max_clients));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Maximum Client Count (0=Unlimited)", str, 10, K_EDIT) > 0)
					startup.max_clients = atoi(str);
				break;
			case 12:
				SAFECOPY(str, duration(startup.max_inactivity, false, strDefault));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Maximum Client Inactivity", str, 10, K_EDIT) > 0)
					startup.max_inactivity = (uint16_t)parse_duration(str);
				break;
			case 13:
				SAFECOPY(str, maximum(startup.max_concurrent_connections));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Maximum Concurrent Connections (0=unlimited)", str, 10, K_EDIT | K_NUMBER) > 0)
					startup.max_concurrent_connections = atoi(str);
				break;
			case 14:
				web_rate_limit_cfg(&startup);
				break;
			case 15:
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Authentication Methods"
				           , startup.default_auth_list, sizeof(startup.default_auth_list) - 1, K_EDIT);
				break;
			case 16:
				SAFEPRINTF(str, "%u", startup.outbuf_drain_timeout);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Output Buffer Drain Timeout (milliseconds)"
				               , str, 5, K_NUMBER | K_EDIT) > 0)
					startup.outbuf_drain_timeout = atoi(str);
				break;
			case 17:
				startup.options ^= BBS_OPT_NO_HOST_LOOKUP;
				break;
			case 18:
				websrvr_cgi_cfg(&startup);
				break;
			case 19:
				websrvr_filebase_cfg(&startup);
				break;
			case 20:
				getar("Web Server Login", startup.login_ars, /* helpbuf: */ NULL);
				break;
			case 21:
				getar("Web Server Login Info Saved", startup.login_info_save, /* helpbuf: */ NULL);
				break;
			case 22:
				js_startup_cfg(&startup.js);
				break;
			case 23:
				login_attempt_cfg(&startup.login_attempt);
				break;
			default:
				if (memcmp(&saved_startup, &startup, sizeof(startup)) != 0)
					uifc.changes = true;
				i = save_changes(WIN_MID);
				if (i < 0)
					continue;
				if (i == 0) {
					FILE* fp = iniOpenFile(cfg.filename, /* for_modify? */ true);
					if (fp == NULL)
						uifc.msgf("Error opening %s", cfg.filename);
					else {
						if (!sbbs_write_ini(
								fp
								, &cfg
								, NULL
								, false
								, NULL
								, false
								, NULL
								, enabled
								, &startup
								, false
								, NULL
								, false
								, NULL
								))
							uifc.msgf("Error writing %s", cfg.filename);
						iniCloseFile(fp);
					}
				}
				sbbs_free_ini(NULL
				              , NULL
				              , NULL //&ftp_startup
				              , &startup
				              , NULL //&mail_startup
				              , NULL //&services_startup
				              );
				return;
		}
	}
}

static void ftpsrvr_cfg(void)
{
	static int    cur, bar;
	char          tmp[256];
	char          str[256];
	bool          enabled = false;
	ftp_startup_t startup = { .size = sizeof startup };

	FILE*         fp = iniOpenFile(cfg.filename, /* for_modify? */ false);
	if (fp == NULL) {
		uifc.msgf("Error opening %s", cfg.filename);
		return;
	}
	uifc.pop(strReadingIniFile);
	bool result = sbbs_read_ini(
		fp
		, cfg.filename
		, NULL
		, NULL
		, NULL
		, &enabled
		, &startup
		, NULL
		, NULL
		, NULL
		, NULL //&mail_startup
		, NULL
		, NULL //&services_startup
		);
	iniCloseFile(fp);
	uifc.pop(NULL);
	if (!result) {
		uifc.msgf("Error reading %s", cfg.filename);
		return;
	}
	ftp_startup_t saved_startup = startup;

	while (1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Enabled", enabled ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Log Level", iniLogLevelStringList()[startup.log_level]);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Network Interfaces", strListCombine(startup.interfaces, tmp, sizeof(tmp), ", "));
		snprintf(opt[i++], MAX_OPLN, "%-30s%u, Data: %u", "Control Port", startup.port, startup.port - 1);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Passive Interface (IPv4)"
		         , startup.options & FTP_OPT_LOOKUP_PASV_IP ? "<automatic>" : IPv4AddressToStr(startup.pasv_ip_addr.s_addr, tmp, sizeof(tmp)));
		snprintf(opt[i++], MAX_OPLN, "%-30s%u - %u", "Passive Port Range", startup.pasv_port_low, startup.pasv_port_high);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Auto-generate Index File", startup.options & FTP_OPT_INDEX_FILE ? startup.index_file_name : strDisabled);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "QWK Message Packet Transfers", startup.options & FTP_OPT_ALLOW_QWK ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "QWK Message Packet Timeout", startup.options & FTP_OPT_ALLOW_QWK ? vduration(startup.qwk_timeout, strDefault) : "N/A");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max Clients", maximum(startup.max_clients));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max Inactivity", vduration(startup.max_inactivity, strDefault));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max Concurrent Connections", maximum(startup.max_concurrent_connections));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s bytes", "Min Uploaded File Size", byte_count_to_str(startup.min_fsize, tmp, sizeof tmp));
		if (startup.max_fsize == 0)
			SAFECOPY(str, "Unlimited");
		else
			snprintf(str, sizeof str, "%s bytes", byte_count_to_str(startup.max_fsize, tmp, sizeof tmp));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max Uploaded File Size", str);
		if (startup.max_requests_per_period > 0 && startup.request_rate_limit_period > 0) {
			snprintf(str, sizeof str, "%u per %s", startup.max_requests_per_period
			         , duration_to_vstr(startup.request_rate_limit_period, tmp, sizeof tmp));
			if (startup.rate_limit.filter > 0)
				SAFECAT(str, ", Auto-Filter");
		} else
			SAFECOPY(str, strDisabled);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Limit Rate of Requests", str);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Sysop File System Access", startup.options & FTP_OPT_NO_LOCAL_FSYS ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Allow Bounce Transfers", startup.options & FTP_OPT_ALLOW_BOUNCE ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Lookup Client Hostname", startup.options & BBS_OPT_NO_HOST_LOOKUP ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Login Requirements", startup.login_ars);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Login Info Save", startup.login_info_save);
		strcpy(opt[i++], "Failed Login Attempts...");

		opt[i][0] = '\0';

		uifc.helpbuf =
			"`FTP Server Settings:`\n"
			"\n"
			"These settings define the initialization parameters of the Synchronet\n"
			"FTP Server.  FTP and FTPS (TLS) protocols are supported for downloads\n"
			"and uploads (including resume) to/from filebases and local filesystem.\n"
			"\n"
			"Active and passive FTP transfers are supported along with QWK message\n"
			"packet transfers, dynamically-generated file indexes, and other advanced\n"
			"features.\n"
			"\n"
			"For full documentation, see `http://wiki.synchro.net/server:ftp`\n"
		;
		switch (uifc.list(WIN_ACT | WIN_ESC | WIN_RHT | WIN_SAV, 0, 0, 0, &cur, &bar
		                  , "FTP Server Settings", opt)) {
			case 0:
				enabled = !enabled;
				uifc.changes = true;
				break;
			case 1:
				uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &startup.log_level, 0, "Log Level", iniLogLevelStringList());
				break;
			case 2:
				strListCombine(startup.interfaces, str, sizeof(str), ", ");
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Network Interfaces (IPv4/6)", str, sizeof(str) - 1, K_EDIT) >= 0) {
					strListFree(&startup.interfaces);
					strListSplitCopy(&startup.interfaces, str, ", ");
					uifc.changes = true;
				}
				break;
			case 3:
				SAFEPRINTF(str, "%u", startup.port);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Control TCP Port", str, 5, K_NUMBER | K_EDIT) > 0)
					startup.port = atoi(str);
				break;
			case 4:
				i = startup.options & FTP_OPT_LOOKUP_PASV_IP ? 0 : 1;
				i = uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0, "Automatically Detect Public IP Address", uifcYesNoOpts);
				if (i == 0)
					startup.options |= FTP_OPT_LOOKUP_PASV_IP;
				else if (i == 1) {
					startup.options &= ~FTP_OPT_LOOKUP_PASV_IP;
					IPv4AddressToStr(startup.pasv_ip_addr.s_addr, str, sizeof(str));
					if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "IPv4 Address for Passive Connections", str, sizeof(str) - 1, K_EDIT) > 0)
						startup.pasv_ip_addr.s_addr = parseIPv4Address(str);
				}
				break;
			case 5:
				SAFEPRINTF(str, "%u", startup.pasv_port_low);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Lowest Passive TCP Port", str, 5, K_NUMBER | K_EDIT) > 0)
					startup.pasv_port_low = atoi(str);
				else
					break;
				SAFEPRINTF(str, "%u", startup.pasv_port_high);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Highest Passive TCP Port", str, 5, K_NUMBER | K_EDIT) > 0)
					startup.pasv_port_high = atoi(str);
				break;
			case 6:
				i = startup.options & FTP_OPT_INDEX_FILE ? 0 : 1;
				i = uifc.list(WIN_SAV | WIN_MID, 0, 0, 0, &i, 0, "Automatically Generate Index Files", uifcYesNoOpts);
				if (i == 0) {
					startup.options |= FTP_OPT_INDEX_FILE;
					uifc.input(WIN_MID | WIN_SAV, 0, 0, "Index Filename"
					           , startup.index_file_name, sizeof(startup.index_file_name) - 1, K_EDIT);
				} else if (i == 1)
					startup.options &= ~FTP_OPT_INDEX_FILE;
				break;
			case 7:
				startup.options ^= FTP_OPT_ALLOW_QWK;
				break;
			case 8:
				SAFECOPY(str, duration(startup.qwk_timeout, false, strDefault));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "QWK Message Packet Creation Timeout", str, 10, K_EDIT) > 0)
					startup.qwk_timeout = (uint16_t)parse_duration(str);
				break;
			case 9:
				SAFECOPY(str, maximum(startup.max_clients));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Maximum Client Count (0=Unlimited)", str, 10, K_EDIT) > 0)
					startup.max_clients = atoi(str);
				break;
			case 10:
				SAFECOPY(str, duration(startup.max_inactivity, false, strDefault));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Maximum Client Inactivity", str, 10, K_EDIT) > 0)
					startup.max_inactivity = (uint16_t)parse_duration(str);
				break;
			case 11:
				SAFECOPY(str, maximum(startup.max_concurrent_connections));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Maximum Concurrent Connections (0=unlimited)", str, 10, K_EDIT | K_NUMBER) > 0)
					startup.max_concurrent_connections = atoi(str);
				break;
			case 12:
				byte_count_to_str(startup.min_fsize, str, sizeof str);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Minimum Uploaded File Size (in bytes)", str, 10, K_EDIT) >= 0)
					startup.min_fsize = parse_byte_count(str, 1);
				break;
			case 13:
				byte_count_to_str(startup.max_fsize, str, sizeof str);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Maximum Uploaded File Size (in bytes, 0 = Unlimited)", str, 10, K_EDIT) >= 0)
					startup.max_fsize = parse_byte_count(str, 1);
				break;
			case 14:
				ftp_rate_limit_cfg(&startup);
				break;
			case 15:
				startup.options ^= FTP_OPT_NO_LOCAL_FSYS;
				break;
			case 16:
				startup.options ^= FTP_OPT_ALLOW_BOUNCE;
				break;
			case 17:
				startup.options ^= BBS_OPT_NO_HOST_LOOKUP;
				break;
			case 18:
				getar("FTP Server Login", startup.login_ars, /* helpbuf: */ NULL);
				break;
			case 19:
				getar("FTP Server Login Info Saved", startup.login_info_save, /* helpbuf: */ NULL);
				break;
			case 20:
				login_attempt_cfg(&startup.login_attempt);
				break;
			default:
				if (memcmp(&saved_startup, &startup, sizeof(startup)) != 0)
					uifc.changes = true;
				i = save_changes(WIN_MID);
				if (i < 0)
					continue;
				if (i == 0) {
					FILE* fp = iniOpenFile(cfg.filename, /* for_modify? */ true);
					if (fp == NULL)
						uifc.msgf("Error opening %s", cfg.filename);
					else {
						if (!sbbs_write_ini(
								fp
								, &cfg
								, NULL
								, false
								, NULL
								, enabled
								, &startup
								, false
								, NULL
								, false
								, NULL
								, false
								, NULL
								))
							uifc.msgf("Error writing %s", cfg.filename);
						iniCloseFile(fp);
					}
				}
				sbbs_free_ini(NULL
				              , NULL
				              , &startup
				              , NULL
				              , NULL //&mail_startup
				              , NULL //&services_startup
				              );
				return;
		}
	}
}

static void sendmail_cfg(mail_startup_t* startup)
{
	const char* p;
	char        str[256];
	static int  cur, bar;

	while (1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Enabled", startup->options & MAIL_OPT_NO_SENDMAIL ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Rescan Interval"
		         , startup->options & MAIL_OPT_NO_SENDMAIL ? "N/A" : vduration(startup->rescan_frequency, strDefault));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Connect Timeout"
		         , startup->options & MAIL_OPT_NO_SENDMAIL ? "N/A" : vduration(startup->connect_timeout, strDisabled));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Auto-exempt Recipients"
		         , startup->options & MAIL_OPT_NO_SENDMAIL ? "N/A" : startup->options & MAIL_OPT_NO_AUTO_EXEMPT ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-30s%u", "Max Delivery Attempts", startup->max_delivery_attempts);
		bool applicable = (startup->options & (MAIL_OPT_RELAY_TX | MAIL_OPT_NO_SENDMAIL)) == MAIL_OPT_RELAY_TX;
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Delivery Method"
		         , startup->options & MAIL_OPT_NO_SENDMAIL ? "N/A" : applicable ? "Relay" : "Direct");
		if (applicable) {
			snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Relay Server Address", startup->relay_server);
			snprintf(opt[i++], MAX_OPLN, "%-30s%u", "Relay Server TCP Port", startup->relay_port);
			if (startup->options & MAIL_OPT_RELAY_AUTH_PLAIN)
				p = "Plain";
			else if (startup->options & MAIL_OPT_RELAY_AUTH_LOGIN)
				p = "Login";
			else if (startup->options & MAIL_OPT_RELAY_AUTH_CRAM_MD5)
				p = "CRAM-MD5";
			else
				p = "None";
			snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Relay Server Authentication", p);
			if (startup->options & (MAIL_OPT_RELAY_AUTH_PLAIN | MAIL_OPT_RELAY_AUTH_LOGIN | MAIL_OPT_RELAY_AUTH_CRAM_MD5)) {
				snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Relay Server Username", startup->relay_user);
				snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Relay Server Password", startup->relay_pass);
			}
		}
		opt[i][0] = '\0';

		uifc.helpbuf =
			"`SendMail Support:`\n"
			"\n"
			"Set the operating parameters of the Synchronet Mail Server SendMail\n"
			"Thread.\n"
			"\n"
			"For full documentation, see `http://wiki.synchro.net/server:mail`\n"
		;
		switch (uifc.list(WIN_ACT | WIN_ESC | WIN_BOT | WIN_SAV, 0, 0, 0, &cur, &bar
		                  , "SendMail Support", opt)) {
			case 0:
				startup->options ^= MAIL_OPT_NO_SENDMAIL;
				break;
			case 1:
				if (startup->options & MAIL_OPT_NO_SENDMAIL)
					break;
				SAFECOPY(str, duration(startup->rescan_frequency, false, strDefault));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "MailBase Rescan Interval", str, 5, K_EDIT) > 0)
					startup->rescan_frequency = (uint16_t)parse_duration(str);
				break;
			case 2:
				if (startup->options & MAIL_OPT_NO_SENDMAIL)
					break;
				SAFECOPY(str, duration(startup->connect_timeout, false, strDisabled));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "SendMail Connect Timeout", str, 5, K_EDIT) > 0)
					startup->connect_timeout = (uint32_t)parse_duration(str);
				break;
			case 3:
				if (startup->options & MAIL_OPT_NO_SENDMAIL)
					break;
				startup->options ^= MAIL_OPT_NO_AUTO_EXEMPT;
				break;
			case 4:
				if (startup->options & MAIL_OPT_NO_SENDMAIL)
					break;
				SAFEPRINTF(str, "%u", startup->max_delivery_attempts);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Maximum Number of SendMail Delivery Attempts", str, 5, K_NUMBER | K_EDIT) > 0)
					startup->max_delivery_attempts = atoi(str);
				break;
			case 5:
				if (startup->options & MAIL_OPT_NO_SENDMAIL)
					break;
				startup->options ^= MAIL_OPT_RELAY_TX;
				break;
			case 6:
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Relay Server Address", startup->relay_server, sizeof(startup->relay_server) - 1, K_EDIT);
				break;
			case 7:
				SAFEPRINTF(str, "%u", startup->relay_port);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Relay Server TCP Port", str, 5, K_NUMBER | K_EDIT) > 0)
					startup->relay_port = atoi(str);
				break;
			case 8:
				i = 0;
				strcpy(opt[i++], "Plain");
				strcpy(opt[i++], "Login");
				strcpy(opt[i++], "CRAM-MD5");
				strcpy(opt[i++], "None");
				opt[i][0] = '\0';
				if (startup->options & MAIL_OPT_RELAY_AUTH_PLAIN)
					i = 0;
				else if (startup->options & MAIL_OPT_RELAY_AUTH_LOGIN)
					i = 1;
				else if (startup->options & MAIL_OPT_RELAY_AUTH_CRAM_MD5)
					i = 2;
				else
					i = 3;
				if (uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0, "Relay Server Authentication Method", opt) < 0)
					break;
				startup->options &= ~(MAIL_OPT_RELAY_AUTH_PLAIN | MAIL_OPT_RELAY_AUTH_LOGIN | MAIL_OPT_RELAY_AUTH_CRAM_MD5);
				switch (i) {
					case 0:
						startup->options |= MAIL_OPT_RELAY_AUTH_PLAIN;
						break;
					case 1:
						startup->options |= MAIL_OPT_RELAY_AUTH_LOGIN;
						break;
					case 2:
						startup->options |= MAIL_OPT_RELAY_AUTH_CRAM_MD5;
						break;
				}
				break;
			case 9:
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Relay Server Username", startup->relay_user, sizeof(startup->relay_user) - 1, K_EDIT);
				break;
			case 10:
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Relay Server Password", startup->relay_pass, sizeof(startup->relay_pass) - 1, K_EDIT);
				break;
			default:
				return;
		}
	}
}

static void submission_srvr_cfg(mail_startup_t* startup)
{
	static int cur, bar;
	char       str[256];

	while (1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-10s%s", "Enabled", startup->options & MAIL_OPT_USE_SUBMISSION_PORT ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-10s%u", "Port", startup->submission_port);
		opt[i][0] = '\0';

		switch (uifc.list(WIN_ACT | WIN_ESC | WIN_MID | WIN_SAV, 0, 0, 0, &cur, &bar
		                  , "Submission Support", opt)) {
			case 0:
				startup->options ^= MAIL_OPT_USE_SUBMISSION_PORT;
				break;
			case 1:
				if (!(startup->options & MAIL_OPT_USE_SUBMISSION_PORT))
					break;
				SAFEPRINTF(str, "%u", startup->submission_port);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "SMTP-Submission TCP Port", str, 5, K_NUMBER | K_EDIT) > 0)
					startup->submission_port = atoi(str);
				break;
			default:
				return;
		}
	}
}

static void submissions_srvr_cfg(mail_startup_t* startup)
{
	static int cur, bar;
	char       str[256];

	while (1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-10s%s", "Enabled", startup->options & MAIL_OPT_TLS_SUBMISSION ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-10s%u", "Port", startup->submissions_port);
		opt[i][0] = '\0';

		switch (uifc.list(WIN_ACT | WIN_ESC | WIN_MID | WIN_SAV, 0, 0, 0, &cur, &bar
		                  , "Submission/TLS Support", opt)) {
			case 0:
				startup->options ^= MAIL_OPT_TLS_SUBMISSION;
				break;
			case 1:
				if (!(startup->options & MAIL_OPT_TLS_SUBMISSION))
					break;
				SAFEPRINTF(str, "%u", startup->submissions_port);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "SMTP-Submission/TLS TCP Port", str, 5, K_NUMBER | K_EDIT) > 0)
					startup->submissions_port = atoi(str);
				break;
			default:
				return;
		}
	}
}

static void pop3_srvr_cfg(mail_startup_t* startup)
{
	static int cur, bar;
	char       str[256];

	while (1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-10s%s", "Enabled", startup->options & MAIL_OPT_ALLOW_POP3 ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-10s%u", "Port", startup->pop3_port);
		opt[i][0] = '\0';

		switch (uifc.list(WIN_ACT | WIN_ESC | WIN_MID | WIN_SAV, 0, 0, 0, &cur, &bar
		                  , "POP3 Support", opt)) {
			case 0:
				startup->options ^= MAIL_OPT_ALLOW_POP3;
				break;
			case 1:
				if (!(startup->options & MAIL_OPT_ALLOW_POP3))
					break;
				SAFEPRINTF(str, "%u", startup->pop3_port);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "POP3 TCP Port", str, 5, K_NUMBER | K_EDIT) > 0)
					startup->pop3_port = atoi(str);
				break;
			default:
				return;
		}
	}
}

static void pop3s_srvr_cfg(mail_startup_t* startup)
{
	static int cur, bar;
	char       str[256];

	while (1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-10s%s", "Enabled", startup->options & MAIL_OPT_TLS_POP3 ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-10s%u", "Port", startup->pop3s_port);
		opt[i][0] = '\0';

		switch (uifc.list(WIN_ACT | WIN_ESC | WIN_MID | WIN_SAV, 0, 0, 0, &cur, &bar
		                  , "POP3/TLS Support", opt)) {
			case 0:
				startup->options ^= MAIL_OPT_TLS_POP3;
				break;
			case 1:
				if (!(startup->options & MAIL_OPT_TLS_POP3))
					break;
				SAFEPRINTF(str, "%u", startup->pop3s_port);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "POP3/TLS TCP Port", str, 5, K_NUMBER | K_EDIT) > 0)
					startup->pop3s_port = atoi(str);
				break;
			default:
				return;
		}
	}
}

static void mailsrvr_cfg(void)
{
	static int     cur, bar;
	char           tmp[256];
	char           str[256];
	const char*    p;
	bool           enabled = false;
	mail_startup_t startup = { .size = sizeof startup };

	FILE*          fp = iniOpenFile(cfg.filename, /* for_modify? */ false);
	if (fp == NULL) {
		uifc.msgf("Error opening %s", cfg.filename);
		return;
	}
	uifc.pop(strReadingIniFile);
	bool result = sbbs_read_ini(
		fp
		, cfg.filename
		, NULL
		, NULL
		, NULL
		, NULL
		, NULL
		, NULL
		, NULL
		, &enabled
		, &startup
		, NULL
		, NULL //&services_startup
		);
	iniCloseFile(fp);
	uifc.pop(NULL);
	if (!result) {
		uifc.msgf("Error reading %s", cfg.filename);
		return;
	}
	mail_startup_t saved_startup = startup;

	while (1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Enabled", enabled ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Log Level", iniLogLevelStringList()[startup.log_level]);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "SMTP Interfaces", strListCombine(startup.interfaces, tmp, sizeof(tmp), ", "));
		snprintf(str, sizeof str, "Port %u", startup.smtp_port);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "SMTP Support", str);
		snprintf(str, sizeof str, "Port %u", startup.submission_port);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Submission Support", startup.options & MAIL_OPT_USE_SUBMISSION_PORT ? str : strDisabled);
		snprintf(str, sizeof str, "Port %u", startup.submissions_port);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Submission/TLS Support", startup.options & MAIL_OPT_TLS_SUBMISSION ? str : strDisabled);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "POP3 Interfaces"
		         , startup.options & (MAIL_OPT_ALLOW_POP3 | MAIL_OPT_TLS_POP3)
		        ? strListCombine(startup.pop3_interfaces, tmp, sizeof(tmp), ", ") : "N/A");
		snprintf(str, sizeof str, "Port %u", startup.pop3_port);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "POP3 Support", startup.options & MAIL_OPT_ALLOW_POP3 ? str : strDisabled);
		snprintf(str, sizeof str, "Port %u", startup.pop3s_port);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "POP3/TLS Support", startup.options & MAIL_OPT_TLS_POP3 ? str : strDisabled);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Mark Retrieved Mail as Read", startup.options & MAIL_OPT_NO_READ_POP3 ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max Clients", maximum(startup.max_clients));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max Inactivity", vduration(startup.max_inactivity, strDefault));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max Concurrent Connections", maximum(startup.max_concurrent_connections));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max Recipients Per Message", maximum(startup.max_recipients));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Max Messages Waiting", maximum(startup.max_msgs_waiting));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s bytes", "Max Receive Message Size", byte_count_to_str(startup.max_msg_size, tmp, sizeof(tmp)));
		if (startup.max_requests_per_period > 0 && startup.request_rate_limit_period > 0) {
			snprintf(str, sizeof str, "%u per %s", startup.max_requests_per_period
			         , duration_to_vstr(startup.request_rate_limit_period, tmp, sizeof tmp));
			if (startup.rate_limit.filter > 0)
				SAFECAT(str, ", Auto-Filter");
		} else
			SAFECOPY(str, strDisabled);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Limit Rate of Requests", str);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Post Recipient", startup.post_to);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Default Recipient", startup.default_user);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Receive By Sysop Aliases", startup.options & MAIL_OPT_ALLOW_SYSOP_ALIASES ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Notify Local Recipients", startup.options & MAIL_OPT_NO_NOTIFY ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Notify Offline Recipients", startup.options & MAIL_OPT_NO_NOTIFY ? "N/A" : (startup.notify_offline_users ? "Yes" : "No"));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Allow Users to Relay Mail", startup.options & MAIL_OPT_ALLOW_RELAY ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Lookup Client Hostname", startup.options & BBS_OPT_NO_HOST_LOOKUP ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Check Headers against DNSBL", startup.options & MAIL_OPT_DNSBL_CHKRECVHDRS ? "Yes" : "No");
		if (startup.options & MAIL_OPT_DNSBL_REFUSE)
			p = "Refuse Session";
		else if (startup.options & MAIL_OPT_DNSBL_IGNORE)
			p = "Silently Ignore";
		else if (startup.options & MAIL_OPT_DNSBL_BADUSER)
			p = "Refuse Mail";
		else
			p = "Tag Mail";
		snprintf(opt[i++], MAX_OPLN, "%-30s%s%s", "DNS-Blacklisted Servers", startup.options & MAIL_OPT_DNSBL_THROTTLE ? "Throttle and " : "", p);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Hash DNS-Blacklisted Msgs", startup.options & MAIL_OPT_DNSBL_SPAMHASH ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Kill SPAM When Read", startup.options & MAIL_OPT_KILL_READ_SPAM ? "Yes": "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Spammer IP-Filter Duration"
		         , vduration(startup.spam_block_duration, strInfinite));
		if (startup.options & MAIL_OPT_RELAY_TX)
			p = "Relay";
		else
			p = "Direct";
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "SendMail Support...", startup.options & MAIL_OPT_NO_SENDMAIL ? strDisabled : p);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Login Requirements", startup.login_ars);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Archive Requirements", startup.archive_ars);
		strcpy(opt[i++], "JavaScript Settings...");
		strcpy(opt[i++], "Failed Login Attempts...");
		opt[i][0] = '\0';

		uifc.helpbuf =
			"`Mail Server Settings:`\n"
			"\n"
			"Set the initial parameters of the Synchronet Mail Server here.  This\n"
			"server supports receiving mail over [E]SMTP and [E]SMTPS protocols as\n"
			"well as sending mail to clients via the POP3 or POP3S protocols and\n"
			"sending mail to other servers (the SendMail thread) via [E]SMTP and\n"
			"[E]SMTPS protocols.\n"
			"\n"
			"For full documentation, see `http://wiki.synchro.net/server:mail`\n"
		;
		switch (uifc.list(WIN_ACT | WIN_ESC | WIN_RHT | WIN_SAV, 0, 0, 0, &cur, &bar
		                  , "Mail Server Settings", opt)) {
			case 0:
				enabled = !enabled;
				uifc.changes = true;
				break;
			case 1:
				uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &startup.log_level, 0, "Log Level", iniLogLevelStringList());
				break;
			case 2:
				strListCombine(startup.interfaces, str, sizeof(str), ", ");
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "SMTP Network Interfaces (IPv4/6)", str, sizeof(str) - 1, K_EDIT) >= 0) {
					strListFree(&startup.interfaces);
					strListSplitCopy(&startup.interfaces, str, ", ");
					uifc.changes = true;
				}
				break;
			case 3:
				SAFEPRINTF(str, "%u", startup.smtp_port);
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "SMTP TCP Port", str, 5, K_NUMBER | K_EDIT) > 0)
					startup.smtp_port = atoi(str);
				break;
			case 4:
				submission_srvr_cfg(&startup);
				break;
			case 5:
				submissions_srvr_cfg(&startup);
				break;
			case 6:
				strListCombine(startup.pop3_interfaces, str, sizeof(str), ", ");
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "POP3 Network Interfaces (IPv4/6)", str, sizeof(str) - 1, K_EDIT) >= 0) {
					strListFree(&startup.pop3_interfaces);
					strListSplitCopy(&startup.pop3_interfaces, str, ", ");
					uifc.changes = true;
				}
				break;
			case 7:
				pop3_srvr_cfg(&startup);
				break;
			case 8:
				pop3s_srvr_cfg(&startup);
				break;
			case 9:
				startup.options ^= MAIL_OPT_NO_READ_POP3;
				break;
			case 10:
				SAFECOPY(str, maximum(startup.max_clients));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Maximum Client Count (0=Unlimited)", str, 10, K_EDIT) > 0)
					startup.max_clients = atoi(str);
				break;
			case 11:
				SAFECOPY(str, duration(startup.max_inactivity, false, strDisabled));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Maximum Client Inactivity", str, 10, K_EDIT) > 0)
					startup.max_inactivity = (uint16_t)parse_duration(str);
				break;
			case 12:
				SAFECOPY(str, maximum(startup.max_concurrent_connections));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Maximum Concurrent (Unauthenticated) Connections", str, 10, K_EDIT) > 0)
					startup.max_concurrent_connections = atoi(str);
				break;
			case 13:
				SAFECOPY(str, maximum(startup.max_recipients));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Maximum Recipients per Message", str, 10, K_EDIT) > 0)
					startup.max_recipients = atoi(str);
				break;
			case 14:
				SAFECOPY(str, maximum(startup.max_msgs_waiting));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Maximum Messages Waiting per User", str, 10, K_EDIT) > 0)
					startup.max_msgs_waiting = atoi(str);
				break;
			case 15:
				byte_count_to_str(startup.max_msg_size, str, sizeof(str));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Maximum Received Message Size (in bytes)", str, 10, K_EDIT) > 0)
					startup.max_msg_size = (uint32_t)parse_byte_count(str, 1);
				break;
			case 16:
				mail_rate_limit_cfg(&startup);
				break;
			case 17:
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Override Recipient of SMTP Posts"
						   , startup.post_to, sizeof startup.post_to -1, K_EDIT);
				break;
			case 18:
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Default Recipient (user alias)"
				           , startup.default_user, sizeof(startup.default_user) - 1, K_EDIT);
				break;
			case 19:
				startup.options ^= MAIL_OPT_ALLOW_SYSOP_ALIASES;
				break;
			case 20:
				startup.options ^= MAIL_OPT_NO_NOTIFY;
				break;
			case 21:
				startup.notify_offline_users = !startup.notify_offline_users;
				break;
			case 22:
				startup.options ^= MAIL_OPT_ALLOW_RELAY;
				break;
			case 23:
				startup.options ^= BBS_OPT_NO_HOST_LOOKUP;
				break;
			case 24:
				startup.options ^= MAIL_OPT_DNSBL_CHKRECVHDRS;
				break;
			case 25:
				i = 0;
				strcpy(opt[i++], "Refuse Session");
				strcpy(opt[i++], "Silently Ignore");
				strcpy(opt[i++], "Refuse Mail");
				strcpy(opt[i++], "Tag Mail");
				opt[i][0] = '\0';
				if (startup.options & MAIL_OPT_DNSBL_REFUSE)
					i = 0;
				else if (startup.options & MAIL_OPT_DNSBL_IGNORE)
					i = 1;
				else if (startup.options & MAIL_OPT_DNSBL_BADUSER)
					i = 2;
				else
					i = 3;
				if (uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0, "DNS-Blacklisted Servers", opt) < 0)
					break;
				startup.options &= ~(MAIL_OPT_DNSBL_REFUSE | MAIL_OPT_DNSBL_IGNORE | MAIL_OPT_DNSBL_BADUSER);
				switch (i) {
					case 0:
						startup.options |= MAIL_OPT_DNSBL_REFUSE;
						break;
					case 1:
						startup.options |= MAIL_OPT_DNSBL_IGNORE;
						break;
					case 2:
						startup.options |= MAIL_OPT_DNSBL_BADUSER;
				}
				i = startup.options & MAIL_OPT_DNSBL_THROTTLE ? 0 : 1;
				if (uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &i, 0, "Throttle DNS-Blacklisted Servers", uifcYesNoOpts) < 0)
					break;
				if (i == 0)
					startup.options |= MAIL_OPT_DNSBL_THROTTLE;
				else
					startup.options &= ~MAIL_OPT_DNSBL_THROTTLE;
				break;
			case 26:
				startup.options ^= MAIL_OPT_DNSBL_SPAMHASH;
				break;
			case 27:
				startup.options ^= MAIL_OPT_KILL_READ_SPAM;
				break;
			case 28:
				SAFECOPY(str, duration(startup.spam_block_duration, false, strInfinite));
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Lifetime of ban of SPAM-bait taker IP", str, 8, K_EDIT) > 0)
					startup.spam_block_duration = (uint)parse_duration(str);
				break;
			case 29:
				sendmail_cfg(&startup);
				break;
			case 30:
				getar("Mail Server Login", startup.login_ars, /* helpbuf: */ NULL);
				break;
			case 31:
				getar("Mail Archive", startup.archive_ars, /* helpbuf: */ NULL);
				break;
			case 32:
				js_startup_cfg(&startup.js);
				break;
			case 33:
				login_attempt_cfg(&startup.login_attempt);
				break;
			default:
				if (memcmp(&saved_startup, &startup, sizeof(startup)) != 0)
					uifc.changes = true;
				i = save_changes(WIN_MID);
				if (i < 0)
					continue;
				if (i == 0) {
					FILE* fp = iniOpenFile(cfg.filename, /* for_modify? */ true);
					if (fp == NULL)
						uifc.msgf("Error opening %s", cfg.filename);
					else {
						if (!sbbs_write_ini(
								fp
								, &cfg
								, NULL
								, false
								, NULL
								, false
								, NULL
								, false
								, NULL
								, enabled
								, &startup
								, false
								, NULL
								))
							uifc.msgf("Error writing %s", cfg.filename);
						iniCloseFile(fp);
					}
				}
				sbbs_free_ini(NULL
				              , NULL
				              , NULL
				              , NULL
				              , &startup
				              , NULL //&services_startup
				              );
				return;
		}
	}
}

static void services_cfg(void)
{
	static int         cur, bar;
	char               tmp[256];
	char               str[256];
	bool               enabled = false;
	services_startup_t startup = { .size = sizeof startup };

	FILE*              fp = iniOpenFile(cfg.filename, /* for_modify? */ false);
	if (fp == NULL) {
		uifc.msgf("Error opening %s", cfg.filename);
		return;
	}
	uifc.pop(strReadingIniFile);
	bool result = sbbs_read_ini(
		fp
		, cfg.filename
		, NULL
		, NULL
		, NULL
		, NULL
		, NULL
		, NULL
		, NULL
		, NULL
		, NULL
		, &enabled
		, &startup
		);
	iniCloseFile(fp);
	uifc.pop(NULL);
	if (!result) {
		uifc.msgf("Error reading %s", cfg.filename);
		return;
	}
	services_startup_t saved_startup = startup;

	while (1) {
		int i = 0;
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Enabled", enabled ? "Yes" : "No");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Log Level", iniLogLevelStringList()[startup.log_level]);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Network Interfaces", strListCombine(startup.interfaces, tmp, sizeof(tmp), ", "));
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Lookup Client Hostname", startup.options & BBS_OPT_NO_HOST_LOOKUP ? "No" : "Yes");
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Configuration File", startup.services_ini);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Login Requirements", startup.login_ars);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Login Info Save", startup.login_info_save);
		if (startup.max_connects_per_period > 0 && startup.connect_rate_limit_period > 0) {
			snprintf(str, sizeof str, "%u per %s", startup.max_connects_per_period
			         , duration_to_vstr(startup.connect_rate_limit_period, tmp, sizeof tmp));
			if (startup.rate_limit.filter > 0)
				SAFECAT(str, ", Auto-Filter");
		} else
			SAFECOPY(str, strDisabled);
		snprintf(opt[i++], MAX_OPLN, "%-30s%s", "Limit Rate of Connections", str);
		strcpy(opt[i++], "JavaScript Settings...");
		strcpy(opt[i++], "Failed Login Attempts...");
		opt[i][0] = '\0';

		uifc.helpbuf =
			"`Services Server Settings:`\n"
			"\n"
			"Initialization settings for the Synchronet Services Server, which hosts\n"
			"plug-in services (NNTP, IRC, IMAP, Finger, Gopher, MSP, etc.) as defined\n"
			"in the Services Configuration File.  Changes take effect after the\n"
			"server is recycled.\n"
			"\n"
			"`Enabled`: start the Services Server when Synchronet launches.\n"
			"\n"
			"`Log Level`: minimum severity of events written to the log.  `Info` is the\n"
			"normal default; raise to `Debug` for troubleshooting, lower (e.g. `Notice`)\n"
			"to reduce log volume.\n"
			"\n"
			"`Network Interfaces`: comma-separated IPv4/IPv6 addresses the server\n"
			"binds to (e.g. `0.0.0.0, ::` to listen on every interface).  Restrict to\n"
			"specific addresses to limit exposure on multi-homed hosts.  Individual\n"
			"services may further override their bindings in the Services\n"
			"Configuration File.\n"
			"\n"
			"`Lookup Client Hostname`: perform a reverse DNS lookup on every\n"
			"connecting client.  `Yes` adds the hostname to logs and makes it\n"
			"available to scripts; `No` is faster and avoids DNS stalls.\n"
			"\n"
			"`Configuration File`: path (relative to `ctrl/`) of the INI file defining\n"
			"the individual services to load.  Default: `services.ini`.\n"
			"\n"
			"`Login Requirements`: Access Requirements String (ARS) a user must\n"
			"satisfy before any authenticated service will accept their login.\n"
			"Blank = no extra restriction beyond per-service checks.\n"
			"\n"
			"`Login Info Save`: ARS controlling which successful logins are recorded\n"
			"in the user record's last-on history.  Blank = save all.\n"
			"\n"
			"`Limit Rate of Connections`: cap connections-per-period per client IP\n"
			"and optionally auto-filter abusers.  See the Rate Limiting sub-menu\n"
			"for per-knob help.\n"
			"\n"
			"`JavaScript Settings...`: runtime tuning for JavaScript-based services\n"
			"(heap, time limit, GC, yield, load path).  Overrides the global\n"
			"JavaScript Settings for this server.\n"
			"\n"
			"`Failed Login Attempts...`: thresholds for temp-filtering or\n"
			"disconnecting clients that fail authentication repeatedly.\n"
			"\n"
			"For full documentation, see `http://wiki.synchro.net/server:services`\n"
		;
		switch (uifc.list(WIN_ACT | WIN_ESC | WIN_RHT | WIN_SAV, 0, 0, 0, &cur, &bar
		                  , "Services Server Settings", opt)) {
			case 0:
				enabled = !enabled;
				uifc.changes = true;
				break;
			case 1:
				uifc.list(WIN_MID | WIN_SAV, 0, 0, 0, &startup.log_level, 0, "Log Level", iniLogLevelStringList());
				break;
			case 2:
				strListCombine(startup.interfaces, str, sizeof(str), ", ");
				if (uifc.input(WIN_MID | WIN_SAV, 0, 0, "Network Interfaces (IPv4/6)", str, sizeof(str) - 1, K_EDIT) >= 0) {
					strListFree(&startup.interfaces);
					strListSplitCopy(&startup.interfaces, str, ", ");
					uifc.changes = true;
				}
				break;
			case 3:
				startup.options ^= BBS_OPT_NO_HOST_LOOKUP;
				break;
			case 4:
				uifc.input(WIN_MID | WIN_SAV, 0, 0, "Services Configuration File", startup.services_ini, sizeof(startup.services_ini) - 1, K_EDIT);
				break;
			case 5:
				getar("Services Login", startup.login_ars, /* helpbuf: */ NULL);
				break;
			case 6:
				getar("Services Login Info Saved", startup.login_info_save, /* helpbuf: */ NULL);
				break;
			case 7:
				services_rate_limit_cfg(&startup);
				break;
			case 8:
				js_startup_cfg(&startup.js);
				break;
			case 9:
				login_attempt_cfg(&startup.login_attempt);
				break;
			default:
				if (memcmp(&saved_startup, &startup, sizeof(startup)) != 0)
					uifc.changes = true;
				i = save_changes(WIN_MID);
				if (i < 0)
					continue;
				if (i == 0) {
					FILE* fp = iniOpenFile(cfg.filename, /* for_modify? */ true);
					if (fp == NULL)
						uifc.msgf("Error opening %s", cfg.filename);
					else {
						if (!sbbs_write_ini(
								fp
								, &cfg
								, NULL
								, false
								, NULL
								, false
								, NULL
								, false
								, NULL
								, false
								, NULL
								, enabled
								, &startup
								))
							uifc.msgf("Error writing %s", cfg.filename);
						iniCloseFile(fp);
					}
				}
				sbbs_free_ini(NULL
				              , NULL
				              , NULL
				              , NULL
				              , NULL
				              , &startup
				              );
				return;
		}
	}
}

void server_cfg(void)
{
	static int srvr_dflt;
	bool       run_bbs, run_ftp, run_web, run_mail, run_services;

	while (1) {
		FILE* fp = iniOpenFile(cfg.filename, /* for_modify? */ false);
		if (fp == NULL) {
			uifc.msgf("Error opening %s", cfg.filename);
			return;
		}
		uifc.pop(strReadingIniFile);
		bool result = sbbs_read_ini(
			fp
			, cfg.filename
			, NULL //&global_startup
			, &run_bbs
			, NULL //&bbs_startup
			, &run_ftp
			, NULL //&ftp_startup
			, &run_web
			, NULL //&web_startup
			, &run_mail
			, NULL //&mail_startup
			, &run_services
			, NULL //&services_startup
			);
		iniCloseFile(fp);
		uifc.pop(NULL);
		if (!result) {
			uifc.msgf("Error reading %s", cfg.filename);
			return;
		}

		int i = 0;
		strcpy(opt[i++], "Global Settings");
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Terminal Server", run_bbs ? "" : strDisabled);
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Web Server", run_web ? "" : strDisabled);
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "FTP Server", run_ftp ? "" : strDisabled);
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Mail Server", run_mail ? "" : strDisabled);
		snprintf(opt[i++], MAX_OPLN, "%-20s%s", "Services Server", run_services ? "" : strDisabled);
		opt[i][0] = '\0';

		uifc.helpbuf =
			"`Server Configuration:`\n"
			"\n"
			"Here you can configure the initialization settings of the various\n"
			"Internet servers that are integrated into Synchronet BBS Software.\n"
			"\n"
			"`Global Settings`   Settings that are applied to multiple servers\n"
			"`Terminal Server`   Provides the traditional BBS user experience\n"
			"`Web Server`        Provides the modern HTTP/HTTPS browser experience\n"
			"`FTP Server`        Serves clients using ye olde File Transfer Protocol\n"
			"`Mail Server`       Supports SMTP and POP3 mail transfer protocols\n"
			"`Services Server`   Supports plug-in style servers: NNTP, IRC, IMAP, etc.\n"
			"\n"
			"For additional advanced Synchronet server initialization settings, see\n"
			"the `ctrl/sbbs.ini` file and `https://wiki.synchro.net/config:sbbs.ini`\n"
			"for reference.\n"
		;
		i = uifc.list(WIN_ORG | WIN_ACT, 0, 0, 0, &srvr_dflt, 0, "Server Configuration", opt);
		switch (i) {
			case 0:
				global_cfg();
				break;
			case 1:
				termsrvr_cfg();
				break;
			case 2:
				websrvr_cfg();
				break;
			case 3:
				ftpsrvr_cfg();
				break;
			case 4:
				mailsrvr_cfg();
				break;
			case 5:
				services_cfg();
				break;
			default:
				return;
		}
	}
}
