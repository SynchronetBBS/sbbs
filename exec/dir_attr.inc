# DIR_ATTR.INC

# Copyright 1995 Digital Dynamics, All rights reserved.

# Baja include file defining DOS Directory Entry Attribute constants
# Specifically used with the SET_FILE_ATTRIB and GET_FILE_ATTRIB functions

# You man include use this file in your source (!include dir_attr.inc),
# but you may not distribute modified versions of this file. It is not
# suggested that you modify this file AT ALL, but rather create your own
# include file(s) for your own constant definitions.

!define FA_NORMAL   0x00
!define FA_RDONLY   0x01
!define FA_HIDDEN   0x02
!define FA_SYSTEM   0x04
!define FA_LABEL    0x08
!define FA_DIREC    0x10
!define FA_ARCH     0x20

# End of DIR_ATTR.INC
