// The additional-domain list is matched as the lines of a filter file
// were (findstr() rules: case-insensitive, '*' wildcard, '!' negation,
// '~' sub-string, '^' prefix, ';' comment, checked in order). The mail
// server matches the configured names with the same function that
// system.findstr() calls for an array, so these cases pin the behavior
// that domains.cfg had.

var cases = [
	// host,                patterns,                                       expected, label
	['example.com',         ['example.com'],                                true,   'exact'],
	['EXAMPLE.com',         ['example.COM'],                                true,   'case-insensitive'],
	['mail.example.com',    ['example.com'],                                false,  'exact is not a suffix match'],
	['mail.example.com',    ['*.example.com'],                              true,   'wildcard sub-domain'],
	['example.com',         ['*.example.com'],                              false,  'wildcard needs the dot'],
	['a.b.example.com',     ['*.example.com'],                              true,   'wildcard any depth'],
	['mail.example.com',    ['mail.*'],                                     true,   'wildcard right side'],
	['bad.example.com',     ['!bad.example.com', '*.example.com'],          false,  'negation first wins'],
	['good.example.com',    ['!bad.example.com', '*.example.com'],          true,   'negation skips non-match'],
	['bad.example.com',     ['*.example.com', '!bad.example.com'],          true,   'earlier match wins'],
	['example.net',         ['example.com', '!example.org'],                true,   'trailing negation of a non-match'],
	['example.org',         ['example.com', '!example.org'],                false,  'trailing negation of a match'],
	['mysub.example',       ['sub~'],                                       true,   'sub-string'],
	['example.net',         ['exam^'],                                      true,   'prefix'],
	['other.net',           ['exam^'],                                      false,  'prefix non-match'],
	['example.com',         [';example.com'],                               false,  'comment never matches'],
	['example.com',         [],                                             false,  'empty list'],
	['',                    ['example.com'],                                false,  'empty host'],
	['odd]name',            ['odd]name'],                                   true,   'bracket in a name'],
];

// system.inet_domain_list puts the system's primary address before the
// configured names. A plain name that does not match a host evaluates
// false without deciding (only a match, or a negated entry's non-match,
// is a decision for that line), and the next line replaces that value,
// so the prepended name cannot change the answer for any other host: the
// configured lines alone decide, including a list ending in a '!' entry.
// An empty configured list leaves only the system address.
var sysaddr = "bbs.example.net";
var prepend_lists = [
	[],
	['example.com'],
	['*.example.com'],
	['!test.example.com', '*.example.com'],
	['*.example.com', '!test.example.com'],		// ends with a negated entry
	['!only.example.org'],
	['sub~', 'exam^'],
	['!test.example.com', sysaddr],			// a '!' entry, then the system address last
	['*.example.com', '!test.example.com', sysaddr.toUpperCase()],
];
var prepend_hosts = ['example.com', 'test.example.com', 'a.example.com', 'only.example.org'
	, 'other.example.org', 'mysub.example', 'example.net', ''];

var failures = [];
prepend_lists.forEach(function (list) {
	var with_sys = [sysaddr].concat(list);
	prepend_hosts.forEach(function (host) {
		var got = system.findstr(with_sys, host);
		var want = system.findstr(list, host);
		if (got !== want)
			failures.push('prepended system address changed ' + JSON.stringify(list) + ' for '
				+ JSON.stringify(host) + ': ' + got + ' instead of ' + want);
	});
	if (system.findstr(with_sys, sysaddr) !== true)
		failures.push('system address not matched with ' + JSON.stringify(list));
});
if (system.findstr([sysaddr], 'unlisted.example') !== false)
	failures.push('[system address] alone matched an unlisted host');

cases.forEach(function (c) {
	var got = system.findstr(c[1], c[0]);
	if (got !== c[2])
		failures.push(c[3] + ': system.findstr(' + JSON.stringify(c[1]) + ', ' + JSON.stringify(c[0])
			+ ') returned ' + got + ', expected ' + c[2]);
});

if (failures.length > 0)
	throw new Error("domain matching test failures:\n  " + failures.join("\n  "));
