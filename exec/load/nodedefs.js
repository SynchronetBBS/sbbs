/* nodedefs.js */

								/********************************************/
 								/* Legal values for Node.status				*/
								/********************************************/
NODE_WFC			=0			/* Waiting for Call							*/
NODE_LOGON          =1			/* at logon prompt							*/
NODE_NEWUSER        =2			/* New user applying						*/
NODE_INUSE          =3			/* In Use									*/
NODE_QUIET          =4			/* In Use - quiet mode						*/
NODE_OFFLINE        =5			/* Offline									*/
NODE_NETTING        =6			/* Networking								*/
NODE_EVENT_WAITING  =7			/* Waiting for all nodes to be inactive		*/
NODE_EVENT_RUNNING  =8			/* Running an external event				*/
NODE_EVENT_LIMBO    =9			/* Allowing another node to run an event	*/		
NODE_LAST_STATUS	=10			/* Must be last								*/
								/********************************************/

								/********************************************/
var NodeStatus		=[			/* Node.status value descriptions			*/
	 "Waiting for call"			/********************************************/
	,"At logon prompt"
	,"New user"
	,"In-use"
	,"Waiting for call"
	,"Offline"
	,"Networking"
	,"Waiting for all nodes to become inactive"
	,"Running external event"
	,"Waiting for node %d to finish external event"
	];			

								/********************************************/
								/* Legal values for Node.action				*/
								/********************************************/
NODE_MAIN			=0          /* Main Prompt								*/
NODE_RMSG           =1          /* Reading Messages							*/
NODE_RMAL           =2          /* Reading Mail								*/
NODE_SMAL           =3          /* Sending Mail								*/
NODE_RTXT           =4          /* Reading G-Files							*/
NODE_RSML           =5          /* Reading Sent Mail						*/
NODE_PMSG           =6          /* Posting Message							*/
NODE_AMSG           =7          /* Auto-message								*/
NODE_XTRN           =8          /* Running External Program					*/
NODE_DFLT           =9          /* Main Defaults Section					*/
NODE_XFER           =10         /* Transfer Prompt							*/
NODE_DLNG           =11         /* Downloading File							*/
NODE_ULNG           =12         /* Uploading File							*/
NODE_BXFR           =13         /* Bidirectional Transfer					*/
NODE_LFIL           =14         /* Listing Files							*/
NODE_LOGN           =15         /* Logging on								*/
NODE_LCHT           =16         /* In Local Chat with Sysop					*/
NODE_MCHT           =17         /* In Multi-Chat with Other Nodes			*/
NODE_GCHT           =18         /* In Local Chat with Guru					*/
NODE_CHAT           =19         /* In Chat Section							*/
NODE_SYSP           =20         /* Sysop Activity							*/
NODE_TQWK           =21         /* Transferring QWK packet					*/
NODE_PCHT           =22         /* In Private Chat							*/
NODE_PAGE           =23         /* Paging another node for Private Chat		*/
NODE_RFSD           =24         /* Retrieving file from seq dev (aux=dev)	*/
NODE_LAST_ACTION	=25			/* Must be last								*/
								/********************************************/

								/********************************************/
var NodeAction		=[			/* Node.action value descriptions			*/
	 "at main menu"				/********************************************/
	,"reading messages"
	,"reading mail"
	,"sending mail"
	,"reading text files"
	,"reading sent mail"
	,"posting message"
	,"posting auto-message"
	,"at external program menu"
	,"changing defaults"
	,"at transfer menu"
	,"downloading"
	,"uploading"
	,"transferring bidirectional"
	,"listing files"
	,"logging on"
	,"in local chat with %s"
	,"in multinode chat"
	,"chatting with %s"
	,"in chat section"
	,"performing sysop activities"
	,"transferring QWK packet"
	,"in private chat with node %u"
	,"paging node %u for private chat"
	,"retrieving from device #%d"
	];

								/********************************************/
                                /* Bit values for Node.misc					*/
								/********************************************/
NODE_ANON			=(1<<0)     /* Anonymous User							*/
NODE_LOCK   		=(1<<1)     /* Locked for sysops only					*/
NODE_INTR   		=(1<<2)     /* Interrupted - hang up					*/
NODE_MSGW   		=(1<<3)     /* Message is waiting (old way)				*/
NODE_POFF   		=(1<<4)     /* Page disabled							*/
NODE_AOFF   		=(1<<5)     /* Activity Alert disabled					*/
NODE_UDAT   		=(1<<6)     /* User data has been updated				*/
NODE_RRUN   		=(1<<7)     /* Re-run this node when log off			*/
NODE_EVENT  		=(1<<8)     /* Must run node event after log off		*/
NODE_DOWN   		=(1<<9)     /* Down this node after logoff				*/
NODE_RPCHT  		=(1<<10)    /* Reset private chat						*/
NODE_NMSG   		=(1<<11)    /* Node message waiting (new way)			*/
NODE_EXT    		=(1<<12)    /* Extended info on node action				*/
NODE_LCHAT			=(1<<13)	/* Being pulled into local chat				*/




