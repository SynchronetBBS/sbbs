/**
 * HighScoreManager - Persistent high score tracking using Synchronet json-db
 * 
 * Manages three types of records:
 * - Track Time: Fastest completion time for a specific track
 * - Lap Time: Fastest single lap time for a specific track  
 * - Circuit Time: Fastest completion time for an entire circuit/cup
 * 
 * Each record type stores top 10 scores.
 */

// Type declarations for Synchronet JSON libraries
declare var JSONdb: any;
declare var JSONClient: any;

// Load json-db for local file storage
if (typeof JSONdb === 'undefined') {
  load('json-db.js');
}

// Load json-client for network storage
if (typeof JSONClient === 'undefined') {
  load('json-client.js');
}

/**
 * High score entry structure
 */
interface IHighScoreEntry {
  playerName: string;
  time: number;          // Time in seconds
  date: number;          // Timestamp (Date.now())
  trackName?: string;    // For track/lap records
  circuitName?: string;  // For circuit records
}

/**
 * High score record types
 */
enum HighScoreType {
  TRACK_TIME = 'track_time',
  LAP_TIME = 'lap_time',
  CIRCUIT_TIME = 'circuit_time'
}

class HighScoreManager {
  private localDb: any;      // JSONdb for local file storage
  private client: any;       // JSONClient for network storage  
  private serviceName: string;
  private useNetwork: boolean;
  private maxEntries: number = 10;

  constructor() {
    var config = OUTRUN_CONFIG.highscores;
    this.serviceName = config.serviceName;
    this.useNetwork = config.server !== 'file' && config.server !== '';
    
    try {
      if (this.useNetwork) {
        // Use JSONClient for network storage (local or remote json-service)
        this.client = new JSONClient(config.server, config.port);
        logInfo('High scores: connecting to ' + config.server + ':' + config.port + 
                ' service=' + this.serviceName);
      } else {
        // Use JSONdb for local file storage
        this.localDb = new JSONdb(config.filePath);
        this.localDb.settings.KEEP_READABLE = true;
        this.localDb.load();
        logInfo('High scores: using local file ' + config.filePath);
      }
    } catch (e) {
      logError("Failed to initialize high score storage: " + e);
      this.localDb = null;
      this.client = null;
    }
  }

  /**
   * Generate a key for storing/retrieving high scores
   */
  private getKey(type: HighScoreType, identifier: string): string {
    var sanitized = identifier.replace(/\s+/g, '_').toLowerCase();
    return type + '.' + sanitized;
  }
  
  /**
   * Get scores from local file storage
   */
  private getScoresLocal(key: string): IHighScoreEntry[] {
    if (!this.localDb) return [];
    try {
      this.localDb.load();
      var data = this.localDb.masterData.data || {};
      var scores = data[key];
      return (scores && Array.isArray(scores)) ? scores : [];
    } catch (e) {
      logError("Failed to read local scores: " + e);
      return [];
    }
  }
  
  /**
   * Get scores from network json-service
   */
  private getScoresNetwork(key: string): IHighScoreEntry[] {
    if (!this.client) return [];
    try {
      var scores = this.client.read(this.serviceName, key, 1);  // 1=LOCK_READ like gooble
      return (scores && Array.isArray(scores)) ? scores : [];
    } catch (e) {
      logError("Failed to read network scores: " + e);
      return [];
    }
  }

  /**
   * Get high score list for a specific record type and identifier
   */
  getScores(type: HighScoreType, identifier: string): IHighScoreEntry[] {
    var key = this.getKey(type, identifier);
    var scores = this.useNetwork ? this.getScoresNetwork(key) : this.getScoresLocal(key);
    
    // Sort by time ascending (fastest first)
    scores.sort(function(a: IHighScoreEntry, b: IHighScoreEntry) {
      return a.time - b.time;
    });
    
    return scores;
  }

  /**
   * Get the #1 high score (fastest time) for a specific record
   */
  getTopScore(type: HighScoreType, identifier: string): IHighScoreEntry | null {
    var scores = this.getScores(type, identifier);
    if (scores.length > 0) {
      return scores[0];
    }
    return null;
  }

  /**
   * Check if a time qualifies for the high score list
   * Returns the position (1-10) if it qualifies, or 0 if it doesn't
   */
  checkQualification(type: HighScoreType, identifier: string, time: number): number {
    var scores = this.getScores(type, identifier);
    
    // If we have fewer than max entries, it always qualifies
    if (scores.length < this.maxEntries) {
      // Find position where it should be inserted
      for (var i = 0; i < scores.length; i++) {
        if (time < scores[i].time) {
          return i + 1;
        }
      }
      return scores.length + 1;
    }
    
    // Check if time beats any existing score
    for (var i = 0; i < scores.length; i++) {
      if (time < scores[i].time) {
        return i + 1;
      }
    }
    
    return 0;  // Doesn't qualify
  }

  /**
   * Submit a new high score
   * Returns the position (1-10) if it made the list, or 0 if it didn't
   */
  submitScore(
    type: HighScoreType,
    identifier: string,
    playerName: string,
    time: number,
    trackName?: string,
    circuitName?: string
  ): number {
    var key = this.getKey(type, identifier);
    var scores = this.getScores(type, identifier);
    
    // Create new entry
    var entry: IHighScoreEntry = {
      playerName: playerName,
      time: time,
      date: Date.now()
    };
    
    if (trackName) entry.trackName = trackName;
    if (circuitName) entry.circuitName = circuitName;
    
    // Add entry to scores
    scores.push(entry);
    
    // Sort by time ascending (fastest first)
    scores.sort(function(a: IHighScoreEntry, b: IHighScoreEntry) {
      return a.time - b.time;
    });
    
    // Trim to max entries
    if (scores.length > this.maxEntries) {
      scores = scores.slice(0, this.maxEntries);
    }
    
    // Find the position of the new entry (1-based)
    var position = 0;
    for (var i = 0; i < scores.length; i++) {
      if (scores[i].time === time && scores[i].date === entry.date) {
        position = i + 1;
        break;
      }
    }
    
    // If not in the list after trimming, it didn't qualify
    if (position === 0) {
      return 0;
    }
    
    // Save scores
    try {
      if (this.useNetwork) {
        this.client.write(this.serviceName, key, scores, 2);  // 2 = LOCK_WRITE
      } else if (this.localDb) {
        var data = this.localDb.masterData.data || {};
        data[key] = scores;
        this.localDb.masterData.data = data;
        this.localDb.save();
      }
    } catch (e) {
      logError("Failed to save high score: " + e);
      return 0;
    }
    
    return position;
  }

  /**
   * Clear all high scores (for testing/admin)
   */
  clearAll(): void {
    try {
      if (this.useNetwork) {
        // For network, we'd need to clear each key individually or have server support
        logWarning("Cannot clear all scores on network storage");
      } else if (this.localDb) {
        this.localDb.masterData.data = {};
        this.localDb.save();
      }
    } catch (e) {
      logError("Failed to clear high scores: " + e);
    }
  }

  /**
   * Clear high scores for a specific record type and identifier
   */
  clear(type: HighScoreType, identifier: string): void {
    var key = this.getKey(type, identifier);
    
    try {
      if (this.useNetwork) {
        this.client.write(this.serviceName, key, [], 2);  // Write empty array
      } else if (this.localDb) {
        this.localDb.load();
        var data = this.localDb.masterData.data || {};
        delete data[key];
        this.localDb.masterData.data = data;
        this.localDb.save();
      }
    } catch (e) {
      logError("Failed to clear high scores: " + e);
    }
  }
}
