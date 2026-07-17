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
  foreign static nameAvailable(name)
  foreign static create(name)
  foreign static copy(source, name)
  foreign static sort()
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
