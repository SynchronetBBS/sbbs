#!/usr/bin/env nodejs
"use strict";

const fs = require("fs");
const path = require("path");
const cp = require("child_process");

const root = __dirname;

function findTsc() {
  const envPath = process.env.TSC_PATH;
  const candidates = [
    envPath,
    path.join(root, "node_modules", "typescript", "lib", "tsc.js"),
    "/sbbs/repo/xtrn/avatar_chat/node_modules/typescript/lib/tsc.js",
    "/sbbs/repo/xtrn/experiment2/node_modules/typescript/lib/tsc.js",
    "/sbbs/repo/xtrn/sysop_token_manager/node_modules/typescript/lib/tsc.js"
  ];

  for (const candidate of candidates) {
    if (candidate && fs.existsSync(candidate)) {
      return candidate;
    }
  }

  return null;
}

function run() {
  const tscPath = findTsc();
  if (!tscPath) {
    console.error("Unable to locate TypeScript compiler.");
    console.error("Set TSC_PATH or install typescript in this project.");
    process.exit(1);
  }

  const args = [tscPath, "-p", path.join(root, "tsconfig.json"), ...process.argv.slice(2)];
  const result = cp.spawnSync(process.execPath, args, {
    stdio: "inherit",
    cwd: root
  });

  if (typeof result.status === "number") {
    process.exit(result.status);
  }
  process.exit(1);
}

run();
