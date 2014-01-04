object StatsForm: TStatsForm
  Left = 544
  Top = 389
  BorderStyle = bsSingle
  Caption = 'Statistics'
  ClientHeight = 159
  ClientWidth = 397
  Color = clBtnFace
  UseDockManager = True
  DefaultMonitor = dmPrimary
  DragKind = dkDock
  DragMode = dmAutomatic
  Font.Charset = ANSI_CHARSET
  Font.Color = clWindowText
  Font.Height = -10
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  OnHide = FormHide
  PixelsPerInch = 96
  TextHeight = 13
  object Today: TGroupBox
    Left = 7
    Top = 7
    Width = 124
    Height = 143
    Caption = 'Today'
    Color = clBtnFace
    Font.Charset = ANSI_CHARSET
    Font.Color = clWindowText
    Font.Height = -12
    Font.Name = 'MS Sans Serif'
    Font.Style = []
    ParentColor = False
    ParentFont = False
    TabOrder = 0
    object LogonsTodayLabel: TLabel
      Left = 7
      Top = 20
      Width = 60
      Height = 13
      AutoSize = False
      Caption = 'Logons'
    end
    object TimeTodayLabel: TLabel
      Left = 7
      Top = 39
      Width = 60
      Height = 13
      AutoSize = False
      Caption = 'Time'
    end
    object EmailTodayLabel: TLabel
      Left = 7
      Top = 59
      Width = 60
      Height = 13
      AutoSize = False
      Caption = 'E-mail'
    end
    object FeedbackTodayLabel: TLabel
      Left = 7
      Top = 78
      Width = 60
      Height = 13
      AutoSize = False
      Caption = 'Feedback'
    end
    object NewUsersTodayLabel: TLabel
      Left = 7
      Top = 98
      Width = 60
      Height = 13
      AutoSize = False
      Caption = 'New Users'
    end
    object PostsTodayLabel: TLabel
      Left = 7
      Top = 117
      Width = 60
      Height = 13
      AutoSize = False
      Caption = 'Posts'
    end
    object LogonsToday: TStaticText
      Left = 73
      Top = 20
      Width = 41
      Height = 16
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 0
    end
    object TimeToday: TStaticText
      Left = 73
      Top = 39
      Width = 41
      Height = 16
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 1
    end
    object EMailToday: TStaticText
      Left = 73
      Top = 59
      Width = 41
      Height = 16
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 2
    end
    object FeedbackToday: TStaticText
      Left = 73
      Top = 78
      Width = 41
      Height = 16
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 3
    end
    object NewUsersToday: TStaticText
      Left = 73
      Top = 98
      Width = 41
      Height = 16
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 4
    end
    object PostsToday: TStaticText
      Left = 73
      Top = 117
      Width = 41
      Height = 16
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 5
    end
  end
  object Total: TGroupBox
    Left = 137
    Top = 7
    Width = 124
    Height = 143
    Caption = 'Total'
    TabOrder = 1
    object Label1: TLabel
      Left = 7
      Top = 20
      Width = 60
      Height = 13
      AutoSize = False
      Caption = 'Logons'
    end
    object Label2: TLabel
      Left = 7
      Top = 39
      Width = 60
      Height = 13
      AutoSize = False
      Caption = 'Time'
    end
    object Label3: TLabel
      Left = 7
      Top = 59
      Width = 60
      Height = 13
      AutoSize = False
      Caption = 'E-mail'
    end
    object Label4: TLabel
      Left = 7
      Top = 78
      Width = 60
      Height = 13
      AutoSize = False
      Caption = 'Feedback'
    end
    object Label5: TLabel
      Left = 7
      Top = 98
      Width = 60
      Height = 13
      AutoSize = False
      Caption = 'Users'
    end
    object TotalLogons: TStaticText
      Left = 73
      Top = 20
      Width = 41
      Height = 16
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 0
    end
    object TotalTimeOn: TStaticText
      Left = 73
      Top = 39
      Width = 41
      Height = 16
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 1
    end
    object TotalEMail: TStaticText
      Left = 73
      Top = 59
      Width = 41
      Height = 16
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 2
    end
    object TotalFeedback: TStaticText
      Left = 73
      Top = 78
      Width = 41
      Height = 16
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 3
    end
    object TotalUsers: TStaticText
      Left = 73
      Top = 98
      Width = 41
      Height = 16
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 4
    end
    object LogButton: TButton
      Left = 8
      Top = 119
      Width = 106
      Height = 20
      Caption = ' View Log'
      TabOrder = 5
      OnClick = LogButtonClick
    end
  end
  object UploadsToday: TGroupBox
    Left = 267
    Top = 7
    Width = 124
    Height = 65
    Caption = 'Uploads Today'
    TabOrder = 2
    object UploadedFilesLabel: TLabel
      Left = 7
      Top = 20
      Width = 60
      Height = 13
      AutoSize = False
      Caption = 'Files'
    end
    object UploadedBytesLabel: TLabel
      Left = 7
      Top = 39
      Width = 59
      Height = 13
      AutoSize = False
      Caption = 'Bytes'
    end
    object UploadedFiles: TStaticText
      Left = 73
      Top = 20
      Width = 41
      Height = 16
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 0
    end
    object UploadedBytes: TStaticText
      Left = 73
      Top = 39
      Width = 41
      Height = 16
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 1
    end
  end
  object DownloadsToday: TGroupBox
    Left = 267
    Top = 85
    Width = 124
    Height = 65
    Caption = 'Downloads Today'
    TabOrder = 3
    object DownloadedFilesLabel: TLabel
      Left = 7
      Top = 20
      Width = 60
      Height = 13
      AutoSize = False
      Caption = 'Files'
    end
    object DownloadedBytesLabel: TLabel
      Left = 7
      Top = 39
      Width = 60
      Height = 13
      AutoSize = False
      Caption = 'Bytes'
    end
    object DownloadedFiles: TStaticText
      Left = 73
      Top = 20
      Width = 41
      Height = 16
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 0
    end
    object DownloadedBytes: TStaticText
      Left = 73
      Top = 39
      Width = 41
      Height = 16
      AutoSize = False
      BorderStyle = sbsSunken
      TabOrder = 1
    end
  end
end
