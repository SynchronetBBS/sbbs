# nodedefs.inc

# Baja include file for NODE.DAB (_NODEFILE) field values

# $id$

# @format.tab-size 8, @format.use-tabs true

					# Possible values for node STATUS
!define NODE_WFC		0	# Waiting for Call
!define NODE_LOGON		1	# at logon prompt
!define NODE_NEWUSER		2	# New user applying
!define NODE_INUSE		3	# In Use
!define NODE_QUIET		4	# In Use - quiet mode
!define NODE_OFFLINE		5	# Offline
!define NODE_NETTING		6	# Networking
!define NODE_EVENT_WAITING	7	# Waiting for all nodes to be inactive
!define NODE_EVENT_RUNNING	8	# Running an external event
!define NODE_EVENT_LIMBO	9	# Allowing another node to run an event

				# Bit values for node MISC
!define NODE_ANON	.0	# Anonymous User
!define NODE_LOCK	.1	# Locked for sysops only
!define NODE_INTR	.2	# Interrupted - hang up
!define NODE_MSGW	.3	# Message is waiting (old way
!define NODE_POFF	.4	# Page disabled
!define NODE_AOFF	.5	# Activity Alert disabled
!define NODE_UDAT	.6	# User data has been updated
!define NODE_RRUN	.7	# Re-run this node when log off
!define NODE_EVENT	.8	# Must run node event after log off
!define NODE_DOWN	.9	# Down this node after logoff
!define NODE_RPCHT	.10	# Reset private chat
!define NODE_NMSG	.11	# Node message waiting (new way
!define NODE_EXT	.12	# Extended info on node action

				# Possible values for node ACTION
!define NODE_MAIN	0	# Main Prompt
!define NODE_RMSG	1	# Reading Messages
!define NODE_RMAL	2	# Reading Mail
!define NODE_SMAL	3	# Sending Mail
!define NODE_RTXT	4	# Reading G-Files
!define NODE_RSML	5	# Reading Sent Mail
!define NODE_PMSG	6	# Posting Message
!define NODE_AMSG	7	# Auto-message
!define NODE_XTRN	8	# Running External Program
!define NODE_DFLT	9	# Main Defaults Section
!define NODE_XFER	10	# Transfer Prompt
!define NODE_DLNG	11	# Downloading File
!define NODE_ULNG	12	# Uploading File
!define NODE_BXFR	13	# Bidirectional Transfer
!define NODE_LFIL	14	# Listing Files
!define NODE_LOGN	15	# Logging on
!define NODE_LCHT	16	# In Local Chat with Sysop
!define NODE_MCHT	17	# In Multi-Chat with Other Nodes
!define NODE_GCHT	18	# In Local Chat with Guru
!define NODE_CHAT	19	# In Chat Section
!define NODE_SYSP	20	# Sysop Activity
!define NODE_TQWK	21	# Transferring QWK packet
!define NODE_PCHT	22	# In Private Chat
!define NODE_PAGE	23	# Paging another node for Private Chat
!define NODE_RFSD	24	# Retrieving file from seq dev (aux=dev

# Record format:
#
# Field 	Length		Description
# -----------------------------------------
# status	1		Current Status of Node
# errors	1		Number of Critical Errors
# action	1		Action User is doing on Node
# useron	2		Number of User on Node
# connection	2		Current Connection (DCE) rate of Node
# misc		2		Miscellaneous bits for node
# aux		2		Auxillary word for node
# extaux	4		Extended aux dword for node

!define NODE_STATUS_OFFSET	0	# Offset to node status
!define NODE_ERRORS_OFFSET	1	# Offset to node errors
!define NODE_ACTION_OFFSET	2	# Offset to node action
!define NODE_USERON_OFFSET	3	# Offest to node useron
!define NODE_CONNECTION_OFFSET	5	# Offset to node connection
!define NODE_MISC_OFFSET	7	# Offset to node misc
!define NODE_AUX_OFFSET 	9	# Offset to node aux
!define NODE_EXTAUX_OFFSET	11	# Offset to node extaux
!define NODE_RECORD_LEN 	15	# Length of node record

# End of NODEDEFS.INC
