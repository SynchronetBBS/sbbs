const fs = require("fs");
const path = require("path");
const childProcess = require("child_process");

const projectDir = __dirname;
const isWatch = process.argv.indexOf("--watch") !== -1;
const typecheckOnly = process.argv.indexOf("--typecheck") !== -1;

function firstExisting(paths) {
  let index;
  for (index = 0; index < paths.length; index += 1) {
    if (fs.existsSync(paths[index])) {
      return paths[index];
    }
  }
  return null;
}

function requireFromCandidates(moduleCandidates) {
  let index;
  let lastError = null;

  for (index = 0; index < moduleCandidates.length; index += 1) {
    try {
      return require(moduleCandidates[index]);
    } catch (error) {
      lastError = error;
    }
  }

  throw lastError || new Error("Unable to resolve required module");
}

function resolveTypeScriptCli() {
  return firstExisting([
    path.join(projectDir, "node_modules", "typescript", "lib", "tsc.js"),
    "/sbbs/repo/xtrn/experiment2/node_modules/typescript/lib/tsc.js",
    "/sbbs/repo/xtrn/sysop_token_manager/node_modules/typescript/lib/tsc.js"
  ]);
}

function resolveEsbuild() {
  return requireFromCandidates([
    path.join(projectDir, "node_modules", "esbuild"),
    "/sbbs/repo/xtrn/experiment2/node_modules/esbuild",
    "/sbbs/repo/xtrn/sysop_token_manager/node_modules/esbuild"
  ]);
}

function runTypecheck() {
  runTsc("tsconfig.json");
}

function runCompile() {
  const buildDir = path.join(projectDir, "build");

  if (fs.existsSync(buildDir)) {
    fs.rmSync(buildDir, { recursive: true, force: true });
  }

  runTsc("tsconfig.build.json");
}

function runTsc(projectFile) {
  const tscPath = resolveTypeScriptCli();
  let result;

  if (!tscPath) {
    throw new Error("TypeScript compiler was not found");
  }

  result = childProcess.spawnSync(
    process.execPath,
    [tscPath, "-p", path.join(projectDir, projectFile)],
    { stdio: "inherit" }
  );

  if (result.status !== 0) {
    process.exit(result.status || 1);
  }
}

function buildOnce() {
  const esbuild = resolveEsbuild();
  const banner = [
    "// Avatar Chat - Auto-generated, do not edit directly.",
    "// Built: " + new Date().toISOString(),
    "load(\"sbbsdefs.js\");",
    "load(\"key_defs.js\");",
    "load(\"frame.js\");",
    "load(\"json-client.js\");",
    "load(\"json-chat.js\");"
  ].join("\n");

  runTypecheck();
  runCompile();

  return esbuild.build({
    entryPoints: [path.join(projectDir, "build", "main.js")],
    bundle: true,
    charset: "ascii",
    format: "iife",
    target: "es5",
    platform: "neutral",
    outfile: path.join(projectDir, "avatar_chat.js"),
    banner: { js: banner },
    sourcemap: false,
    minify: false,
    logLevel: "info"
  }).then(function () {
    const outputFile = path.join(projectDir, "avatar_chat.js");
    const buildDir = path.join(projectDir, "build");
    let content = fs.readFileSync(outputFile, "utf8");

    content = content.replace(/"use strict";\s*/g, "");
    fs.writeFileSync(outputFile, content, "utf8");
    if (fs.existsSync(buildDir)) {
      fs.rmSync(buildDir, { recursive: true, force: true });
    }
    console.log("Built " + outputFile);
  });
}

if (typecheckOnly) {
  runTypecheck();
} else if (isWatch) {
  buildOnce().then(function () {
    console.log("Watch mode is not wired for the fallback toolchain; ran a single build instead.");
  }).catch(function (error) {
    console.error(error);
    process.exit(1);
  });
} else {
  buildOnce().catch(function (error) {
    console.error(error);
    process.exit(1);
  });
}
