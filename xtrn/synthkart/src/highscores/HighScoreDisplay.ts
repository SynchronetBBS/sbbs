/**
 * HighScoreDisplay - Renders high score lists to the screen
 */

/**
 * Display high scores in a formatted box.
 * Uses fixed 80x24 viewport for consistent layout.
 * @param scores - Array of high score entries
 * @param title - Title of the display
 * @param trackOrCircuitName - Track or circuit name
 * @param playerPosition - Optional: 1-based position of player's newly set score to highlight
 */
function displayHighScores(
  scores: IHighScoreEntry[],
  title: string,
  trackOrCircuitName: string,
  playerPosition?: number
): void {
  console.clear(LIGHTGRAY, false);
  
  // Fixed 80x24 viewport
  var screenWidth = 80;
  var screenHeight = 24;
  
  // Colors
  var titleAttr = colorToAttr({ fg: YELLOW, bg: BG_BLACK });
  var headerAttr = colorToAttr({ fg: LIGHTCYAN, bg: BG_BLACK });
  var nameAttr = colorToAttr({ fg: WHITE, bg: BG_BLACK });
  var timeAttr = colorToAttr({ fg: LIGHTGREEN, bg: BG_BLACK });
  var dateAttr = colorToAttr({ fg: DARKGRAY, bg: BG_BLACK });
  var emptyAttr = colorToAttr({ fg: DARKGRAY, bg: BG_BLACK });
  var boxAttr = colorToAttr({ fg: LIGHTCYAN, bg: BG_BLACK });
  var highlightNameAttr = colorToAttr({ fg: LIGHTCYAN, bg: BG_BLUE });
  var highlightTimeAttr = colorToAttr({ fg: WHITE, bg: BG_BLUE });
  var highlightDateAttr = colorToAttr({ fg: LIGHTGRAY, bg: BG_BLUE });
  
  // Box dimensions
  var boxWidth = Math.min(70, screenWidth - 4);
  var boxHeight = 18;
  var boxX = Math.floor((screenWidth - boxWidth) / 2);
  var topY = Math.floor((screenHeight - boxHeight) / 2);
  
  // Draw box border
  console.gotoxy(boxX, topY);
  console.attributes = boxAttr;
  console.print(GLYPH.DBOX_TL);
  for (var i = 1; i < boxWidth - 1; i++) {
    console.print(GLYPH.DBOX_H);
  }
  console.print(GLYPH.DBOX_TR + "\r\n");
  
  for (var j = 1; j < boxHeight - 1; j++) {
    console.gotoxy(boxX, topY + j);
    console.print(GLYPH.DBOX_V);
    console.gotoxy(boxX + boxWidth - 1, topY + j);
    console.print(GLYPH.DBOX_V + "\r\n");
  }
  
  console.gotoxy(boxX, topY + boxHeight - 1);
  console.print(GLYPH.DBOX_BL);
  for (var i = 1; i < boxWidth - 1; i++) {
    console.print(GLYPH.DBOX_H);
  }
  console.print(GLYPH.DBOX_BR + "\r\n");
  
  // Title
  console.gotoxy(boxX + Math.floor((boxWidth - title.length) / 2), topY + 2);
  console.attributes = titleAttr;
  console.print(title);
  
  // Track/Circuit name
  console.gotoxy(boxX + Math.floor((boxWidth - trackOrCircuitName.length) / 2), topY + 3);
  console.attributes = headerAttr;
  console.print(trackOrCircuitName);
  
  // Header
  console.gotoxy(boxX + 3, topY + 5);
  console.attributes = headerAttr;
  console.print("RANK  PLAYER NAME           TIME        DATE");
  
  // Scores
  var startY = topY + 6;
  for (var i = 0; i < 10; i++) {
    console.gotoxy(boxX + 3, startY + i);
    
    // Check if this is the player's highlighted position
    var isHighlighted = (playerPosition !== undefined && playerPosition === i + 1);
    
    if (i < scores.length) {
      var score = scores[i];
      var rank = (i + 1) + ".";
      if (i < 9) rank = " " + rank;  // Pad single digits
      
      // Rank
      console.attributes = isHighlighted ? highlightNameAttr : nameAttr;
      console.print(rank + "   ");
      
      // Player name (truncate if needed)
      var name = score.playerName;
      if (name.length > 18) {
        name = name.substring(0, 15) + "...";
      }
      while (name.length < 18) {
        name += " ";
      }
      console.print(name + "  ");
      
      // Time
      console.attributes = isHighlighted ? highlightTimeAttr : timeAttr;
      var timeStr = LapTimer.format(score.time);
      console.print(timeStr);
      while (timeStr.length < 10) {
        timeStr += " ";
        console.print(" ");
      }
      console.print("  ");
      
      // Date
      console.attributes = isHighlighted ? highlightDateAttr : dateAttr;
      var date = new Date(score.date);
      var dateStr = (date.getMonth() + 1) + "/" + date.getDate() + "/" + date.getFullYear();
      console.print(dateStr);
      
      // Add indicator for player's new score
      if (isHighlighted) {
        console.attributes = colorToAttr({ fg: YELLOW, bg: BG_BLACK });
        console.print(" <-- YOU!");
      }
    } else {
      // Empty slot
      console.attributes = emptyAttr;
      var rank = (i + 1) + ".";
      if (i < 9) rank = " " + rank;
      console.print(rank + "   ---");
    }
  }
  
  // Footer
  console.gotoxy(boxX + Math.floor((boxWidth - 24) / 2), topY + boxHeight - 2);
  console.attributes = headerAttr;
  console.print("Press any key to continue");
}

/**
 * Display high scores and wait for key press.
 * @param type - Type of high score (track time, lap time, circuit time)
 * @param identifier - Track or circuit identifier
 * @param title - Display title
 * @param trackOrCircuitName - Display name for track or circuit
 * @param highScoreManager - HighScoreManager instance
 * @param playerPosition - Optional: 1-based position of player's newly set score to highlight
 */
function showHighScoreList(
  type: HighScoreType,
  identifier: string,
  title: string,
  trackOrCircuitName: string,
  highScoreManager: HighScoreManager,
  playerPosition?: number
): void {
  var scores = highScoreManager.getScores(type, identifier);
  displayHighScores(scores, title, trackOrCircuitName, playerPosition);
  // Reset line counter to prevent auto-pause, then wait for key with long timeout
  console.line_counter = 0;
  console.inkey(K_NONE, 300000);  // Wait up to 5 minutes
}

/**
 * Display a compact single-line high score (for embedding in other screens)
 */
function displayTopScoreLine(
  label: string,
  score: IHighScoreEntry | null,
  x: number,
  y: number,
  labelAttr: number,
  valueAttr: number
): void {
  console.gotoxy(x, y);
  console.attributes = labelAttr;
  console.print(label + ": ");
  
  console.attributes = valueAttr;
  if (score) {
    console.print(LapTimer.format(score.time) + " by " + score.playerName);
  } else {
    console.print("--:--.-- (No record)");
  }
}

/**
 * Display track high scores in a two-column layout (Track Time | Lap Time).
 * Uses fixed 80x24 viewport for consistent layout.
 * @param trackId - Track identifier
 * @param trackName - Track display name
 * @param highScoreManager - HighScoreManager instance
 * @param trackTimePosition - Player's position in track time list (0 = not on list)
 * @param lapTimePosition - Player's position in lap time list (0 = not on list)
 */
function showTwoColumnHighScores(
  trackId: string,
  trackName: string,
  highScoreManager: HighScoreManager,
  trackTimePosition: number,
  lapTimePosition: number
): void {
  console.clear(LIGHTGRAY, false);
  
  // Fixed 80x24 viewport
  var viewWidth = 80;
  var viewHeight = 24;
  
  // Get scores
  var trackScores = highScoreManager.getScores(HighScoreType.TRACK_TIME, trackId);
  var lapScores = highScoreManager.getScores(HighScoreType.LAP_TIME, trackId);
  
  // Colors
  var titleAttr = colorToAttr({ fg: YELLOW, bg: BG_BLACK });
  var headerAttr = colorToAttr({ fg: LIGHTCYAN, bg: BG_BLACK });
  var colHeaderAttr = colorToAttr({ fg: WHITE, bg: BG_BLACK });
  var rankAttr = colorToAttr({ fg: WHITE, bg: BG_BLACK });
  var nameAttr = colorToAttr({ fg: LIGHTGRAY, bg: BG_BLACK });
  var timeAttr = colorToAttr({ fg: LIGHTGREEN, bg: BG_BLACK });
  var emptyAttr = colorToAttr({ fg: DARKGRAY, bg: BG_BLACK });
  var boxAttr = colorToAttr({ fg: LIGHTCYAN, bg: BG_BLACK });
  var highlightAttr = colorToAttr({ fg: LIGHTCYAN, bg: BG_BLUE });
  var promptAttr = colorToAttr({ fg: LIGHTMAGENTA, bg: BG_BLACK });
  var newScoreAttr = colorToAttr({ fg: YELLOW, bg: BG_BLACK });
  
  // Box dimensions - full width for two columns
  var boxWidth = 76;
  var boxHeight = 20;
  var boxX = Math.floor((viewWidth - boxWidth) / 2);
  var topY = Math.floor((viewHeight - boxHeight) / 2);
  
  // Draw box border
  console.gotoxy(boxX, topY);
  console.attributes = boxAttr;
  console.print(GLYPH.DBOX_TL);
  for (var i = 1; i < boxWidth - 1; i++) {
    console.print(GLYPH.DBOX_H);
  }
  console.print(GLYPH.DBOX_TR);
  
  for (var j = 1; j < boxHeight - 1; j++) {
    console.gotoxy(boxX, topY + j);
    console.print(GLYPH.DBOX_V);
    console.gotoxy(boxX + boxWidth - 1, topY + j);
    console.print(GLYPH.DBOX_V);
  }
  
  console.gotoxy(boxX, topY + boxHeight - 1);
  console.print(GLYPH.DBOX_BL);
  for (var i = 1; i < boxWidth - 1; i++) {
    console.print(GLYPH.DBOX_H);
  }
  console.print(GLYPH.DBOX_BR);
  
  // Title
  var title = "=== HIGH SCORES ===";
  console.gotoxy(boxX + Math.floor((boxWidth - title.length) / 2), topY + 1);
  console.attributes = titleAttr;
  console.print(title);
  
  // Track name
  console.gotoxy(boxX + Math.floor((boxWidth - trackName.length) / 2), topY + 2);
  console.attributes = headerAttr;
  console.print(trackName);
  
  // Column headers
  var leftColX = boxX + 3;
  var rightColX = boxX + 40;
  
  console.gotoxy(leftColX, topY + 4);
  console.attributes = colHeaderAttr;
  console.print("TRACK TIME");
  
  console.gotoxy(rightColX, topY + 4);
  console.print("BEST LAP");
  
  // Divider line
  console.gotoxy(boxX + 37, topY + 4);
  console.attributes = boxAttr;
  console.print(GLYPH.BOX_V);
  for (var j = 5; j < boxHeight - 3; j++) {
    console.gotoxy(boxX + 37, topY + j);
    console.print(GLYPH.BOX_V);
  }
  
  // Draw scores - 10 rows, two columns
  var startY = topY + 5;
  for (var i = 0; i < 10; i++) {
    // Left column - Track Time
    console.gotoxy(leftColX, startY + i);
    var trackHighlighted = (trackTimePosition > 0 && trackTimePosition === i + 1);
    
    if (i < trackScores.length) {
      var score = trackScores[i];
      var rank = (i + 1) + ".";
      if (i < 9) rank = " " + rank;
      
      console.attributes = trackHighlighted ? highlightAttr : rankAttr;
      console.print(rank + " ");
      
      // Name (truncated to 11 chars)
      var name = score.playerName;
      if (name.length > 11) name = name.substring(0, 10) + ".";
      while (name.length < 11) name += " ";
      console.attributes = trackHighlighted ? highlightAttr : nameAttr;
      console.print(name + " ");
      
      // Time
      console.attributes = trackHighlighted ? highlightAttr : timeAttr;
      console.print(LapTimer.format(score.time));
      
      // Date (M/D/YYYY)
      var d = new Date(score.date);
      var dateStr = " " + (d.getMonth() + 1) + "/" + d.getDate() + "/" + d.getFullYear();
      console.attributes = trackHighlighted ? highlightAttr : emptyAttr;
      console.print(dateStr);
      
      if (trackHighlighted) {
        console.attributes = newScoreAttr;
        console.print("*");
      }
    } else {
      console.attributes = emptyAttr;
      var rank = (i + 1) + ".";
      if (i < 9) rank = " " + rank;
      console.print(rank + " ---");
    }
    
    // Right column - Lap Time
    console.gotoxy(rightColX, startY + i);
    var lapHighlighted = (lapTimePosition > 0 && lapTimePosition === i + 1);
    
    if (i < lapScores.length) {
      var score = lapScores[i];
      var rank = (i + 1) + ".";
      if (i < 9) rank = " " + rank;
      
      console.attributes = lapHighlighted ? highlightAttr : rankAttr;
      console.print(rank + " ");
      
      // Name (truncated to 11 chars)
      var name = score.playerName;
      if (name.length > 11) name = name.substring(0, 10) + ".";
      while (name.length < 11) name += " ";
      console.attributes = lapHighlighted ? highlightAttr : nameAttr;
      console.print(name + " ");
      
      // Time
      console.attributes = lapHighlighted ? highlightAttr : timeAttr;
      console.print(LapTimer.format(score.time));
      
      // Date (M/D/YYYY)
      var d = new Date(score.date);
      var dateStr = " " + (d.getMonth() + 1) + "/" + d.getDate() + "/" + d.getFullYear();
      console.attributes = lapHighlighted ? highlightAttr : emptyAttr;
      console.print(dateStr);
      
      if (lapHighlighted) {
        console.attributes = newScoreAttr;
        console.print(" *");
      }
    } else {
      console.attributes = emptyAttr;
      var rank = (i + 1) + ".";
      if (i < 9) rank = " " + rank;
      console.print(rank + " ---");
    }
  }
  
  // Legend for new scores
  if (trackTimePosition > 0 || lapTimePosition > 0) {
    console.gotoxy(boxX + 3, topY + boxHeight - 3);
    console.attributes = newScoreAttr;
    console.print("* = Your new high score!");
  }
  
  // Footer prompt
  var prompt = "Press any key to continue";
  console.gotoxy(boxX + Math.floor((boxWidth - prompt.length) / 2), topY + boxHeight - 2);
  console.attributes = promptAttr;
  console.print(prompt);
  
  // Reset line counter to prevent auto-pause, then wait for key with long timeout
  console.line_counter = 0;
  console.inkey(K_NONE, 300000);  // Wait up to 5 minutes
}
