// Tests for system.findstr() pattern-matching, including IPv4 and IPv6
// CIDR support. findstr() against an in-memory string array exercises the
// same findstr_in_list() / findstr_compare() path used by `ip.can` /
// `ip-silent.can` / `host.can` and the rate-limit auto-filter accept-time
// checks (see src/sbbs3/findstr.c).
//
// Regression for issue #1145 (IPv6 CIDR was treated as literal-string-only)
// and the latent IPv4 /0 undefined-behavior shift in is_cidr_match().

var cases = [
	// search,                  patterns,                                   expected,  label

	// --- Plain string matching (non-IP) ---
	['hello',                   ['hello'],                                  true,   'string: exact'],
	['hello',                   ['HELLO'],                                  true,   'string: case-insensitive'],
	['hello world',             ['world~'],                                 true,   'string: ~ substring'],
	['hello world',             ['hello^'],                                 true,   'string: ^ left-fragment'],
	['hello world',             ['hello*world'],                            true,   'string: * left-and-right'],
	['hello',                   [';hello'],                                 false,  'string: ; comment never matches'],
	['hello',                   ['!hello'],                                 false,  'string: ! reverse-match suppresses'],
	['world',                   ['!hello'],                                 true,   'string: ! reverse-match for non-member'],

	// --- IPv4 CIDR matching ---
	['192.0.2.5',               ['192.0.2.0/24'],                           true,   'v4: inside /24'],
	['192.0.2.5',               ['198.51.100.0/24'],                        false,  'v4: outside /24'],
	['192.0.2.5',               ['192.0.2.5'],                              true,   'v4: exact host'],
	['192.0.2.5',               ['192.0.2.5/32'],                           true,   'v4: /32 == exact'],
	['192.0.2.5',               ['192.0.2.6/32'],                           false,  'v4: /32 differs by one bit'],
	['192.0.2.5',               ['0.0.0.0/0'],                              true,   'v4: /0 matches anything'],
	['203.0.113.1',             ['0.0.0.0/0'],                              true,   'v4: /0 matches another address'],
	['192.0.2.5',               ['!192.0.2.0/24'],                          false,  'v4: !CIDR suppresses inside-prefix'],
	['10.0.0.1',                ['!192.0.2.0/24'],                          true,   'v4: !CIDR for non-member'],
	['192.0.2.255',             ['192.0.2.0/24'],                           true,   'v4: /24 boundary high'],
	['192.0.3.0',               ['192.0.2.0/24'],                           false,  'v4: /24 boundary just past'],

	// --- IPv6 exact-string matching (preserved behavior) ---
	['2001:db8::5',             ['2001:db8::5'],                            true,   'v6: exact host'],
	['2001:db8::5',             ['2001:db8::6'],                            false,  'v6: different host'],

	// --- IPv6 CIDR matching (new, per issue #1145) ---
	['2001:db8::5',             ['2001:db8::/32'],                          true,   'v6: inside /32'],
	['2001:db8::5',             ['2001:db8:1234::/48'],                     false,  'v6: outside /48'],
	['2001:db8:1:2::5',         ['2001:db8:1:2::/64'],                      true,   'v6: inside /64'],
	['2001:db8:1:3::5',         ['2001:db8:1:2::/64'],                      false,  'v6: outside /64 (adjacent)'],
	['2001:db8::abcd',          ['2001:db8::abcd/128'],                     true,   'v6: /128 == exact'],
	['2001:db8::abcd',          ['2001:db8::abce/128'],                     false,  'v6: /128 differs by one bit'],
	['2001:db8::5',             ['::/0'],                                   true,   'v6: /0 matches anything'],
	['fe80::1',                 ['::/0'],                                   true,   'v6: /0 matches another address'],
	['::1',                     ['::/127'],                                 true,   'v6: ::1 inside ::/127'],
	['::2',                     ['::/127'],                                 false,  'v6: ::2 outside ::/127 (one bit beyond)'],

	// --- IPv6 CIDR bit-boundary stress (partial-byte mask correctness) ---
	['2001:db8:8000::',         ['2001:db8::/33'],                          false,  'v6: /33 bit 33 differs'],
	['2001:db8:7fff::',         ['2001:db8::/33'],                          true,   'v6: /33 bit 33 matches'],

	// --- IPv6 reverse-match ---
	['2001:db8::5',             ['!2001:db8::/32'],                         false,  'v6: !CIDR suppresses inside-prefix'],
	['2001:db8::5',             ['!2001:db8:1234::/48'],                    true,   'v6: !CIDR for non-member'],

	// --- Cross-family: no false matches across IPv4/IPv6 ---
	['192.0.2.5',               ['2001:db8::/32'],                          false,  'mixed: v4 query, v6 CIDR pattern'],
	['2001:db8::5',             ['192.0.2.0/24'],                           false,  'mixed: v6 query, v4 CIDR pattern'],

	// --- Multi-pattern lists ---
	['2001:db8::1',             ['10.0.0.0/8', '203.0.113.7', '2001:db8::/32'],     true,   'list: v6 CIDR after v4 entries'],
	['203.0.113.7',             ['10.0.0.0/8', '203.0.113.7', '2001:db8::/32'],     true,   'list: exact v4 mid-list'],
	['198.51.100.1',            ['10.0.0.0/8', '203.0.113.7', '2001:db8::/32'],     false,  'list: no match'],

	// --- Malformed patterns must not crash and must not falsely match ---
	['2001:db8::5',             ['notanaddress/32'],                        false,  'malformed: garbage with /32'],
	['2001:db8::5',             ['/32'],                                    false,  'malformed: empty addr / slash 32'],
	['2001:db8::5',             ['2001:db8::/200'],                         false,  'malformed: v6 subnet > 128'],
	['192.0.2.5',               ['192.0.2.0/99'],                           false,  'malformed: v4 subnet > 32'],
];

var failures = [];
for (var i = 0; i < cases.length; i++) {
	var search   = cases[i][0];
	var patterns = cases[i][1];
	var expected = cases[i][2];
	var label    = cases[i][3];
	var got      = system.findstr(patterns, search);
	if (got !== expected) {
		failures.push(label + ': system.findstr(' + JSON.stringify(patterns) + ', ' + JSON.stringify(search)
			+ ') returned ' + got + ', expected ' + expected);
	}
}

if (failures.length > 0)
	throw new Error('system.findstr() test failures:\n  ' + failures.join('\n  '));
