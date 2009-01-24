object FtpForm: TFtpForm
  Left = 856
  Top = 689
  Width = 350
  Height = 150
  Caption = 'FTP Server'
  Color = clBtnFace
  DragKind = dkDock
  DragMode = dmAutomatic
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -13
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
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
    EdgeInner = esLowered
    Flat = True
    Images = MainForm.ImageList
    ParentShowHint = False
    ShowHint = True
    TabOrder = 0
    object StartButton: TToolButton
      Left = 0
      Top = 0
      Action = MainForm.FtpStart
    end
    object StopButton: TToolButton
      Left = 23
      Top = 0
      Action = MainForm.FtpStop
    end
    object RecycleButton: TToolButton
      Left = 46
      Top = 0
      Action = MainForm.FtpRecycle
    end
    object ToolButton1: TToolButton
      Left = 69
      Top = 0
      Width = 8
      Caption = 'ToolButton1'
      ImageIndex = 3
      Style = tbsSeparator
    end
    object ConfigureButton: TToolButton
      Left = 77
      Top = 0
      Action = MainForm.FtpConfigure
    end
    object ToolButton2: TToolButton
      Left = 100
      Top = 0
      Width = 8
      Caption = 'ToolButton2'
      ImageIndex = 5
      Style = tbsSeparator
    end
    object Status: TStaticText
      Left = 108
      Top = 0
      Width = 150
      Height = 22
      Hint = 'FTP Server Status'
      AutoSize = False
      BorderStyle = sbsSunken
      Caption = 'Down'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 0
    end
    object ToolButton3: TToolButton
      Left = 258
      Top = 0
      Width = 8
      Caption = 'ToolButton3'
      ImageIndex = 6
      Style = tbsSeparator
    end
    object ProgressBar: TProgressBar
      Left = 266
      Top = 0
      Width = 75
      Height = 22
      Hint = 'FTP Server Utilization'
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
