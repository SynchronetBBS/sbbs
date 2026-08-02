/**
 * Minimal ambient declarations so the vitest tests can read bundled font
 * fixtures from disk without pulling all of @types/node (which would fight the
 * ES5-only lib constraint). Runtime is real Node under vitest; this only
 * satisfies `tsc --noEmit`.
 */

declare module 'fs' {
  export function readFileSync(path: string): { readonly length: number;[index: number]: number };
  export function existsSync(path: string): boolean;
}

declare module 'path' {
  export function join(...parts: string[]): string;
}

declare var __dirname: string;
declare var process: { env: { [key: string]: string | undefined } };
