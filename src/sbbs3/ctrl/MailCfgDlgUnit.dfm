object MailCfgDlg: TMailCfgDlg
  Left = 163
  Top = 575
  BorderStyle = bsDialog
  Caption = 'Mail Server Configuration'
  ClientHeight = 246
  ClientWidth = 286
  Color = clBtnFace
  ParentFont = True
  OldCreateOrder = True
  Position = poScreenCenter
  OnShow = FormShow
  PixelsPerInch = 96
  TextHeight = 13
  object OKBtn: TButton
    Left = 20
    Top = 210
    Width = 75
    Height = 25
    Anchors = [akLeft, akBottom]
    Caption = 'OK'
    Default = True
    ModalResult = 1
    TabOrder = 0
    OnClick = OKBtnClick
  end
  object CancelBtn: TButton
    Left = 104
    Top = 210
    Width = 76
    Height = 25
    Anchors = [akLeft, akBottom]
    Cancel = True
    Caption = 'Cancel'
    ModalResult = 2
    TabOrder = 1
  end
  object ApplyButton: TButton
    Left = 189
    Top = 210
    Width = 75
    Height = 25
    Anchors = [akLeft, akBottom]
    Caption = 'Apply'
    TabOrder = 2
    OnClick = OKBtnClick
  end
  object PageControl: TPageControl
    Left = 3
    Top = 3
    Width = 278
    Height = 199
    ActivePage = SMTPTabSheet
    TabOrder = 3
    object GeneralTabSheet: TTabSheet
      Caption = 'General'
      object InterfaceLabel: TLabel
        Left = 7
        Top = 37
        Width = 85
        Height = 20
        AutoSize = False
        Caption = 'Interface (IP)'
        ParentShowHint = False
        ShowHint = True
      end
      object MaxClientsLabel: TLabel
        Left = 7
        Top = 63
        Width = 85
        Height = 20
        AutoSize = False
        Caption = 'Max Clients'
      end
      object MaxInactivityLabel: TLabel
        Left = 7
        Top = 89
        Width = 85
        Height = 20
        AutoSize = False
        Caption = 'Max Inactivity'
      end
      object Label1: TLabel
        Left = 7
        Top = 115
        Width = 85
        Height = 20
        AutoSize = False
        Caption = 'Attempt Delivery'
      end
      object Label2: TLabel
        Left = 7
        Top = 141
        Width = 85
        Height = 20
        AutoSize = False
        Caption = 'Rescan Freq'
      end
      object AutoStartCheckBox: TCheckBox
        Left = 7
        Top = 10
        Width = 118
        Height = 19
        Hint = 
          'Check this box if you want the mail server to start up automatic' +
          'ally'
        Caption = 'Auto Startup'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object NetworkInterfaceEdit: TEdit
        Left = 92
        Top = 37
        Width = 156
        Height = 21
        Hint = 
          'Enter your Network adapter'#39's static IP address here or blank for' +
          ' <ANY>'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object MaxClientsEdit: TEdit
        Left = 92
        Top = 63
        Width = 39
        Height = 21
        Hint = 'Maximum Number of simultaneous clients (default=10)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object MaxInactivityEdit: TEdit
        Left = 92
        Top = 89
        Width = 39
        Height = 21
        Hint = 'Maximum Number of seconds of inactivity before disconnect'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object HostnameCheckBox: TCheckBox
        Left = 150
        Top = 10
        Width = 119
        Height = 19
        Hint = 
          'Check this box if you want to automatically lookup client'#39's host' +
          'names via DNS'
        Caption = 'Hostname Lookup'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object DebugTXCheckBox: TCheckBox
        Left = 150
        Top = 60
        Width = 124
        Height = 20
        Hint = 
          'Check this box to log all transmitted mail commands and response' +
          's (for debugging)'
        Caption = 'Log TX'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 5
      end
      object DebugHeadersCheckBox: TCheckBox
        Left = 150
        Top = 88
        Width = 124
        Height = 19
        Hint = 'Check this box to log all received mail headers (for debugging)'
        Caption = 'Log RX Headers'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 6
      end
      object LogFileCheckBox: TCheckBox
        Left = 150
        Top = 114
        Width = 124
        Height = 19
        Hint = 
          'Check this box to save log entries to a file (in your DATA direc' +
          'tory)'
        Caption = 'Log to Disk'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 7
      end
      object DeliveryAttemptsEdit: TEdit
        Left = 92
        Top = 115
        Width = 39
        Height = 21
        Hint = 'Maximum number of delivery attempts'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 8
      end
      object RescanFreqEdit: TEdit
        Left = 92
        Top = 141
        Width = 39
        Height = 21
        Hint = 'Seconds between message base rescans'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 9
      end
    end
    object SMTPTabSheet: TTabSheet
      Caption = 'SMTP'
      ImageIndex = 1
      object SpamFilterLabel: TLabel
        Left = 7
        Top = 36
        Width = 57
        Height = 13
        Caption = 'Spam Filters'
      end
      object TelnetPortLabel: TLabel
        Left = 7
        Top = 10
        Width = 85
        Height = 19
        AutoSize = False
        Caption = 'Port'
      end
      object RBLCheckBox: TCheckBox
        Left = 91
        Top = 36
        Width = 46
        Height = 19
        Hint = 'Use Realtime Blackhole List (RBL) Spam Filter'
        Caption = 'RBL'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object RSSCheckBox: TCheckBox
        Left = 150
        Top = 36
        Width = 46
        Height = 19
        Hint = 'Use Relay Spam Stopper (RSS) Spam Filter'
        Caption = 'RSS'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object DULCheckBox: TCheckBox
        Left = 208
        Top = 36
        Width = 46
        Height = 19
        Hint = 'Use Dail-up User List (DUL) Spam Filter'
        Caption = 'DUL'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object SMTPPortEdit: TEdit
        Left = 92
        Top = 10
        Width = 39
        Height = 21
        Hint = 
          'Enter the port number for the SMTP server to listen on (default=' +
          '25)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object UserNumberCheckBox: TCheckBox
        Left = 150
        Top = 10
        Width = 124
        Height = 19
        Hint = 'Allow mail to be sent to users by number (i.e "1@yourbbs.com")'
        Caption = 'RX by User Number'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object DNSRadioButton: TRadioButton
        Left = 7
        Top = 59
        Width = 85
        Height = 19
        Hint = 
          'Check this box to send mail directly to user'#39's mail servers (req' +
          'uires DNS Server access)'
        Caption = 'DNS Server'
        Checked = True
        ParentShowHint = False
        ShowHint = True
        TabOrder = 5
        TabStop = True
        OnClick = DNSRadioButtonClick
      end
      object DNSServerEdit: TEdit
        Left = 92
        Top = 62
        Width = 156
        Height = 21
        Hint = 'Enter the host name or IP address of your DNS server'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 6
      end
      object RelayRadioButton: TRadioButton
        Left = 7
        Top = 85
        Width = 85
        Height = 19
        Hint = 'Check this box to route all mail through your ISP'#39's SMTP Server'
        Caption = 'Relay Server'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 7
        OnClick = DNSRadioButtonClick
      end
      object SMTPServerEdit: TEdit
        Left = 92
        Top = 88
        Width = 156
        Height = 21
        Hint = 
          'Enter the host name or IP address of your ISP'#39's SMTP server (for' +
          ' relaying mail)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 8
      end
    end
    object POP3TabSheet: TTabSheet
      Caption = 'POP3'
      ImageIndex = 2
      object POP3PortLabel: TLabel
        Left = 7
        Top = 10
        Width = 83
        Height = 19
        AutoSize = False
        Caption = 'Port'
      end
      object POP3PortEdit: TEdit
        Left = 92
        Top = 10
        Width = 39
        Height = 21
        Hint = 
          'Enter the port number for the POP3 server to listen on (default=' +
          '110)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object POP3LogCheckBox: TCheckBox
        Left = 150
        Top = 10
        Width = 57
        Height = 19
        Hint = 'Check this box if you want POP3 sessions logged'
        Caption = 'Log'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object POP3EnabledCheckBox: TCheckBox
        Left = 208
        Top = 10
        Width = 66
        Height = 19
        Hint = 
          'Check this box to enable the POP3 server (requires restart of ma' +
          'il server)'
        Caption = 'Enabled'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
        OnClick = POP3EnabledCheckBoxClick
      end
    end
    object SoundTabSheet: TTabSheet
      Caption = 'Sound'
      ImageIndex = 3
      object POP3SoundLabel: TLabel
        Left = 7
        Top = 11
        Width = 52
        Height = 19
        AutoSize = False
        Caption = 'POP3'
      end
      object InboundSoundLabel: TLabel
        Left = 7
        Top = 37
        Width = 58
        Height = 19
        AutoSize = False
        Caption = 'SMTP In'
      end
      object OutboundSoundLabel: TLabel
        Left = 7
        Top = 63
        Width = 58
        Height = 19
        AutoSize = False
        Caption = 'SMTP Out'
      end
      object POP3SoundEdit: TEdit
        Left = 72
        Top = 11
        Width = 169
        Height = 21
        Hint = 'WAV file to play when accepting POP3 connections'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object POP3SoundButton: TButton
        Left = 247
        Top = 11
        Width = 20
        Height = 21
        Caption = '...'
        TabOrder = 1
        OnClick = POP3SoundButtonClick
      end
      object InboundSoundEdit: TEdit
        Left = 72
        Top = 37
        Width = 168
        Height = 21
        Hint = 'WAV file to play when inbound SMTP connections are accepted'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object InboundSoundButton: TButton
        Left = 247
        Top = 37
        Width = 20
        Height = 21
        Caption = '...'
        TabOrder = 3
        OnClick = InboundSoundButtonClick
      end
      object OutboundSoundButton: TButton
        Left = 247
        Top = 63
        Width = 20
        Height = 21
        Caption = '...'
        TabOrder = 4
        OnClick = OutboundSoundButtonClick
      end
      object OutboundSoundEdit: TEdit
        Left = 72
        Top = 63
        Width = 168
        Height = 21
        Hint = 'WAV file to play when sending mail'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 5
      end
    end
  end
  object OpenDialog: TOpenDialog
    Filter = 'Wave Files|*.wav'
    Left = 8
    Top = 480
  end
end
