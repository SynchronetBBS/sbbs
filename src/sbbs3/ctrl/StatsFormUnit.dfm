object StatsForm: TStatsForm
  Left = 544
  Top = 389
  BorderStyle = bsSingle
  Caption = 'Statistics'
  ClientHeight = 196
  ClientWidth = 489
  Color = clBtnFace
  UseDockManager = True
  DragKind = dkDock
  DragMode = dmAutomatic
  Font.Charset = ANSI_CHARSET
  Font.Color = clWindowText
  Font.Height = -13
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  OnHide = FormHide
  PixelsPerInch = 120
  TextHeight = 16
  object Today: TGroupBox
    Left = 9
    Top = 9
    Width = 152
    Height = 176
    Caption = 'Today'
    Color = clBtnFace
    Font.Charset = ANSI_CHARSET
    Font.Color = clWindowText
    Font.Height = -15
    Font.Name = 'MS Sans Serif'
    Font.Style = []
    ParentColor = False
    ParentFont = False
    TabOrder = 0
    object LogonsTodayLabel: TLabel
      Left = 9
      Top = 25
      Width = 73
      Height = 16
      AutoSize = False
      Caption = 'Logons'
    end
    object TimeTodayLabel: TLabel
      Left = 9
      Top = 48
      Width = 73
      Height = 16
      AutoSize = False
      Caption = 'Time'
    end
    object EmailTodayLabel: TLabel
      Left = 9
      Top = 73
      Width = 73
      Height = 16
      AutoSize = False
      Caption = 'E-mail'
    end
    object FeedbackTodayLabel: TLabel
      Left = 9
      Top = 96
      Width = 73
      Height = 16
      AutoSize = False
      Caption = 'Feedback'
    end
    object NewUsersTodayLabel: TLabel
      Left = 9
      Top = 121
      Width = 73
      Height = 16
      AutoSize = False
      Caption = 'New Users'
    end
    object PostsTodayLabel: TLabel
      Left = 9
      Top = 144
      Width = 73
      Height = 16
      AutoSize = False
      Caption = 'Posts'
    end
    object LogonsToday: TStaticText
      Left = 90
      Top = 25
      Width = 50
      Height = 19
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 0
    end
    object TimeToday: TStaticText
      Left = 90
      Top = 48
      Width = 50
      Height = 20
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 1
    end
    object EMailToday: TStaticText
      Left = 90
      Top = 73
      Width = 50
      Height = 19
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 2
    end
    object FeedbackToday: TStaticText
      Left = 90
      Top = 96
      Width = 50
      Height = 20
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 3
    end
    object NewUsersToday: TStaticText
      Left = 90
      Top = 121
      Width = 50
      Height = 19
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 4
    end
    object PostsToday: TStaticText
      Left = 90
      Top = 144
      Width = 50
      Height = 20
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 5
    end
  end
  object Total: TGroupBox
    Left = 169
    Top = 9
    Width = 152
    Height = 176
    Caption = 'Total'
    TabOrder = 1
    object Label1: TLabel
      Left = 9
      Top = 25
      Width = 73
      Height = 16
      AutoSize = False
      Caption = 'Logons'
    end
    object Label2: TLabel
      Left = 9
      Top = 48
      Width = 73
      Height = 16
      AutoSize = False
      Caption = 'Time'
    end
    object Label3: TLabel
      Left = 9
      Top = 73
      Width = 73
      Height = 16
      AutoSize = False
      Caption = 'E-mail'
    end
    object Label4: TLabel
      Left = 9
      Top = 96
      Width = 73
      Height = 16
      AutoSize = False
      Caption = 'Feedback'
    end
    object Label5: TLabel
      Left = 9
      Top = 121
      Width = 73
      Height = 16
      AutoSize = False
      Caption = 'Users'
    end
    object TotalLogons: TStaticText
      Left = 90
      Top = 25
      Width = 50
      Height = 19
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 0
    end
    object TotalTimeOn: TStaticText
      Left = 90
      Top = 48
      Width = 50
      Height = 20
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 1
    end
    object TotalEMail: TStaticText
      Left = 90
      Top = 73
      Width = 50
      Height = 19
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 2
    end
    object TotalFeedback: TStaticText
      Left = 90
      Top = 96
      Width = 50
      Height = 20
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 3
    end
    object TotalUsers: TStaticText
      Left = 90
      Top = 121
      Width = 50
      Height = 19
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 4
    end
    object LogButton: TButton
      Left = 10
      Top = 146
      Width = 130
      Height = 25
      Caption = ' View Log'
      TabOrder = 5
      OnClick = LogButtonClick
    end
  end
  object UploadsToday: TGroupBox
    Left = 329
    Top = 9
    Width = 152
    Height = 80
    Caption = 'Uploads Today'
    TabOrder = 2
    object UploadedFilesLabel: TLabel
      Left = 9
      Top = 25
      Width = 73
      Height = 16
      AutoSize = False
      Caption = 'Files'
    end
    object UploadedBytesLabel: TLabel
      Left = 9
      Top = 48
      Width = 72
      Height = 16
      AutoSize = False
      Caption = 'Bytes'
    end
    object UploadedFiles: TStaticText
      Left = 90
      Top = 25
      Width = 50
      Height = 19
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 0
    end
    object UploadedBytes: TStaticText
      Left = 90
      Top = 48
      Width = 50
      Height = 20
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 1
    end
  end
  object DownloadsToday: TGroupBox
    Left = 329
    Top = 105
    Width = 152
    Height = 80
    Caption = 'Downloads Today'
    TabOrder = 3
    object DownloadedFilesLabel: TLabel
      Left = 9
      Top = 25
      Width = 73
      Height = 16
      AutoSize = False
      Caption = 'Files'
    end
    object DownloadedBytesLabel: TLabel
      Left = 9
      Top = 48
      Width = 73
      Height = 16
      AutoSize = False
      Caption = 'Bytes'
    end
    object DownloadedFiles: TStaticText
      Left = 90
      Top = 25
      Width = 50
      Height = 19
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 0
    end
    object DownloadedBytes: TStaticText
      Left = 90
      Top = 48
      Width = 50
      Height = 20
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 1
    end
  end
end
