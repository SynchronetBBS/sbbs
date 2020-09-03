{ Synchronet User Editor (Delphi 5 for Win32 project) }

{ $Id: MainFormUnit.pas,v 1.12 2020/03/31 02:06:20 rswindell Exp $ }

{****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************}

{ User Data function Notes:

    The Tag member of the Edit boxes is used to indicate modified data.
    GetUserXxxx() calls GetXxxxField() sets the Tag to 0.
    PutUserXxxx() only gets PutXxxxField() if Tag = 1.
    GetXxxxField() and PutXxxxField() do not care about TEdits or Tags.
}

unit MainFormUnit;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, Menus, ComCtrls, ExtCtrls, Buttons, ActnList, ImgList, CheckLst,
  ToolWin, Registry;

type
  TForm1 = class(TForm)
    MainMenu: TMainMenu;
    FileMenuItem: TMenuItem;
    Panel: TPanel;
    FileExitMenuItem: TMenuItem;
    FileDeleteMenuItem: TMenuItem;
    PageControl: TPageControl;
    PersonalTabSheet: TTabSheet;
    Label2: TLabel;
    NameEdit: TEdit;
    Label1: TLabel;
    ComputerEdit: TEdit;
    NetmailAddress: TLabel;
    NetmailEdit: TEdit;
    Label7: TLabel;
    NoteEdit: TEdit;
    HandleLabel: TLabel;
    HandleEdit: TEdit;
    Label5: TLabel;
    PasswordEdit: TEdit;
    Label6: TLabel;
    BirthdateEdit: TEdit;
    Label8: TLabel;
    ModemEdit: TEdit;
    Label16: TLabel;
    SexEdit: TEdit;
    Label18: TLabel;
    PhoneEdit: TEdit;
    SecurityTabSheet: TTabSheet;
    Label10: TLabel;
    LevelEdit: TEdit;
    Label11: TLabel;
    ExpireEdit: TEdit;
    StatsTabSheet: TTabSheet;
    TopPanel: TPanel;
    NumberLabel: TLabel;
    NumberEdit: TEdit;
    TotalStaticText: TStaticText;
    AliasLabel: TLabel;
    AliasEdit: TEdit;
    Label24: TLabel;
    CommentEdit: TEdit;
    FileNewUserMenuItem: TMenuItem;
    Label25: TLabel;
    ExemptionsEdit: TEdit;
    Label26: TLabel;
    RestrictionsEdit: TEdit;
    Label27: TLabel;
    CreditsEdit: TEdit;
    Label28: TLabel;
    MinutesEdit: TEdit;
    Label29: TLabel;
    FreeCreditsEdit: TEdit;
    FileSaveUserMenuItem: TMenuItem;
    ActionList: TActionList;
    SaveUser: TAction;
    NewUser: TAction;
    RefreshUser: TAction;
    ImageList: TImageList;
    DeleteUser: TAction;
    FileRefreshUserMenuItem: TMenuItem;
    GroupBox1: TGroupBox;
    TimeOnEdit: TEdit;
    Label22: TLabel;
    Label23: TLabel;
    TimeOnTodayEdit: TEdit;
    Label30: TLabel;
    ExtraTimeEdit: TEdit;
    Label31: TLabel;
    LastCallTimeEdit: TEdit;
    GroupBox2: TGroupBox;
    Label20: TLabel;
    LogonsEdit: TEdit;
    Label21: TLabel;
    LogonsTodayEdit: TEdit;
    GroupBox3: TGroupBox;
    Label17: TLabel;
    FirstOnEdit: TEdit;
    Label19: TLabel;
    LastOnEdit: TEdit;
    GroupBox4: TGroupBox;
    Label32: TLabel;
    PostsTotalEdit: TEdit;
    Label33: TLabel;
    PostsTodayEdit: TEdit;
    GroupBox5: TGroupBox;
    Label34: TLabel;
    Label35: TLabel;
    EmailTotalEdit: TEdit;
    EmailTodayEdit: TEdit;
    Label36: TLabel;
    FeedbackEdit: TEdit;
    GroupBox6: TGroupBox;
    Label37: TLabel;
    UploadedFilesEdit: TEdit;
    Label38: TLabel;
    UploadedBytesEdit: TEdit;
    GroupBox7: TGroupBox;
    Label39: TLabel;
    Label40: TLabel;
    DownloadedFilesEdit: TEdit;
    DownloadedBytesEdit: TEdit;
    GroupBox8: TGroupBox;
    Label12: TLabel;
    Flags1Edit: TEdit;
    Label13: TLabel;
    Flags2Edit: TEdit;
    Label14: TLabel;
    Flags3Edit: TEdit;
    Label15: TLabel;
    Flags4Edit: TEdit;
    GroupBox9: TGroupBox;
    AddressEdit: TEdit;
    LocationEdit: TEdit;
    ZipCodeEdit: TEdit;
    DeactivateUser: TAction;
    N1: TMenuItem;
    FileDeactivateUserMenuItem: TMenuItem;
    SettingsTabSheet: TTabSheet;
    GroupBox10: TGroupBox;
    Label3: TLabel;
    RowsEdit: TEdit;
    GroupBox11: TGroupBox;
    GroupBox13: TGroupBox;
    GroupBox15: TGroupBox;
    ShellEdit: TEdit;
    ExpertCheckBox: TCheckBox;
    ChatCheckListBox: TCheckListBox;
    TerminalCheckListBox: TCheckListBox;
    LogonCheckListBox: TCheckListBox;
    ToolBar: TToolBar;
    ToolButton1: TToolButton;
    ToolButton2: TToolButton;
    ToolButton3: TToolButton;
    ToolButton4: TToolButton;
    ToolButton5: TToolButton;
    FindEdit: TEdit;
    ToolButton6: TToolButton;
    FindNextButton: TButton;
    FindButton: TButton;
    ScrollBar: TScrollBar;
    Label42: TLabel;
    LeechEdit: TEdit;
    Status: TEdit;
    ToolButton7: TToolButton;
    MsgFileSettingsTabSheet: TTabSheet;
    GroupBox16: TGroupBox;
    Label41: TLabel;
    MessageCheckListBox: TCheckListBox;
    EditorEdit: TEdit;
    GroupBox12: TGroupBox;
    Label4: TLabel;
    Label9: TLabel;
    TempFileExtEdit: TEdit;
    ProtocolEdit: TEdit;
    FileCheckListBox: TCheckListBox;
    GroupBox14: TGroupBox;
    QWKCheckListBox: TCheckListBox;
    ExtendedCommentTabSheet: TTabSheet;
    Memo: TMemo;
    procedure FormShow(Sender: TObject);
    procedure ScrollBarChange(Sender: TObject);
    procedure NumberEditKeyPress(Sender: TObject; var Key: Char);
    procedure FileExitMenuItemClick(Sender: TObject);
    procedure EditChange(Sender: TObject);
    procedure SaveUserExecute(Sender: TObject);
    procedure NewUserExecute(Sender: TObject);
    procedure DeleteUserExecute(Sender: TObject);
    procedure FormClose(Sender: TObject; var Action: TCloseAction);
    procedure DeactivateUserExecute(Sender: TObject);
    procedure FindButtonClick(Sender: TObject);
    procedure FindEditKeyPress(Sender: TObject; var Key: Char);
  private
    { Private declarations }
  public
    { Public declarations }
    data_dir: AnsiString;
    users: Integer;
    user_misc: Integer;
    user_chat: Integer;
    user_qwk: Integer;
    function LastUser: Integer;
    procedure GetUserData(usernumber: Integer);
    procedure PutUserData(usernumber: Integer);
  end;

var
  Form1: TForm1;

implementation

const ETX=#3; { End Of Text character, all fields use for padding/terminator }
const CR=#13; { Carriage-return }
const LF=#10; { Line-feed }

const					    { String lengths					   	    }
    LEN_ALIAS		=25;	{ User alias						   	    }
    LEN_NAME		=25;	{ User name								    }
    LEN_HANDLE		=8; 	{ User chat handle 						    }
    LEN_NOTE		=30;	{ User note								    }
    LEN_COMP		=30;	{ User computer description				    }
    LEN_COMMENT 	=60;	{ User comment 							    }
    LEN_NETMAIL 	=60;	{ NetMail forwarding address		   	    }
    LEN_OLDPASS		=8;	    { User password							    }
    LEN_PHONE		=12;	{ User phone number						    }
    LEN_BIRTH		=8;	    { Birthday in xx/xx/YY format		   	    }
    LEN_ADDRESS 	=30;	{ User address 							    }
    LEN_LOCATION	=30;	{ Location (City, State)			   	    }
    LEN_ZIPCODE 	=10;	{ Zip/Postal code					   	    }
    LEN_MODEM		=8;	    { User modem type description		   	    }
    LEN_FDESC		=58;	{ File description 						    }
    LEN_FCDT		=9;	    { 9 digits for file credit values	   	    }
    LEN_TITLE		=70;	{ Message title							    }
    LEN_MAIN_CMD	=40;	{ unused }
    LEN_PASS		=40;	{ password }
    LEN_SCAN_CMD	=35;	{ unused }
    LEN_IPADDR		=45;
    LEN_CID 		=25;	{ Caller ID (phone number) 				    }
    LEN_ARSTR		=40;	{ Max length of Access Requirement string	}
    LEN_CHATACTCMD	=9;	    { Chat action command				   	    }
    LEN_CHATACTOUT	=65;	{ Chat action output string				    }


{ This is a list of offsets into the USER.DAT file for different variables
  that are stored (for each user) }
const
    U_ALIAS 	=0;			                { Offset to alias }
    U_NAME		=U_ALIAS+LEN_ALIAS;         { Offset to name }
    U_HANDLE	=U_NAME+LEN_NAME;
    U_NOTE		=U_HANDLE+LEN_HANDLE+2;
    U_COMP		=U_NOTE+LEN_NOTE;
    U_COMMENT	=U_COMP+LEN_COMP+2;

    U_NETMAIL	=U_COMMENT+LEN_COMMENT+2;

    U_ADDRESS	=U_NETMAIL+LEN_NETMAIL+2;
    U_LOCATION	=U_ADDRESS+LEN_ADDRESS;
    U_ZIPCODE	=U_LOCATION+LEN_LOCATION;

    U_OLDPASS	=U_ZIPCODE+LEN_ZIPCODE+2;
    U_PHONE  	=U_OLDPASS+LEN_OLDPASS; { Offset to phone-number }
    U_BIRTH  	=U_PHONE+12; 		    { Offset to users birthday	}
    U_MODEM     =U_BIRTH+8;
    U_LASTON	=U_MODEM+8;
    U_FIRSTON	=U_LASTON+8;
    U_EXPIRE    =U_FIRSTON+8;
    U_PWMOD     =U_EXPIRE+8;

    U_LOGONS    =U_PWMOD+8+2;
    U_LTODAY    =U_LOGONS+5;
    U_TIMEON    =U_LTODAY+5;
    U_TEXTRA  	=U_TIMEON+5;
    U_TTODAY    =U_TEXTRA+5;
    U_TLAST     =U_TTODAY+5;
    U_POSTS     =U_TLAST+5;
    U_EMAILS    =U_POSTS+5;
    U_FBACKS    =U_EMAILS+5;
    U_ETODAY	=U_FBACKS+5;
    U_PTODAY	=U_ETODAY+5;

    U_ULB       =U_PTODAY+5+2;
    U_ULS       =U_ULB+10;
    U_DLB       =U_ULS+5;
    U_DLS       =U_DLB+10;
    U_CDT		=U_DLS+5;
    U_MIN		=U_CDT+10;

    U_LEVEL 	=U_MIN+10+2; 	{ Offset to Security Level    }
    U_FLAGS1	=U_LEVEL+2;  	{ Offset to Flags }
    U_TL		=U_FLAGS1+8; 	{ Offset to unused field }
    U_FLAGS2	=U_TL+2;
    U_EXEMPT	=U_FLAGS2+8;
    U_REST		=U_EXEMPT+8;
    U_ROWS		=U_REST+8+2; 	{ Number of Rows on user's monitor }
    U_SEX		=U_ROWS+2; 		{ Sex, Del, ANSI, color etc.	   }
    U_MISC		=U_SEX+1; 		{ Miscellaneous flags in 8-byte hex }
    U_OLDXEDIT	=U_MISC+8; 		{ External editor (Version 1 method)  }
    U_LEECH 	=U_OLDXEDIT+2; 	{ two hex digits - leech attempt count }
    U_CURSUB	=U_LEECH+2;  	{ Current sub (internal code  }
    U_CURDIR	=U_CURSUB+8; 	{ Current dir (internal code  }
    U_CMDSET	=U_CURDIR+8; 	{ unused }
    U_MAIN_CMD	=U_CMDSET+2+2; 	{ unused }
    U_PASS		=U_MAIN_CMD+LEN_MAIN_CMD; 		{ password }
    U_SCAN_CMD	=U_PASS+LEN_PASS+2;  			{ unused }
    U_IPADDR	=U_SCAN_CMD+LEN_SCAN_CMD; 		{ unused }
    U_FREECDT	=U_IPADDR+LEN_IPADDR+2;
    U_FLAGS3	=U_FREECDT+10; 	{ Flag set #3 }
    U_FLAGS4	=U_FLAGS3+8;	{ Flag set #4 }
    U_XEDIT 	=U_FLAGS4+8; 	{ External editor (code)  }
    U_SHELL 	=U_XEDIT+8;  	{ Command shell (code)  }
    U_QWK		=U_SHELL+8;  	{ QWK settings }
    U_TMPEXT	=U_QWK+8; 		{ QWK extension }
    U_CHAT		=U_TMPEXT+3; 	{ Chat settings }
    U_NS_TIME	=U_CHAT+8; 		{ New-file scan date/time }
    U_PROT		=U_NS_TIME+8; 	{ Default transfer protocol }
    U_UNUSED	=U_PROT+1;
    U_LEN		=(U_UNUSED+28+2);

{ Bit values for user_misc 		}
const
    DELETED 	=(1 shl 0);         { Deleted user }
    ANSI		=(1 shl 1); 		{ Supports ANSI terminal emulation  }
    UCOLOR		=(1 shl 2); 		{ Send color codes 				    }
    RIP 		=(1 shl 3); 		{ Supports RIP terminal emulation	}
    UPAUSE		=(1 shl 4); 		{ Pause on every screen full		}
    SPIN		=(1 shl 5); 		{ Spinning cursor - Same as K_SPIN  }
    INACTIVE	=(1 shl 6); 		{ Inactive user slot				}
    EXPERT		=(1 shl 7); 		{ Expert menu mode 				    }
    ANFSCAN 	=(1 shl 8); 		{ Auto New file scan				}
    CLRSCRN 	=(1 shl 9); 		{ Clear screen before each message  }
    QUIET		=(1 shl 10);		{ Quiet mode upon logon			    }
    BATCHFLAG	=(1 shl 11);		{ File list allow batch dl flags	}
    NETMAIL 	=(1 shl 12);		{ Forward e-mail to fidonet addr	}
    CURSUB		=(1 shl 13);		{ Remember current sub-board/dir	}
    ASK_NSCAN	=(1 shl 14);		{ Ask for newscanning upon logon	}
    NO_EXASCII	=(1 shl 15);		{ Don't send extended ASCII         }
    ASK_SSCAN	=(1 shl 16);		{ Ask for messages to you at logon  }
    AUTOTERM	=(1 shl 17);		{ Autodetect terminal type 		    }
    COLDKEYS	=(1 shl 18);		{ No hot-keys						}
    EXTDESC 	=(1 shl 19);		{ Extended file descriptions		}
    AUTOHANG	=(1 shl 20);		{ Auto-hang-up after transfer		}
    WIP 		=(1 shl 21);		{ Supports WIP terminal emulation	}
    AUTOLOGON	=(1 shl 22);		{ AutoLogon via IP					}
    HTML		=(1 shl 23);		{ Using Zuul/HTML terminal				}
    NOPAUSESPIN	=(1 shl 24);		{ No spinning cursor at pause prompt	}
    CTERM_FONTS	=(1 shl 25);		{ Loadable fonts are supported			}
    PETSCII		=(1 shl 26);		{ Commodore PET/CBM terminal			}
    SWAP_DELETE	=(1 shl 27);		{ Swap Delete and Backspace keys		}
    ICE_COLOR	=(1 shl 28);		{ Bright background color support		}

{ Bit values for user_chat 		}
const
    CHAT_ECHO	=(1 shl 0);	{ Multinode chat echo						}
    CHAT_ACTION =(1 shl 1);	{ Chat actions 							    }
    CHAT_NOPAGE =(1 shl 2);	{ Can't be paged                            }
    CHAT_NOACT	=(1 shl 3);	{ No activity alerts						}
    CHAT_SPLITP =(1 shl 4);	{ Split screen private chat				    }

{ Bits for user_qwk }
const
    QWK_FILES	=(1 shl 0); 		{ Include new files list			}
    QWK_EMAIL	=(1 shl 1); 		{ Include unread e-mail			    }
    QWK_ALLMAIL =(1 shl 2); 		{ Include ALL e-mail				}
    QWK_DELMAIL =(1 shl 3); 		{ Delete e-mail after download 	    }
    QWK_BYSELF	=(1 shl 4); 		{ Include messages from self		}
    QWK_UNUSED	=(1 shl 5); 		{ Currently unused 				    }
    QWK_EXPCTLA =(1 shl 6); 		{ Expand ctrl-a codes to ascii 	    }
    QWK_RETCTLA =(1 shl 7); 		{ Retain ctrl-a codes				}
    QWK_ATTACH	=(1 shl 8); 		{ Include file attachments 		    }
    QWK_NOINDEX =(1 shl 9); 		{ Do not create index files in QWK  }
    QWK_TZ		=(1 shl 10);		{ Include "@TZ" time zone in msgs   }
    QWK_VIA 	=(1 shl 11);		{ Include "@VIA" seen-bys in msgs   }
    QWK_NOCTRL	=(1 shl 12);		{ No extraneous control files		}

{$R *.DFM}

{ Returns total number of users in database }
function TForm1.LastUser: Integer;
var Str: AnsiString;
    f: TFileStream;
begin
    Str:=data_dir+'user/user.dat';
    try
        f:=TFileStream.Create(Str,fmOpenRead or fmShareDenyNone);
        Result := f.Size div U_LEN;
        f.Free;
    except
        Result := 0;
    end;
end;

procedure SaveChanges;
begin
    if Application.MessageBox('Save Changes','User Modified',MB_YESNO)=IDYES
    then
        Form1.SaveUserExecute(Form1)
    else
        Form1.GetUserData(Form1.ScrollBar.Position);
end;

{ ************* }
{ GET USER DATA }
{ ************* }

{ Parses a single text data field }
function GetTextField(buf : PChar; maxlen : Integer): AnsiString;
var str: AnsiString;
    len: Integer;
    term: PChar;
begin
    term:=StrScan(buf,ETX);      { Look for end-of-text marker }
    if term = nil then
        len:=maxlen   { not found? }
    else
        len:=Term-buf;
    if len > maxlen then len:=maxlen;
    SetString(str,buf,len);
    Result:=str;
end;

{ Get a 16-bit decimal integer field }
function GetShortIntField(buf : PChar): AnsiString;
begin
    Result:=GetTextField(buf,5);
end;

{ Get a 32-bit decimal integer field }
function GetLongIntField(buf : PChar): AnsiString;
begin
    Result:=GetTextField(buf,10);
end;

{ Parses a flag field returning string with flag letters and spaces }
{ This function can be used for exemptions and restrictions too }
function GetFlagsField(buf : PChar): AnsiString;
var str: AnsiString;
    flagstr: AnsiString;
    flags: Integer;
    i: Integer;
begin
    str:='0x'+GetTextField(buf,8);
    flags:=StrToIntDef(str,0);
    for i:=0 to 25 do
        if flags AND (1 shl i) <> 0 then
            flagstr:=flagstr+Char(65+i)
        else
            flagstr:=flagstr+' ';
    Result:=TrimRight(flagstr);
end;

{ Get a hexadecimal integer field (of any size) }
function GetHexField(buf : PChar; maxlen : Integer): Integer;
var str: AnsiString;
begin
    str:='0x'+GetTextField(buf,maxlen);
    Result:=StrToIntDef(str,0);
end;

{ Convert a hexadecimal time field (in unix format) to xx/xx/YY }
function GetDateField(buf : PChar): AnsiString;
var str: AnsiString;
    date: TDateTime;
    time: Integer;
begin
    str:='0x'+GetTextField(buf,8);
    time:=StrToIntDef(str,0);
    if time=0 then begin
        Result:='00/00/00';
        Exit;
    end;
    time:=time div (24*60*60);          { convert from seconds to days }
    date:=EncodeDate(1970,1,1)+time;   { convert to days since 1970 }
    Result:=DateToStr(date);
end;

{ Reads a user data record to an Edit box and clears modified flag }
function GetUserText(Edit: Tedit; buf: PChar; maxlen: Integer) : AnsiString;
begin
    Edit.Text:=GetTextField(buf,maxlen);
    Edit.Tag:=0; { clear modified flag }
    Edit.MaxLength:=maxlen;
    Result:=Edit.Text;
end;

{ Reads a user data record to an Edit box and clears modified flag }
function GetUserShortInt(Edit: Tedit; buf: PChar) : AnsiString;
begin
    GetUserText(Edit,buf,5);
    Result:=Edit.Text;
end;

{ Reads a user data record to an Edit box and clears modified flag }
function GetUserLongInt(Edit: Tedit; buf: PChar) : AnsiString;
begin
    GetUserText(Edit,buf,10);
    Result:=Edit.Text;
end;

{ Reads and parses a single user record, filling in edit boxes, etc. }
procedure TForm1.GetUserData(usernumber: Integer);
var Str: AnsiString;
    f: TFileStream;
    buf: array[0..U_LEN] of Char;
begin
    { Open file and read user record }
    Str:=data_dir+'user/user.dat';
    try
        f:=TFileStream.Create(Str,fmOpenRead or fmShareDenyNone);
    except
        Exit;
    end;
    f.Seek((usernumber-1)*U_LEN,soFromBeginning);
    f.Read(buf,U_LEN);
    f.Free;

    { ********************** }
    { Parse user data buffer }
    { ********************** }

    { Personal }
    GetUserText(AliasEdit,buf+U_ALIAS,LEN_ALIAS);
    GetUserText(NameEdit,buf+U_NAME,LEN_NAME);
    GetUserText(HandleEdit,buf+U_HANDLE,LEN_HANDLE);
    GetUserText(ComputerEdit,buf+U_COMP,LEN_COMP);
    GetUserText(NetMailEdit,buf+U_NETMAIL,LEN_NETMAIL);
    GetUserText(AddressEdit,buf+U_ADDRESS,LEN_ADDRESS);
    GetUserText(LocationEdit,buf+U_LOCATION,LEN_LOCATION);
    GetUserText(NoteEdit,buf+U_NOTE,LEN_NOTE);
    GetUserText(BirthDateEdit,buf+U_BIRTH,LEN_BIRTH);
    GetUserText(PasswordEdit,buf+U_PASS,LEN_PASS);
    GetUserText(ModemEdit,buf+U_MODEM,LEN_MODEM);
    GetUserText(ZipCodeEdit,buf+U_ZIPCODE,LEN_ZIPCODE);
    GetUserText(SexEdit,buf+U_SEX,1);
    GetUserText(PhoneEdit,buf+U_PHONE,LEN_PHONE);
    GetUserText(CommentEdit,buf+U_COMMENT,LEN_COMMENT);

    { Settings }
    GetUserText(RowsEdit,buf+U_ROWS,2);
    GetUserText(ShellEdit,buf+U_SHELL,8);
    GetUserText(EditorEdit,buf+U_XEDIT,8);
    GetUserText(ProtocolEdit,buf+U_PROT,1);
    GetUserText(TempFileExtEdit,buf+U_TMPEXT,3);

    { Read 'misc' bit-field }
    user_misc:=GetHexField(buf+U_MISC,8);
    ExpertCheckBox.Checked:=user_misc AND EXPERT <> 0;
    ExpertCheckBox.Tag:=0;

    TerminalCheckListBox.Checked[0]:=user_misc AND AUTOTERM <> 0;
    TerminalCheckListBox.Checked[1]:=user_misc AND NO_EXASCII = 0;
    TerminalCheckListBox.Checked[2]:=user_misc AND ANSI <> 0;
    TerminalCheckListBox.Checked[3]:=user_misc AND UCOLOR <> 0;
    TerminalCheckListBox.Checked[4]:=user_misc AND RIP <> 0;
    TerminalCheckListBox.Checked[5]:=user_misc AND WIP <> 0;
    TerminalCheckListBox.Checked[6]:=user_misc AND UPAUSE <> 0;
    TerminalCheckListBox.Checked[7]:=user_misc AND COLDKEYS = 0;
    TerminalCheckListBox.Checked[8]:=user_misc AND SPIN <> 0;
    TerminalCheckListBox.Tag:=0;

    MessageCheckListBox.Checked[0]:=user_misc AND NETMAIL <> 0;
    MessageCheckListBox.Checked[1]:=user_misc AND CLRSCRN <> 0;
    MessageCheckListBox.Tag:=0;

    FileCheckListBox.Checked[0]:=user_misc AND ANFSCAN <> 0;
    FileCheckListBox.Checked[1]:=user_misc AND EXTDESC <> 0;
    FileCheckListBox.Checked[2]:=user_misc AND BATCHFLAG <> 0;
    FileCheckListBox.Checked[3]:=user_misc AND AUTOHANG <> 0;
    FileCheckListBox.Tag:=0;

    LogonCheckListBox.Checked[0]:=user_misc AND ASK_NSCAN <> 0;
    LogonCheckListBox.Checked[1]:=user_misc AND ASK_SSCAN <> 0;
    LogonCheckListBox.Checked[2]:=user_misc AND CURSUB <> 0;
    LogonCheckListBox.Checked[3]:=user_misc AND QUIET <> 0;
    LogonCheckListBox.Checked[4]:=user_misc AND AUTOLOGON <> 0;
    LogonCheckListBox.Tag:=0;

    { Read 'QWK' bit-field }
    user_qwk:=GetHexField(buf+U_QWK,8);
    QWKCheckListBox.Checked[0]:=user_qwk AND QWK_FILES <> 0;
    QWKCheckListBox.Checked[1]:=user_qwk AND QWK_EMAIL <> 0;
    QWKCheckListBox.Checked[2]:=user_qwk AND QWK_ALLMAIL <> 0;
    QWKCheckListBox.Checked[3]:=user_qwk AND QWK_DELMAIL <> 0;
    QWKCheckListBox.Checked[4]:=user_qwk AND QWK_BYSELF <> 0;
    QWKCheckListBox.Checked[5]:=user_qwk AND QWK_EXPCTLA <> 0;
    QWKCheckListBox.Checked[6]:=user_qwk AND QWK_RETCTLA = 0;
    QWKCheckListBox.Checked[7]:=user_qwk AND QWK_ATTACH <> 0;
    QWKCheckListBox.Checked[8]:=user_qwk AND QWK_NOINDEX <> 0;
    QWKCheckListBox.Checked[9]:=user_qwk AND QWK_TZ <> 0;
    QWKCheckListBox.Checked[10]:=user_qwk AND QWK_VIA <> 0;
    QWKCheckListBox.Checked[11]:=user_qwk AND QWK_NOCTRL = 0;
    QWKCheckListBox.Tag:=0;

    { Read 'chat' bit-field }
    user_chat:=GetHexField(buf+U_CHAT,8);
    ChatCheckListBox.Checked[0]:=user_chat AND CHAT_ECHO <> 0;
    ChatCheckListBox.Checked[1]:=user_chat AND CHAT_ACTION <> 0;
    ChatCheckListBox.Checked[2]:=user_chat AND CHAT_NOPAGE = 0;
    ChatCheckListBox.Checked[3]:=user_chat AND CHAT_NOACT = 0;
    ChatCheckListBox.Checked[4]:=user_chat AND CHAT_SPLITP <> 0;
    ChatCheckListBox.Tag:=0;

    { Initialize controls based on bits set/unset }
    if user_misc AND DELETED <> 0 then begin
        Status.Text := 'Deleted User';
        Status.Color := clRed;
        end
    else if user_misc AND INACTIVE <> 0 then begin
        Status.Text := 'Inactive User';
        Status.Color := clYellow;
        end
    else begin
        Status.Text := 'Active User';
        Status.Color := clMenu;
        end;

    { Security }
    GetUserText(LevelEdit,buf+U_LEVEL,2);
    ExpireEdit.Text:=GetDateField(buf+U_EXPIRE);
    ExpireEdit.Tag:=0;
    Flags1Edit.Text:=GetFlagsField(buf+U_FLAGS1);
    Flags1Edit.Tag:=0;
    Flags2Edit.Text:=GetFlagsField(buf+U_FLAGS2);
    Flags2Edit.Tag:=0;
    Flags3Edit.Text:=GetFlagsField(buf+U_FLAGS3);
    Flags3Edit.Tag:=0;
    Flags4Edit.Text:=GetFlagsField(buf+U_FLAGS4);
    Flags4Edit.Tag:=0;
    ExemptionsEdit.Text:=GetFlagsField(buf+U_EXEMPT);
    ExemptionsEdit.Tag:=0;
    RestrictionsEdit.Text:=GetFlagsField(buf+U_REST);
    RestrictionsEdit.Tag:=0;
    GetUserLongInt(CreditsEdit,buf+U_CDT);
    GetUserLongInt(FreeCreditsEdit,buf+U_FREECDT);
    GetUserLongInt(MinutesEdit,buf+U_MIN);

    { Stats }
    FirstOnEdit.Text:=GetDateField(buf+U_FIRSTON);
    FirstOnEdit.Tag:=0;
    LastOnEdit.Text:=GetDateField(buf+U_LASTON);
    LastOnEdit.Tag:=0;
    GetUserShortInt(LogonsEdit,buf+U_LOGONS);
    GetUserShortInt(LogonsTodayEdit,buf+U_LTODAY);
    GetUserShortInt(TimeOnEdit,buf+U_TIMEON);
    GetUserShortInt(LastCallTimeEdit,buf+U_TLAST);
    GetUserShortInt(TimeOnTodayEdit,buf+U_TTODAY);
    GetUserShortInt(ExtraTimeEdit,buf+U_TEXTRA);
    GetUserShortInt(PostsTotalEdit,buf+U_POSTS);
    GetUserShortInt(PostsTodayEdit,buf+U_PTODAY);
    GetUserShortInt(EmailTotalEdit,buf+U_EMAILS);
    GetUserShortInt(EmailTodayEdit,buf+U_ETODAY);
    GetUserShortInt(FeedbackEdit,buf+U_FBACKS);
    GetUserShortInt(UploadedFilesEdit,buf+U_ULS);
    GetUserLongInt(UploadedBytesEdit,buf+U_ULB);
    GetUserShortInt(DownloadedFilesEdit,buf+U_DLS);
    GetUserLongInt(DownloadedBytesEdit,buf+U_DLB);
    LeechEdit.Text:=IntToStr(GetHexField(buf+U_LEECH,2));

    { etc... }

    { Extended Comment }
    Memo.Lines.Clear();
    Str:=data_dir+Format('USER/%.4d.MSG',[usernumber]);
    if FileExists(Str) then Memo.Lines.LoadFromFile(Str);
    Memo.Tag:=0;

    { Update User Number }
    NumberEdit.Text:=IntToStr(ScrollBar.Position);
    SaveUser.Enabled:=false;   { no changes have been made yet }
end;

{ ************* }
{ PUT USER DATA }
{ ************* }

{ Encodes a single text data field (of any length) }
procedure PutTextField(buf : PChar; str : AnsiString; maxlen : Integer);
var len:Integer;
    i:Integer;
begin
    for i:=0 to maxlen-1 do buf[i]:=ETX;
    len:=Length(str);
    if len > maxlen then len:=maxlen;
    for i:=0 to len-1 do buf[i]:=str[i+1];
end;

{ Encodes a flag field (A-Z) into a 32-bit Hex string }
procedure PutFlagsField(buf : PChar; str : AnsiString);
var flags: Integer;
    i:Integer;
begin
    flags:=0;
    for i:=0 to 25 do
        if Pos(Chr(65+i),str) <> 0 then
            flags:=flags OR (1 shl i);
    PutTextField(buf,IntToHex(flags,8),8);
end;

{ Converts a date string in xx/xx/YY format into a unix time_t format in hex }
procedure PutDateField(buf : PChar; str : AnsiString);
var val: Integer;
begin
    { convert to days since 1970 }
    try
        val:=Round(StrToDate(str)-EncodeDate(1970,1,1));
    except
        val:=0;
    end;
    if val < 0 then val:=0;
    { convert from days to seconds }
    val:=val*(24*60*60);
    PutTextField(buf,IntToHex(val,8),8);
end;

{ Writes to a 16-bit decimal integer field }
procedure PutShortIntField(buf : PChar; str : AnsiString);
begin
    PutTextField(buf, str, 5);
end;

{ Writes to a 32-bit decimal integer field }
procedure PutLongIntField(buf : PChar; str : AnsiString);
begin
    PutTextField(buf, str, 10);
end;

{ Writes to a hexadecimal integer field (of any length) }
procedure PutHexField(buf : PChar; val : Integer; maxlen : Integer);
begin
    PutTextField(buf,IntToHex(val,maxlen),maxlen);
end;

{ Writes a user Edit box's contents to the buffer, if changed, resets Tag }
procedure PutUserText(Edit:Tedit; buf:PChar; maxlen:Integer);
begin
    if Edit.Tag = 1 then    { field modified, change record }
        PutTextField(buf,Edit.Text,maxlen);
    Edit.Tag:=0; { clear modified flag }
end;

{ Writes a flag Edit box's contents to the buffer, if changed }
{ This function can be used for exemptions and restrictions too }
procedure PutUserFlags(Edit:Tedit; buf:PChar);
begin
    if Edit.Tag = 1 then { field modified, change record }
        PutFlagsField(buf,Edit.Text);
    Edit.Tag:=0; { clear modified flag }
end;

{ Writes a 16-bit integer Edit box's contents to the buffer, if changed }
procedure PutUserShortInt(Edit:Tedit; buf:PChar);
begin
    if Edit.Tag = 1 then { field modified, change record }
        PutTextField(buf,Edit.Text,5);
    Edit.Tag:=0; { clear modified flag }
end;

{ Writes a 32-bit integer Edit box's contents to the buffer, if changed }
procedure PutUserLongInt(Edit:Tedit; buf:PChar);
begin
    if Edit.Tag = 1 then { field modified, change record }
        PutTextField(buf,Edit.Text,10);
    Edit.Tag:=0; { clear modified flag }
end;

{ Writes a date Edit box's contents to the buffer, if changed }
procedure PutUserDate(Edit:Tedit; buf:PChar);
begin
    if Edit.Tag = 1 then { field modified, change record }
        PutDateField(buf,Edit.Text);
    Edit.Tag:=0; { clear modified flag }
end;

function SetBit(set_it: bool; field: Cardinal; bit: Cardinal): Cardinal;
begin
    if set_it then
       Result:=field OR bit    { set bit }
    else
        Result:=field AND NOT bit;  { clear bit }
end;

{ Writes a complete user record. }
procedure TForm1.PutUserData(usernumber:Integer);
var Str: AnsiString;
    f: TFileStream;
    i: Integer;
    buf: array[0..U_LEN] of Char;
begin
    if AliasEdit.Tag = 1 then begin
        { Set-up buffer for NAME.DAT record }
        if user_misc AND DELETED <> 0 then
            PutTextField(buf,'',LEN_ALIAS)
        else
            PutTextField(buf,AliasEdit.Text,LEN_ALIAS);
        buf[LEN_ALIAS]:=CR;
        buf[LEN_ALIAS+1]:=LF;

        { Open NAME.DAT write user name }
        Str:=data_dir+'USER/NAME.DAT';
        if FileExists(Str) then
            f:=TFileStream.Create(Str,fmOpenWrite or fmShareExclusive)
        else
            f:=TFileStream.Create(Str,fmCreate or fmShareExclusive);
        f.Seek((usernumber-1)*(LEN_ALIAS+2),soFromBeginning);
        f.Write(buf,LEN_ALIAS+2);
        f.Free;
    end;

    { Initialize USER record buffer }
    for i:=0 to U_LEN-1 do buf[i]:=ETX;

    { Open file and read current user record }
    Str:=data_dir+'user/user.dat';
    if FileExists(Str) then
        f:=TFileStream.Create(Str,fmOpenReadWrite or fmShareDenyNone)
    else
        f:=TFileStream.Create(Str,fmCreate or fmShareDenyNone);

    f.Seek((usernumber-1)*U_LEN,soFromBeginning);
    f.Read(buf,U_LEN);

    { Update changed fields }
    PutUserText(AliasEdit,buf+U_ALIAS,LEN_ALIAS);
    PutUserText(NameEdit,buf+U_NAME,LEN_NAME);
    PutUserText(HandleEdit,buf+U_HANDLE,LEN_HANDLE);
    PutUserText(ComputerEdit,buf+U_COMP,LEN_COMP);
    PutUserText(NetMailEdit,buf+U_NETMAIL,LEN_NETMAIL);
    PutUserText(AddressEdit,buf+U_ADDRESS,LEN_ADDRESS);
    PutUserText(LocationEdit,buf+U_LOCATION,LEN_LOCATION);
	PutUserText(NoteEdit,buf+U_NOTE,LEN_NOTE);
    PutUserText(ZipCodeEdit,buf+U_ZIPCODE,LEN_ZIPCODE);
    PutUserText(PasswordEdit,buf+U_PASS,LEN_PASS);
    PutUserText(PhoneEdit,buf+U_PHONE,LEN_PHONE);
    PutUserText(BirthDateEdit,buf+U_BIRTH,LEN_BIRTH);
    PutUserText(ModemEdit,buf+U_MODEM,LEN_MODEM);
    PutUserText(SexEdit,buf+U_SEX,1);
    PutUserText(CommentEdit,buf+U_COMMENT,LEN_COMMENT);
    { etc. }

    { Settings }
    PutUserText(RowsEdit,buf+U_ROWS,2);
    PutUserText(ShellEdit,buf+U_SHELL,8);
    PutUserText(EditorEdit,buf+U_XEDIT,8);
    PutUserText(ProtocolEdit,buf+U_PROT,1);
    PutUserText(TempFileExtEdit,buf+U_TMPEXT,3);

    { Write MISC bit-field}
    if (ExpertCheckBox.Tag = 1)
        or (LogonCheckListBox.Tag = 1)
        or (TerminalCheckListBox.Tag = 1)
        or (MessageCheckListBox.Tag = 1)
        or (FileCheckListBox.Tag = 1)
        then begin
        user_misc:=SetBit(ExpertCheckBox.Checked, user_misc, EXPERT);

        { TerminalCeckListBox }
        user_misc:=SetBit(TerminalCheckListBox.Checked[0],user_misc,AUTOTERM);
        user_misc:=SetBit(NOT TerminalCheckListBox.Checked[1],user_misc,NO_EXASCII);
        user_misc:=SetBit(TerminalCheckListBox.Checked[2],user_misc,ANSI);
        user_misc:=SetBit(TerminalCheckListBox.Checked[3],user_misc,UCOLOR);
        user_misc:=SetBit(TerminalCheckListBox.Checked[4],user_misc,RIP);
        user_misc:=SetBit(TerminalCheckListBox.Checked[5],user_misc,WIP);
        user_misc:=SetBit(TerminalCheckListBox.Checked[6],user_misc,UPAUSE);
        user_misc:=SetBit(NOT TerminalCheckListBox.Checked[7],user_misc,COLDKEYS);
        user_misc:=SetBit(TerminalCheckListBox.Checked[8],user_misc,SPIN);

        { MessageCheckListBox }
        user_misc:=SetBit(MessageCheckListBox.Checked[0],user_misc,NETMAIL);
        user_misc:=SetBit(MessageCheckListBox.Checked[1],user_misc,CLRSCRN);

        { FileCheckListBox }
        user_misc:=SetBit(FileCheckListBox.Checked[0],user_misc,ANFSCAN);
        user_misc:=SetBit(FileCheckListBox.Checked[1],user_misc,EXTDESC);
        user_misc:=SetBit(FileCheckListBox.Checked[2],user_misc,BATCHFLAG);
        user_misc:=SetBit(FileCheckListBox.Checked[3],user_misc,AUTOHANG);

        { LogonCheckListBox }
        user_misc:=SetBit(LogonCheckListBox.Checked[0],user_misc,ASK_NSCAN);
        user_misc:=SetBit(LogonCheckListBox.Checked[1],user_misc,ASK_SSCAN);
        user_misc:=SetBit(LogonCheckListBox.Checked[2],user_misc,CURSUB);
        user_misc:=SetBit(LogonCheckListBox.Checked[3],user_misc,QUIET);
        user_misc:=SetBit(LogonCheckListBox.Checked[4],user_misc,AUTOLOGON);

        PutHexField(buf+U_MISC, user_misc, 8);
        end;

    if (ChatCheckListBox.Tag = 1) then begin
        user_chat:=SetBit(ChatCheckListBox.Checked[0],user_chat,CHAT_ECHO);
        user_chat:=SetBit(ChatCheckListBox.Checked[1],user_chat,CHAT_ACTION);
        user_chat:=SetBit(NOT ChatCheckListBox.Checked[2],user_chat,CHAT_NOPAGE);
        user_chat:=SetBit(NOT ChatCheckListBox.Checked[3],user_chat,CHAT_NOACT);
        user_chat:=SetBit(ChatCheckListBox.Checked[4],user_chat,CHAT_SPLITP);

        PutHexField(buf+U_CHAT, user_chat, 8);
        end;

    if (QWKCheckListBox.Tag =1) then begin
        user_qwk:=SetBit(QWKCheckListBox.Checked[0],user_qwk,QWK_FILES);
        user_qwk:=SetBit(QWKCheckListBox.Checked[1],user_qwk,QWK_EMAIL);
        user_qwk:=SetBit(QWKCheckListBox.Checked[2],user_qwk,QWK_ALLMAIL);
        user_qwk:=SetBit(QWKCheckListBox.Checked[3],user_qwk,QWK_DELMAIL);
        user_qwk:=SetBit(QWKCheckListBox.Checked[4],user_qwk,QWK_BYSELF);
        user_qwk:=SetBit(QWKCheckListBox.Checked[5],user_qwk,QWK_EXPCTLA);
        user_qwk:=SetBit(NOT QWKCheckListBox.Checked[6],user_qwk,QWK_RETCTLA);
        user_qwk:=SetBit(QWKCheckListBox.Checked[7],user_qwk,QWK_ATTACH);
        user_qwk:=SetBit(QWKCheckListBox.Checked[8],user_qwk,QWK_NOINDEX);
        user_qwk:=SetBit(QWKCheckListBox.Checked[9],user_qwk,QWK_TZ);
        user_qwk:=SetBit(QWKCheckListBox.Checked[10],user_qwk,QWK_VIA);
        user_qwk:=SetBit(NOT QWKCheckListBox.Checked[11],user_qwk,QWK_NOCTRL);

        PutHexField(buf+U_QWK, user_qwk, 8);
        end;

    { Security }
    PutUserText(LevelEdit,buf+U_LEVEL,2);
    PutUserDate(ExpireEdit,buf+U_EXPIRE);
    PutUserFlags(Flags1Edit,buf+U_FLAGS1);
    PutUserFlags(Flags2Edit,buf+U_FLAGS2);
    PutUserFlags(Flags3Edit,buf+U_FLAGS3);
    PutUserFlags(Flags4Edit,buf+U_FLAGS4);
    PutUserFlags(ExemptionsEdit,buf+U_EXEMPT);
    PutUserFlags(RestrictionsEdit,buf+U_REST);
    PutUserLongInt(CreditsEdit,buf+U_CDT);
    PutUserLongInt(FreeCreditsEdit,buf+U_FREECDT);
    PutUserLongInt(MinutesEdit,buf+U_MIN);
    { etc. }

    { Stats }
    PutUserDate(FirstOnEdit,buf+U_FIRSTON);
    PutUserDate(LastOnEdit,buf+U_LASTON);
    PutUserShortInt(LogonsEdit,buf+U_LOGONS);
    PutUserShortInt(LogonsTodayEdit,buf+U_LTODAY);
    PutUserShortInt(TimeOnEdit,buf+U_TIMEON);
    PutUserShortInt(TimeOnTodayEdit,buf+U_TTODAY);
    PutUserShortInt(LastCallTimeEdit,buf+U_TLAST);
    PutUserShortInt(ExtraTimeEdit,buf+U_TEXTRA);
    PutUserShortInt(PostsTotalEdit,buf+U_POSTS);
    PutUserShortInt(PostsTodayEdit,buf+U_PTODAY);
    PutUserShortInt(EmailTotalEdit,buf+U_EMAILS);
    PutUserShortInt(EmailTodayEdit,buf+U_ETODAY);
    PutUserShortInt(FeedbackEdit,buf+U_FBACKS);
    PutUserShortInt(UploadedFilesEdit,buf+U_ULS);
    PutUserLongInt(UploadedBytesEdit,buf+U_ULB);
    PutUserShortInt(DownloadedFilesEdit,buf+U_DLS);
    PutUserLongInt(DownloadedBytesEdit,buf+U_DLB);
    PutHexField(buf+U_LEECH, StrToIntDef(LeechEdit.Text,0), 2);
    { etc. }

    { Write user record and close file }
    f.Seek((usernumber-1)*U_LEN,soFromBeginning);
    f.Write(buf,U_LEN);
    f.Free;

    { Extended Comemnt }
    if Memo.Tag = 1 then begin
        Str:=data_dir+Format('USER/%.4d.MSG',[usernumber]);
        if Memo.Lines.Count<>0 then
            Memo.Lines.SaveToFile(Str)
        else
            DeleteFile(Str);
    end;

    SaveUser.Enabled:=false;
end;

{ ********* }
{ MAIN FORM }
{ ********* }

{ There's probably a better place to do this init stuff... constructor? }
procedure TForm1.FormShow(Sender: TObject);
begin

    { Over-ride Locale settings here }
    DateSeparator:='/';

    if (ShortDateFormat = 'M/d/yy')
	or (ShortDateFormat = 'M/d/yyyy')
	or (ShortDateFormat = 'MM/dd/yy')
	or (ShortDateFormat = 'MM/dd/yyyy')
	then
        ShortDateFormat:='mm/dd/yy'     { American }
    else
        ShortDateFormat:='dd/mm/yy';    { European }

    data_dir:=ParamStr(1);
{    if Length(data_dir)=0 then data_dir:='.\'; }

    users:=LastUser();

    if users = 0 then   { Create user if none exist }
        users:=1;

    ScrollBar.Min:=1;
    ScrollBar.Max:=users;
    TotalStaticText.Caption:='of '+IntToStr(users);
    { *********************************** }
    { Set max lengths for edit boxes here }
    { *********************************** }
    { Personal }
    AliasEdit.MaxLength:=LEN_ALIAS;
    NameEdit.MaxLength:=LEN_NAME;
    PhoneEdit.MaxLength:=LEN_PHONE;
    HandleEdit.MaxLength:=LEN_HANDLE;
    ComputerEdit.MaxLength:=LEN_COMP;
    AddressEdit.MaxLength:=LEN_ADDRESS;
    LocationEdit.MaxLength:=LEN_LOCATION;
    ZipCodeEdit.MaxLength:=LEN_ZIPCODE;
    ModemEdit.MaxLength:=LEN_MODEM;
    CommentEdit.MaxLength:=LEN_COMMENT;
    { Security }
    PasswordEdit.MaxLength:=LEN_PASS;
    PhoneEdit.MaxLength:=LEN_PHONE;
    ModemEdit.MaxLength:=LEN_MODEM;
    SexEdit.MaxLength:=1;
    { Stats }
    { etc. }

    ScrollBar.Position:=StrToIntDef(ParamStr(2),1);
    GetUserData(ScrollBar.Position);

    PageControl.ActivePage:=PersonalTabSheet;
end;

{ Change user }
procedure TForm1.ScrollBarChange(Sender: TObject);
begin
    if SaveUser.Enabled then SaveChanges();
    users:=LastUser(); { this could change dynamically }
    ScrollBar.Max:=users;
    TotalStaticText.Caption:='of '+IntToStr(users);
    GetUserData(ScrollBar.Position);
end;

{ Better than on OnChange event, waits til users hits enter key }
procedure TForm1.NumberEditKeyPress(Sender: TObject; var Key: Char);
var val : Integer;
begin
    if Key <> #13 then Exit;
    users:=lastuser;
    val:=StrToIntDef(NumberEdit.Text,0);
    if (val = 0) or (val > users) then
        NumberEdit.Text:=IntToStr(ScrollBar.Position)
    else begin
        ScrollBar.Position:=val;
        GetUserData(val);
        Key:=#0;
    end
end;

procedure TForm1.FileExitMenuItemClick(Sender: TObject);
begin
    Close;
end;

{ OnChange event for ALL User Data Edit boxes }
procedure TForm1.EditChange(Sender: TObject);
begin
    SaveUser.Enabled:=true;
    (Sender as TComponent).Tag:=1;   { Mark as modified }
end;

{ Create a New User record }
procedure TForm1.SaveUserExecute(Sender: TObject);
begin
    PutUserData(StrToIntDef(NumberEdit.Text,ScrollBar.Position));
end;

procedure TForm1.NewUserExecute(Sender: TObject);
begin
    if SaveUser.Enabled then SaveChanges();

    { New users's number }
    users:=lastuser()+1;

    { Initialize fields to default values }
    AliasEdit.Text:='New User';
    AliasEdit.Tag := 1;
    LevelEdit.Text:='10';
    LevelEdit.Tag :=1;
    FirstOnEdit.Text:=DatetoStr(Date);
    FirstOnEdit.Tag :=1;
    LastOnEdit.Text:=DateToStr(Date);
    LastOnEdit.Tag :=1;
    TerminalCheckListBox.Checked[0] := true; // AUTOTERM
    TerminalCheckListBox.Checked[1] := true; // EXASCII
    TerminalCheckListBox.Checked[2] := true; // ANSI
    TerminalCheckListBox.Checked[3] := true; // COLOR
    TerminalCheckListBox.Checked[6] := true; // UPAUSE
    TerminalCheckListBox.Checked[7] := true; // HOTKEYS
    FileCheckListBox.Checked[2] := true;    // BATCHFLAG
    LogonCheckListBox.Tag:=1;

    { Create the new record }
    PutUserData(users);

    { Set scroll bar and usernumber text }
    ScrollBar.Max:=users;
    ScrollBar.Position:=users;
    NumberEdit.Text:=IntToStr(users);

end;

procedure TForm1.DeleteUserExecute(Sender: TObject);
begin
    user_misc:=user_misc xor DELETED;
    LogonCheckListBox.Tag:=1;   { flag as modified }
    AliasEdit.Tag := 1;
    PutUserData(ScrollBar.Position);
    GetUserData(ScrollBar.Position);
end;

procedure TForm1.FormClose(Sender: TObject; var Action: TCloseAction);
begin
    if SaveUser.Enabled then SaveChanges();
end;

procedure TForm1.DeactivateUserExecute(Sender: TObject);
begin
    user_misc:=user_misc xor INACTIVE;
    LogonCheckListBox.Tag:=1;   { flag as modified }
    PutUserData(ScrollBar.Position);
    GetUserData(ScrollBar.Position);
end;

procedure TForm1.FindButtonClick(Sender: TObject);
var Str: AnsiString;
    SearchStr: AnsiString;
    f: TFileStream;
    usernumber: Integer;
    buf: array[0..U_LEN] of Char;
begin
    SearchStr:=AnsiUpperCase(FindEdit.Text);
    usernumber:=0;
    { Open USER.DAT to search for string }
    Str:=data_dir+'user/user.dat';
    if not FileExists(Str) then Exit;
    f:=TFileStream.Create(Str,fmOpenRead or fmShareDenyNone);
    if Sender = FindNextButton then
        f.Seek((ScrollBar.Position)*(U_LEN),soFromBeginning);
    while (f.Position < f.Size) and (usernumber=0) do begin
        f.Read(buf,U_LEN);
        SetString(Str,buf,U_LEN);
        Str:=AnsiUpperCase(Str);
        if Pos(SearchStr,Str) <> 0 then
            usernumber:=f.Position div (U_LEN);
        end;
    f.Free;
    if usernumber <> 0 then begin
        ScrollBar.Position:=usernumber;
        end;
end;

procedure TForm1.FindEditKeyPress(Sender: TObject; var Key: Char);
begin
    if Key = #13 then begin
        FindButtonClick(Sender);
        Key:=#0;
    end;
end;

end.
