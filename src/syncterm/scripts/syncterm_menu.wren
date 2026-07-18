// Trusted main-menu data model.  This module is available only in the
// persistent menu VM; connected-session VMs cannot import or bind it.

class MenuReadStatus {
  static ok                           { 0 }
  static passwordRequired             { 1 }
  static decryptFailed                { 2 }
  static readFailed                   { 3 }
  static migrationListOpenFailed      { 4 }
  static migrationIniOpenFailed       { 5 }
  static migrationIniReadFailed       { 6 }
  static migrationListWriteFailed     { 7 }
  static migrationIniWriteFailed      { 8 }
}

class Menu {
  // Transactionally replace the in-memory directory.  On failure the
  // previous generation remains active.  password may be null.
  foreign static load(password)
  foreign static quickConnect(url)
  // A detached Surface snapshot, or null when no completed session exists.
  foreign static offlineScrollback
  foreign static prepareOfflineScrollback()
  foreign static offlineScrollbackHasStatus
  foreign static openUrl(url)
  foreign static openHyperlink(id)
  foreign static setHyperlinkHover(id)
  // Classic main-screen chrome supplied by the trusted host.
  foreign static applicationTitle
  foreign static timeText
  foreign static showEntry(entry)
  foreign static statusMessage(status)
  foreign static entries
  foreign static canAppendEntry
  foreign static defaults
  foreign static settings
  foreign static nameAvailable(name)
  foreign static create(name)
  foreign static copy(source, name)
  foreign static sort()
  // Sort fields are [fieldId, displayName, defaultReversed].  Profiles
  // are [name, signedFieldIdList]; negating an ID toggles its direction.
  foreign static sortFields
  foreign static sortProfiles
  foreign static activeSortProfile
  foreign static saveSortProfiles()
  foreign static setActiveSortProfile(index)
  foreign static addSortProfile(index, name, order)
  foreign static updateSortProfile(index, name, order)
  foreign static deleteSortProfile(index)
  // Catalog rows are [numeric value, display name].
  foreign static connectionTypes
  foreign static defaultPort(connectionType)
  foreign static addressFamilies
  foreign static rates
  // Query a serial device's supported rates, falling back to rates when
  // the device cannot be opened or does not publish a rate catalog.
  foreign static serialRates(device)
  foreign static musicModes
  foreign static ripModes
  foreign static flowControls
  foreign static flowControlsNoRts
  foreign static parities
  // [minimumColorCount, sixteen mode-specific conio default colors].
  foreign static paletteDefaults(screenMode)
  foreign static fontsCatalog
  foreign static logLevels
  foreign static screenModes
  // Each build-dependent pair is [numeric value, display name].
  foreign static outputModes
  foreign static cursorStyles
  foreign static audioModes
  // [category, display name, enabled] rows for this executable.
  foreign static buildOptions
  // Build-specific maximum path length accepted by menu models.
  foreign static maxPathLength
  foreign static encryptionAvailable
  foreign static scalingModes
  foreign static colors
  foreign static backgroundColors
  foreign static currentScreenMode
  foreign static setScreenMode(mode)
  foreign static fileLocations
  foreign static encryptionAlgorithm
  foreign static encryptionKeySize
  foreign static encryptionName
  foreign static setEncryption(algorithm, keySize, password)
  // Web-list rows are [name, URI].  Add/refresh return null on success
  // or an error String; update/delete return Bool.
  foreign static webLists
  foreign static addWebList(name, uri, index)
  foreign static updateWebList(index, uri)
  foreign static deleteWebList(index)
  foreign static refreshWebList(index)
  // Configured custom fonts are C-owned.  Structural changes invalidate
  // existing MenuFont handles; saveFonts() persists and reloads slots.
  foreign static fonts
  foreign static fontsDirty
  foreign static reloadFonts()
  foreign static createFont(name, index)
  foreign static saveFonts()
}

class MenuEncryption {
  static none     { 0 }
  static aes      { 6 }
  static chacha20 { 7 }
}

class MenuFontSlot {
  static eightByEight    { 0 }
  static eightByFourteen { 1 }
  static eightBySixteen  { 2 }
  static twelveByTwenty  { 3 }
}

foreign class MenuFont {
  foreign name
  foreign name=(value)
  // path() is for display only.  setFile() requires a read-authorized
  // File returned by the trusted picker and validates its exact size.
  foreign path(slot)
  foreign setFile(slot, file)
  foreign clearFile(slot)
  foreign delete()
  foreign toString
}

// C-owned editable snapshot of program settings.  apply() updates the
// running program; save() also writes syncterm.ini.
foreign class Settings {
  foreign dirty
  foreign apply()
  foreign save()
  foreign reload()
  foreign confirmClose
  foreign confirmClose=(value)
  foreign promptSave
  foreign promptSave=(value)
  foreign invertWheel
  foreign invertWheel=(value)
  foreign modemDevice
  foreign modemDevice=(value)
  foreign modemInit
  foreign modemInit=(value)
  foreign modemDial
  foreign modemDial=(value)
  foreign listPath
  foreign listPath=(value)
  foreign shellTerm
  foreign shellTerm=(value)
  foreign startupMode
  foreign startupMode=(value)
  foreign outputMode
  foreign outputMode=(value)
  foreign cursorStyle
  foreign cursorStyle=(value)
  foreign audioModes
  foreign audioModes=(value)
  foreign scrollbackLines
  foreign scrollbackLines=(value)
  foreign modemRate
  foreign modemRate=(value)
  foreign scalingMode
  foreign scalingMode=(value)
  foreign kdfShift
  foreign kdfShift=(value)
  foreign customRows
  foreign customRows=(value)
  foreign customColumns
  foreign customColumns=(value)
  foreign customFontHeight
  foreign customFontHeight=(value)
  foreign customAspectWidth
  foreign customAspectWidth=(value)
  foreign customAspectHeight
  foreign customAspectHeight=(value)
  foreign frameColor
  foreign frameColor=(value)
  foreign textColor
  foreign textColor=(value)
  foreign backgroundColor
  foreign backgroundColor=(value)
  foreign inverseColor
  foreign inverseColor=(value)
  foreign lightbarColor
  foreign lightbarColor=(value)
  foreign lightbarBackgroundColor
  foreign lightbarBackgroundColor=(value)
}

// Instances are host-created views of real struct bbslist records.  A
// reload, create, or delete invalidates handles from the older generation.
foreign class BBS {
  foreign name
  foreign rename(name)
  foreign type
  foreign id
  foreign added
  foreign connected
  foreign calls
  foreign connTypeName
  foreign dirty
  foreign save()
  foreign delete()

  foreign addr
  foreign addr=(value)
  foreign port
  foreign port=(value)
  foreign connType
  foreign connType=(value)
  foreign user
  foreign user=(value)
  foreign password
  foreign password=(value)
  foreign syspass
  foreign syspass=(value)
  foreign music
  foreign music=(value)
  foreign rip
  foreign rip=(value)
  foreign comment
  foreign comment=(value)
  foreign addressFamily
  foreign addressFamily=(value)
  foreign termName
  foreign termName=(value)
  foreign screenMode
  foreign screenMode=(value)
  foreign bpsRate
  foreign bpsRate=(value)
  foreign font
  foreign font=(value)
  foreign noStatus
  foreign noStatus=(value)
  foreign hidePopups
  foreign hidePopups=(value)
  foreign yellowIsYellow
  foreign yellowIsYellow=(value)
  foreign forceLcf
  foreign forceLcf=(value)
  foreign dlDir
  foreign dlDir=(value)
  foreign ulDir
  foreign ulDir=(value)
  foreign logFile
  foreign logFile=(value)
  foreign appendLogFile
  foreign appendLogFile=(value)
  foreign xferLogLevel
  foreign xferLogLevel=(value)
  foreign telnetLogLevel
  foreign telnetLogLevel=(value)
  foreign stopBits
  foreign stopBits=(value)
  foreign dataBits
  foreign dataBits=(value)
  foreign parity
  foreign parity=(value)
  foreign flowControl
  foreign flowControl=(value)
  foreign telnetNoBinary
  foreign telnetNoBinary=(value)
  foreign deferTelnetNegotiation
  foreign deferTelnetNegotiation=(value)
  foreign ghostProgram
  foreign ghostProgram=(value)
  foreign sftpPublicKey
  foreign sftpPublicKey=(value)
  foreign sshAllowAes128Cbc
  foreign sshAllowAes128Cbc=(value)
  foreign sshAcceptEarlyData
  foreign sshAcceptEarlyData=(value)
  foreign palette
  foreign palette=(value)
  foreign sortOrder
  foreign sortOrder=(value)
  foreign lfExpand
  foreign lfExpand=(value)
  foreign toString
}
