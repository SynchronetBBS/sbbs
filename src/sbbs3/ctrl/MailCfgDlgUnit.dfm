object MailCfgDlg: TMailCfgDlg
  Left = 492
  Top = 423
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
    ActivePage = GeneralTabSheet
    TabOrder = 3
    object GeneralTabSheet: TTabSheet
      Caption = 'General'
      object InterfaceLabel: TLabel
        Left = 7
        Top = 36
        Width = 85
        Height = 20
        AutoSize = False
        Caption = 'Interface (IP)'
        ParentShowHint = False
        ShowHint = True
      end
      object MaxClientsLabel: TLabel
        Left = 7
        Top = 62
        Width = 85
        Height = 20
        AutoSize = False
        Caption = 'Max Clients'
      end
      object MaxInactivityLabel: TLabel
        Left = 7
        Top = 88
        Width = 85
        Height = 20
        AutoSize = False
        Caption = 'Max Inactivity'
      end
      object AutoStartCheckBox: TCheckBox
        Left = 7
        Top = 10
        Width = 118
        Height = 21
        Hint = 'Automatically startup mail servers'
        Caption = 'Auto Startup'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object NetworkInterfaceEdit: TEdit
        Left = 92
        Top = 36
        Width = 150
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
        Top = 62
        Width = 39
        Height = 21
        Hint = 'Maximum number of simultaneous clients (default=10)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object MaxInactivityEdit: TEdit
        Left = 92
        Top = 88
        Width = 39
        Height = 21
        Hint = 
          'Maximum number of seconds of inactivity before disconnect (defau' +
          'lt=120)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object HostnameCheckBox: TCheckBox
        Left = 150
        Top = 10
        Width = 119
        Height = 21
        Hint = 'Automatically lookup client'#39's hostnames via DNS'
        Caption = 'Hostname Lookup'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object DebugTXCheckBox: TCheckBox
        Left = 150
        Top = 62
        Width = 119
        Height = 21
        Hint = 'Log all transmitted mail commands and responses (for debugging)'
        Caption = 'Log TX'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 5
      end
      object LogFileCheckBox: TCheckBox
        Left = 150
        Top = 88
        Width = 119
        Height = 21
        Hint = 'Save log entries to a file (in your DATA directory)'
        Caption = 'Log to Disk'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 6
      end
    end
    object SMTPTabSheet: TTabSheet
      Caption = 'SMTP'
      ImageIndex = 1
      object SpamFilterLabel: TLabel
        Left = 7
        Top = 36
        Width = 57
        Height = 21
        Caption = 'Spam Filters'
      end
      object TelnetPortLabel: TLabel
        Left = 7
        Top = 10
        Width = 85
        Height = 21
        AutoSize = False
        Caption = 'Listening Port'
      end
      object InboundSoundLabel: TLabel
        Left = 7
        Top = 140
        Width = 82
        Height = 19
        AutoSize = False
        Caption = 'Receive Sound'
      end
      object RBLCheckBox: TCheckBox
        Left = 92
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
        Hint = 'TCP port number for incoming SMTP connections (default=25)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object UserNumberCheckBox: TCheckBox
        Left = 150
        Top = 10
        Width = 120
        Height = 19
        Hint = 
          'Allow mail to be received for users by number (i.e "1@yourbbs.co' +
          'm")'
        Caption = 'RX by User Number'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object InboundSoundEdit: TEdit
        Left = 92
        Top = 140
        Width = 150
        Height = 21
        Hint = 'Sound file to play when inbound SMTP connections are accepted'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 5
      end
      object InboundSoundButton: TButton
        Left = 247
        Top = 140
        Width = 20
        Height = 21
        Caption = '...'
        TabOrder = 6
        OnClick = InboundSoundButtonClick
      end
      object DebugHeadersCheckBox: TCheckBox
        Left = 150
        Top = 62
        Width = 119
        Height = 21
        Hint = 'Log all received mail headers (for debugging)'
        Caption = 'Log Headers'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 7
      end
    end
    object POP3TabSheet: TTabSheet
      Caption = 'POP3'
      ImageIndex = 2
      object POP3PortLabel: TLabel
        Left = 7
        Top = 10
        Width = 83
        Height = 21
        AutoSize = False
        Caption = 'Listening Port'
      end
      object POP3SoundLabel: TLabel
        Left = 7
        Top = 140
        Width = 82
        Height = 21
        AutoSize = False
        Caption = 'Login Sound'
      end
      object POP3PortEdit: TEdit
        Left = 92
        Top = 10
        Width = 39
        Height = 21
        Hint = 'TCP port number for incoming POP3 connections (default=110)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object POP3LogCheckBox: TCheckBox
        Left = 150
        Top = 10
        Width = 57
        Height = 19
        Hint = 'Log all POP3 user activity'
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
        Hint = 'Enable the POP3 server (requires restart of mail server)'
        Caption = 'Enabled'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
        OnClick = POP3EnabledCheckBoxClick
      end
      object POP3SoundEdit: TEdit
        Left = 92
        Top = 140
        Width = 150
        Height = 21
        Hint = 'Sound file to play when accepting POP3 connections'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object POP3SoundButton: TButton
        Left = 247
        Top = 140
        Width = 20
        Height = 21
        Caption = '...'
        TabOrder = 4
        OnClick = POP3SoundButtonClick
      end
    end
    object SendMailTabSheet: TTabSheet
      Caption = 'SendMail'
      ImageIndex = 3
      object OutboundSoundLabel: TLabel
        Left = 7
        Top = 140
        Width = 82
        Height = 19
        AutoSize = False
        Caption = 'Sending Sound'
      end
      object RelayPortLabel: TLabel
        Left = 160
        Top = 114
        Width = 32
        Height = 21
        Alignment = taRightJustify
        AutoSize = False
        Caption = 'Port'
      end
      object Label1: TLabel
        Left = 7
        Top = 10
        Width = 85
        Height = 20
        AutoSize = False
        Caption = 'Max Attempts'
      end
      object Label2: TLabel
        Left = 136
        Top = 10
        Width = 57
        Height = 20
        Hint = 'Frequency (in seconds) of delivery attempts'
        Alignment = taRightJustify
        AutoSize = False
        Caption = 'Frequency'
      end
      object OutboundSoundButton: TButton
        Left = 247
        Top = 140
        Width = 20
        Height = 21
        Caption = '...'
        TabOrder = 0
        OnClick = OutboundSoundButtonClick
      end
      object OutboundSoundEdit: TEdit
        Left = 92
        Top = 140
        Width = 150
        Height = 21
        Hint = 'Sound file to play when sending mail'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object DNSRadioButton: TRadioButton
        Left = 7
        Top = 36
        Width = 85
        Height = 21
        Hint = 
          'Send mail directly to addressed mail server (requires DNS server' +
          ' access)'
        Caption = 'DNS Server'
        Checked = True
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
        TabStop = True
        OnClick = DNSRadioButtonClick
      end
      object DNSServerEdit: TEdit
        Left = 92
        Top = 36
        Width = 150
        Height = 21
        Hint = 'Host name or IP address of your ISP'#39's DNS server'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object TcpDnsCheckBox: TCheckBox
        Left = 203
        Top = 62
        Width = 53
        Height = 21
        Hint = 'Use TCP packets (instead of UDP) for DNS queries'
        Caption = 'TCP'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object RelayRadioButton: TRadioButton
        Left = 7
        Top = 88
        Width = 85
        Height = 21
        Hint = 'Route all mail through an SMTP relay server'
        Caption = 'Relay Server'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 5
        OnClick = DNSRadioButtonClick
      end
      object RelayServerEdit: TEdit
        Left = 92
        Top = 88
        Width = 150
        Height = 21
        Hint = 
          'Host name or IP address of external SMTP server (for relaying ma' +
          'il)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 6
      end
      object RelayPortEdit: TEdit
        Left = 203
        Top = 114
        Width = 39
        Height = 21
        Hint = 'TCP port number for the SMTP relay server (default=25)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 7
      end
      object DeliveryAttemptsEdit: TEdit
        Left = 92
        Top = 10
        Width = 39
        Height = 21
        Hint = 'Maximum number of delivery attempts'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 8
      end
      object RescanFreqEdit: TEdit
        Left = 203
        Top = 10
        Width = 39
        Height = 21
        Hint = 'Seconds between message base rescans'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 9
      end
    end
  end
  object OpenDialog: TOpenDialog
    Filter = 'Wave Files|*.wav'
    Left = 8
    Top = 480
  end
end
