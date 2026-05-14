nyhBoolean Search Syntaxn

n  At any hcText to search for:n prompt you may use boolean operators
  to combine multiple search terms:

     hgword1 & word2n     both present                  hyANDn keyword works too
     hgword1 | word2n     either present                hyORn  keyword works too
     hg! wordn             absent                        hyNOTn keyword works too
     hg( ... )n            grouping; precedence is NOT > AND > OR
     hg"word"n             whole word; won't match inside a longer word
     hg"phrase with space"n literal text, may contain &, |, !, parens

nhcExamplesnhw:n
     gyahoo and NOT bouncedn           'yahoo' present, 'bounced' absent
     g"PCBoard" | "Wildcat"n          either whole word
     g(spam | scam) & ! sysopn         spam/scam without the word sysop
     g"smith & jones"n                 literal phrase including the &

  Matching is case-insensitive substring (or whole-word inside quotes).
  Press h?n at any search prompt to redisplay this help.
