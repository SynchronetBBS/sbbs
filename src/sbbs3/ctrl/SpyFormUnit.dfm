object SpyForm: TSpyForm
  Left = 244
  Top = 195
  Width = 536
  Height = 336
  Caption = 'Spy'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -13
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  OnDestroy = FormDestroy
  OnShow = FormShow
  PixelsPerInch = 120
  TextHeight = 16
  object Log: TMemo
    Left = 0
    Top = 0
    Width = 528
    Height = 304
    Align = alClient
    Color = clBlack
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWhite
    Font.Height = -13
    Font.Name = 'Terminal'
    Font.Style = []
    ParentFont = False
    ReadOnly = True
    ScrollBars = ssBoth
    TabOrder = 0
  end
  object Timer: TTimer
    Enabled = False
    Interval = 500
    OnTimer = SpyTimerTick
    Left = 72
    Top = 48
  end
end
