import { AvatarChatApp } from "./app/avatar-chat-app";
import { loadConfig } from "./io/config";

function loadIdleAnimationModules(): void {
  // Always reload — js.global persists across door invocations on the same node,
  // so stale cached modules would never pick up file changes.
  try {
    (js.global as any).CanvasAnimations = load(js.exec_dir + "lib/canvas-animations.js");
  } catch (error) {
    log("Avatar Chat: canvas-animations.js unavailable: " + String(error));
  }

  try {
    (js.global as any).AvatarsFloat = load(js.exec_dir + "lib/avatars-float.js");
  } catch (error) {
    log("Avatar Chat: avatars-float.js unavailable: " + String(error));
  }
}

function main(): void {
  loadIdleAnimationModules();

  const app = new AvatarChatApp(loadConfig());

  try {
    app.run();
  } catch (error) {
    log("Avatar Chat fatal error: " + String(error));
    console.clear(BG_BLACK | LIGHTGRAY);
    console.home();
    console.writeln("Avatar Chat error:");
    console.writeln(String(error));
  }
}

main();
