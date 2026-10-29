#!/usr/bin/env python3
"""Import only the Unicode 18 NFD/case-fold primitives used by v2 cache keys.

Usage: python3 tools/import_cache_unicode.py /path/to/v2/syncterm
The source generators record the upstream Unicode URLs and SHA-256 digests.
No collation, composition, grapheme, or display-name policy is imported.
"""
from pathlib import Path
import re
import sys

source = Path(sys.argv[1]) / 'src/text'
normalization = (source / 'unicode_normalization/generated/st_unicode_normalization.c').read_text()
normalization = re.sub(r'#include "st_unicode_normalization.h"\n', '', normalization)
normalization = re.sub(r'struct composition \{[^\n]+\n', '', normalization)
normalization = re.sub(r'static const struct composition compositions\[\] = \{.*?\n\};\n', '', normalization, flags=re.S)
normalization = normalization.split('\nbool\nst_unicode_canonical_compose')[0]
normalization = normalization.replace('\nuint8_t\nst_unicode_', '\nstatic uint8_t\nst_unicode_')
normalization = normalization.replace('\nsize_t\nst_unicode_', '\nstatic size_t\nst_unicode_')
compare = (source / 'unicode_compare/generated/st_unicode_compare_data.c').read_text()
fold = 'struct fold_row' + compare.split('struct fold_row', 1)[1].split('\n', 1)[0] + '\n\n'
fold += 'static const uint32_t fold_values' + compare.split('static const uint32_t fold_values', 1)[1].split('\nstatic const struct st_unicode_collation_element', 1)[0]
fold += '\nstatic size_t\nst_unicode_casefold_mapping' + compare.split('\nsize_t\nst_unicode_casefold_mapping', 1)[1].split('\nsize_t\nst_unicode_collation_mapping', 1)[0]
output = Path(__file__).resolve().parent.parent
(output / 'bbs_cache_unicode.h').write_text('/* Imported by tools/import_cache_unicode.py; do not edit.\n * Cache-key compatibility subset of SyncTERM v2 Unicode 18.0.0 data.\n * Source: SyncTERM v2.0a, commit 9c121a0.\n * Upstream: https://www.unicode.org/Public/18.0.0/ucd/UnicodeData.txt\n *           https://www.unicode.org/Public/18.0.0/ucd/CaseFolding.txt\n * See LICENSE.UNICODE for the data license.\n */\n\n' + normalization + '\n' + fold)
(output / 'LICENSE.UNICODE').write_bytes((source / 'unicode_text/LICENSE.UNICODE').read_bytes())
