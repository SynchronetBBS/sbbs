namespace ZZT {
  var COLOR_NAMES: string[] = ["", "Blue", "Green", "Cyan", "Red", "Purple", "Yellow", "White"];
  var DIAGONAL_DELTA_X: number[] = [-1, 0, 1, 1, 1, 0, -1, -1];
  var DIAGONAL_DELTA_Y: number[] = [1, 1, 1, 0, -1, -1, -1, 0];
  var NEIGHBOR_DELTA_X: number[] = [0, 0, -1, 1];
  var NEIGHBOR_DELTA_Y: number[] = [-1, 1, 0, 0];
  var TRANSPORTER_NS_CHARS: string = "^~^-v_v-";
  var TRANSPORTER_EW_CHARS: string =
    "(<(" + String.fromCharCode(179) + ")>)" + String.fromCharCode(179);

  function randomInt(maxExclusive: number): number {
    if (maxExclusive <= 0) {
      return 0;
    }
    return Math.floor(Math.random() * maxExclusive);
  }

  function clamp(value: number, min: number, max: number): number {
    if (value < min) {
      return min;
    }
    if (value > max) {
      return max;
    }
    return value;
  }

  function elementDefaultTick(_statId: number): void {
  }

  function elementDefaultTouch(_x: number, _y: number, _sourceStatId: number, _context: TouchContext): void {
  }

  function elementDefaultDraw(_x: number, _y: number): number {
    return "?".charCodeAt(0);
  }

  function elementMessageTimerTick(statId: number): void {
    var stat: Stat = Board.Stats[statId];
    if (stat.X !== 0) {
      return;
    }

    if (stat.P2 > 0) {
      if (Board.Info.Message.length > 0) {
        VideoWriteText(
          Math.floor((60 - Board.Info.Message.length) / 2),
          24,
          0x09 + (stat.P2 % 7),
          " " + Board.Info.Message + " "
        );
      }
      stat.P2 -= 1;
    }

    if (stat.P2 <= 0) {
      RemoveStat(statId);
      CurrentStatTicked -= 1;
      BoardDrawBorder();
      Board.Info.Message = "";
    }
  }

  function elementDamagingTouch(x: number, y: number, sourceStatId: number, _context: TouchContext): void {
    BoardAttack(sourceStatId, x, y);
  }

  function elementLionTick(statId: number): void {
    var stat: Stat = Board.Stats[statId];
    var dir: Direction;
    var tx: number;
    var ty: number;
    var destElement: number;

    if (stat.P1 < randomInt(10)) {
      dir = CalcDirectionRnd();
    } else {
      dir = CalcDirectionSeek(stat.X, stat.Y);
    }

    tx = stat.X + dir.DeltaX;
    ty = stat.Y + dir.DeltaY;
    destElement = Board.Tiles[tx][ty].Element;

    if (ElementDefs[destElement].Walkable) {
      MoveStat(statId, tx, ty);
    } else if (destElement === E_PLAYER) {
      BoardAttack(statId, tx, ty);
    }
  }

  function elementTigerTick(statId: number): void {
    var stat: Stat = Board.Stats[statId];
    var shot: boolean = false;
    var bulletElement: number = stat.P2 >= 0x80 ? E_STAR : E_BULLET;

    if ((randomInt(10) * 3) <= (stat.P2 % 0x80)) {
      if (Difference(stat.X, Board.Stats[0].X) <= 2) {
        shot = BoardShoot(bulletElement, stat.X, stat.Y, 0, Signum(Board.Stats[0].Y - stat.Y), SHOT_SOURCE_ENEMY);
      }

      if (!shot && Difference(stat.Y, Board.Stats[0].Y) <= 2) {
        BoardShoot(bulletElement, stat.X, stat.Y, Signum(Board.Stats[0].X - stat.X), 0, SHOT_SOURCE_ENEMY);
      }
    }

    elementLionTick(statId);
  }

  function elementRuffianTick(statId: number): void {
    var stat: Stat = Board.Stats[statId];
    var dir: Direction;
    var tx: number;
    var ty: number;
    var destElement: number;

    if (stat.StepX === 0 && stat.StepY === 0) {
      if ((stat.P2 + 8) <= randomInt(17)) {
        if (stat.P1 >= randomInt(9)) {
          dir = CalcDirectionSeek(stat.X, stat.Y);
        } else {
          dir = CalcDirectionRnd();
        }
        stat.StepX = dir.DeltaX;
        stat.StepY = dir.DeltaY;
      }
      return;
    }

    if ((stat.Y === Board.Stats[0].Y || stat.X === Board.Stats[0].X) && randomInt(9) <= stat.P1) {
      dir = CalcDirectionSeek(stat.X, stat.Y);
      stat.StepX = dir.DeltaX;
      stat.StepY = dir.DeltaY;
    }

    tx = stat.X + stat.StepX;
    ty = stat.Y + stat.StepY;
    destElement = Board.Tiles[tx][ty].Element;

    if (destElement === E_PLAYER) {
      BoardAttack(statId, tx, ty);
    } else if (ElementDefs[destElement].Walkable) {
      MoveStat(statId, tx, ty);
      if ((stat.P2 + 8) <= randomInt(17)) {
        stat.StepX = 0;
        stat.StepY = 0;
      }
    } else {
      stat.StepX = 0;
      stat.StepY = 0;
    }
  }

  function elementBearTick(statId: number): void {
    var stat: Stat = Board.Stats[statId];
    var deltaX: number = 0;
    var deltaY: number = 0;
    var tx: number;
    var ty: number;
    var destElement: number;

    if (stat.X !== Board.Stats[0].X && Difference(stat.Y, Board.Stats[0].Y) <= (8 - stat.P1)) {
      deltaX = Signum(Board.Stats[0].X - stat.X);
    } else if (Difference(stat.X, Board.Stats[0].X) <= (8 - stat.P1)) {
      deltaY = Signum(Board.Stats[0].Y - stat.Y);
    }

    tx = stat.X + deltaX;
    ty = stat.Y + deltaY;
    destElement = Board.Tiles[tx][ty].Element;

    if (ElementDefs[destElement].Walkable) {
      MoveStat(statId, tx, ty);
    } else if (destElement === E_PLAYER || destElement === E_BREAKABLE) {
      BoardAttack(statId, tx, ty);
    }
  }

  function elementSharkTick(statId: number): void {
    var stat: Stat = Board.Stats[statId];
    var dir: Direction;
    var tx: number;
    var ty: number;
    var destElement: number;

    if (stat.P1 < randomInt(10)) {
      dir = CalcDirectionRnd();
    } else {
      dir = CalcDirectionSeek(stat.X, stat.Y);
    }

    tx = stat.X + dir.DeltaX;
    ty = stat.Y + dir.DeltaY;
    destElement = Board.Tiles[tx][ty].Element;

    if (destElement === E_WATER) {
      MoveStat(statId, tx, ty);
    } else if (destElement === E_PLAYER) {
      BoardAttack(statId, tx, ty);
    }
  }

  function elementBulletTick(statId: number): void {
    var stat: Stat = Board.Stats[statId];
    var tx: number = stat.X + stat.StepX;
    var ty: number = stat.Y + stat.StepY;
    var element: number = Board.Tiles[tx][ty].Element;
    var targetStatId: number = -1;

    if (ElementDefs[element].Walkable || element === E_WATER) {
      MoveStat(statId, tx, ty);
      return;
    }

    if (element === E_BREAKABLE || (ElementDefs[element].Destructible && (element === E_PLAYER || stat.P1 === SHOT_SOURCE_PLAYER))) {
      BoardAttack(statId, tx, ty);
      return;
    }

    if (element === E_OBJECT || element === E_SCROLL) {
      targetStatId = GetStatIdAt(tx, ty);
    }

    RemoveStat(statId);
    if (targetStatId > 0) {
      OopSend(-targetStatId, "SHOT", false);
    }
  }

  function elementSpinningGunDraw(_x: number, _y: number): number {
    switch (CurrentTick % 8) {
      case 0:
      case 1:
        return 24;
      case 2:
      case 3:
        return 26;
      case 4:
      case 5:
        return 25;
      default:
        return 27;
    }
  }

  function elementSpinningGunTick(statId: number): void {
    var stat: Stat = Board.Stats[statId];
    var bulletElement: number = stat.P2 >= 0x80 ? E_STAR : E_BULLET;
    var dir: Direction;
    var shot: boolean = false;

    if (randomInt(9) >= (stat.P2 % 0x80)) {
      return;
    }

    if (randomInt(9) <= stat.P1) {
      if (Difference(stat.X, Board.Stats[0].X) <= 2) {
        shot = BoardShoot(bulletElement, stat.X, stat.Y, 0, Signum(Board.Stats[0].Y - stat.Y), SHOT_SOURCE_ENEMY);
      }
      if (!shot && Difference(stat.Y, Board.Stats[0].Y) <= 2) {
        BoardShoot(bulletElement, stat.X, stat.Y, Signum(Board.Stats[0].X - stat.X), 0, SHOT_SOURCE_ENEMY);
      }
    } else {
      dir = CalcDirectionRnd();
      BoardShoot(bulletElement, stat.X, stat.Y, dir.DeltaX, dir.DeltaY, SHOT_SOURCE_ENEMY);
    }
  }

  function elementLineDraw(x: number, y: number): number {
    var lineChars: string =
      String.fromCharCode(249) +
      String.fromCharCode(208) +
      String.fromCharCode(210) +
      String.fromCharCode(186) +
      String.fromCharCode(181) +
      String.fromCharCode(188) +
      String.fromCharCode(187) +
      String.fromCharCode(185) +
      String.fromCharCode(198) +
      String.fromCharCode(200) +
      String.fromCharCode(201) +
      String.fromCharCode(204) +
      String.fromCharCode(205) +
      String.fromCharCode(202) +
      String.fromCharCode(203) +
      String.fromCharCode(206);
    var i: number;
    var v: number = 1;
    var shift: number = 1;
    var neighbor: number;

    for (i = 0; i < 4; i += 1) {
      neighbor = Board.Tiles[x + NEIGHBOR_DELTA_X[i]][y + NEIGHBOR_DELTA_Y[i]].Element;
      if (neighbor === E_LINE || neighbor === E_BOARD_EDGE) {
        v += shift;
      }
      shift <<= 1;
    }

    return lineChars.charCodeAt(clamp(v - 1, 0, lineChars.length - 1));
  }

  function elementConveyorTickGeneric(x: number, y: number, direction: number): void {
    var i: number;
    var iStat: number;
    var ix: number;
    var iy: number;
    var canMove: boolean;
    var tiles: Tile[] = [];
    var iMin: number;
    var iMax: number;
    var tmpTile: Tile;
    var elem: number;

    if (direction === 1) {
      iMin = 0;
      iMax = 8;
    } else {
      iMin = 7;
      iMax = -1;
    }

    canMove = true;
    i = iMin;
    while (i !== iMax) {
      tiles[i] = cloneTile(Board.Tiles[x + DIAGONAL_DELTA_X[i]][y + DIAGONAL_DELTA_Y[i]]);
      elem = tiles[i].Element;
      if (elem === E_EMPTY) {
        canMove = true;
      } else if (!ElementDefs[elem].Pushable) {
        canMove = false;
      }
      i += direction;
    }

    i = iMin;
    while (i !== iMax) {
      elem = tiles[i].Element;
      if (canMove) {
        if (ElementDefs[elem].Pushable) {
          ix = x + DIAGONAL_DELTA_X[(i - direction + 8) % 8];
          iy = y + DIAGONAL_DELTA_Y[(i - direction + 8) % 8];

          if (ElementDefs[elem].Cycle > -1) {
            tmpTile = cloneTile(Board.Tiles[x + DIAGONAL_DELTA_X[i]][y + DIAGONAL_DELTA_Y[i]]);
            iStat = GetStatIdAt(x + DIAGONAL_DELTA_X[i], y + DIAGONAL_DELTA_Y[i]);
            Board.Tiles[x + DIAGONAL_DELTA_X[i]][y + DIAGONAL_DELTA_Y[i]] = cloneTile(tiles[i]);
            Board.Tiles[ix][iy].Element = E_EMPTY;
            if (iStat >= 0) {
              MoveStat(iStat, ix, iy);
            }
            Board.Tiles[x + DIAGONAL_DELTA_X[i]][y + DIAGONAL_DELTA_Y[i]] = tmpTile;
          } else {
            Board.Tiles[ix][iy] = cloneTile(tiles[i]);
            BoardDrawTile(ix, iy);
          }

          if (!ElementDefs[tiles[(i + direction + 8) % 8].Element].Pushable) {
            Board.Tiles[x + DIAGONAL_DELTA_X[i]][y + DIAGONAL_DELTA_Y[i]].Element = E_EMPTY;
            BoardDrawTile(x + DIAGONAL_DELTA_X[i], y + DIAGONAL_DELTA_Y[i]);
          }
        } else {
          canMove = false;
        }
      } else if (elem === E_EMPTY) {
        canMove = true;
      } else if (!ElementDefs[elem].Pushable) {
        canMove = false;
      }
      i += direction;
    }
  }

  function elementConveyorCWDraw(_x: number, _y: number): number {
    switch (Math.floor(CurrentTick / ElementDefs[E_CONVEYOR_CW].Cycle) % 4) {
      case 0:
        return 179;
      case 1:
        return 47;
      case 2:
        return 196;
      default:
        return 92;
    }
  }

  function elementConveyorCWTick(statId: number): void {
    var stat: Stat = Board.Stats[statId];
    BoardDrawTile(stat.X, stat.Y);
    elementConveyorTickGeneric(stat.X, stat.Y, 1);
  }

  function elementConveyorCCWDraw(_x: number, _y: number): number {
    switch (Math.floor(CurrentTick / ElementDefs[E_CONVEYOR_CCW].Cycle) % 4) {
      case 3:
        return 179;
      case 2:
        return 47;
      case 1:
        return 196;
      default:
        return 92;
    }
  }

  function elementConveyorCCWTick(statId: number): void {
    var stat: Stat = Board.Stats[statId];
    BoardDrawTile(stat.X, stat.Y);
    elementConveyorTickGeneric(stat.X, stat.Y, -1);
  }

  function elementTransporterMove(x: number, y: number, deltaX: number, deltaY: number): void {
    var statId: number = GetStatIdAt(x + deltaX, y + deltaY);
    var ix: number;
    var iy: number;
    var newX: number = -1;
    var newY: number = -1;
    var finishSearch: boolean = false;
    var isValidDest: boolean = true;
    var elem: number;
    var scanStatId: number;
    var transStat: Stat;

    if (statId < 0) {
      return;
    }
    transStat = Board.Stats[statId];
    if (deltaX !== transStat.StepX || deltaY !== transStat.StepY) {
      return;
    }

    ix = transStat.X;
    iy = transStat.Y;

    while (!finishSearch) {
      ix += deltaX;
      iy += deltaY;
      elem = Board.Tiles[ix][iy].Element;

      if (elem === E_BOARD_EDGE) {
        finishSearch = true;
      } else if (isValidDest) {
        isValidDest = false;
        if (!ElementDefs[elem].Walkable) {
          ElementPushablePush(ix, iy, deltaX, deltaY);
          elem = Board.Tiles[ix][iy].Element;
        }
        if (ElementDefs[elem].Walkable) {
          finishSearch = true;
          newX = ix;
          newY = iy;
        } else {
          newX = -1;
        }
      }

      if (elem === E_TRANSPORTER) {
        scanStatId = GetStatIdAt(ix, iy);
        if (scanStatId >= 0) {
          if (Board.Stats[scanStatId].StepX === -deltaX && Board.Stats[scanStatId].StepY === -deltaY) {
            isValidDest = true;
          }
        }
      }
    }

    if (newX !== -1) {
      ElementMove(transStat.X - deltaX, transStat.Y - deltaY, newX, newY);
      SoundQueue(3, "\x30\x01\x42\x01\x34\x01\x46\x01\x38\x01\x4A\x01\x40\x01\x52\x01");
    }
  }

  function elementTransporterTouch(x: number, y: number, _sourceStatId: number, context: TouchContext): void {
    elementTransporterMove(x - context.DeltaX, y - context.DeltaY, context.DeltaX, context.DeltaY);
    context.DeltaX = 0;
    context.DeltaY = 0;
  }

  function elementTransporterTick(statId: number): void {
    var stat: Stat = Board.Stats[statId];
    BoardDrawTile(stat.X, stat.Y);
  }

  function elementTransporterDraw(x: number, y: number): number {
    var statId: number = GetStatIdAt(x, y);
    var stat: Stat;
    var idx: number;

    if (statId < 0) {
      return 197;
    }

    stat = Board.Stats[statId];
    if (stat.StepX === 0) {
      idx = stat.StepY * 2 + 3 + (Math.floor(CurrentTick / Math.max(1, stat.Cycle)) % 4);
      return TRANSPORTER_NS_CHARS.charCodeAt(clamp(idx - 1, 0, TRANSPORTER_NS_CHARS.length - 1));
    }

    idx = stat.StepX * 2 + 3 + (Math.floor(CurrentTick / Math.max(1, stat.Cycle)) % 4);
    return TRANSPORTER_EW_CHARS.charCodeAt(clamp(idx - 1, 0, TRANSPORTER_EW_CHARS.length - 1));
  }

  function elementPusherDraw(x: number, y: number): number {
    var statId: number = GetStatIdAt(x, y);
    var stat: Stat;
    if (statId < 0) {
      return 16;
    }

    stat = Board.Stats[statId];
    if (stat.StepX === 1) {
      return 16;
    }
    if (stat.StepX === -1) {
      return 17;
    }
    if (stat.StepY === -1) {
      return 30;
    }
    return 31;
  }

  function elementPusherTick(statId: number): void {
    var stat: Stat = Board.Stats[statId];
    var startX: number = stat.X;
    var startY: number = stat.Y;
    var chainId: number;

    if (!ElementDefs[Board.Tiles[stat.X + stat.StepX][stat.Y + stat.StepY].Element].Walkable) {
      ElementPushablePush(stat.X + stat.StepX, stat.Y + stat.StepY, stat.StepX, stat.StepY);
    }

    statId = GetStatIdAt(startX, startY);
    if (statId < 0) {
      return;
    }
    stat = Board.Stats[statId];

    if (ElementDefs[Board.Tiles[stat.X + stat.StepX][stat.Y + stat.StepY].Element].Walkable) {
      MoveStat(statId, stat.X + stat.StepX, stat.Y + stat.StepY);
      SoundQueue(2, "\x15\x01");

      chainId = GetStatIdAt(stat.X - (stat.StepX * 2), stat.Y - (stat.StepY * 2));
      if (chainId >= 0 &&
          Board.Tiles[Board.Stats[chainId].X][Board.Stats[chainId].Y].Element === E_PUSHER &&
          Board.Stats[chainId].StepX === stat.StepX &&
          Board.Stats[chainId].StepY === stat.StepY) {
        elementPusherTick(chainId);
      }
    }
  }

  function elementSlimeTick(statId: number): void {
    var stat: Stat = Board.Stats[statId];
    var dir: number;
    var color: number;
    var changedTiles: number = 0;
    var startX: number;
    var startY: number;
    var nx: number;
    var ny: number;
    var newStat: Stat;

    if (stat.P1 < stat.P2) {
      stat.P1 += 1;
      return;
    }

    color = Board.Tiles[stat.X][stat.Y].Color;
    stat.P1 = 0;
    startX = stat.X;
    startY = stat.Y;

    for (dir = 0; dir < 4; dir += 1) {
      nx = startX + NEIGHBOR_DELTA_X[dir];
      ny = startY + NEIGHBOR_DELTA_Y[dir];
      if (ElementDefs[Board.Tiles[nx][ny].Element].Walkable) {
        if (changedTiles === 0) {
          MoveStat(statId, nx, ny);
          Board.Tiles[startX][startY].Color = color;
          Board.Tiles[startX][startY].Element = E_BREAKABLE;
          BoardDrawTile(startX, startY);
        } else {
          newStat = createDefaultStat();
          newStat.P2 = stat.P2;
          AddStat(nx, ny, E_SLIME, color, ElementDefs[E_SLIME].Cycle, newStat);
          Board.Stats[Board.StatCount].P2 = stat.P2;
        }
        changedTiles += 1;
      }
    }

    if (changedTiles === 0) {
      RemoveStat(statId);
      Board.Tiles[startX][startY].Element = E_BREAKABLE;
      Board.Tiles[startX][startY].Color = color;
      BoardDrawTile(startX, startY);
    }
  }

  function elementSlimeTouch(x: number, y: number, _sourceStatId: number, _context: TouchContext): void {
    var color: number = Board.Tiles[x][y].Color;
    var statId: number = GetStatIdAt(x, y);
    if (statId >= 0) {
      DamageStat(statId);
    }
    Board.Tiles[x][y].Element = E_BREAKABLE;
    Board.Tiles[x][y].Color = color;
    BoardDrawTile(x, y);
    SoundQueue(2, "\x20\x01\x23\x01");
  }

  function elementBlinkWallDraw(_x: number, _y: number): number {
    return 206;
  }

  function elementBlinkWallTick(statId: number): void {
    var stat: Stat = Board.Stats[statId];
    var ix: number;
    var iy: number;
    var rayElement: number;
    var elem: number;

    if (stat.P3 === 0) {
      stat.P3 = stat.P1 + 1;
    }

    if (stat.P3 !== 1) {
      stat.P3 -= 1;
      return;
    }

    ix = stat.X + stat.StepX;
    iy = stat.Y + stat.StepY;
    rayElement = stat.StepX !== 0 ? E_BLINK_RAY_EW : E_BLINK_RAY_NS;

    if (Board.Tiles[ix][iy].Element === rayElement && Board.Tiles[ix][iy].Color === Board.Tiles[stat.X][stat.Y].Color) {
      while (Board.Tiles[ix][iy].Element === rayElement && Board.Tiles[ix][iy].Color === Board.Tiles[stat.X][stat.Y].Color) {
        Board.Tiles[ix][iy].Element = E_EMPTY;
        BoardDrawTile(ix, iy);
        ix += stat.StepX;
        iy += stat.StepY;
      }
      stat.P3 = (stat.P2 * 2) + 1;
      return;
    }

    while (true) {
      elem = Board.Tiles[ix][iy].Element;
      if (elem === E_BOARD_EDGE) {
        break;
      }

      if (elem === E_PLAYER) {
        BoardDamageTile(ix, iy);
      }

      if (!ElementDefs[Board.Tiles[ix][iy].Element].Walkable && Board.Tiles[ix][iy].Element !== E_EMPTY) {
        if (ElementDefs[Board.Tiles[ix][iy].Element].Destructible) {
          BoardDamageTile(ix, iy);
        }
        if (Board.Tiles[ix][iy].Element !== E_EMPTY) {
          break;
        }
      }

      Board.Tiles[ix][iy].Element = rayElement;
      Board.Tiles[ix][iy].Color = Board.Tiles[stat.X][stat.Y].Color;
      BoardDrawTile(ix, iy);

      ix += stat.StepX;
      iy += stat.StepY;
    }

    stat.P3 = (stat.P2 * 2) + 1;
  }

  function elementDuplicatorDraw(x: number, y: number): number {
    var statId: number = GetStatIdAt(x, y);
    var p1: number;
    if (statId < 0) {
      return 250;
    }
    p1 = Board.Stats[statId].P1;
    if (p1 === 1) {
      return 250;
    }
    if (p1 === 2) {
      return 249;
    }
    if (p1 === 3) {
      return 248;
    }
    if (p1 === 4) {
      return 111;
    }
    if (p1 === 5) {
      return 79;
    }
    return 250;
  }

  function elementDuplicatorTick(statId: number): void {
    var stat: Stat = Board.Stats[statId];
    var srcX: number;
    var srcY: number;
    var dstX: number;
    var dstY: number;
    var sourceStatId: number;
    var sourceStat: Stat;

    if (stat.P1 <= 4) {
      stat.P1 += 1;
      BoardDrawTile(stat.X, stat.Y);
      stat.Cycle = (9 - stat.P2) * 3;
      return;
    }

    stat.P1 = 0;
    srcX = stat.X + stat.StepX;
    srcY = stat.Y + stat.StepY;
    dstX = stat.X - stat.StepX;
    dstY = stat.Y - stat.StepY;

    if (Board.Tiles[dstX][dstY].Element !== E_EMPTY) {
      ElementPushablePush(dstX, dstY, -stat.StepX, -stat.StepY);
    }

    if (Board.Tiles[dstX][dstY].Element === E_EMPTY) {
      sourceStatId = GetStatIdAt(srcX, srcY);
      if (sourceStatId > 0) {
        sourceStat = cloneStat(Board.Stats[sourceStatId]);
        AddStat(
          dstX,
          dstY,
          Board.Tiles[srcX][srcY].Element,
          Board.Tiles[srcX][srcY].Color,
          Board.Stats[sourceStatId].Cycle,
          sourceStat
        );
        BoardDrawTile(dstX, dstY);
      } else if (sourceStatId !== 0) {
        Board.Tiles[dstX][dstY] = cloneTile(Board.Tiles[srcX][srcY]);
        BoardDrawTile(dstX, dstY);
      }
    }

    stat.Cycle = (9 - stat.P2) * 3;
    BoardDrawTile(stat.X, stat.Y);
  }

  function elementObjectTick(statId: number): void {
    var stat: Stat;
    var tx: number;
    var ty: number;

    if (statId < 0 || statId > Board.StatCount) {
      return;
    }

    stat = Board.Stats[statId];
    if (stat.DataPos >= 0) {
      stat.DataPos = OopExecute(statId, stat.DataPos, "Interaction");
    }

    if (statId < 0 || statId > Board.StatCount) {
      return;
    }

    stat = Board.Stats[statId];
    if (stat.X < 0 || stat.X > BOARD_WIDTH + 1 || stat.Y < 0 || stat.Y > BOARD_HEIGHT + 1) {
      return;
    }
    if (Board.Tiles[stat.X][stat.Y].Element !== E_OBJECT) {
      return;
    }

    if (stat.StepX !== 0 || stat.StepY !== 0) {
      tx = stat.X + stat.StepX;
      ty = stat.Y + stat.StepY;
      if (tx < 0 || tx > BOARD_WIDTH + 1 || ty < 0 || ty > BOARD_HEIGHT + 1) {
        OopSend(-statId, "THUD", false);
        return;
      }
      if (ElementDefs[Board.Tiles[tx][ty].Element].Walkable) {
        MoveStat(statId, tx, ty);
      } else {
        OopSend(-statId, "THUD", false);
      }
    }
  }

  function elementObjectDraw(x: number, y: number): number {
    var statId: number = GetStatIdAt(x, y);
    if (statId < 0) {
      return 2;
    }
    return Board.Stats[statId].P1;
  }

  function elementObjectTouch(x: number, y: number, _sourceStatId: number, _context: TouchContext): void {
    var statId: number = GetStatIdAt(x, y);
    if (statId >= 0) {
      OopSend(-statId, "TOUCH", false);
    }
  }

  function elementCentipedeHeadTick(statId: number): void {
    var stat: Stat = Board.Stats[statId];
    var oldX: number = stat.X;
    var oldY: number = stat.Y;
    var dir: Direction;
    var tx: number;
    var ty: number;
    var tryDirs: Direction[] = [];
    var i: number;
    var prevX: number;
    var prevY: number;
    var followerId: number;
    var follower: Stat;
    var nextX: number;
    var nextY: number;

    if (stat.X === Board.Stats[0].X && randomInt(10) < stat.P1) {
      stat.StepY = Signum(Board.Stats[0].Y - stat.Y);
      stat.StepX = 0;
    } else if (stat.Y === Board.Stats[0].Y && randomInt(10) < stat.P1) {
      stat.StepX = Signum(Board.Stats[0].X - stat.X);
      stat.StepY = 0;
    } else if ((randomInt(10) * 4) < stat.P2 || (stat.StepX === 0 && stat.StepY === 0)) {
      dir = CalcDirectionRnd();
      stat.StepX = dir.DeltaX;
      stat.StepY = dir.DeltaY;
    }

    tryDirs.push({ DeltaX: stat.StepX, DeltaY: stat.StepY });
    tryDirs.push({ DeltaX: stat.StepY, DeltaY: -stat.StepX });
    tryDirs.push({ DeltaX: -stat.StepY, DeltaY: stat.StepX });
    tryDirs.push({ DeltaX: -stat.StepX, DeltaY: -stat.StepY });

    for (i = 0; i < tryDirs.length; i += 1) {
      tx = stat.X + tryDirs[i].DeltaX;
      ty = stat.Y + tryDirs[i].DeltaY;
      if (ElementDefs[Board.Tiles[tx][ty].Element].Walkable || Board.Tiles[tx][ty].Element === E_PLAYER) {
        stat.StepX = tryDirs[i].DeltaX;
        stat.StepY = tryDirs[i].DeltaY;
        break;
      }
    }

    tx = stat.X + stat.StepX;
    ty = stat.Y + stat.StepY;
    if (!(ElementDefs[Board.Tiles[tx][ty].Element].Walkable || Board.Tiles[tx][ty].Element === E_PLAYER)) {
      stat.StepX = 0;
      stat.StepY = 0;
      return;
    }

    if (Board.Tiles[tx][ty].Element === E_PLAYER) {
      BoardAttack(statId, tx, ty);
      return;
    }

    MoveStat(statId, tx, ty);
    prevX = oldX;
    prevY = oldY;
    followerId = stat.Follower;

    while (followerId > 0) {
      follower = Board.Stats[followerId];
      nextX = follower.X;
      nextY = follower.Y;
      follower.StepX = prevX - follower.X;
      follower.StepY = prevY - follower.Y;
      MoveStat(followerId, prevX, prevY);
      prevX = nextX;
      prevY = nextY;
      followerId = follower.Follower;
    }
  }

  function elementCentipedeSegmentTick(statId: number): void {
    var stat: Stat = Board.Stats[statId];
    if (stat.Leader < 0) {
      if (stat.Leader < -1) {
        Board.Tiles[stat.X][stat.Y].Element = E_CENTIPEDE_HEAD;
      } else {
        stat.Leader -= 1;
      }
    }
  }

  function elementStarDraw(x: number, y: number): number {
    var tile: Tile = Board.Tiles[x][y];
    var chars: string = String.fromCharCode(179) + "/" + String.fromCharCode(196) + "\\";
    tile.Color += 1;
    if (tile.Color > 15) {
      tile.Color = 9;
    }
    return chars.charCodeAt(CurrentTick % 4);
  }

  function elementStarTick(statId: number): void {
    var stat: Stat = Board.Stats[statId];
    var dir: Direction;
    var tx: number;
    var ty: number;
    var destElement: number;

    stat.P2 -= 1;
    if (stat.P2 <= 0) {
      RemoveStat(statId);
      return;
    }

    if ((stat.P2 % 2) !== 0) {
      BoardDrawTile(stat.X, stat.Y);
      return;
    }

    dir = CalcDirectionSeek(stat.X, stat.Y);
    stat.StepX = dir.DeltaX;
    stat.StepY = dir.DeltaY;
    tx = stat.X + stat.StepX;
    ty = stat.Y + stat.StepY;
    destElement = Board.Tiles[tx][ty].Element;

    if (destElement === E_PLAYER || destElement === E_BREAKABLE) {
      BoardAttack(statId, tx, ty);
      return;
    }

    if (!ElementDefs[destElement].Walkable) {
      ElementPushablePush(tx, ty, stat.StepX, stat.StepY);
    }

    if (ElementDefs[Board.Tiles[tx][ty].Element].Walkable || Board.Tiles[tx][ty].Element === E_WATER) {
      MoveStat(statId, tx, ty);
    }
  }

  function elementKeyTouch(x: number, y: number, _sourceStatId: number, _context: TouchContext): void {
    var color: number = Board.Tiles[x][y].Color;
    var key: number = color % 8;
    if (key === 0) {
      key = Math.floor((color / 16) % 16);
      if (key >= 8) {
        key = 0;
      }
    }
    if (key < 1 || key > 7) {
      return;
    }

    if (World.Info.Keys[key]) {
      DisplayMessage(160, "You already have a " + COLOR_NAMES[key] + " key!");
      SoundQueue(2, "\x30\x02\x20\x02");
      return;
    }

    World.Info.Keys[key] = true;
    Board.Tiles[x][y].Element = E_EMPTY;
    GameUpdateSidebar();
    DisplayMessage(160, "You now have the " + COLOR_NAMES[key] + " key.");
    SoundQueue(2, "\x40\x01\x44\x01\x47\x01\x40\x01\x44\x01\x47\x01\x40\x01\x44\x01\x47\x01\x50\x02");
  }

  function elementAmmoTouch(x: number, y: number, _sourceStatId: number, _context: TouchContext): void {
    World.Info.Ammo += 5;
    Board.Tiles[x][y].Element = E_EMPTY;
    GameUpdateSidebar();
    SoundQueue(2, "\x30\x01\x31\x01\x32\x01");
    if (MessageAmmoNotShown) {
      MessageAmmoNotShown = false;
      DisplayMessage(180, "Ammunition - 5 shots per container.");
    }
  }

  function elementGemTouch(x: number, y: number, _sourceStatId: number, _context: TouchContext): void {
    World.Info.Gems += 1;
    World.Info.Health += 1;
    World.Info.Score += 10;
    Board.Tiles[x][y].Element = E_EMPTY;
    GameUpdateSidebar();
    SoundQueue(2, "\x40\x01\x37\x01\x34\x01\x30\x01");
    if (MessageGemNotShown) {
      MessageGemNotShown = false;
      DisplayMessage(180, "Gems give you Health!");
    }
  }

  function elementPassageTouch(x: number, y: number, _sourceStatId: number, context: TouchContext): void {
    BoardPassageTeleport(x, y);
    context.DeltaX = 0;
    context.DeltaY = 0;
  }

  function elementDoorTouch(x: number, y: number, _sourceStatId: number, _context: TouchContext): void {
    var color: number = Board.Tiles[x][y].Color;
    var key: number = Math.floor((color / 16) % 8);
    if (key === 0) {
      key = color % 16;
      if (key >= 8) {
        key = 0;
      }
    }
    if (key < 1 || key > 7) {
      return;
    }

    if (World.Info.Keys[key]) {
      Board.Tiles[x][y].Element = E_EMPTY;
      World.Info.Keys[key] = false;
      GameUpdateSidebar();
      DisplayMessage(180, "The " + COLOR_NAMES[key] + " door is now open.");
      SoundQueue(3, "\x30\x01\x37\x01\x3B\x01\x30\x01\x37\x01\x3B\x01\x40\x04");
    } else {
      DisplayMessage(180, "The " + COLOR_NAMES[key] + " door is locked!");
      SoundQueue(3, "\x17\x01\x10\x01");
    }
  }

  function elementScrollTick(statId: number): void {
    var stat: Stat = Board.Stats[statId];
    var tile: Tile = Board.Tiles[stat.X][stat.Y];
    tile.Color += 1;
    if (tile.Color > 0x0f) {
      tile.Color = 0x09;
    }
    BoardDrawTile(stat.X, stat.Y);
  }

  function elementScrollTouch(x: number, y: number, _sourceStatId: number, _context: TouchContext): void {
    var statId: number = GetStatIdAt(x, y);
    SoundQueue(2, SoundParse("c-c+d-d+e-e+f-f+g-g"));
    if (statId >= 0) {
      Board.Stats[statId].DataPos = 0;
      Board.Stats[statId].DataPos = OopExecute(statId, Board.Stats[statId].DataPos, "Scroll");
    }

    statId = GetStatIdAt(x, y);
    while (statId > 0) {
      RemoveStat(statId);
      statId = GetStatIdAt(x, y);
    }

    if (Board.Tiles[x][y].Element === E_SCROLL) {
      Board.Tiles[x][y].Element = E_EMPTY;
      Board.Tiles[x][y].Color = 0;
    }

    // Match DOS behavior expectations: touched scroll should not persist visually.
    BoardDrawTile(x, y);
  }

  function elementTorchTouch(x: number, y: number, _sourceStatId: number, _context: TouchContext): void {
    World.Info.Torches += 1;
    Board.Tiles[x][y].Element = E_EMPTY;
    GameUpdateSidebar();
    if (MessageTorchNotShown) {
      MessageTorchNotShown = false;
      DisplayMessage(180, "Torch - used for lighting in the underground.");
    }
    SoundQueue(3, "\x30\x01\x39\x01\x34\x02");
  }

  function elementEnergizerTouch(x: number, y: number, _sourceStatId: number, _context: TouchContext): void {
    SoundQueue(
      9,
      "\x20\x03\x23\x03\x24\x03\x25\x03\x35\x03\x25\x03\x23\x03\x20\x03" +
      "\x30\x03\x23\x03\x24\x03\x25\x03\x35\x03\x25\x03\x23\x03\x20\x03" +
      "\x30\x03\x23\x03\x24\x03\x25\x03\x35\x03\x25\x03\x23\x03\x20\x03" +
      "\x30\x03\x23\x03\x24\x03\x25\x03\x35\x03\x25\x03\x23\x03\x20\x03" +
      "\x30\x03\x23\x03\x24\x03\x25\x03\x35\x03\x25\x03\x23\x03\x20\x03" +
      "\x30\x03\x23\x03\x24\x03\x25\x03\x35\x03\x25\x03\x23\x03\x20\x03" +
      "\x30\x03\x23\x03\x24\x03\x25\x03\x35\x03\x25\x03\x23\x03\x20\x03"
    );
    Board.Tiles[x][y].Element = E_EMPTY;
    World.Info.EnergizerTicks = 75;
    GameUpdateSidebar();
    if (MessageEnergizerNotShown) {
      MessageEnergizerNotShown = false;
      DisplayMessage(180, "Energizer - You are invincible");
    }
    OopSend(0, "ALL:ENERGIZE", false);
  }

  function elementForestTouch(x: number, y: number, _sourceStatId: number, _context: TouchContext): void {
    Board.Tiles[x][y].Element = E_EMPTY;
    BoardDrawTile(x, y);
    SoundQueue(3, "\x39\x01");
    if (MessageForestNotShown) {
      MessageForestNotShown = false;
      DisplayMessage(180, "A path is cleared through the forest.");
    }
  }

  function elementFakeTouch(_x: number, _y: number, _sourceStatId: number, _context: TouchContext): void {
    if (MessageFakeNotShown) {
      MessageFakeNotShown = false;
      DisplayMessage(120, "A fake wall - secret passage!");
    }
  }

  function elementInvisibleTouch(x: number, y: number, _sourceStatId: number, _context: TouchContext): void {
    Board.Tiles[x][y].Element = E_NORMAL;
    BoardDrawTile(x, y);
    SoundQueue(3, "\x12\x01\x10\x01");
    DisplayMessage(120, "You are blocked by an invisible wall.");
  }

  function elementWaterTouch(_x: number, _y: number, _sourceStatId: number, _context: TouchContext): void {
    SoundQueue(3, "\x40\x01\x50\x01");
    DisplayMessage(100, "Your way is blocked by water.");
  }

  function elementPushableTouch(x: number, y: number, _sourceStatId: number, context: TouchContext): void {
    ElementPushablePush(x, y, context.DeltaX, context.DeltaY);
    SoundQueue(2, "\x15\x01");
  }

  export function DrawPlayerSurroundings(x: number, y: number, bombPhase: number): void {
    var ix: number;
    var iy: number;
    var statId: number;

    for (ix = (x - TORCH_DX) - 1; ix <= (x + TORCH_DX) + 1; ix += 1) {
      if (ix < 1 || ix > BOARD_WIDTH) {
        continue;
      }

      for (iy = (y - TORCH_DY) - 1; iy <= (y + TORCH_DY) + 1; iy += 1) {
        if (iy < 1 || iy > BOARD_HEIGHT) {
          continue;
        }

        if (bombPhase > 0 && (((ix - x) * (ix - x)) + ((iy - y) * (iy - y) * 2) < TORCH_DIST_SQR)) {
          if (bombPhase === 1) {
            if (ElementDefs[Board.Tiles[ix][iy].Element].ParamTextName.length > 0) {
              statId = GetStatIdAt(ix, iy);
              if (statId > 0) {
                OopSend(-statId, "BOMBED", false);
              }
            }

            if (ElementDefs[Board.Tiles[ix][iy].Element].Destructible || Board.Tiles[ix][iy].Element === E_STAR) {
              BoardDamageTile(ix, iy);
            }

            if (Board.Tiles[ix][iy].Element === E_EMPTY || Board.Tiles[ix][iy].Element === E_BREAKABLE) {
              Board.Tiles[ix][iy].Element = E_BREAKABLE;
              Board.Tiles[ix][iy].Color = 0x09 + randomInt(7);
            }
          } else if (bombPhase === 2) {
            if (Board.Tiles[ix][iy].Element === E_BREAKABLE) {
              Board.Tiles[ix][iy].Element = E_EMPTY;
            }
          }
        }

        BoardDrawTile(ix, iy);
      }
    }
  }

  function elementBombDraw(x: number, y: number): number {
    var statId: number = GetStatIdAt(x, y);
    if (statId < 0) {
      return 11;
    }
    if (Board.Stats[statId].P1 <= 1) {
      return 11;
    }
    return 48 + Board.Stats[statId].P1;
  }

  function elementBombTick(statId: number): void {
    var stat: Stat = Board.Stats[statId];
    var oldX: number;
    var oldY: number;

    if (stat.P1 <= 0) {
      return;
    }

    stat.P1 -= 1;
    BoardDrawTile(stat.X, stat.Y);

    if (stat.P1 === 1) {
      SoundQueue(1, "\x60\x01\x50\x01\x40\x01\x30\x01\x20\x01\x10\x01");
      DrawPlayerSurroundings(stat.X, stat.Y, 1);
    } else if (stat.P1 === 0) {
      oldX = stat.X;
      oldY = stat.Y;
      RemoveStat(statId);
      DrawPlayerSurroundings(oldX, oldY, 2);
    } else {
      if ((stat.P1 % 2) === 0) {
        SoundQueue(1, "\xF8\x01");
      } else {
        SoundQueue(1, "\xF5\x01");
      }
    }
  }

  function elementBombTouch(x: number, y: number, _sourceStatId: number, context: TouchContext): void {
    var statId: number = GetStatIdAt(x, y);
    if (statId < 0) {
      return;
    }

    if (Board.Stats[statId].P1 === 0) {
      Board.Stats[statId].P1 = 9;
      DisplayMessage(160, "Bomb activated!");
      SoundQueue(4, "\x30\x01\x35\x01\x40\x01\x45\x01\x50\x01");
      BoardDrawTile(x, y);
      return;
    }

    ElementPushablePush(x, y, context.DeltaX, context.DeltaY);
  }

  function elementBoardEdgeTouch(_x: number, _y: number, sourceStatId: number, context: TouchContext): void {
    var neighborId: number = 3;
    var boardId: number;
    var entryX: number = Board.Stats[0].X;
    var entryY: number = Board.Stats[0].Y;
    var touchContext: TouchContext;
    var entryElement: number;

    if (context.DeltaY === -1) {
      neighborId = 0;
      entryY = BOARD_HEIGHT;
    } else if (context.DeltaY === 1) {
      neighborId = 1;
      entryY = 1;
    } else if (context.DeltaX === -1) {
      neighborId = 2;
      entryX = BOARD_WIDTH;
    } else {
      neighborId = 3;
      entryX = 1;
    }

    if (Board.Info.NeighborBoards[neighborId] === 0) {
      return;
    }

    boardId = World.Info.CurrentBoard;
    BoardChange(Board.Info.NeighborBoards[neighborId]);

    entryElement = Board.Tiles[entryX][entryY].Element;
    touchContext = {
      DeltaX: context.DeltaX,
      DeltaY: context.DeltaY
    };

    if (entryElement !== E_PLAYER) {
      var entryTouch: ((x: number, y: number, sourceStatId: number, context: TouchContext) => void) | null =
        ElementDefs[entryElement].TouchProc;
      if (entryTouch !== null) {
        entryTouch(entryX, entryY, sourceStatId, touchContext);
      }
    }

    if (ElementDefs[Board.Tiles[entryX][entryY].Element].Walkable || Board.Tiles[entryX][entryY].Element === E_PLAYER) {
      if (Board.Tiles[entryX][entryY].Element !== E_PLAYER) {
        MovePlayerStat(entryX, entryY);
      }
      TransitionDrawBoardChange();
      context.DeltaX = 0;
      context.DeltaY = 0;
      BoardEnter();
    } else {
      BoardChange(boardId);
    }
  }

  function elementPlayerTick(statId: number): void {
    var stat: Stat = Board.Stats[statId];
    var touchContext: TouchContext;
    var desiredDeltaX: number;
    var desiredDeltaY: number;
    var shootX: number;
    var shootY: number;
    var bulletCount: number = 0;
    var i: number;
    var key: string = InputKeyPressed.length > 0 ? InputKeyPressed.charAt(0) : String.fromCharCode(0);
    var fireRequested: boolean;
    var torchRequested: boolean;
    var fireDirX: number;
    var fireDirY: number;

    if (World.Info.EnergizerTicks > 0) {
      if (ElementDefs[E_PLAYER].Character === String.fromCharCode(2)) {
        ElementDefs[E_PLAYER].Character = String.fromCharCode(1);
      } else {
        ElementDefs[E_PLAYER].Character = String.fromCharCode(2);
      }

      if ((CurrentTick % 2) !== 0) {
        Board.Tiles[stat.X][stat.Y].Color = 0x0f;
      } else {
        Board.Tiles[stat.X][stat.Y].Color = (((CurrentTick % 7) + 1) * 16) + 0x0f;
      }

      BoardDrawTile(stat.X, stat.Y);
    } else if (
      Board.Tiles[stat.X][stat.Y].Color !== 0x1f ||
      ElementDefs[E_PLAYER].Character !== String.fromCharCode(2)
    ) {
      Board.Tiles[stat.X][stat.Y].Color = 0x1f;
      ElementDefs[E_PLAYER].Character = String.fromCharCode(2);
      BoardDrawTile(stat.X, stat.Y);
    }

    if (World.Info.Health <= 0) {
      InputDeltaX = 0;
      InputDeltaY = 0;
      InputShiftPressed = false;
      InputFireDirX = 0;
      InputFireDirY = 0;
      if (GetStatIdAt(0, 0) === -1) {
        DisplayMessage(32000, " Game over  -  Press ESCAPE");
      }
      TickTimeDuration = 0;
      SoundBlockQueueing = true;
    }

    fireDirX = InputFireDirX;
    fireDirY = InputFireDirY;
    fireRequested = InputFirePressed || InputShiftPressed || key === " " || fireDirX !== 0 || fireDirY !== 0;
    if (fireRequested) {
      // Space/shift fire should survive high-frequency input polling until consumed by player tick.
      InputFirePressed = false;
      InputFireDirX = 0;
      InputFireDirY = 0;

      if (fireDirX !== 0 || fireDirY !== 0) {
        PlayerDirX = fireDirX;
        PlayerDirY = fireDirY;
      } else if (InputShiftPressed && (InputDeltaX !== 0 || InputDeltaY !== 0)) {
        PlayerDirX = InputDeltaX;
        PlayerDirY = InputDeltaY;
      }

      shootX = PlayerDirX;
      shootY = PlayerDirY;
      if (shootX !== 0 || shootY !== 0) {
        if (Board.Info.MaxShots <= 0) {
          if (MessageNoShootingNotShown) {
            MessageNoShootingNotShown = false;
            DisplayMessage(160, "Can't shoot in this place!");
          }
        } else if (World.Info.Ammo <= 0) {
          if (MessageOutOfAmmoNotShown) {
            MessageOutOfAmmoNotShown = false;
            DisplayMessage(160, "You don't have any ammo!");
          }
        } else {
          for (i = 0; i <= Board.StatCount; i += 1) {
            if (Board.Tiles[Board.Stats[i].X][Board.Stats[i].Y].Element === E_BULLET && Board.Stats[i].P1 === SHOT_SOURCE_PLAYER) {
              bulletCount += 1;
            }
          }

          if (bulletCount < Board.Info.MaxShots && BoardShoot(E_BULLET, stat.X, stat.Y, shootX, shootY, SHOT_SOURCE_PLAYER)) {
            World.Info.Ammo -= 1;
            GameUpdateSidebar();
            SoundQueue(2, "\x40\x01\x30\x01\x20\x01");
            InputDeltaX = 0;
            InputDeltaY = 0;
          }
        }
      }
    } else if (InputDeltaX !== 0 || InputDeltaY !== 0) {
      desiredDeltaX = InputDeltaX;
      desiredDeltaY = InputDeltaY;

      PlayerDirX = desiredDeltaX;
      PlayerDirY = desiredDeltaY;
      touchContext = {
        DeltaX: desiredDeltaX,
        DeltaY: desiredDeltaY
      };

      var destTouch: ((x: number, y: number, sourceStatId: number, context: TouchContext) => void) | null =
        ElementDefs[Board.Tiles[stat.X + desiredDeltaX][stat.Y + desiredDeltaY].Element].TouchProc;
      if (destTouch !== null) {
        destTouch(
          stat.X + desiredDeltaX,
          stat.Y + desiredDeltaY,
          0,
          touchContext
        );
      }

      InputDeltaX = touchContext.DeltaX;
      InputDeltaY = touchContext.DeltaY;

      if ((InputDeltaX !== 0 || InputDeltaY !== 0) &&
          ElementDefs[Board.Tiles[stat.X + InputDeltaX][stat.Y + InputDeltaY].Element].Walkable) {
        MovePlayerStat(stat.X + InputDeltaX, stat.Y + InputDeltaY);
      }
    }

    key = key.toUpperCase();
    torchRequested = InputTorchPressed || key === "T";
    if (torchRequested) {
      InputTorchPressed = false;
    }

    if (torchRequested && World.Info.TorchTicks <= 0) {
      if (World.Info.Torches > 0) {
        if (Board.Info.IsDark) {
          World.Info.Torches -= 1;
          World.Info.TorchTicks = TORCH_DURATION;
          DrawPlayerSurroundings(stat.X, stat.Y, 0);
          GameUpdateSidebar();
        } else if (MessageRoomNotDarkNotShown) {
          MessageRoomNotDarkNotShown = false;
          DisplayMessage(160, "Don't need torch - room is not dark!");
        }
      } else if (MessageOutOfTorchesNotShown) {
        MessageOutOfTorchesNotShown = false;
        DisplayMessage(160, "You don't have any torches!");
      }
    } else if (key === KEY_ESCAPE || key === "Q") {
      GamePromptEndPlay();
    } else if (key === "S") {
      GameWorldSave("Save game:", SavedGameFileName, ".SAV");
    } else if (key === "P") {
      if (World.Info.Health > 0) {
        GamePaused = true;
      }
    } else if (key === "B") {
      SoundEnabled = !SoundEnabled;
      SoundClearQueue();
      if (SoundEnabled) {
        SoundWorldMusicOnBoardChanged(Board.Name);
      }
      GameUpdateSidebar();
      InputKeyPressed = " ";
    } else if (key === "H") {
      TextWindowDisplayFile("GAME.HLP", "Playing ZZT");
    } else if (key === "F") {
      TextWindowDisplayFile("ORDER.HLP", "Order form");
    } else if (key === "?") {
      GameDebugPrompt();
      InputKeyPressed = String.fromCharCode(0);
    }

    if (World.Info.TorchTicks > 0) {
      World.Info.TorchTicks -= 1;
      if (World.Info.TorchTicks <= 0) {
        DrawPlayerSurroundings(stat.X, stat.Y, 0);
        SoundQueue(3, "\x30\x01\x20\x01\x10\x01");
      }
      if ((World.Info.TorchTicks % 40) === 0 || World.Info.TorchTicks === 0) {
        GameUpdateSidebar();
      }
    }

    if (World.Info.EnergizerTicks > 0) {
      World.Info.EnergizerTicks -= 1;
      if (World.Info.EnergizerTicks === 10) {
        SoundQueue(9, "\x20\x03\x1A\x03\x17\x03\x16\x03\x15\x03\x13\x03\x10\x03");
      } else if (World.Info.EnergizerTicks <= 0) {
        Board.Tiles[stat.X][stat.Y].Color = ElementDefs[E_PLAYER].Color;
        BoardDrawTile(stat.X, stat.Y);
      }
    }

    if (Board.Info.TimeLimitSec > 0 && World.Info.Health > 0) {
      if (WorldHasTimeElapsed(100)) {
        World.Info.BoardTimeSec += 1;

        if ((Board.Info.TimeLimitSec - 10) === World.Info.BoardTimeSec) {
          DisplayMessage(160, "Running out of time!");
          SoundQueue(3, "\x40\x06\x45\x06\x40\x06\x35\x06\x40\x06\x45\x06\x40\x0A");
        } else if (World.Info.BoardTimeSec > Board.Info.TimeLimitSec) {
          DamageStat(0);
        }

        GameUpdateSidebar();
      }
    }
  }

  function elementMonitorTick(_statId: number): void {
    var key: string = InputKeyPressed.length > 0 ? InputKeyPressed.charAt(0).toUpperCase() : String.fromCharCode(0);
    if (key === KEY_ESCAPE || key === "A" || key === "E" || key === "H" || key === "N" ||
        key === "P" || key === "Q" || key === "R" || key === "S" || key === "W" || key === "|") {
      GamePlayExitRequested = true;
    }
  }

  function initElementDefs(): void {
    var i: number;

    for (i = 0; i <= MAX_ELEMENT; i += 1) {
      ElementDefs[i] = createElementDefDefault();
      ElementDefs[i].TickProc = elementDefaultTick;
      ElementDefs[i].DrawProc = elementDefaultDraw;
      ElementDefs[i].TouchProc = elementDefaultTouch;
    }

    ElementDefs[E_EMPTY].Character = " ";
    ElementDefs[E_EMPTY].Color = 0x70;
    ElementDefs[E_EMPTY].Pushable = true;
    ElementDefs[E_EMPTY].Walkable = true;
    ElementDefs[E_EMPTY].Name = "Empty";

    ElementDefs[E_BOARD_EDGE].Character = " ";
    ElementDefs[E_BOARD_EDGE].Color = 0x00;
    ElementDefs[E_BOARD_EDGE].TouchProc = elementBoardEdgeTouch;
    ElementDefs[E_BOARD_EDGE].Name = "Board edge";

    ElementDefs[E_MONITOR].Character = " ";
    ElementDefs[E_MONITOR].Color = 0x07;
    ElementDefs[E_MONITOR].Cycle = 1;
    ElementDefs[E_MONITOR].TickProc = elementMonitorTick;
    ElementDefs[E_MONITOR].Name = "Monitor";

    ElementDefs[E_MESSAGE_TIMER].Cycle = 1;
    ElementDefs[E_MESSAGE_TIMER].TickProc = elementMessageTimerTick;

    ElementDefs[E_PLAYER].Character = String.fromCharCode(2);
    ElementDefs[E_PLAYER].Color = 0x1f;
    ElementDefs[E_PLAYER].Destructible = true;
    ElementDefs[E_PLAYER].Pushable = true;
    ElementDefs[E_PLAYER].VisibleInDark = true;
    ElementDefs[E_PLAYER].Cycle = 1;
    ElementDefs[E_PLAYER].TickProc = elementPlayerTick;
    ElementDefs[E_PLAYER].EditorCategory = CATEGORY_ITEM;
    ElementDefs[E_PLAYER].EditorShortcut = "Z";
    ElementDefs[E_PLAYER].Name = "Player";
    ElementDefs[E_PLAYER].CategoryName = "Items:";

    ElementDefs[E_WATER].Character = String.fromCharCode(176);
    ElementDefs[E_WATER].Color = 0xf9;
    ElementDefs[E_WATER].PlaceableOnTop = true;
    ElementDefs[E_WATER].TouchProc = elementWaterTouch;
    ElementDefs[E_WATER].EditorCategory = CATEGORY_TERRAIN;
    ElementDefs[E_WATER].EditorShortcut = "W";
    ElementDefs[E_WATER].Name = "Water";
    ElementDefs[E_WATER].CategoryName = "Terrains:";

    ElementDefs[E_FOREST].Character = String.fromCharCode(176);
    ElementDefs[E_FOREST].Color = 0x20;
    ElementDefs[E_FOREST].TouchProc = elementForestTouch;
    ElementDefs[E_FOREST].EditorCategory = CATEGORY_TERRAIN;
    ElementDefs[E_FOREST].EditorShortcut = "F";
    ElementDefs[E_FOREST].Name = "Forest";

    ElementDefs[E_SOLID].Character = String.fromCharCode(219);
    ElementDefs[E_SOLID].EditorCategory = CATEGORY_TERRAIN;
    ElementDefs[E_SOLID].CategoryName = "Walls:";
    ElementDefs[E_SOLID].EditorShortcut = "S";
    ElementDefs[E_SOLID].Name = "Solid";

    ElementDefs[E_NORMAL].Character = String.fromCharCode(178);
    ElementDefs[E_NORMAL].EditorCategory = CATEGORY_TERRAIN;
    ElementDefs[E_NORMAL].EditorShortcut = "N";
    ElementDefs[E_NORMAL].Name = "Normal";

    ElementDefs[E_BREAKABLE].Character = String.fromCharCode(177);
    ElementDefs[E_BREAKABLE].EditorCategory = CATEGORY_TERRAIN;
    ElementDefs[E_BREAKABLE].EditorShortcut = "B";
    ElementDefs[E_BREAKABLE].Name = "Breakable";

    ElementDefs[E_LINE].Character = String.fromCharCode(206);
    ElementDefs[E_LINE].Name = "Line";

    ElementDefs[E_BULLET].Character = String.fromCharCode(248);
    ElementDefs[E_BULLET].Color = 0x0f;
    ElementDefs[E_BULLET].Destructible = true;
    ElementDefs[E_BULLET].Cycle = 1;
    ElementDefs[E_BULLET].TickProc = elementBulletTick;
    ElementDefs[E_BULLET].TouchProc = elementDamagingTouch;
    ElementDefs[E_BULLET].Name = "Bullet";

    ElementDefs[E_STAR].Character = "S";
    ElementDefs[E_STAR].Color = 0x0f;
    ElementDefs[E_STAR].Cycle = 1;
    ElementDefs[E_STAR].TickProc = elementStarTick;
    ElementDefs[E_STAR].TouchProc = elementDamagingTouch;
    ElementDefs[E_STAR].HasDrawProc = true;
    ElementDefs[E_STAR].DrawProc = elementStarDraw;
    ElementDefs[E_STAR].Name = "Star";

    ElementDefs[E_DUPLICATOR].Character = String.fromCharCode(250);
    ElementDefs[E_DUPLICATOR].Color = 0x0f;
    ElementDefs[E_DUPLICATOR].Cycle = 2;
    ElementDefs[E_DUPLICATOR].TickProc = elementDuplicatorTick;
    ElementDefs[E_DUPLICATOR].HasDrawProc = true;
    ElementDefs[E_DUPLICATOR].DrawProc = elementDuplicatorDraw;
    ElementDefs[E_DUPLICATOR].EditorCategory = CATEGORY_ITEM;
    ElementDefs[E_DUPLICATOR].EditorShortcut = "U";
    ElementDefs[E_DUPLICATOR].Name = "Duplicator";
    ElementDefs[E_DUPLICATOR].ParamDirName = "Source direction?";
    ElementDefs[E_DUPLICATOR].Param2Name = "Duplication rate?;SF";

    ElementDefs[E_CONVEYOR_CW].Character = "/";
    ElementDefs[E_CONVEYOR_CW].Cycle = 3;
    ElementDefs[E_CONVEYOR_CW].HasDrawProc = true;
    ElementDefs[E_CONVEYOR_CW].TickProc = elementConveyorCWTick;
    ElementDefs[E_CONVEYOR_CW].DrawProc = elementConveyorCWDraw;
    ElementDefs[E_CONVEYOR_CW].EditorCategory = CATEGORY_ITEM;
    ElementDefs[E_CONVEYOR_CW].EditorShortcut = "1";
    ElementDefs[E_CONVEYOR_CW].Name = "Clockwise";
    ElementDefs[E_CONVEYOR_CW].CategoryName = "Conveyors:";

    ElementDefs[E_CONVEYOR_CCW].Character = "\\";
    ElementDefs[E_CONVEYOR_CCW].Cycle = 2;
    ElementDefs[E_CONVEYOR_CCW].HasDrawProc = true;
    ElementDefs[E_CONVEYOR_CCW].TickProc = elementConveyorCCWTick;
    ElementDefs[E_CONVEYOR_CCW].DrawProc = elementConveyorCCWDraw;
    ElementDefs[E_CONVEYOR_CCW].EditorCategory = CATEGORY_ITEM;
    ElementDefs[E_CONVEYOR_CCW].EditorShortcut = "2";
    ElementDefs[E_CONVEYOR_CCW].Name = "Counter";

    ElementDefs[E_SPINNING_GUN].Character = String.fromCharCode(24);
    ElementDefs[E_SPINNING_GUN].Cycle = 2;
    ElementDefs[E_SPINNING_GUN].TickProc = elementSpinningGunTick;
    ElementDefs[E_SPINNING_GUN].HasDrawProc = true;
    ElementDefs[E_SPINNING_GUN].DrawProc = elementSpinningGunDraw;
    ElementDefs[E_SPINNING_GUN].EditorCategory = CATEGORY_CREATURE;
    ElementDefs[E_SPINNING_GUN].EditorShortcut = "G";
    ElementDefs[E_SPINNING_GUN].Name = "Spinning gun";
    ElementDefs[E_SPINNING_GUN].Param1Name = "Intelligence?";
    ElementDefs[E_SPINNING_GUN].Param2Name = "Firing rate?";
    ElementDefs[E_SPINNING_GUN].ParamBulletTypeName = "Firing type?";

    ElementDefs[E_LION].Character = String.fromCharCode(234);
    ElementDefs[E_LION].Color = 0x0c;
    ElementDefs[E_LION].Destructible = true;
    ElementDefs[E_LION].Pushable = true;
    ElementDefs[E_LION].Cycle = 2;
    ElementDefs[E_LION].TickProc = elementLionTick;
    ElementDefs[E_LION].TouchProc = elementDamagingTouch;
    ElementDefs[E_LION].ScoreValue = 1;
    ElementDefs[E_LION].EditorCategory = CATEGORY_CREATURE;
    ElementDefs[E_LION].EditorShortcut = "L";
    ElementDefs[E_LION].Name = "Lion";
    ElementDefs[E_LION].CategoryName = "Beasts:";
    ElementDefs[E_LION].Param1Name = "Intelligence?";

    ElementDefs[E_TIGER].Character = String.fromCharCode(227);
    ElementDefs[E_TIGER].Color = 0x0b;
    ElementDefs[E_TIGER].Destructible = true;
    ElementDefs[E_TIGER].Pushable = true;
    ElementDefs[E_TIGER].Cycle = 2;
    ElementDefs[E_TIGER].TickProc = elementTigerTick;
    ElementDefs[E_TIGER].TouchProc = elementDamagingTouch;
    ElementDefs[E_TIGER].ScoreValue = 2;
    ElementDefs[E_TIGER].EditorCategory = CATEGORY_CREATURE;
    ElementDefs[E_TIGER].EditorShortcut = "T";
    ElementDefs[E_TIGER].Name = "Tiger";
    ElementDefs[E_TIGER].Param1Name = "Intelligence?";
    ElementDefs[E_TIGER].Param2Name = "Firing rate?";
    ElementDefs[E_TIGER].ParamBulletTypeName = "Firing type?";

    ElementDefs[E_CENTIPEDE_HEAD].Character = String.fromCharCode(233);
    ElementDefs[E_CENTIPEDE_HEAD].Destructible = true;
    ElementDefs[E_CENTIPEDE_HEAD].Cycle = 2;
    ElementDefs[E_CENTIPEDE_HEAD].TickProc = elementCentipedeHeadTick;
    ElementDefs[E_CENTIPEDE_HEAD].TouchProc = elementDamagingTouch;
    ElementDefs[E_CENTIPEDE_HEAD].ScoreValue = 1;
    ElementDefs[E_CENTIPEDE_HEAD].EditorCategory = CATEGORY_CREATURE;
    ElementDefs[E_CENTIPEDE_HEAD].EditorShortcut = "H";
    ElementDefs[E_CENTIPEDE_HEAD].Name = "Head";
    ElementDefs[E_CENTIPEDE_HEAD].CategoryName = "Centipedes";
    ElementDefs[E_CENTIPEDE_HEAD].Param1Name = "Intelligence?";
    ElementDefs[E_CENTIPEDE_HEAD].Param2Name = "Deviance?";

    ElementDefs[E_CENTIPEDE_SEGMENT].Character = "O";
    ElementDefs[E_CENTIPEDE_SEGMENT].Destructible = true;
    ElementDefs[E_CENTIPEDE_SEGMENT].Cycle = 2;
    ElementDefs[E_CENTIPEDE_SEGMENT].TickProc = elementCentipedeSegmentTick;
    ElementDefs[E_CENTIPEDE_SEGMENT].TouchProc = elementDamagingTouch;
    ElementDefs[E_CENTIPEDE_SEGMENT].ScoreValue = 3;
    ElementDefs[E_CENTIPEDE_SEGMENT].EditorCategory = CATEGORY_CREATURE;
    ElementDefs[E_CENTIPEDE_SEGMENT].EditorShortcut = "S";
    ElementDefs[E_CENTIPEDE_SEGMENT].Name = "Segment";

    ElementDefs[E_BEAR].Character = String.fromCharCode(153);
    ElementDefs[E_BEAR].Color = 0x06;
    ElementDefs[E_BEAR].Destructible = true;
    ElementDefs[E_BEAR].Pushable = true;
    ElementDefs[E_BEAR].Cycle = 3;
    ElementDefs[E_BEAR].TickProc = elementBearTick;
    ElementDefs[E_BEAR].TouchProc = elementDamagingTouch;
    ElementDefs[E_BEAR].ScoreValue = 1;
    ElementDefs[E_BEAR].EditorCategory = CATEGORY_CREATURE;
    ElementDefs[E_BEAR].EditorShortcut = "B";
    ElementDefs[E_BEAR].Name = "Bear";
    ElementDefs[E_BEAR].CategoryName = "Creatures:";
    ElementDefs[E_BEAR].Param1Name = "Sensitivity?";

    ElementDefs[E_RUFFIAN].Character = String.fromCharCode(5);
    ElementDefs[E_RUFFIAN].Color = 0x0d;
    ElementDefs[E_RUFFIAN].Destructible = true;
    ElementDefs[E_RUFFIAN].Pushable = true;
    ElementDefs[E_RUFFIAN].Cycle = 1;
    ElementDefs[E_RUFFIAN].TickProc = elementRuffianTick;
    ElementDefs[E_RUFFIAN].TouchProc = elementDamagingTouch;
    ElementDefs[E_RUFFIAN].ScoreValue = 2;
    ElementDefs[E_RUFFIAN].EditorCategory = CATEGORY_CREATURE;
    ElementDefs[E_RUFFIAN].EditorShortcut = "R";
    ElementDefs[E_RUFFIAN].Name = "Ruffian";
    ElementDefs[E_RUFFIAN].Param1Name = "Intelligence?";
    ElementDefs[E_RUFFIAN].Param2Name = "Resting time?";

    ElementDefs[E_SHARK].Character = "^";
    ElementDefs[E_SHARK].Color = 0x07;
    ElementDefs[E_SHARK].Cycle = 3;
    ElementDefs[E_SHARK].TickProc = elementSharkTick;
    ElementDefs[E_SHARK].EditorCategory = CATEGORY_CREATURE;
    ElementDefs[E_SHARK].EditorShortcut = "Y";
    ElementDefs[E_SHARK].Name = "Shark";
    ElementDefs[E_SHARK].Param1Name = "Intelligence?";

    ElementDefs[E_SLIME].Character = "*";
    ElementDefs[E_SLIME].Color = COLOR_CHOICE_ON_BLACK;
    ElementDefs[E_SLIME].Cycle = 3;
    ElementDefs[E_SLIME].TickProc = elementSlimeTick;
    ElementDefs[E_SLIME].TouchProc = elementSlimeTouch;
    ElementDefs[E_SLIME].EditorCategory = CATEGORY_CREATURE;
    ElementDefs[E_SLIME].EditorShortcut = "V";
    ElementDefs[E_SLIME].Name = "Slime";
    ElementDefs[E_SLIME].Param2Name = "Movement speed?;FS";

    ElementDefs[E_AMMO].Character = String.fromCharCode(132);
    ElementDefs[E_AMMO].Color = 0x03;
    ElementDefs[E_AMMO].Pushable = true;
    ElementDefs[E_AMMO].TouchProc = elementAmmoTouch;
    ElementDefs[E_AMMO].EditorCategory = CATEGORY_ITEM;
    ElementDefs[E_AMMO].EditorShortcut = "A";
    ElementDefs[E_AMMO].Name = "Ammo";

    ElementDefs[E_TORCH].Character = String.fromCharCode(157);
    ElementDefs[E_TORCH].Color = 0x06;
    ElementDefs[E_TORCH].VisibleInDark = true;
    ElementDefs[E_TORCH].TouchProc = elementTorchTouch;
    ElementDefs[E_TORCH].EditorCategory = CATEGORY_ITEM;
    ElementDefs[E_TORCH].EditorShortcut = "T";
    ElementDefs[E_TORCH].Name = "Torch";

    ElementDefs[E_GEM].Character = String.fromCharCode(4);
    ElementDefs[E_GEM].Pushable = true;
    ElementDefs[E_GEM].Destructible = true;
    ElementDefs[E_GEM].TouchProc = elementGemTouch;
    ElementDefs[E_GEM].EditorCategory = CATEGORY_ITEM;
    ElementDefs[E_GEM].EditorShortcut = "G";
    ElementDefs[E_GEM].Name = "Gem";

    ElementDefs[E_KEY].Character = String.fromCharCode(12);
    ElementDefs[E_KEY].Pushable = true;
    ElementDefs[E_KEY].TouchProc = elementKeyTouch;
    ElementDefs[E_KEY].EditorCategory = CATEGORY_ITEM;
    ElementDefs[E_KEY].EditorShortcut = "K";
    ElementDefs[E_KEY].Name = "Key";

    ElementDefs[E_DOOR].Character = String.fromCharCode(10);
    ElementDefs[E_DOOR].Color = COLOR_WHITE_ON_CHOICE;
    ElementDefs[E_DOOR].TouchProc = elementDoorTouch;
    ElementDefs[E_DOOR].EditorCategory = CATEGORY_ITEM;
    ElementDefs[E_DOOR].EditorShortcut = "D";
    ElementDefs[E_DOOR].Name = "Door";

    ElementDefs[E_SCROLL].Character = String.fromCharCode(232);
    ElementDefs[E_SCROLL].Color = 0x0f;
    ElementDefs[E_SCROLL].Pushable = true;
    ElementDefs[E_SCROLL].Cycle = 1;
    ElementDefs[E_SCROLL].TouchProc = elementScrollTouch;
    ElementDefs[E_SCROLL].TickProc = elementScrollTick;
    ElementDefs[E_SCROLL].EditorCategory = CATEGORY_ITEM;
    ElementDefs[E_SCROLL].EditorShortcut = "S";
    ElementDefs[E_SCROLL].Name = "Scroll";
    ElementDefs[E_SCROLL].ParamTextName = "Edit text of scroll";

    ElementDefs[E_OBJECT].Character = String.fromCharCode(2);
    ElementDefs[E_OBJECT].Cycle = 3;
    ElementDefs[E_OBJECT].HasDrawProc = true;
    ElementDefs[E_OBJECT].DrawProc = elementObjectDraw;
    ElementDefs[E_OBJECT].TickProc = elementObjectTick;
    ElementDefs[E_OBJECT].TouchProc = elementObjectTouch;
    ElementDefs[E_OBJECT].EditorCategory = CATEGORY_CREATURE;
    ElementDefs[E_OBJECT].EditorShortcut = "O";
    ElementDefs[E_OBJECT].Name = "Object";
    ElementDefs[E_OBJECT].Param1Name = "Character?";
    ElementDefs[E_OBJECT].ParamTextName = "Edit Program";

    ElementDefs[E_PASSAGE].Character = String.fromCharCode(240);
    ElementDefs[E_PASSAGE].Color = COLOR_WHITE_ON_CHOICE;
    ElementDefs[E_PASSAGE].Cycle = 0;
    ElementDefs[E_PASSAGE].VisibleInDark = true;
    ElementDefs[E_PASSAGE].TouchProc = elementPassageTouch;
    ElementDefs[E_PASSAGE].EditorCategory = CATEGORY_ITEM;
    ElementDefs[E_PASSAGE].EditorShortcut = "P";
    ElementDefs[E_PASSAGE].Name = "Passage";
    ElementDefs[E_PASSAGE].ParamBoardName = "Room thru passage?";

    ElementDefs[E_BOMB].Character = String.fromCharCode(11);
    ElementDefs[E_BOMB].Pushable = true;
    ElementDefs[E_BOMB].Cycle = 6;
    ElementDefs[E_BOMB].HasDrawProc = true;
    ElementDefs[E_BOMB].DrawProc = elementBombDraw;
    ElementDefs[E_BOMB].TickProc = elementBombTick;
    ElementDefs[E_BOMB].TouchProc = elementBombTouch;
    ElementDefs[E_BOMB].EditorCategory = CATEGORY_ITEM;
    ElementDefs[E_BOMB].EditorShortcut = "B";
    ElementDefs[E_BOMB].Name = "Bomb";

    ElementDefs[E_ENERGIZER].Character = String.fromCharCode(127);
    ElementDefs[E_ENERGIZER].Color = 0x05;
    ElementDefs[E_ENERGIZER].TouchProc = elementEnergizerTouch;
    ElementDefs[E_ENERGIZER].EditorCategory = CATEGORY_ITEM;
    ElementDefs[E_ENERGIZER].EditorShortcut = "E";
    ElementDefs[E_ENERGIZER].Name = "Energizer";

    ElementDefs[E_FAKE].Character = String.fromCharCode(178);
    ElementDefs[E_FAKE].PlaceableOnTop = true;
    ElementDefs[E_FAKE].Walkable = true;
    ElementDefs[E_FAKE].TouchProc = elementFakeTouch;
    ElementDefs[E_FAKE].EditorCategory = CATEGORY_TERRAIN;
    ElementDefs[E_FAKE].EditorShortcut = "A";
    ElementDefs[E_FAKE].Name = "Fake";

    ElementDefs[E_INVISIBLE].Character = " ";
    ElementDefs[E_INVISIBLE].TouchProc = elementInvisibleTouch;
    ElementDefs[E_INVISIBLE].EditorCategory = CATEGORY_TERRAIN;
    ElementDefs[E_INVISIBLE].EditorShortcut = "I";
    ElementDefs[E_INVISIBLE].Name = "Invisible";

    ElementDefs[E_BLINK_WALL].Character = String.fromCharCode(206);
    ElementDefs[E_BLINK_WALL].Cycle = 1;
    ElementDefs[E_BLINK_WALL].TickProc = elementBlinkWallTick;
    ElementDefs[E_BLINK_WALL].HasDrawProc = true;
    ElementDefs[E_BLINK_WALL].DrawProc = elementBlinkWallDraw;
    ElementDefs[E_BLINK_WALL].EditorCategory = CATEGORY_TERRAIN;
    ElementDefs[E_BLINK_WALL].EditorShortcut = "L";
    ElementDefs[E_BLINK_WALL].Name = "Blink wall";
    ElementDefs[E_BLINK_WALL].Param1Name = "Starting time";
    ElementDefs[E_BLINK_WALL].Param2Name = "Period";
    ElementDefs[E_BLINK_WALL].ParamDirName = "Wall direction";

    ElementDefs[E_BLINK_RAY_EW].Character = String.fromCharCode(205);
    ElementDefs[E_BLINK_RAY_NS].Character = String.fromCharCode(186);

    ElementDefs[E_RICOCHET].Character = "*";
    ElementDefs[E_RICOCHET].Color = 0x0a;
    ElementDefs[E_RICOCHET].EditorCategory = CATEGORY_TERRAIN;
    ElementDefs[E_RICOCHET].EditorShortcut = "R";
    ElementDefs[E_RICOCHET].Name = "Ricochet";

    ElementDefs[E_BOULDER].Character = String.fromCharCode(254);
    ElementDefs[E_BOULDER].Pushable = true;
    ElementDefs[E_BOULDER].TouchProc = elementPushableTouch;
    ElementDefs[E_BOULDER].EditorCategory = CATEGORY_TERRAIN;
    ElementDefs[E_BOULDER].EditorShortcut = "O";
    ElementDefs[E_BOULDER].Name = "Boulder";

    ElementDefs[E_SLIDER_NS].Character = String.fromCharCode(18);
    ElementDefs[E_SLIDER_NS].TouchProc = elementPushableTouch;
    ElementDefs[E_SLIDER_NS].EditorCategory = CATEGORY_TERRAIN;
    ElementDefs[E_SLIDER_NS].EditorShortcut = "1";
    ElementDefs[E_SLIDER_NS].Name = "Slider (NS)";

    ElementDefs[E_SLIDER_EW].Character = String.fromCharCode(29);
    ElementDefs[E_SLIDER_EW].TouchProc = elementPushableTouch;
    ElementDefs[E_SLIDER_EW].EditorCategory = CATEGORY_TERRAIN;
    ElementDefs[E_SLIDER_EW].EditorShortcut = "2";
    ElementDefs[E_SLIDER_EW].Name = "Slider (EW)";

    ElementDefs[E_TRANSPORTER].Character = String.fromCharCode(197);
    ElementDefs[E_TRANSPORTER].TouchProc = elementTransporterTouch;
    ElementDefs[E_TRANSPORTER].HasDrawProc = true;
    ElementDefs[E_TRANSPORTER].DrawProc = elementTransporterDraw;
    ElementDefs[E_TRANSPORTER].Cycle = 2;
    ElementDefs[E_TRANSPORTER].TickProc = elementTransporterTick;
    ElementDefs[E_TRANSPORTER].EditorCategory = CATEGORY_TERRAIN;
    ElementDefs[E_TRANSPORTER].EditorShortcut = "T";
    ElementDefs[E_TRANSPORTER].Name = "Transporter";
    ElementDefs[E_TRANSPORTER].ParamDirName = "Direction?";

    ElementDefs[E_PUSHER].Character = String.fromCharCode(16);
    ElementDefs[E_PUSHER].Color = COLOR_CHOICE_ON_BLACK;
    ElementDefs[E_PUSHER].HasDrawProc = true;
    ElementDefs[E_PUSHER].DrawProc = elementPusherDraw;
    ElementDefs[E_PUSHER].Cycle = 4;
    ElementDefs[E_PUSHER].TickProc = elementPusherTick;
    ElementDefs[E_PUSHER].EditorCategory = CATEGORY_CREATURE;
    ElementDefs[E_PUSHER].EditorShortcut = "P";
    ElementDefs[E_PUSHER].Name = "Pusher";
    ElementDefs[E_PUSHER].ParamDirName = "Push direction?";

    ElementDefs[E_LINE].HasDrawProc = true;
    ElementDefs[E_LINE].DrawProc = elementLineDraw;

    ElementDefs[E_TEXT_BLUE].Character = " ";
    ElementDefs[E_TEXT_GREEN].Character = " ";
    ElementDefs[E_TEXT_CYAN].Character = " ";
    ElementDefs[E_TEXT_RED].Character = " ";
    ElementDefs[E_TEXT_PURPLE].Character = " ";
    ElementDefs[E_TEXT_YELLOW].Character = " ";
    ElementDefs[E_TEXT_WHITE].Character = " ";

    EditorPatternCount = 5;
    EditorPatterns[1] = E_SOLID;
    EditorPatterns[2] = E_NORMAL;
    EditorPatterns[3] = E_BREAKABLE;
    EditorPatterns[4] = E_EMPTY;
    EditorPatterns[5] = E_LINE;
  }

  export function ResetMessageNotShownFlags(): void {
    MessageAmmoNotShown = true;
    MessageOutOfAmmoNotShown = true;
    MessageNoShootingNotShown = true;
    MessageTorchNotShown = true;
    MessageOutOfTorchesNotShown = true;
    MessageRoomNotDarkNotShown = true;
    MessageHintTorchNotShown = true;
    MessageForestNotShown = true;
    MessageFakeNotShown = true;
    MessageGemNotShown = true;
    MessageEnergizerNotShown = true;
  }

  export function InitElementsEditor(): void {
    initElementDefs();
    ElementDefs[E_INVISIBLE].Character = String.fromCharCode(176);
    ElementDefs[E_INVISIBLE].Color = COLOR_CHOICE_ON_BLACK;
    ForceDarknessOff = true;
  }

  export function InitElementsGame(): void {
    initElementDefs();
    ForceDarknessOff = false;
  }

  export function InitEditorStatSettings(): void {
    var i: number;

    PlayerDirX = 0;
    PlayerDirY = 0;

    for (i = 0; i <= MAX_ELEMENT; i += 1) {
      World.EditorStatSettings[i] = {
        P1: 4,
        P2: 4,
        P3: 0,
        StepX: 0,
        StepY: -1
      };
    }

    World.EditorStatSettings[E_OBJECT].P1 = 1;
    World.EditorStatSettings[E_BEAR].P1 = 8;
  }
}
