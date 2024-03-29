object MailForm: TMailForm
  Left = 735
  Top = 317
  Width = 480
  Height = 150
  Caption = 'Mail Server'
  Color = clBtnFace
  UseDockManager = True
  DefaultMonitor = dmPrimary
  DragKind = dkDock
  DragMode = dmAutomatic
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -12
  Font.Name = 'Microsoft Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  Position = poDefault
  OnHide = FormHide
  PixelsPerInch = 96
  TextHeight = 13
  object ToolBar: TToolBar
    Left = 0
    Top = 0
    Width = 464
    Height = 25
    Caption = 'ToolBar'
    EdgeBorders = []
    Flat = True
    Images = MainForm.ImageList
    ParentShowHint = False
    ShowHint = True
    TabOrder = 0
    object StartButton: TToolButton
      Left = 0
      Top = 0
      Action = MainForm.MailStart
      ParentShowHint = False
      ShowHint = True
    end
    object LogPauseButton: TToolButton
      Left = 23
      Top = 0
      Action = MainForm.MailPause
      Style = tbsCheck
    end
    object StopButton: TToolButton
      Left = 46
      Top = 0
      Action = MainForm.MailStop
      ParentShowHint = False
      ShowHint = True
    end
    object RecycleButton: TToolButton
      Left = 69
      Top = 0
      Action = MainForm.MailRecycle
    end
    object ToolButton4: TToolButton
      Left = 92
      Top = 0
      Width = 8
      Caption = 'ToolButton4'
      ImageIndex = 3
      Style = tbsSeparator
    end
    object ConfigureButton: TToolButton
      Left = 100
      Top = 0
      Action = MainForm.MailConfigure
    end
    object ToolButton6: TToolButton
      Left = 123
      Top = 0
      Width = 8
      Caption = 'ToolButton6'
      ImageIndex = 5
      Style = tbsSeparator
    end
    object Status: TStaticText
      Left = 131
      Top = 0
      Width = 150
      Height = 22
      Hint = 'Mail Server Status'
      AutoSize = False
      BorderStyle = sbsSunken
      Caption = 'Down'
      TabOrder = 0
    end
    object ToolButton1: TToolButton
      Left = 281
      Top = 0
      Width = 8
      Caption = 'ToolButton1'
      ImageIndex = 6
      Style = tbsSeparator
    end
    object ProgressBar: TProgressBar
      Left = 289
      Top = 0
      Width = 75
      Height = 22
      Hint = 'Mail Server Utilization'
      Min = 0
      Max = 100
      Smooth = True
      Step = 1
      TabOrder = 1
    end
    object ToolButton2: TToolButton
      Left = 364
      Top = 0
      Width = 8
      Caption = 'ToolButton2'
      ImageIndex = 7
      Style = tbsSeparator
    end
    object LogLevelText: TStaticText
      Left = 372
      Top = 0
      Width = 75
      Height = 22
      Hint = 'Log Level'
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 2
    end
    object LogLevelUpDown: TUpDown
      Left = 447
      Top = 0
      Width = 16
      Height = 22
      Hint = 'Log Level Adjustment'
      Min = 0
      Max = 7
      Position = 0
      TabOrder = 3
      Wrap = False
      OnChangingEx = LogLevelUpDownChangingEx
    end
  end
  object Log: TRichEdit
    Left = 0
    Top = 25
    Width = 464
    Height = 86
    Align = alClient
    Color = clBlack
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWhite
    Font.Height = -12
    Font.Name = 'Microsoft Sans Serif'
    Font.Style = []
    HideScrollBars = False
    ParentFont = False
    PopupMenu = MainForm.LogPopupMenu
    ReadOnly = True
    ScrollBars = ssBoth
    TabOrder = 1
    WordWrap = False
  end
end
