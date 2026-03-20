# Copilot Instructions - Synchronet + TypeScript Standards

> **This document is the operating contract for AI coding agents in this repository.**
> Code that violates these rules is considered incorrect, even if it compiles.

---

## 1. Runtime Reality

**Target runtime: Synchronet JavaScript (SpiderMonkey-family runtime), not Node.js or browsers.**

- Runtime code executes inside Synchronet script context.
- Runtime loading uses `load("file.js")`.
- Final deliverable must be a runnable Synchronet script (typically one flattened `dist/*.js` file).
- Do not rely on runtime module systems (`require`, ESM imports, CommonJS exports).

**Synchronet globals are authoritative:**

- `console`, `js`, `bbs`, `user`, `system`
- `file_area`, `msg_area`
- Synchronet-provided libraries in `exec/load/*.js`

Example:
```javascript
load("sbbsdefs.js");
load("frame.js");
load("tree.js");
```

---

## 2. Why TypeScript Is Required

TypeScript is mandatory because it reduces invalid AI-generated code and keeps large refactors safe.

- Types make contracts explicit so AI tools produce compatible code more consistently.
- `strict` typing catches dead paths, impossible states, and missing null checks before runtime.
- Stable interfaces reduce drift between subsystems and prevent "guessing" APIs.
- Clear type boundaries lower hallucination risk around Synchronet objects and custom models.
- Typed data models make JSON/file parsing safer and easier to validate.

**Rule:** TypeScript is used for authoring; compiled JavaScript is used for Synchronet runtime execution.

---

## 3. Hard Prohibitions

### 3.1 No Node.js or Browser APIs in Synchronet Runtime Code

Forbidden:

- `process`, `Buffer`, `fs`, `path`
- `window`, `document`, `fetch`, `XMLHttpRequest`
- `Promise`/`async` if target runtime compatibility is uncertain
- Module loader assumptions at runtime

### 3.2 Do Not Rebuild Synchronet APIs

Forbidden:

- Wrappers that duplicate `console`, `bbs`, `File`, or core Synchronet behavior
- "Portable platform layer" abstractions not needed for this project

Preferred:

- Direct Synchronet API usage
- Thin typed facades only where needed for compile-time safety

### 3.3 No Runtime Module Syntax in Final Output

Forbidden in generated deployable script:

```javascript
import ...
export ...
module.exports = ...
require(...)
```

---

## 4. Project Structure Guidelines

Use clear subsystem boundaries with minimal coupling.

- `src/main.ts`: entrypoint only
- `src/app/`: orchestration, lifecycle, state transitions
- `src/input/`: raw key parsing to domain actions
- `src/domain/`: core entities and business rules
- `src/render/`: all terminal output and view composition
- `src/io/`: file/config persistence and parsing
- `src/timing/`: tick/frame timing and scheduler logic
- `src/util/`: pure helpers
- `src/synchro/`: Synchronet type declarations and runtime compatibility notes

Rules:

- No circular dependencies.
- Rendering does not own business logic.
- Input does not mutate unrelated systems directly.
- Synchronet-specific assumptions stay isolated and explicit.

---

## 5. Build and Output Contract

- Compile TypeScript to Synchronet-compatible JavaScript.
- Prefer a single deployable script when practical.
- Verify emitted output contains no forbidden module syntax.
- Treat build warnings as actionable, not optional.

Recommended `tsconfig` posture:

- `"strict": true`
- `"noImplicitAny": true`
- `"noUncheckedIndexedAccess": true`
- `"noFallthroughCasesInSwitch": true`

---

## 6. Performance Rules for Terminal Apps

- Minimize console writes.
- Prefer `frame.js` (or proven alternatives) over manual per-cell output loops.
- Redraw only changed regions.
- Reuse buffers in hot paths.
- Avoid unnecessary allocations each tick/frame.
- Profile first, optimize second.

---

## 7. File I/O and Data Safety

- Use Synchronet `File` object APIs.
- Build paths from `js.exec_dir` unless a different base is explicitly required.
- Handle missing files and parse failures gracefully.
- Validate external data before using it.
- Never assume shape of loaded JSON without checks.

---

## 8. Error Handling and Cleanup

- Use `try/finally` around lifecycle boundaries to restore terminal state.
- Log with Synchronet logging facilities.
- Fail fast on unrecoverable startup errors.
- Degrade gracefully for recoverable runtime errors.

---

## 9. AI Agent Coding Rules

- Prefer small, typed functions with explicit inputs/outputs.
- Avoid speculative abstractions and premature frameworks.
- Do not generate unused classes, placeholder systems, or dead interfaces.
- Keep changes local and explain assumptions in comments only when necessary.
- Update type definitions and docs when contracts change.

---

## 10. Testing Strategy

- Run type-check and build on every change.
- Manually test inside Synchronet for runtime behavior.
- Unit-test pure TypeScript utilities separately where feasible.
- Add regression checks for previously fixed runtime issues.

---

## 11. Git and Change Hygiene

- Keep commits focused and subsystem-scoped.
- Do not commit generated artifacts unless repo policy requires it.
- Do not commit temp files, editor junk, or local caches.
- Keep docs aligned with architecture and runtime decisions.

---

## 12. Pre-Commit Checklist

- [ ] Type-check passes (`strict` mode).
- [ ] Build succeeds.
- [ ] Output script is Synchronet-runtime compatible.
- [ ] No Node/browser APIs in runtime code.
- [ ] No runtime module syntax in deployable output.
- [ ] Subsystem boundaries still respected.
- [ ] Docs/types updated for changed contracts.

---

## Non-Negotiables

1. Synchronet runtime compatibility is the top constraint.
2. TypeScript is required for maintainability and AI reliability.
3. Use Synchronet-native capabilities before inventing abstractions.
4. Generated runtime output must be deployable in Synchronet without manual patching.

