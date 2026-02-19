// Borland C++ Builder
// Copyright (c) 1995, 2002 by Borland Software Corporation
// All rights reserved

// (DO NOT EDIT: machine generated header) 'EmulVT.pas' rev: 6.00

#ifndef EmulVTHPP
#define EmulVTHPP

#pragma delphiheader begin
#pragma option push -w-
#pragma option push -Vx
#include <Clipbrd.hpp>	// Pascal unit
#include <StdCtrls.hpp>	// Pascal unit
#include <Dialogs.hpp>	// Pascal unit
#include <Forms.hpp>	// Pascal unit
#include <Controls.hpp>	// Pascal unit
#include <Graphics.hpp>	// Pascal unit
#include <Classes.hpp>	// Pascal unit
#include <Messages.hpp>	// Pascal unit
#include <Windows.hpp>	// Pascal unit
#include <SysUtils.hpp>	// Pascal unit
#include <SysInit.hpp>	// Pascal unit
#include <System.hpp>	// Pascal unit

//-- user supplied -----------------------------------------------------------

namespace Emulvt
{
//-- type declarations -------------------------------------------------------
#pragma option push -b-
enum TBackColors { vtsBlack, vtsRed, vtsGreen, vtsYellow, vtsBlue, vtsMagenta, vtsCyan, vtsWhite };
#pragma option pop

#pragma option push -b-
enum TScreenOption { vtoBackColor, vtoCopyBackOnClear };
#pragma option pop

typedef Set<TScreenOption, vtoBackColor, vtoCopyBackOnClear>  TScreenOptions;

typedef char TXlatTable[256];

typedef char *PXlatTable;

typedef SmallString<50>  TFuncKeyValue;

typedef TFuncKeyValue *PFuncKeyValue;

#pragma pack(push, 1)
struct TFuncKey
{
	char ScanCode;
	Classes::TShiftState Shift;
	bool Ext;
	TFuncKeyValue Value;
} ;
#pragma pack(pop)

typedef TFuncKey TFuncKeysTable[64];

typedef TFuncKey *PFuncKeysTable;

typedef void __fastcall (__closure *TKeyBufferEvent)(System::TObject* Sender, char * Buffer, int Len);

typedef void __fastcall (__closure *TKeyDownEvent)(System::TObject* Sender, int &VirtKey, Classes::TShiftState &Shift, bool &ShiftLock, char &ScanCode, bool &Ext);

class DELPHICLASS TLine;
class PASCALIMPLEMENTATION TLine : public System::TObject 
{
	typedef System::TObject inherited;
	
public:
	char Txt[513];
	Byte Att[513];
	__fastcall TLine(void);
	void __fastcall Clear(Byte Attr);
public:
	#pragma option push -w-inl
	/* TObject.Destroy */ inline __fastcall virtual ~TLine(void) { }
	#pragma option pop
	
};


typedef TLine* TLineArray[16383];

typedef TLine* *PLineArray;

class DELPHICLASS TScreen;
class PASCALIMPLEMENTATION TScreen : public System::TObject 
{
	typedef System::TObject inherited;
	
public:
	TLine* *FLines;
	int FRow;
	int FCol;
	int FRowSaved;
	int FColSaved;
	int FScrollRowTop;
	int FScrollRowBottom;
	Byte FAttribute;
	bool FForceHighBit;
	bool FReverseVideo;
	bool FUnderLine;
	int FRowCount;
	int FColCount;
	int FBackRowCount;
	int FBackEndRow;
	TBackColors FBackColor;
	TScreenOptions FOptions;
	System::SmallString<80>  FEscBuffer;
	bool FEscFlag;
	bool Focused;
	bool FAutoLF;
	bool FAutoCR;
	bool FAutoWrap;
	bool FCursorOff;
	bool FCKeyMode;
	bool FNoXlat;
	bool FNoXlatInitial;
	int FCntLiteral;
	bool FCarbonMode;
	char *FXlatInputTable;
	char *FXlatOutputTable;
	char FCharSetG0;
	char FCharSetG1;
	char FCharSetG2;
	char FCharSetG3;
	bool FAllInvalid;
	#pragma pack(push, 1)
	Types::TRect FInvRect;
	#pragma pack(pop)
	
	Classes::TNotifyEvent FOnCursorVisible;
	__fastcall TScreen(void);
	__fastcall virtual ~TScreen(void);
	void __fastcall AdjustFLines(int NewCount);
	void __fastcall CopyScreenToBack(void);
	void __fastcall SetRowCount(int NewCount);
	void __fastcall SetBackRowCount(int NewCount);
	void __fastcall InvRect(int nRow, int nCol);
	void __fastcall InvClear(void);
	void __fastcall SetLines(int I, TLine* Value);
	TLine* __fastcall GetLines(int I);
	void __fastcall WriteChar(char Ch);
	void __fastcall WriteStr(AnsiString Str);
	AnsiString __fastcall ReadStr();
	void __fastcall GotoXY(int X, int Y);
	void __fastcall WriteLiteralChar(char Ch);
	void __fastcall ProcessEscape(char EscCmd);
	void __fastcall SetAttr(char Att);
	void __fastcall CursorRight(void);
	void __fastcall CursorLeft(void);
	void __fastcall CursorDown(void);
	void __fastcall CursorUp(void);
	void __fastcall CarriageReturn(void);
	void __fastcall ScrollUp(void);
	void __fastcall ScrollDown(void);
	void __fastcall ClearScreen(void);
	void __fastcall BackSpace(void);
	void __fastcall Eol(void);
	void __fastcall Eop(void);
	void __fastcall ProcessESC_D(void);
	void __fastcall ProcessESC_M(void);
	void __fastcall ProcessESC_E(void);
	void __fastcall ProcessCSI_u(void);
	void __fastcall ProcessCSI_I(void);
	void __fastcall ProcessCSI_J(void);
	void __fastcall ProcessCSI_K(void);
	void __fastcall ProcessCSI_L(void);
	void __fastcall ProcessCSI_M(void);
	void __fastcall ProcessCSI_m_lc(void);
	void __fastcall ProcessCSI_n_lc(void);
	void __fastcall ProcessCSI_at(void);
	void __fastcall ProcessCSI_r_lc(void);
	void __fastcall ProcessCSI_s_lc(void);
	void __fastcall ProcessCSI_u_lc(void);
	void __fastcall ProcessCSI_7(void);
	void __fastcall ProcessCSI_8(void);
	void __fastcall ProcessCSI_H(void);
	void __fastcall ProcessCSI_h_lc(void);
	void __fastcall ProcessCSI_l_lc(void);
	void __fastcall ProcessCSI_A(void);
	void __fastcall ProcessCSI_B(void);
	void __fastcall ProcessCSI_C(void);
	void __fastcall ProcessCSI_D(void);
	void __fastcall ProcessCSI_P(void);
	void __fastcall ProcessCSI_S(void);
	void __fastcall ProcessCSI_T(void);
	void __fastcall process_charset_G0(char EscCmd);
	void __fastcall process_charset_G1(char EscCmd);
	void __fastcall process_charset_G2(char EscCmd);
	void __fastcall process_charset_G3(char EscCmd);
	void __fastcall UnimplementedEscape(char EscCmd);
	void __fastcall InvalidEscape(char EscCmd);
	int __fastcall GetEscapeParam(int From, int &Value);
	__property Classes::TNotifyEvent OnCursorVisible = {read=FOnCursorVisible, write=FOnCursorVisible};
	__property TLine* Lines[int I] = {read=GetLines, write=SetLines};
};


class DELPHICLASS TCustomEmulVT;
class PASCALIMPLEMENTATION TCustomEmulVT : public Controls::TCustomControl 
{
	typedef Controls::TCustomControl inherited;
	
private:
	int FCharPos[514];
	int FLinePos[514];
	TextFile FFileHandle;
	bool FCursorVisible;
	bool FCaretShown;
	bool FCaretCreated;
	int FLineHeight;
	float FLineZoom;
	int FCharWidth;
	float FCharZoom;
	bool FGraphicDraw;
	int FInternalLeading;
	Forms::TFormBorderStyle FBorderStyle;
	int FBorderWidth;
	bool FAutoRepaint;
	Graphics::TFont* FFont;
	Stdctrls::TScrollBar* FVScrollBar;
	int FTopLine;
	bool FLocalEcho;
	TKeyBufferEvent FOnKeyBuffer;
	TKeyDownEvent FOnKeyDown;
	int FFKeys;
	bool FMonoChrome;
	bool FLog;
	Forms::TMessageEvent FAppOnMessage;
	bool FFlagCirconflexe;
	bool FFlagTrema;
	#pragma pack(push, 1)
	Types::TRect FSelectRect;
	#pragma pack(pop)
	
	int FTopMargin;
	int FLeftMargin;
	int FRightMargin;
	int FBottomMargin;
	HPALETTE FPal;
	tagPALETTEENTRY FPaletteEntries[16];
	int FMarginColor;
	HIDESBASE MESSAGE void __fastcall WMPaint(Messages::TWMPaint &Message);
	HIDESBASE MESSAGE void __fastcall WMSetFocus(Messages::TWMSetFocus &Message);
	HIDESBASE MESSAGE void __fastcall WMKillFocus(Messages::TWMKillFocus &Message);
	HIDESBASE MESSAGE void __fastcall WMLButtonDown(Messages::TWMMouse &Message);
	HIDESBASE MESSAGE void __fastcall WMPaletteChanged(Messages::TMessage &Message);
	void __fastcall VScrollBarScroll(System::TObject* Sender, Stdctrls::TScrollCode ScrollCode, int &ScrollPos);
	void __fastcall SetCaret(void);
	void __fastcall AdjustScrollBar(void);
	bool __fastcall ProcessFKeys(char ScanCode, Classes::TShiftState Shift, bool Ext);
	PFuncKeyValue __fastcall FindFKeys(char ScanCode, Classes::TShiftState Shift, bool Ext);
	void __fastcall CursorVisibleEvent(System::TObject* Sender);
	HIDESBASE void __fastcall SetFont(Graphics::TFont* Value);
	void __fastcall SetAutoLF(bool Value);
	void __fastcall SetAutoCR(bool Value);
	void __fastcall SetAutoWrap(bool Value);
	void __fastcall SetXlat(bool Value);
	void __fastcall SetLog(bool Value);
	void __fastcall SetRows(int Value);
	void __fastcall SetCols(int Value);
	void __fastcall SetBackRows(int Value);
	void __fastcall SetTopLine(int Value);
	void __fastcall SetBackColor(TBackColors Value);
	void __fastcall SetOptions(TScreenOptions Value);
	void __fastcall SetLineHeight(int Value);
	bool __fastcall GetAutoLF(void);
	bool __fastcall GetAutoCR(void);
	bool __fastcall GetAutoWrap(void);
	bool __fastcall GetXlat(void);
	int __fastcall GetRows(void);
	int __fastcall GetCols(void);
	int __fastcall GetBackRows(void);
	TBackColors __fastcall GetBackColor(void);
	TScreenOptions __fastcall GetOptions(void);
	void __fastcall SetMarginColor(const int Value);
	void __fastcall SetLeftMargin(const int Value);
	void __fastcall SetBottomMargin(const int Value);
	void __fastcall SetRightMargin(const int Value);
	void __fastcall SetTopMargin(const int Value);
	
protected:
	TScreen* FScreen;
	void __fastcall AppMessageHandler(tagMSG &Msg, bool &Handled);
	virtual void __fastcall DoKeyBuffer(char * Buffer, int Len);
	void __fastcall PaintGraphicChar(HDC DC, int X, int Y, Types::PRect rc, char ch);
	
public:
	__fastcall virtual TCustomEmulVT(Classes::TComponent* AOwner);
	__fastcall virtual ~TCustomEmulVT(void);
	void __fastcall ShowCursor(void);
	HIDESBASE void __fastcall SetCursor(int Row, int Col);
	void __fastcall WriteChar(char Ch);
	void __fastcall WriteStr(AnsiString Str);
	void __fastcall WriteBuffer(void * Buffer, int Len);
	AnsiString __fastcall ReadStr();
	void __fastcall CopyHostScreen(void);
	void __fastcall Clear(void);
	void __fastcall UpdateScreen(void);
	int __fastcall SnapPixelToRow(int Y);
	int __fastcall SnapPixelToCol(int X);
	int __fastcall PixelToRow(int Y);
	int __fastcall PixelToCol(int X);
	void __fastcall MouseToCell(int X, int Y, int &ACol, int &ARow);
	void __fastcall SetLineZoom(float newValue);
	void __fastcall SetCharWidth(int newValue);
	void __fastcall SetCharZoom(float newValue);
	DYNAMIC void __fastcall KeyPress(char &Key);
	__property float LineZoom = {read=FLineZoom, write=SetLineZoom};
	__property int CharWidth = {read=FCharWidth, write=SetCharWidth, nodefault};
	__property float CharZoom = {read=FCharZoom, write=SetCharZoom};
	__property bool GraphicDraw = {read=FGraphicDraw, write=FGraphicDraw, nodefault};
	__property int TopLine = {read=FTopLine, write=SetTopLine, nodefault};
	__property Stdctrls::TScrollBar* VScrollBar = {read=FVScrollBar};
	__property int TopMargin = {read=FTopMargin, write=SetTopMargin, nodefault};
	__property int LeftMargin = {read=FLeftMargin, write=SetLeftMargin, nodefault};
	__property int RightMargin = {read=FRightMargin, write=SetRightMargin, nodefault};
	__property int BottomMargin = {read=FBottomMargin, write=SetBottomMargin, nodefault};
	__property int MarginColor = {read=FMarginColor, write=SetMarginColor, nodefault};
	
private:
	void __fastcall PaintOneLine(HDC DC, int Y, int Y1, const TLine* Line, int nColFrom, int nColTo, bool Blank);
	void __fastcall SetupFont(void);
	__property AnsiString Text = {read=ReadStr, write=WriteStr};
	__property OnMouseMove ;
	__property OnMouseDown ;
	__property OnMouseUp ;
	__property OnClick ;
	__property OnKeyPress ;
	__property TKeyBufferEvent OnKeyBuffer = {read=FOnKeyBuffer, write=FOnKeyBuffer};
	__property TKeyDownEvent OnKeyDown = {read=FOnKeyDown, write=FOnKeyDown};
	__property Ctl3D ;
	__property Align  = {default=0};
	__property TabStop  = {default=0};
	__property TabOrder  = {default=-1};
	__property Forms::TBorderStyle BorderStyle = {read=FBorderStyle, write=FBorderStyle, nodefault};
	__property bool AutoRepaint = {read=FAutoRepaint, write=FAutoRepaint, nodefault};
	__property Graphics::TFont* Font = {read=FFont, write=SetFont};
	__property bool LocalEcho = {read=FLocalEcho, write=FLocalEcho, nodefault};
	__property bool AutoLF = {read=GetAutoLF, write=SetAutoLF, nodefault};
	__property bool AutoCR = {read=GetAutoCR, write=SetAutoCR, nodefault};
	__property bool AutoWrap = {read=GetAutoWrap, write=SetAutoWrap, nodefault};
	__property bool Xlat = {read=GetXlat, write=SetXlat, nodefault};
	__property bool MonoChrome = {read=FMonoChrome, write=FMonoChrome, nodefault};
	__property bool Log = {read=FLog, write=SetLog, nodefault};
	__property int Rows = {read=GetRows, write=SetRows, nodefault};
	__property int Cols = {read=GetCols, write=SetCols, nodefault};
	__property int LineHeight = {read=FLineHeight, write=SetLineHeight, nodefault};
	__property int FKeys = {read=FFKeys, write=FFKeys, nodefault};
	__property Types::TRect SelectRect = {read=FSelectRect, write=FSelectRect};
	__property int BackRows = {read=GetBackRows, write=SetBackRows, nodefault};
	__property TBackColors BackColor = {read=GetBackColor, write=SetBackColor, nodefault};
	__property TScreenOptions Options = {read=GetOptions, write=SetOptions, nodefault};
public:
	#pragma option push -w-inl
	/* TWinControl.CreateParented */ inline __fastcall TCustomEmulVT(HWND ParentWindow) : Controls::TCustomControl(ParentWindow) { }
	#pragma option pop
	
};


class DELPHICLASS TEmulVT;
class PASCALIMPLEMENTATION TEmulVT : public TCustomEmulVT 
{
	typedef TCustomEmulVT inherited;
	
public:
	__property TScreen* Screen = {read=FScreen};
	__property SelectRect ;
	__property Text ;
	
__published:
	__property OnMouseMove ;
	__property OnMouseDown ;
	__property OnMouseUp ;
	__property OnClick ;
	__property OnKeyPress ;
	__property OnKeyDown ;
	__property OnKeyBuffer ;
	__property Ctl3D ;
	__property Align  = {default=0};
	__property BorderStyle ;
	__property AutoRepaint ;
	__property Font ;
	__property LocalEcho ;
	__property AutoLF ;
	__property AutoCR ;
	__property AutoWrap ;
	__property Xlat ;
	__property MonoChrome ;
	__property Log ;
	__property Rows ;
	__property Cols ;
	__property BackRows ;
	__property BackColor ;
	__property Options ;
	__property LineHeight ;
	__property CharWidth ;
	__property TabStop  = {default=0};
	__property TabOrder  = {default=-1};
	__property FKeys ;
	__property TopMargin ;
	__property LeftMargin ;
	__property RightMargin ;
	__property BottomMargin ;
	__property MarginColor ;
public:
	#pragma option push -w-inl
	/* TCustomEmulVT.Create */ inline __fastcall virtual TEmulVT(Classes::TComponent* AOwner) : TCustomEmulVT(AOwner) { }
	#pragma option pop
	#pragma option push -w-inl
	/* TCustomEmulVT.Destroy */ inline __fastcall virtual ~TEmulVT(void) { }
	#pragma option pop
	
public:
	#pragma option push -w-inl
	/* TWinControl.CreateParented */ inline __fastcall TEmulVT(HWND ParentWindow) : TCustomEmulVT(ParentWindow) { }
	#pragma option pop
	
};


//-- var, const, procedure ---------------------------------------------------
static const Byte EmulVTVersion = 0xdb;
extern PACKAGE AnsiString CopyRight;
static const Word MAX_ROW = 0x200;
static const Word MAX_COL = 0x200;
static const Shortint NumPaletteEntries = 0x10;
static const Shortint F_BLACK = 0x0;
static const Shortint F_BLUE = 0x1;
static const Shortint F_GREEN = 0x2;
static const Shortint F_CYAN = 0x3;
static const Shortint F_RED = 0x4;
static const Shortint F_MAGENTA = 0x5;
static const Shortint F_BROWN = 0x6;
static const Shortint F_WHITE = 0x7;
static const Shortint B_BLACK = 0x0;
static const Shortint B_BLUE = 0x1;
static const Shortint B_GREEN = 0x2;
static const Shortint B_CYAN = 0x3;
static const Shortint B_RED = 0x4;
static const Shortint B_MAGENTA = 0x5;
static const Shortint B_BROWN = 0x6;
static const Shortint B_WHITE = 0x7;
static const Shortint F_INTENSE = 0x8;
static const Byte B_BLINK = 0x80;
extern PACKAGE TFuncKey FKeys1[64];
extern PACKAGE TFuncKey FKeys2[64];
extern PACKAGE TFuncKey FKeys3[64];
extern PACKAGE char ibm_iso8859_1_G0[256];
extern PACKAGE char ibm_iso8859_1_G1[256];
extern PACKAGE char Output[256];
extern PACKAGE void __fastcall Register(void);
extern PACKAGE bool __fastcall AddFKey(TFuncKey * FKeys, char ScanCode, Classes::TShiftState Shift, bool Ext,  TFuncKeyValue &Value);
extern PACKAGE void __fastcall FKeysToFile(TFuncKey * FKeys, AnsiString FName);
extern PACKAGE void __fastcall FileToFKeys(TFuncKey * FKeys, AnsiString FName);

}	/* namespace Emulvt */
using namespace Emulvt;
#pragma option pop	// -w-
#pragma option pop	// -Vx

#pragma delphiheader end.
//-- end unit ----------------------------------------------------------------
#endif	// EmulVT
