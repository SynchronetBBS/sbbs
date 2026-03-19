export interface AvatarChatConfig {
  host: string;
  port: number;
  defaultChannel: string;
  maxHistory: number;
  pollDelayMs: number;
  reconnectDelayMs: number;
  inputMaxLength: number;
}

const DEFAULT_CONFIG: AvatarChatConfig = {
  host: "127.0.0.1",
  port: 10088,
  defaultChannel: "main",
  maxHistory: 200,
  pollDelayMs: 25,
  reconnectDelayMs: 3000,
  inputMaxLength: 500
};

function readString(file: File, key: string, defaultValue: string): string {
  const value = file.iniGetValue(null, key, defaultValue);
  if (value === undefined || value === null) {
    return defaultValue;
  }
  return String(value);
}

function readNumber(file: File, key: string, defaultValue: number): number {
  const value = file.iniGetValue(null, key, defaultValue);
  const parsed = parseInt(String(value), 10);
  if (isNaN(parsed)) {
    return defaultValue;
  }
  return parsed;
}

export function loadConfig(): AvatarChatConfig {
  const configPath = js.exec_dir + "avatar_chat.ini";
  const file = new File(configPath);
  const config: AvatarChatConfig = {
    host: DEFAULT_CONFIG.host,
    port: DEFAULT_CONFIG.port,
    defaultChannel: DEFAULT_CONFIG.defaultChannel,
    maxHistory: DEFAULT_CONFIG.maxHistory,
    pollDelayMs: DEFAULT_CONFIG.pollDelayMs,
    reconnectDelayMs: DEFAULT_CONFIG.reconnectDelayMs,
    inputMaxLength: DEFAULT_CONFIG.inputMaxLength
  };

  if (!file.open("r")) {
    return config;
  }

  config.host = readString(file, "host", DEFAULT_CONFIG.host);
  config.port = readNumber(file, "port", DEFAULT_CONFIG.port);
  config.defaultChannel = readString(file, "default_channel", DEFAULT_CONFIG.defaultChannel);
  config.maxHistory = readNumber(file, "max_history", DEFAULT_CONFIG.maxHistory);
  config.pollDelayMs = readNumber(file, "poll_delay_ms", DEFAULT_CONFIG.pollDelayMs);
  config.reconnectDelayMs = readNumber(file, "reconnect_delay_ms", DEFAULT_CONFIG.reconnectDelayMs);
  config.inputMaxLength = readNumber(file, "input_max_length", DEFAULT_CONFIG.inputMaxLength);
  file.close();

  if (config.port < 1) {
    config.port = DEFAULT_CONFIG.port;
  }
  if (config.maxHistory < 10) {
    config.maxHistory = DEFAULT_CONFIG.maxHistory;
  }
  if (config.pollDelayMs < 5) {
    config.pollDelayMs = DEFAULT_CONFIG.pollDelayMs;
  }
  if (config.reconnectDelayMs < 250) {
    config.reconnectDelayMs = DEFAULT_CONFIG.reconnectDelayMs;
  }
  if (config.inputMaxLength < 64) {
    config.inputMaxLength = DEFAULT_CONFIG.inputMaxLength;
  }

  return config;
}
