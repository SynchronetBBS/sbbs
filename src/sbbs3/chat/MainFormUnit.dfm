object MainForm: TMainForm
  Left = 787
  Top = 244
  Width = 417
  Height = 251
  Caption = 'Synchronet Chat'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  Position = poDefaultPosOnly
  OnClose = FormClose
  OnCreate = FormCreate
  OnShow = FormShow
  PixelsPerInch = 96
  TextHeight = 13
  object Splitter1: TSplitter
    Left = 0
    Top = 85
    Width = 401
    Height = 2
    Cursor = crVSplit
    Align = alTop
  end
  object Local: TMemo
    Left = 0
    Top = 87
    Width = 401
    Height = 126
    Align = alClient
    Color = clBlack
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clLime
    Font.Height = -12
    Font.Name = 'System'
    Font.Pitch = fpFixed
    Font.Style = []
    ParentFont = False
    ReadOnly = True
    ScrollBars = ssVertical
    TabOrder = 0
    WantTabs = True
    OnClick = LocalEnter
    OnEnter = LocalEnter
    OnKeyPress = LocalKeyPress
  end
  object Remote: TMemo
    Left = 0
    Top = 0
    Width = 401
    Height = 85
    TabStop = False
    Align = alTop
    Color = clBlack
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clGreen
    Font.Height = -12
    Font.Name = 'System'
    Font.Pitch = fpFixed
    Font.Style = []
    HideSelection = False
    ParentFont = False
    ReadOnly = True
    ScrollBars = ssVertical
    TabOrder = 1
  end
  object InputTimer: TTimer
    OnTimer = InputTimerTick
    Left = 72
    Top = 40
  end
  object Timer: TTimer
    Enabled = False
    OnTimer = TimerTick
    Left = 128
    Top = 48
  end
end
