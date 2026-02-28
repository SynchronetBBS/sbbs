/**
 * ANSILoader - Loads ANSI art files into a character/attribute grid.
 * 
 * Uses Synchronet's Graphic class with auto_extend for efficient memory:
 * - Start with minimal height (1 row)
 * - Enable auto_extend so grid grows during ANSI parsing
 * - Result is exact-sized grid matching actual ANSI content
 * 
 * Output grid is cells[row][col] for easy frame injection via setData(x, y, char, attr).
 */

interface ANSICell {
  char: string;
  attr: number;  // Combined fg/bg attribute (same format as Frame.setData expects)
}

interface ANSIImage {
  width: number;
  height: number;
  cells: ANSICell[][];  // [row][col] - rows first for easy iteration
}

/**
 * Load ANSI files using Synchronet's Graphic class.
 * Parses ANSI escape codes into a proper grid structure for frame injection.
 */
class ANSILoader {
  // Default directory for ANSI files
  static defaultDirectory: string = '/sbbs/xtrn/outrun/ansi_art';
  
  // Track if graphic.js has been loaded
  private static _graphicLoaded: boolean = false;
  
  /**
   * Ensure graphic.js is loaded.
   */
  private static ensureGraphicLoaded(): void {
    if (!ANSILoader._graphicLoaded) {
      try {
        load('graphic.js');
        ANSILoader._graphicLoaded = true;
        logInfo("ANSILoader: graphic.js loaded successfully");
      } catch (e) {
        logWarning("ANSILoader: Failed to load graphic.js - " + e);
      }
    }
  }
  
  /**
   * Load and parse an ANSI file into a grid structure.
   * Uses auto_extend for efficient memory - grid grows to exact content size.
   * 
   * @param pathOrFilename - Full path or filename to load
   * @param optDirectory - Optional directory (only used if pathOrFilename is just a filename)
   * @returns ANSIImage with exact-sized cells[row][col] grid, or null on failure
   */
  static load(pathOrFilename: string, optDirectory?: string): ANSIImage | null {
    // Ensure graphic.js is loaded
    ANSILoader.ensureGraphicLoaded();
    
    // If it looks like a full path, use it directly; otherwise combine with directory
    var path: string;
    if (pathOrFilename.indexOf('/') >= 0 || pathOrFilename.indexOf('\\') >= 0) {
      path = pathOrFilename;
    } else {
      var dir = optDirectory || ANSILoader.defaultDirectory;
      path = dir + '/' + pathOrFilename;
    }
    
    try {
      // Check file exists
      if (typeof file_exists === 'function' && !file_exists(path)) {
        logWarning("ANSI file not found: " + path);
        return null;
      }
      
      // Create a minimal Graphic object with auto_extend enabled.
      // Width = 80 (standard ANSI width), height = 1 (will grow as needed)
      // @ts-ignore - Graphic is loaded at runtime
      var graphic = new Graphic(80, 1);
      
      // Enable auto_extend - grid will grow during ANSI parsing
      graphic.auto_extend = true;
      
      // Load the ANSI file - this parses ANSI escape codes into data[x][y] grid
      // With auto_extend=true, it grows height as newlines are encountered
      if (!graphic.load(path)) {
        logWarning("Failed to load ANSI file via Graphic: " + path);
        return null;
      }
      
      // Get actual dimensions after parsing (height grew to fit content)
      var width = graphic.width;
      var height = graphic.height;
      
      // Convert Graphic's data[x][y] format to our cells[row][col] format
      // This allows easy frame injection: for each row, for each col, setData(col, row, char, attr)
      var cells: ANSICell[][] = [];
      
      for (var row = 0; row < height; row++) {
        cells[row] = [];
        for (var col = 0; col < width; col++) {
          var cell = graphic.data[col][row];  // Graphic uses data[x][y]
          cells[row][col] = {
            char: cell.ch || ' ',
            attr: cell.attr || 7
          };
        }
      }
      
      logInfo("ANSILoader: Loaded " + path + " (" + width + "x" + height + " actual size)");
      
      return {
        width: width,
        height: height,
        cells: cells
      };
    } catch (e) {
      logWarning("Error loading ANSI file: " + path + " - " + e);
      return null;
    }
  }
  
  /**
   * Scan a directory for ANSI files.
   * @param dirPath - Directory to scan
   * @returns Array of full file paths
   */
  static scanDirectory(dirPath?: string): string[] {
    var dir = dirPath || ANSILoader.defaultDirectory;
    var files: string[] = [];
    
    try {
      // Synchronet uses the global directory() function with a glob pattern
      // It returns an array of full paths matching the pattern
      if (typeof directory === 'function') {
        var glob = dir + '/*.ans';
        var allFiles = directory(glob);
        if (allFiles && allFiles.length) {
          for (var i = 0; i < allFiles.length; i++) {
            files.push(allFiles[i]);  // Full paths returned by directory()
          }
        }
      }
      
      logInfo("ANSILoader.scanDirectory: Found " + files.length + " files in " + dir);
    } catch (e) {
      logWarning("Error scanning ANSI directory: " + dir + " - " + e);
    }
    
    return files;
  }
  
}

// Type declarations for Synchronet functions
declare function directory(glob: string): string[];
declare function file_exists(path: string): boolean;

// Graphic class declaration (loaded at runtime via load('graphic.js'))
declare class Graphic {
  constructor(width: number, height: number, attr?: number, ch?: string);
  width: number;
  height: number;
  auto_extend: boolean;  // When true, grid grows during ANSI parsing
  data: { ch: string; attr: number }[][];  // data[x][y]
  load(filename: string, offset?: number): boolean;
}
