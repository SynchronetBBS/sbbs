# Vendor portability review

Status: IN PROGRESS, continued through the sntrup integer correction on2026-11-10.
The earlier findings in this document described an older source revision.
The current implementation and remaining acceptance requirements are below;
this record does not certify the complete vendor implementation.

## Current source reconciliation

| Area | Current implementation | Remaining proof |
| --- | --- | --- |
| sntrup761 generic helper symbols | `kex/sntrup761_optblocker.c` defines all three zero-initialized volatile objects, and `CMakeLists.txt` includes it. R022 now separately compiles/runs native and forced-generic canonical-source arithmetic and raw KEM controls against the real backends. | The exact range/promotion/caller record qualifies the corrected source; actual foreign compiler/width execution and full timing/primitive acceptance remain separate. Linking the object alone is not runtime proof. |
| sntrup761 unused annotations | `CRYPTOINT_MAYBE_UNUSED` selects the GNU/Clang attribute, the MSVC warning pragma or an empty fallback. | Native Windows and other compiler acceptance retain their own scope; compiler recognition does not establish constant-time behavior. |
| libcrux population count | The current glue selects MSVC, GNU-compatible or portable C code. The portable branch counts the bits in a `uint8_t` using bounded masks and shifts. The former absent-fallback item is resolved in source. | Exercise compiler branches, including the portable implementation; inspect generated code for the supported compiler/target combinations. |
| libcrux inclusion | A conventional `LIBCRUX_MLKEM768_SHA3_H` outer guard replaces the former pragma. Implementation reading is complete. | Complete upstream/local-delta and remaining caller-contract review. |
| ML-KEM byte encoding | `core_num__u64_9__to_le_bytes`, `core_num__u64_9__from_le_bytes` and `core_num__u32_8__from_le_bytes` explicitly assemble or emit octets. `mlkem768.c` no longer selects byte order through compiler macros. | Finish all other arithmetic, object-layout and width assumptions. Current-host vectors alone do not establish other-target execution. |

## Generated ML-KEM source review

The implementation throughout `kex/libcrux_mlkem768_sha3.h`
has now been read. Generated block annotations were omitted from the code view
with source line numbers preserved; substantive descriptions/preconditions were
then read separately, and repetitive Rust implementation/type annotations were
indexed. The 256-row rejection table is checked in full against an independent
mask-to-byte-index construction. Reading completion does not close the remaining
correctness, portability, cleanup, provenance or execution requirements.

The byte-conversion shifts are bounded by the eight/four-byte loops and use
unsigned widened operands. The wrapping helpers intentionally convert to the
destination unsigned width. The nonzero helper operates on an octet, and the
selection mask chooses 32 bytes without a source-level secret-dependent branch.
This is a source observation, not a generated-code timing result.

The generated slice macros do not validate lengths. Establish their bounds
from every reachable caller, including multiplication/addition, array conversion,
padding, equal-length comparison and split operations. The inspected ML-KEM
decapsulation splits the fixed 2,400-byte secret into 1,152/1,184/32/32-byte
regions; its comparison receives two 1,088-byte ciphertext slices and its
selection receives two 32-byte secret slices. Other caller chains remain open.
The SHA3-512/256 block loaders use72/136-byte rates; final absorption derives
its remainder with the same modulus, below the rate. SHAKE256 uses136 bytes
and delimiter31, while SHA3 uses delimiter6. The reachable incremental SHAKE128
path absorbs fixed34-byte matrix seeds, initially emits504 bytes and later168
bytes. All rate blocks fit the 5-by-5 state and use explicit little-endian
conversion. NTT table indices/loop scheduling, arithmetic intermediate bounds
and all secret cleanup remain separate obligations.

The compiler's current macro-expanded AST of the production `mlkem768.c` translation
unit indexes504 function definitions, with347 reached from the three private
wrappers and no reached indirect calls. External calls are `dssh_random`,
`dssh_cleanse`, `memcpy`, `memcmp` and `abort`. The reached `memcmp` occurs in
public-key canonical validation; `abort` occurs in three unwrap helpers whose
conversion glue unconditionally assigns the success tag. The generic buffered
XOF routines, NEON stubs, private-key-validation helper and11-bit ring serializer
are outside that native wrapper call closure. These are scoped native compiler
observations, not other-platform or argument-invariant proofs. The compact
compiler index and command/diagnostics remain in build-audit-logs.

## Direct glue corrections and acceptance

The private range helper now follows unsigned half-open range semantics even
when start exceeds end. The private11-bit polynomial serializer uses its full
352-byte result extent. Neither changes an installed declaration, export or
supported algorithm. `test/test_vendor_glue.c` checks empty/reversed/maximum
ranges;3329 full256-coefficient encodings against scalar rounding/bit packing,
including outer guards; all256 shuffle rows; and all128 NTT constants against
modular exponentiation and seven-bit reversal.

The standalone target deliberately has no provider/library dependency. Nine
sequential configure/build/test stages pass, one selected CTest each on the
ASan/UBSan, Botan-build and TSAN configurations, with one build/test job on
CPUs0–7. ASan leak detection is disabled because this native runtime does not
support it; the initial unsupported-setting failure is retained separately.
No sanitizer diagnostics or skipped cases occur in the accepted executions.
The [hash-bound validation record](release-1.0-evidence/vendor-glue-validation.json)
binds exact sources, executables, commands and logs. These checks establish the
reported glue/model scope, not production provider/module/peer acceptance.

## Remaining cross-file acceptance

The arithmetic continuation below resolves its specified signed conversion,
reduction and fixed-path bounds cluster. Finish all other caller/width and
stack assumptions, secret
intermediate lifetime, generic-path behavior, upstream correspondence and
independent positive/negative vectors. Check the wrappers and both providers
against the exact protocol and validation contracts. Preserve the distinction
between source inspection, native execution and other-platform evidence.

Existing historical conversion/parity test results remain recorded in
`release-1.0-audit.md` with their original source/provider scope.
Native Windows testing remains the final CI stage after the
local audit and validation requirements are complete.

## Sntrup signed-width continuation

The [complete integer continuation](release-1.0-evidence/sntrup-arithmetic-review.md)
now binds an actual native generic-build failure, compatible correction and
106 passing CTests across ASan/UBSan, Botan and full-crypto unsuppressed TSAN.
Thirty-six strict direct-source cases distinguish native/generic compilation,
exhaustive reviewed domains and their full KEM roundtrips. Named-array regression,
2538 controlled KEX scenarios, twelve production OpenSSH exchanges and nine
handshake-only controls retain their explicit scopes. All306 source inputs stay
fixed during acceptance, with no internal skips or sanitizer diagnostic.

Defined signed reconstruction/floor shifts, fixed-width field intermediates and
mask selection, public codec/division bounds, samplers and unsigned packing are
reconciled with exact primary/source models. The55-definition/two-profile AST
also retains the52-owner retirement mapping. Twenty-four optimized native Clang
probe bodies have no conditional jumps/calls or value-indexed loads. This does
not close other-target timing, all compiler-created copies, complete primitive
known answers, upstream correspondence or Windows/SDK/release acceptance.

## Signed arithmetic continuation on base f2fb657

The glue now constructs signed low words explicitly and computes floor shifts
through unsigned values and bounded wide subtraction. Barrett quotient/products
remain32-bit even with16-bit int. Montgomery low words, quotient extraction,
message compression and vector sign masks use defined conversions/shifts.
Bit extraction uses a32-bit constant and handles the complete byte-sized count
domain. The two new helpers are private; no public header, ABI, supported method
or allocation/ownership behavior changes.

The direct runner checks262144 low-word inputs, all65536 signed16 values for
Barrett and each production shift,18 int32 boundary values across0..33 and
UINT_MAX shifts, and589824 Montgomery inputs spanning every low word at nine
high-word planes. Its independent oracles use remainder/centering,64-bit exact
division and scalar bit packing, rather than the production shift expressions.
All65536 sign-mask/representative inputs, all3329 field values for message and
4/5/10/11-bit compression, and all256 bit counts are checked. Prior polynomial,
shuffle, constant and range checks remain active.

A conservative range model follows the reviewed fixed K=3 paths: lazy forward
NTT bounds16552 for public/ciphertext inputs and14371 for eta2 secret inputs;
even two4095-bound operands yield a rank-three inverse input bound6531. All
modeled vector sums/differences stay within int16, and products within int32.
Inverse stages, domain conversions and final decryption subtraction are
included. The model is source-reconciled mathematical evidence, not formal
verification or native16-bit execution. Arbitrary caller contracts of unused
generic generated routines remain separate.

All99 supervised commands exit0: native build/inventory/CTest stages3/4/3 and
90 exact protocol leaves, ten cases with each host key on each configuration.
Each exact process selects one passing leaf, with no skips or sanitizer report.
Retained storage tests pass7/5/7 groups; Botan also passes both independent KEM
directions. The run uses at most two build/test jobs on CPUs0–7. Nineteen selected
artifacts are bound:12 change and7 remain identical to their preceding evidence.
All three shared libraries still export exactly125 public functions.

The actual native Clang19.1.7 x86_64 -O2 output for three primitive probes has no
conditional jumps, calls or value-indexed loads. This observation is limited to
those routines/compiler/target, and does not certify other compilers or the
whole vendor implementation's timing. The
[validation record](release-1.0-evidence/validation-2026-11-06-vendor-arithmetic.json)
binds sources, native executables, commands, model, compiler AST and assembly.
Whole secret/provider/platform/upstream/peer/SDK and release gates remain open;
the independent native TLS qualification retains its separate recorded scope.


R110 subsequently adds the complete official100-record valid-response and
sampled-input corpus in [the known-answer review](release-1.0-evidence/sntrup-kat-review.md). Both native/generic
implementations and providers reproduce exact decapsulation and fixed-input
encoding/cache/ciphertext/session answers. This closes that positive-answer gap;
deterministic key generation/random sampling, invalid answers and other stated
upstream/compiler/platform/timing/runtime/release qualifications remain open.


R111 subsequently closes native complete positive seeded generation/sampling
correspondence in [the seeded-generation review](release-1.0-evidence/sntrup-generation-review.md). All100 published seeds, full
generated keys, sampled ciphertexts and session secrets match in both arithmetic
modes and providers. The test adapter preserves the original per-word DRBG
request boundaries; production randomness is unchanged. Independent invalid
answers and other source/timing/foreign-target/runtime/release gates remain open.


R112 adds [independent rejection answers](release-1.0-evidence/sntrup-rejection-review.md) for all confirmation bits and
body-byte positions of one record, plus three selected cases on every published
record. Full fallback bytes, input preservation/guards and actual before/after
positive controls pass on both arithmetic modes/providers. These are independently
derived specification answers; upstream/timing/foreign-target/runtime/SDK and
other release qualifications remain open.


R113 completes [the bundled/current-upstream correspondence map](release-1.0-evidence/sntrup-correspondence-review.md) and explicitly
interprets bulk sample words in place. All56 local definitions,93 omitted helpers
and16 native architecture branches are reconciled. Current primitive/KEX/peer
acceptance and complete native builds pass, including the corrected standalone
math dependency. Foreign execution/stack/timing, compiler/provider copies, native
runtime and other release qualifications remain open.


R114 adds [the complete reachable bundled owner inventory and CCA scratch
retirement](release-1.0-evidence/mlkem-retirement-review.md).37 actual named sites are qualified by live observations and complete
official answers across three existing native profiles. Deeper polynomial/hash,
logical-return/compiler and native Botan owners, wider requirements and runtime/
platform/release acceptance remain OPEN.
