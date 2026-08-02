// Bundles tsc's ES5 output (build/) into single files Synchronet can load().
//
// Two-step build (tsc -> esbuild) is deliberate: esbuild cannot LOWER modern
// syntax to ES5 (classes etc.), but it happily BUNDLES code that is already
// ES5. tsc does the lowering, esbuild does the linking. target:'es5' below
// acts as a tripwire: if any post-ES5 syntax sneaks into the output, the
// bundle step fails loudly instead of producing a file that syntax-errors
// on the BBS.
import { build } from 'esbuild';

const common = {
  bundle: true,
  format: 'iife',
  target: 'es5',
  platform: 'neutral',
  mainFields: ['main'],
  logLevel: 'info',
  // Baked-in build stamp: shown in the title bar and logged at startup so a
  // running editor can always be matched to a build. Friendly local form:
  // "YYYY-MM-DD HH:MM" (build machine's local time).
  define: {
    BUILD_STAMP: JSON.stringify((() => {
      const d = new Date();
      const p = (n) => String(n).padStart(2, '0');
      return `${d.getFullYear()}-${p(d.getMonth() + 1)}-${p(d.getDate())} ${p(d.getHours())}:${p(d.getMinutes())}`;
    })()),
  },
};

// The editor itself: registered in ctrl/xtrn.ini as
//   cmd=?/sbbs/xtrn/future_edit/HERMedIT.js %f
await build({
  ...common,
  entryPoints: ['build/main.js'],
  outfile: 'HERMedIT.js',
});

// Headless smoke runner for jsexec (no terminal): exercises the pure core
// (codecs, wrap, flatten, drop-file parsers) on the real SpiderMonkey engine.
await build({
  ...common,
  entryPoints: ['build/smoke.js'],
  outfile: 'dist/smoke_runner.js',
});
