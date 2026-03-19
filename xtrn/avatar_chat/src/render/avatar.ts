export interface AvatarCache {
  [key: string]: string | null;
}

export interface AvatarDimensions {
  width: number;
  height: number;
}

interface RouteMap {
  [qwkid: string]: string[];
}

let routeMapCache: RouteMap | null = null;

export function resolveAvatarDimensions(avatarLib: AvatarLibrary | null): AvatarDimensions {
  if (avatarLib && avatarLib.defs) {
    return {
      width: avatarLib.defs.width || 10,
      height: avatarLib.defs.height || 6
    };
  }

  return {
    width: 10,
    height: 6
  };
}

function avatarCacheKey(nick: ChatNick, ownAlias: string): string {
  const embeddedAvatar = nick.avatar ? String(nick.avatar).replace(/^\s+|\s+$/g, "") : "";
  const remoteKey = resolveAvatarNetaddr(nick) || nick.host || "local";

  if (embeddedAvatar.length) {
    return nick.name.toUpperCase() + "@EMBEDDED:" + md5_calc(embeddedAvatar);
  }

  if (nick.name.toUpperCase() === ownAlias.toUpperCase()) {
    return "local:" + nick.name.toUpperCase();
  }
  return nick.name.toUpperCase() + "@" + String(remoteKey).toUpperCase();
}

function normalizeQwkId(qwkid: string | undefined): string | null {
  if (!qwkid) {
    return null;
  }

  qwkid = String(qwkid);
  if (!qwkid.length) {
    return null;
  }

  return qwkid.toUpperCase();
}

function resolveAvatarNetaddr(nick: ChatNick): string | null {
  const qwkid = normalizeQwkId(nick.qwkid);

  if (qwkid) {
    return qwkid;
  }

  return nick.host || null;
}

function pushLookupCandidate(list: string[], seen: { [key: string]: boolean }, value: string | null | undefined): void {
  const normalized = value ? String(value).replace(/^\s+|\s+$/g, "").toUpperCase() : "";

  if (!normalized.length || seen[normalized]) {
    return;
  }

  seen[normalized] = true;
  list.push(normalized);
}

function loadRouteMap(): RouteMap {
  const map: RouteMap = {};
  const routeFile = new File(system.data_dir + "qnet/route.dat");

  if (routeMapCache) {
    return routeMapCache;
  }

  if (!routeFile.open("r")) {
    routeMapCache = map;
    return routeMapCache;
  }

  try {
    while (!routeFile.eof) {
      const line = routeFile.readln(2048);
      const match = line ? line.match(/^\s*\d{2}\/\d{2}\/\d{2}\s+([^:\s]+):([^:\s]+)/) : null;
      const qwkid = match ? normalizeQwkId(match[1]) : null;
      const route = match ? normalizeQwkId(match[2]) : null;

      if (!qwkid || !route || qwkid === route) {
        continue;
      }

      if (!map[qwkid]) {
        map[qwkid] = [];
      }

      if (map[qwkid].indexOf(route) === -1) {
        map[qwkid].push(route);
      }
    }
  } finally {
    routeFile.close();
  }

  routeMapCache = map;
  return routeMapCache;
}

function resolveAvatarLookupCandidates(nick: ChatNick): string[] {
  const candidates: string[] = [];
  const seen: { [key: string]: boolean } = {};
  const qwkid = normalizeQwkId(nick.qwkid);
  const routeMap = qwkid ? loadRouteMap() : null;
  let index = 0;

  pushLookupCandidate(candidates, seen, qwkid);

  if (qwkid && routeMap && routeMap[qwkid]) {
    for (index = 0; index < routeMap[qwkid].length; index += 1) {
      pushLookupCandidate(candidates, seen, routeMap[qwkid][index]);
    }
  }

  pushLookupCandidate(candidates, seen, nick.host || null);
  return candidates;
}

function lookupRemoteAvatar(avatarLib: AvatarLibrary, nick: ChatNick, bbsId: string | null): AvatarObject | null {
  const candidates = resolveAvatarLookupCandidates(nick);
  let avatarObj: AvatarObject | false | null | undefined = null;
  let index = 0;
  let nameHash = "";

  if (typeof avatarLib.read_netuser !== "function") {
    for (index = 0; index < candidates.length; index += 1) {
      avatarObj = avatarLib.read(0, nick.name, candidates[index], bbsId) || null;
      if (avatarObj && !avatarObj.disabled && avatarObj.data) {
        return avatarObj;
      }
    }
    return null;
  }

  nameHash = "md5:" + md5_calc(nick.name);
  for (index = 0; index < candidates.length; index += 1) {
    avatarObj = avatarLib.read_netuser(nick.name, candidates[index]) || null;
    if (!avatarObj && nameHash.length) {
      avatarObj = avatarLib.read_netuser(nameHash, candidates[index]) || null;
    }
    if (avatarObj && !avatarObj.disabled && avatarObj.data) {
      return avatarObj;
    }
  }

  return null;
}

export function lookupAvatarBinary(
  avatarLib: AvatarLibrary | null,
  nick: ChatNick | null,
  ownAlias: string,
  ownUserNumber: number,
  cache: AvatarCache
): string | null {
  let cached;
  let avatarObj;
  let decoded;
  let localUserNumber;
  let localQwkId;
  let nickQwkId;
  let netaddr;
  let key = "";

  if (!avatarLib || !nick || !nick.name) {
    return null;
  }

  key = avatarCacheKey(nick, ownAlias);
  cached = cache[key];
  if (cached !== undefined) {
    return cached;
  }

  avatarObj = null;

  try {
    localQwkId = normalizeQwkId(system.qwk_id);
    nickQwkId = normalizeQwkId(nick.qwkid);
    netaddr = resolveAvatarNetaddr(nick);

    if (nick.avatar && String(nick.avatar).replace(/^\s+|\s+$/g, "").length) {
      avatarObj = {
        data: String(nick.avatar).replace(/^\s+|\s+$/g, "")
      } as AvatarObject;
    } else if (nick.name.toUpperCase() === ownAlias.toUpperCase()) {
      avatarObj = avatarLib.read(ownUserNumber, ownAlias, null, null) || null;
    } else {
      localUserNumber = 0;

      if (!nickQwkId || nickQwkId === localQwkId) {
        localUserNumber = system.matchuser(nick.name);
      }

      if (localUserNumber > 0) {
        avatarObj = avatarLib.read(localUserNumber, nick.name, null, null) || null;
      } else {
        avatarObj = lookupRemoteAvatar(avatarLib, nick, nick.host || null);
        if (!avatarObj) {
          avatarObj = avatarLib.read(0, nick.name, netaddr, nick.host || null) || null;
        }
      }
    }
  } catch (_error) {
    avatarObj = null;
  }

  if (!avatarObj || avatarObj.disabled || !avatarObj.data) {
    cache[key] = null;
    return null;
  }

  try {
    decoded = base64_decode(avatarObj.data);
  } catch (_decodeError) {
    decoded = null;
  }

  decoded = decoded || null;
  cache[key] = decoded;
  return decoded;
}

export function blitAvatarToFrame(
  frame: Frame,
  avatarBinary: string,
  avatarWidth: number,
  avatarHeight: number,
  x: number,
  y: number
): void {
  let offset = 0;
  let row = 0;

  for (row = 0; row < avatarHeight; row += 1) {
    let column = 0;

    for (column = 0; column < avatarWidth; column += 1) {
      let ch = "";
      let attr = 0;

      if (x + column > frame.width || y + row > frame.height) {
        offset += 2;
        continue;
      }

      if (offset + 1 >= avatarBinary.length) {
        return;
      }

      ch = avatarBinary.substr(offset, 1);
      attr = ascii(avatarBinary.substr(offset + 1, 1));
      frame.setData(x + column - 1, y + row - 1, ch, attr, false);
      offset += 2;
    }
  }
}
