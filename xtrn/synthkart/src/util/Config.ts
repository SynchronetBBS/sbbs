/**
 * Config - Load and parse synthkart.ini configuration file
 */

/**
 * Configuration structure
 */
interface OutrunConfig {
  general: {
    gameName: string;
  };
  highscores: {
    server: string;      // 'file' for local file, hostname for json-service
    port: number;         // json-service port (default 10088)
    serviceName: string;  // scope for json-service
    filePath: string;     // local file path (when server='file')
  };
  ansiTunnel: {
    directory: string;
    maxRows: number;
    scrollSpeed: number;
  };
}

/**
 * Load and parse synthkart.ini configuration
 */
function loadConfig(): OutrunConfig {
  var configPath = js.exec_dir + 'synthkart.ini';
  
  // Default configuration
  var config: OutrunConfig = {
    general: {
      gameName: 'OUTRUN'
    },
    highscores: {
      server: 'file',
      port: 10088,
      serviceName: 'synthkart',
      filePath: 'highscores.json'
    },
    ansiTunnel: {
      directory: 'ansi_art',
      maxRows: 2000,
      scrollSpeed: 0.5
    }
  };
  
  // Try to load config file
  if (!file_exists(configPath)) {
    logWarning('Config file not found: ' + configPath + ', using defaults');
    return config;
  }
  
  try {
    var file = new File(configPath);
    if (!file.open('r')) {
      logWarning('Failed to open config file: ' + configPath);
      return config;
    }
    
    var currentSection = '';
    var line: string;
    
    while ((line = file.readln()) !== null) {
      // Remove whitespace
      line = line.trim();
      
      // Skip comments and empty lines
      if (line === '' || line.charAt(0) === ';' || line.charAt(0) === '#') {
        continue;
      }
      
      // Check for section header
      if (line.charAt(0) === '[' && line.charAt(line.length - 1) === ']') {
        currentSection = line.substring(1, line.length - 1).toLowerCase().replace(/_/g, '');
        continue;
      }
      
      // Parse key=value
      var equals = line.indexOf('=');
      if (equals === -1) continue;
      
      var key = line.substring(0, equals).trim().toLowerCase().replace(/_/g, '');
      var value = line.substring(equals + 1).trim();
      
      // Remove quotes if present
      if (value.length >= 2 && value.charAt(0) === '"' && value.charAt(value.length - 1) === '"') {
        value = value.substring(1, value.length - 1);
      }
      
      // Assign to config based on section
      if (currentSection === 'general') {
        if (key === 'gamename') config.general.gameName = value;
      } else if (currentSection === 'highscores') {
        if (key === 'server') {
          config.highscores.server = value;
        } else if (key === 'port') {
          var p = parseInt(value, 10);
          if (!isNaN(p) && p > 0) config.highscores.port = p;
        } else if (key === 'servicename') {
          config.highscores.serviceName = value;
        } else if (key === 'filepath') {
          config.highscores.filePath = value;
        }
      } else if (currentSection === 'ansitunnel') {
        if (key === 'directory') {
          config.ansiTunnel.directory = value;
        } else if (key === 'maxrows') {
          var num = parseInt(value, 10);
          if (!isNaN(num) && num > 0) config.ansiTunnel.maxRows = num;
        } else if (key === 'scrollspeed') {
          var speed = parseFloat(value);
          if (!isNaN(speed) && speed > 0) config.ansiTunnel.scrollSpeed = speed;
        }
      }
    }
    
    file.close();
  } catch (e) {
    logError('Error loading config file: ' + e);
  }
  
  // Resolve relative paths to absolute
  if (config.highscores.filePath.charAt(0) !== '/' && 
      config.highscores.filePath.indexOf(':') === -1) {
    config.highscores.filePath = js.exec_dir + config.highscores.filePath;
  }
  
  if (config.ansiTunnel.directory.charAt(0) !== '/' && 
      config.ansiTunnel.directory.indexOf(':') === -1) {
    config.ansiTunnel.directory = js.exec_dir + config.ansiTunnel.directory;
  }
  
  logInfo('Config loaded: ansiDir=' + config.ansiTunnel.directory + 
          ' hsServer=' + config.highscores.server);
  
  return config;
}

// Global config instance
var OUTRUN_CONFIG = loadConfig();
