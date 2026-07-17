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
  foreign static statusMessage(status)
  foreign static entries
  foreign static defaults
  foreign static settings
  foreign static nameAvailable(name)
  foreign static create(name)
  foreign static copy(source, name)
  foreign static sort()
  foreign static screenModes
  // Each build-dependent pair is [numeric value, display name].
  foreign static outputModes
  foreign static cursorStyles
  foreign static audioModes
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
}

class MenuEncryption {
  static none     { 0 }
  static aes      { 6 }
  static chacha20 { 7 }
}

// C-owned editable snapshot of program settings.  Setters affect only
// this snapshot until save(); reload() discards pending changes.
foreign class Settings {
  foreign dirty
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
