# file_io.inc

# Baja include file defining various File I/O related constants

# $id: $

# @format.tab-size 8, @format.use-tabs true

!define O_RDONLY	 1	# Read only
!define O_WRONLY	 2	# Write only
!define O_RDWR		 4	# Read/write

# Flag values for open only

!define O_CREAT     0x0100	# create and open file
!define O_TRUNC     0x0200	# open with truncation
!define O_EXCL	    0x0400	# exclusive open
!define O_APPEND    0x0800	# to end of file

!define O_NOINHERIT 0x80
!define O_DENYALL   0x10
!define O_DENYWRITE 0x20
!define O_DENYREAD  0x30
!define O_DENYNONE  0x40

# Seek (FSET_POS) constants for 'whence'

!define SEEK_SET    0		# Seek from beginning of file
!define SEEK_CUR    1		# Seek from current position
!define SEEK_END    2		# Seek from end of file

# End of FILE_IO.INC
