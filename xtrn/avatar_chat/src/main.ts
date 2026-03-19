import { AvatarChatApp } from "./app/avatar-chat-app";
import { loadConfig } from "./io/config";

function main(): void {
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
