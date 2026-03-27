export interface IdleAnimationConfig {
  enabled: boolean;
  idleTimeoutSeconds: number;
  switchInterval: number;
  fps: number;
  random: boolean;
  sequence: string[];
  disable: string[];
  clearOnSwitch: boolean;
  figletMessages: string;
  figletRefresh: number;
  figletFonts: string;
  figletColors: boolean;
  figletMove: boolean;
  useAvatarFrames: boolean;
  starCount: number;
  auroraSpeed: number;
  auroraWave: number;
  matrixSparse: number;
  plasmaSpeed: number;
  plasmaScale: number;
  tunnelSpeed: number;
  tunnelScale: number;
  lissajousSpeed: number;
  fireDecay: number;
  rippleCount: number;
}

export interface AvatarChatConfig {
  host: string;
  port: number;
  defaultChannel: string;
  motdChannel: string;
  motdHostSystem: string;
  motdHostQwkid: string;
  maxHistory: number;
  pollDelayMs: number;
  reconnectDelayMs: number;
  inputMaxLength: number;
  idleAnimations: IdleAnimationConfig;
}

const DEFAULT_IDLE: IdleAnimationConfig = {
  enabled: true,
  idleTimeoutSeconds: 180,
  switchInterval: 60,
  fps: 4,
  random: true,
  sequence: [],
  disable: [],
  clearOnSwitch: true,
  figletMessages: "Avatar Chat",
  figletRefresh: 180,
  figletFonts: "",
  figletColors: true,
  figletMove: true,
  useAvatarFrames: true,
  starCount: 180,
  auroraSpeed: 0.12,
  auroraWave: 0.35,
  matrixSparse: 4,
  plasmaSpeed: 0.18,
  plasmaScale: 0.12,
  tunnelSpeed: 0.22,
  tunnelScale: 0.17,
  lissajousSpeed: 0.12,
  fireDecay: 1,
  rippleCount: 4
};

const DEFAULT_CONFIG: AvatarChatConfig = {
  host: "127.0.0.1",
  port: 10088,
  defaultChannel: "main",
  motdChannel: "motd",
  motdHostSystem: "",
  motdHostQwkid: "",
  maxHistory: 200,
  pollDelayMs: 25,
  reconnectDelayMs: 3000,
  inputMaxLength: 500,
  idleAnimations: DEFAULT_IDLE
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

function readFloat(file: File, key: string, defaultValue: number): number {
  const value = file.iniGetValue(null, key, defaultValue);
  const parsed = parseFloat(String(value));
  if (isNaN(parsed)) {
    return defaultValue;
  }
  return parsed;
}

function readBool(file: File, key: string, defaultValue: boolean): boolean {
  const value = file.iniGetValue(null, key, defaultValue);
  if (value === undefined || value === null) {
    return defaultValue;
  }
  const s = String(value).toLowerCase().trim();
  if (s === "true" || s === "1" || s === "yes") {
    return true;
  }
  if (s === "false" || s === "0" || s === "no") {
    return false;
  }
  return defaultValue;
}

function readList(file: File, key: string, defaultValue: string[]): string[] {
  const value = file.iniGetValue(null, key, "");
  if (value === undefined || value === null || String(value).trim() === "") {
    return defaultValue;
  }
  return String(value).split(",").map(function(s: string) { return s.trim(); }).filter(Boolean);
}

function loadIdleAnimConfig(file: File): IdleAnimationConfig {
  return {
    enabled: readBool(file, "idle_enabled", DEFAULT_IDLE.enabled),
    idleTimeoutSeconds: readNumber(file, "idle_timeout_seconds", DEFAULT_IDLE.idleTimeoutSeconds),
    switchInterval: readNumber(file, "idle_switch_interval", DEFAULT_IDLE.switchInterval),
    fps: readNumber(file, "idle_fps", DEFAULT_IDLE.fps),
    random: readBool(file, "idle_random", DEFAULT_IDLE.random),
    sequence: readList(file, "idle_sequence", DEFAULT_IDLE.sequence),
    disable: readList(file, "idle_disable", DEFAULT_IDLE.disable),
    clearOnSwitch: readBool(file, "idle_clear_on_switch", DEFAULT_IDLE.clearOnSwitch),
    figletMessages: readString(file, "idle_figlet_messages", DEFAULT_IDLE.figletMessages),
    figletRefresh: readNumber(file, "idle_figlet_refresh", DEFAULT_IDLE.figletRefresh),
    figletFonts: readString(file, "idle_figlet_fonts", DEFAULT_IDLE.figletFonts),
    figletColors: readBool(file, "idle_figlet_colors", DEFAULT_IDLE.figletColors),
    figletMove: readBool(file, "idle_figlet_move", DEFAULT_IDLE.figletMove),
    useAvatarFrames: readBool(file, "idle_use_avatar_frames", DEFAULT_IDLE.useAvatarFrames),
    starCount: readNumber(file, "idle_star_count", DEFAULT_IDLE.starCount),
    auroraSpeed: readFloat(file, "idle_aurora_speed", DEFAULT_IDLE.auroraSpeed),
    auroraWave: readFloat(file, "idle_aurora_wave", DEFAULT_IDLE.auroraWave),
    matrixSparse: readNumber(file, "idle_matrix_sparse", DEFAULT_IDLE.matrixSparse),
    plasmaSpeed: readFloat(file, "idle_plasma_speed", DEFAULT_IDLE.plasmaSpeed),
    plasmaScale: readFloat(file, "idle_plasma_scale", DEFAULT_IDLE.plasmaScale),
    tunnelSpeed: readFloat(file, "idle_tunnel_speed", DEFAULT_IDLE.tunnelSpeed),
    tunnelScale: readFloat(file, "idle_tunnel_scale", DEFAULT_IDLE.tunnelScale),
    lissajousSpeed: readFloat(file, "idle_lissajous_speed", DEFAULT_IDLE.lissajousSpeed),
    fireDecay: readNumber(file, "idle_fire_decay", DEFAULT_IDLE.fireDecay),
    rippleCount: readNumber(file, "idle_ripple_count", DEFAULT_IDLE.rippleCount)
  };
}

export function loadConfig(): AvatarChatConfig {
  const configPath = js.exec_dir + "avatar_chat.ini";
  const file = new File(configPath);
  const config: AvatarChatConfig = {
    host: DEFAULT_CONFIG.host,
    port: DEFAULT_CONFIG.port,
    defaultChannel: DEFAULT_CONFIG.defaultChannel,
    motdChannel: DEFAULT_CONFIG.motdChannel,
    motdHostSystem: DEFAULT_CONFIG.motdHostSystem,
    motdHostQwkid: DEFAULT_CONFIG.motdHostQwkid,
    maxHistory: DEFAULT_CONFIG.maxHistory,
    pollDelayMs: DEFAULT_CONFIG.pollDelayMs,
    reconnectDelayMs: DEFAULT_CONFIG.reconnectDelayMs,
    inputMaxLength: DEFAULT_CONFIG.inputMaxLength,
    idleAnimations: DEFAULT_IDLE
  };

  if (!file.open("r")) {
    return config;
  }

  config.host = readString(file, "host", DEFAULT_CONFIG.host);
  config.port = readNumber(file, "port", DEFAULT_CONFIG.port);
  config.defaultChannel = readString(file, "default_channel", DEFAULT_CONFIG.defaultChannel);
  config.motdChannel = readString(file, "motd_channel", DEFAULT_CONFIG.motdChannel);
  config.motdHostSystem = readString(file, "motd_host_system", DEFAULT_CONFIG.motdHostSystem);
  config.motdHostQwkid = readString(file, "motd_host_qwkid", DEFAULT_CONFIG.motdHostQwkid);
  config.maxHistory = readNumber(file, "max_history", DEFAULT_CONFIG.maxHistory);
  config.pollDelayMs = readNumber(file, "poll_delay_ms", DEFAULT_CONFIG.pollDelayMs);
  config.reconnectDelayMs = readNumber(file, "reconnect_delay_ms", DEFAULT_CONFIG.reconnectDelayMs);
  config.inputMaxLength = readNumber(file, "input_max_length", DEFAULT_CONFIG.inputMaxLength);
  config.idleAnimations = loadIdleAnimConfig(file);
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
