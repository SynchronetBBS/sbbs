object MailForm: TMailForm
  Left = 670
  Top = 170
  Width = 350
  Height = 150
  Caption = 'Mail Server'
  Color = clBtnFace
  UseDockManager = True
  DragKind = dkDock
  DragMode = dmAutomatic
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -14
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  Position = poDefault
  OnHide = FormHide
  PixelsPerInch = 120
  TextHeight = 16
  object ToolBar: TToolBar
    Left = 0
    Top = 0
    Width = 342
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
    object StopButton: TToolButton
      Left = 23
      Top = 0
      Action = MainForm.MailStop
      ParentShowHint = False
      ShowHint = True
    end
    object RecycleButton: TToolButton
      Left = 46
      Top = 0
      Action = MainForm.MailRecycle
    end
    object ToolButton4: TToolButton
      Left = 69
      Top = 0
      Width = 8
      Caption = 'ToolButton4'
      ImageIndex = 3
      Style = tbsSeparator
    end
    object ConfigureButton: TToolButton
      Left = 77
      Top = 0
      Action = MainForm.MailConfigure
    end
    object ToolButton6: TToolButton
      Left = 100
      Top = 0
      Width = 8
      Caption = 'ToolButton6'
      ImageIndex = 5
      Style = tbsSeparator
    end
    object Status: TStaticText
      Left = 108
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
      Left = 258
      Top = 0
      Width = 8
      Caption = 'ToolButton1'
      ImageIndex = 6
      Style = tbsSeparator
    end
    object ProgressBar: TProgressBar
      Left = 266
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
  end
  object Log: TRichEdit
    Left = 0
    Top = 25
    Width = 342
    Height = 92
    Align = alClient
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindowText
    Font.Height = -14
    Font.Name = 'MS Sans Serif'
    Font.Style = []
    HideScrollBars = False
    ParentFont = False
    ReadOnly = True
    ScrollBars = ssBoth
    TabOrder = 1
    WordWrap = False
  end
end
