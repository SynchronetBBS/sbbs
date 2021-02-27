object MailCfgDlg: TMailCfgDlg
  Left = 1213
  Top = 393
  BorderStyle = bsDialog
  Caption = 'Mail Server Configuration'
  ClientHeight = 246
  ClientWidth = 286
  Color = clBtnFace
  ParentFont = True
  OldCreateOrder = True
  Position = poScreenCenter
  OnShow = FormShow
  DesignSize = (
    286
    246)
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
    ActivePage = POP3TabSheet
    TabIndex = 2
    TabOrder = 3
    object GeneralTabSheet: TTabSheet
      Caption = 'General'
      object InterfaceLabel: TLabel
        Left = 7
        Top = 36
        Width = 85
        Height = 19
        AutoSize = False
        Caption = 'Interfaces (IPs)'
        ParentShowHint = False
        ShowHint = True
      end
      object MaxClientsLabel: TLabel
        Left = 7
        Top = 62
        Width = 85
        Height = 19
        AutoSize = False
        Caption = 'Max Clients'
      end
      object MaxInactivityLabel: TLabel
        Left = 7
        Top = 88
        Width = 85
        Height = 19
        AutoSize = False
        Caption = 'Max Inactivity'
      end
      object LinesPerYieldLabel: TLabel
        Left = 7
        Top = 140
        Width = 85
        Height = 19
        AutoSize = False
        Caption = 'Lines Per Yield'
      end
      object MaxMsgsLabel: TLabel
        Left = 7
        Top = 114
        Width = 85
        Height = 19
        AutoSize = False
        Caption = 'Max Msgs'
      end
      object AutoStartCheckBox: TCheckBox
        Left = 7
        Top = 10
        Width = 118
        Height = 19
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
          'Comma-separated list of IP addresses to accept incoming connecti' +
          'ons'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object MaxClientsEdit: TEdit
        Left = 92
        Top = 62
        Width = 39
        Height = 21
        Hint = 'Maximum number of simultaneous clients (default=10)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
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
        TabOrder = 4
      end
      object HostnameCheckBox: TCheckBox
        Left = 150
        Top = 10
        Width = 119
        Height = 19
        Hint = 'Automatically lookup client'#39's hostnames via DNS'
        Caption = 'Hostname Lookup'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object DebugTXCheckBox: TCheckBox
        Left = 150
        Top = 62
        Width = 119
        Height = 19
        Hint = 'Log all transmitted mail commands and responses (for debugging)'
        Caption = 'Log Transmissions'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 7
      end
      object LogFileCheckBox: TCheckBox
        Left = 150
        Top = 140
        Width = 119
        Height = 19
        Hint = 'Save log entries to a file (in your DATA directory)'
        Caption = 'Log to Disk'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 10
      end
      object LinesPerYieldEdit: TEdit
        Left = 92
        Top = 140
        Width = 39
        Height = 21
        Hint = 
          'Number of lines of message text sent/received between time-slice' +
          ' yields'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 6
      end
      object DebugRXCheckBox: TCheckBox
        Left = 150
        Top = 88
        Width = 119
        Height = 19
        Hint = 'Log all transmitted mail commands and responses (for debugging)'
        Caption = 'Log Responses'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 8
      end
      object DebugHeadersCheckBox: TCheckBox
        Left = 150
        Top = 114
        Width = 119
        Height = 19
        Hint = 'Log all received mail headers (for debugging)'
        Caption = 'Log RX Headers'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 9
      end
      object MaxMsgsWaitingEdit: TEdit
        Left = 92
        Top = 114
        Width = 39
        Height = 21
        Hint = 'Maximum number of messages waiting per user (0=unlimited)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 5
      end
    end
    object SMTPTabSheet: TTabSheet
      Caption = 'SMTP'
      ImageIndex = 1
      object SMTPPortLabel: TLabel
        Left = 7
        Top = 10
        Width = 85
        Height = 19
        AutoSize = False
        Caption = 'TCP Port'
      end
      object DefaultUserLabel: TLabel
        Left = 7
        Top = 114
        Width = 85
        Height = 19
        AutoSize = False
        Caption = 'Default Recipient'
      end
      object MaxRecipientsLabel: TLabel
        Left = 157
        Top = 88
        Width = 84
        Height = 19
        AutoSize = False
        Caption = 'Max Recipients'
      end
      object MaxMsgSizeLabel: TLabel
        Left = 7
        Top = 88
        Width = 85
        Height = 19
        AutoSize = False
        Caption = 'Max Msg Size'
      end
      object SubPortLabel: TLabel
        Left = 7
        Top = 60
        Width = 85
        Height = 19
        AutoSize = False
        Caption = 'Submission Port'
      end
      object TLSSubPortLabel: TLabel
        Left = 7
        Top = 36
        Width = 85
        Height = 19
        AutoSize = False
        Caption = 'TLS Port'
      end
      object SMTPPortEdit: TEdit
        Left = 92
        Top = 10
        Width = 39
        Height = 21
        Hint = 'TCP port number for incoming SMTP connections (default=25)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object DefaultUserEdit: TEdit
        Left = 92
        Top = 114
        Width = 143
        Height = 21
        Hint = 
          'Mail for unknown users will go into this user'#39's mailbox (e.g. "s' +
          'ysop")'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 8
      end
      object MaxRecipientsEdit: TEdit
        Left = 241
        Top = 88
        Width = 26
        Height = 21
        Hint = 'Maximum number of recipients for a single message'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 7
      end
      object MaxMsgSizeEdit: TEdit
        Left = 92
        Top = 88
        Width = 58
        Height = 21
        Hint = 'Maximum received message size (in bytes)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 6
      end
      object AuthViaIpCheckBox: TCheckBox
        Left = 7
        Top = 142
        Width = 234
        Height = 19
        Hint = 
          'Allow SMTP authentication via other protocols (e.g. POP3, Telnet' +
          ', etc) for relay'
        Caption = 'Allow Authentication via POP3, Telnet, etc.'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 9
      end
      object NotifyCheckBox: TCheckBox
        Left = 150
        Top = 10
        Width = 119
        Height = 19
        Hint = 'Notify local mail recipients of received e-mails'
        Caption = 'Notify Recipients'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object SubPortEdit: TEdit
        Left = 92
        Top = 60
        Width = 39
        Height = 21
        Hint = 'TCP port number for incoming SMTP submissions (default=587)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object UseSubPortCheckBox: TCheckBox
        Left = 150
        Top = 60
        Width = 117
        Height = 19
        Hint = 'Enable the SMTP submission port'
        Caption = 'Enabled'
        TabOrder = 5
        OnClick = UseSubPortCheckBoxClick
      end
      object TLSSubPortEdit: TEdit
        Left = 92
        Top = 36
        Width = 39
        Height = 21
        Hint = 'TCP port number for incoming SMTP/TLS submissions'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object TLSSubPortCheckBox: TCheckBox
        Left = 150
        Top = 36
        Width = 117
        Height = 19
        Hint = 'Enable the SMTP/TLS submission port'
        Caption = 'Enabled'
        TabOrder = 3
        OnClick = TLSSubPortCheckBoxClick
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
        Caption = 'TCP Port'
      end
      object TLSPOP3PortLabel: TLabel
        Left = 7
        Top = 36
        Width = 83
        Height = 19
        AutoSize = False
        Caption = 'TLS Port'
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
        Top = 62
        Width = 107
        Height = 19
        Hint = 'Log all POP3 user activity'
        Caption = 'Log All Activity'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object POP3EnabledCheckBox: TCheckBox
        Left = 150
        Top = 10
        Width = 66
        Height = 19
        Hint = 'Enable the POP3 server (requires restart of mail server)'
        Caption = 'Enabled'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
        OnClick = POP3EnabledCheckBoxClick
      end
      object TLSPOP3PortEdit: TEdit
        Left = 92
        Top = 36
        Width = 39
        Height = 21
        Hint = 'TCP port number for incoming POP3S connections'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object TLSPOP3EnabledCheckBox: TCheckBox
        Left = 150
        Top = 36
        Width = 66
        Height = 19
        Hint = 'Enable the TLS POP3 port'
        Caption = 'Enabled'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
        OnClick = POP3EnabledCheckBoxClick
      end
    end
    object SendMailTabSheet: TTabSheet
      Caption = 'SendMail'
      ImageIndex = 3
      object DeliveryAttemptsLabel: TLabel
        Left = 7
        Top = 36
        Width = 85
        Height = 19
        AutoSize = False
        Caption = 'Max Attempts'
      end
      object RescanFreqLabel: TLabel
        Left = 136
        Top = 36
        Width = 57
        Height = 19
        Hint = 'Frequency (in seconds) of delivery attempts'
        Alignment = taRightJustify
        AutoSize = False
        Caption = 'Frequency'
      end
      object DNSServerLabel: TLabel
        Left = 7
        Top = 120
        Width = 57
        Height = 13
        Caption = 'DNS Server'
      end
      object DefCharsetLabel: TLabel
        Left = 7
        Top = 62
        Width = 85
        Height = 19
        AutoSize = False
        Caption = 'Default Charset'
      end
      object ConnectTimeoutLabel: TLabel
        Left = 7
        Top = 146
        Width = 85
        Height = 19
        AutoSize = False
        Caption = 'Connect Timeout'
      end
      object DNSRadioButton: TRadioButton
        Left = 7
        Top = 94
        Width = 150
        Height = 21
        Hint = 
          'Send mail directly to addressed mail server (requires DNS server' +
          ' access)'
        Caption = 'Direct Mail Delivery'
        Checked = True
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
        TabStop = True
        OnClick = DNSRadioButtonClick
      end
      object DNSServerEdit: TEdit
        Left = 92
        Top = 120
        Width = 150
        Height = 21
        Hint = 'Host name or IP address of your ISP'#39's DNS server'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 6
      end
      object TcpDnsCheckBox: TCheckBox
        Left = 203
        Top = 146
        Width = 53
        Height = 20
        Hint = 'Use TCP packets (instead of UDP) for DNS queries'
        Caption = 'TCP'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 7
      end
      object RelayRadioButton: TRadioButton
        Left = 137
        Top = 94
        Width = 156
        Height = 21
        Hint = 'Route all mail through an SMTP relay server'
        Caption = 'Use Relay Server'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 5
        OnClick = DNSRadioButtonClick
      end
      object DeliveryAttemptsEdit: TEdit
        Left = 92
        Top = 36
        Width = 39
        Height = 21
        Hint = 'Maximum number of delivery attempts'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object RescanFreqEdit: TEdit
        Left = 203
        Top = 36
        Width = 39
        Height = 21
        Hint = 'Seconds between message base rescans'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object SendMailCheckBox: TCheckBox
        Left = 7
        Top = 10
        Width = 66
        Height = 19
        Hint = 'Enable the SendMail thread (requires restart of mail server)'
        Caption = 'Enabled'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
        OnClick = SendMailCheckBoxClick
      end
      object DefCharsetEdit: TEdit
        Left = 92
        Top = 62
        Width = 65
        Height = 21
        Hint = 'Character set specified for locally generated e-mail messages'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
      end
      object ConnectTimeoutEdit: TEdit
        Left = 92
        Top = 146
        Width = 39
        Height = 21
        Hint = 
          'Maximum number of seconds to wait for outbound connection (0=ind' +
          'efinite)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 8
      end
    end
    object RelayTabSheet: TTabSheet
      Caption = 'Relay'
      ImageIndex = 6
      object RelayPortLabel: TLabel
        Left = 195
        Top = 10
        Width = 27
        Height = 21
        Alignment = taRightJustify
        AutoSize = False
        Caption = 'Port'
      end
      object RelayServerLabel: TLabel
        Left = 7
        Top = 10
        Width = 31
        Height = 13
        Caption = 'Server'
      end
      object RelayServerEdit: TEdit
        Left = 53
        Top = 10
        Width = 136
        Height = 21
        Hint = 
          'Host name or IP address of external SMTP server (for relaying ma' +
          'il)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object RelayPortEdit: TEdit
        Left = 229
        Top = 10
        Width = 32
        Height = 21
        Hint = 'TCP port number for the SMTP relay server (default=25)'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
      end
      object RelayAuthGroupBox: TGroupBox
        Left = 7
        Top = 33
        Width = 260
        Height = 130
        Caption = 'Authentication:'
        TabOrder = 2
        object RelayAuthNameLabel: TLabel
          Left = 13
          Top = 75
          Width = 65
          Height = 21
          AutoSize = False
          Caption = 'Name'
        end
        object RelayAuthPassLabel: TLabel
          Left = 13
          Top = 98
          Width = 65
          Height = 21
          AutoSize = False
          Caption = 'Password'
        end
        object RelayAuthNoneRadioButton: TRadioButton
          Left = 13
          Top = 23
          Width = 92
          Height = 19
          Caption = 'None'
          Checked = True
          TabOrder = 0
          TabStop = True
          OnClick = RelayAuthRadioButtonClick
        end
        object RelayAuthPlainRadioButton: TRadioButton
          Left = 137
          Top = 23
          Width = 92
          Height = 19
          Caption = 'Plain'
          TabOrder = 1
          OnClick = RelayAuthRadioButtonClick
        end
        object RelayAuthLoginRadioButton: TRadioButton
          Left = 137
          Top = 46
          Width = 92
          Height = 20
          Caption = 'Login'
          TabOrder = 2
          OnClick = RelayAuthRadioButtonClick
        end
        object RelayAuthCramMD5RadioButton: TRadioButton
          Left = 13
          Top = 46
          Width = 92
          Height = 20
          Caption = 'CRAM-MD5'
          TabOrder = 3
          OnClick = RelayAuthRadioButtonClick
        end
        object RelayAuthNameEdit: TEdit
          Left = 85
          Top = 75
          Width = 151
          Height = 21
          Hint = 'User name to authenticate as'
          ParentShowHint = False
          ShowHint = True
          TabOrder = 4
        end
        object RelayAuthPassEdit: TEdit
          Left = 85
          Top = 98
          Width = 151
          Height = 21
          Hint = 'Password for authentication'
          ParentShowHint = False
          PasswordChar = '*'
          ShowHint = True
          TabOrder = 5
        end
      end
    end
    object SoundTabSheet: TTabSheet
      Caption = 'Sound'
      ImageIndex = 4
      object SMTPSoundLabel: TLabel
        Left = 7
        Top = 10
        Width = 82
        Height = 19
        AutoSize = False
        Caption = 'Receive Mail'
      end
      object POP3SoundLabel: TLabel
        Left = 7
        Top = 62
        Width = 82
        Height = 21
        AutoSize = False
        Caption = 'POP3 Login'
      end
      object OutboundSoundLabel: TLabel
        Left = 7
        Top = 36
        Width = 82
        Height = 19
        AutoSize = False
        Caption = 'Sending Mail'
      end
      object InboundSoundEdit: TEdit
        Left = 92
        Top = 10
        Width = 150
        Height = 21
        Hint = 'Sound file to play when inbound SMTP connections are accepted'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
      end
      object InboundSoundButton: TButton
        Left = 247
        Top = 10
        Width = 20
        Height = 21
        Caption = '...'
        TabOrder = 1
        OnClick = InboundSoundButtonClick
      end
      object POP3SoundEdit: TEdit
        Left = 92
        Top = 62
        Width = 150
        Height = 21
        Hint = 'Sound file to play when accepting POP3 connections'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 4
      end
      object POP3SoundButton: TButton
        Left = 247
        Top = 62
        Width = 20
        Height = 21
        Caption = '...'
        TabOrder = 5
        OnClick = POP3SoundButtonClick
      end
      object OutboundSoundEdit: TEdit
        Left = 92
        Top = 36
        Width = 150
        Height = 21
        Hint = 'Sound file to play when sending mail'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
      end
      object OutboundSoundButton: TButton
        Left = 247
        Top = 36
        Width = 20
        Height = 21
        Caption = '...'
        TabOrder = 3
        OnClick = OutboundSoundButtonClick
      end
    end
    object DNSBLTabSheet: TTabSheet
      Caption = 'DNSBL'
      ImageIndex = 5
      ParentShowHint = False
      ShowHint = True
      object Label1: TLabel
        Left = 104
        Top = 85
        Width = 32
        Height = 13
        Caption = 'Label1'
      end
      object DNSBLServersButton: TButton
        Left = 7
        Top = 7
        Width = 91
        Height = 21
        Hint = 'DNS-based Blacklists'
        Caption = 'Blacklists'
        TabOrder = 0
        OnClick = DNSBLServersButtonClick
      end
      object DNSBLGroupBox: TGroupBox
        Left = 7
        Top = 33
        Width = 260
        Height = 130
        Caption = 'Mail from Blacklisted Servers:'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
        object BLSubjectLabel: TLabel
          Left = 13
          Top = 75
          Width = 65
          Height = 21
          AutoSize = False
          Caption = 'Subject'
        end
        object BLHeaderLabel: TLabel
          Left = 13
          Top = 98
          Width = 65
          Height = 21
          AutoSize = False
          Caption = 'Header'
        end
        object BLRefuseRadioButton: TRadioButton
          Left = 13
          Top = 23
          Width = 124
          Height = 19
          Hint = 'Refuse mail session with blacklisted servers'
          Caption = 'Refuse Session'
          ParentShowHint = False
          ShowHint = True
          TabOrder = 0
          OnClick = DNSBLRadioButtonClick
        end
        object BLIgnoreRadioButton: TRadioButton
          Left = 13
          Top = 46
          Width = 131
          Height = 20
          Hint = 'Pretend to receive blacklisted mail'
          Caption = 'Silently Ignore'
          ParentShowHint = False
          ShowHint = True
          TabOrder = 2
          OnClick = DNSBLRadioButtonClick
        end
        object BLBadUserRadioButton: TRadioButton
          Left = 137
          Top = 23
          Width = 104
          Height = 19
          Hint = 'Refuse mail address from blacklisted servers'
          Caption = 'Refuse Mail'
          TabOrder = 1
          OnClick = DNSBLRadioButtonClick
        end
        object BLTagRadioButton: TRadioButton
          Left = 137
          Top = 46
          Width = 78
          Height = 20
          Hint = 'Tag blacklisted mail with header and/or subject'
          Caption = 'Tag with:'
          ParentShowHint = False
          ShowHint = True
          TabOrder = 3
          OnClick = DNSBLRadioButtonClick
        end
        object BLSubjectEdit: TEdit
          Left = 85
          Top = 75
          Width = 151
          Height = 21
          Hint = 'Flag to add to subject of DNS-blacklisted mail'
          ParentShowHint = False
          ShowHint = True
          TabOrder = 4
        end
        object BLHeaderEdit: TEdit
          Left = 85
          Top = 98
          Width = 151
          Height = 21
          Hint = 'Flag to add to subject of DNS-blacklisted mail'
          ParentShowHint = False
          ShowHint = True
          TabOrder = 5
        end
      end
      object DNSBLExemptionsButton: TButton
        Left = 104
        Top = 7
        Width = 92
        Height = 21
        Hint = 'Blacklist Exempted IPs, hostnames, and e-mail addresses'
        Caption = 'Exemptions'
        TabOrder = 1
        OnClick = DNSBLExemptionsButtonClick
      end
      object DNSBLSpamHashCheckBox: TCheckBox
        Left = 208
        Top = 7
        Width = 59
        Height = 21
        Hint = 'Store hashes of messages from blacklisted servers in SPAM base '
        Caption = 'Hash'
        TabOrder = 2
      end
    end
    object AdvancedTabSheet: TTabSheet
      Caption = 'Advanced'
      ImageIndex = 7
      object AdvancedCheckListBox: TCheckListBox
        Left = 13
        Top = 13
        Width = 248
        Height = 150
        ItemHeight = 13
        Items.Strings = (
          'SendMail: Ignore '#39'in transit'#39' attribute'
          'Retain received mail files (in temp directory)'
          'Allow receipt of mail by user number'
          'Allow receipt of mail to '#39'sysop'#39' and '#39'postmaster'#39
          'Allow authenticated users to relay mail'
          'Check '#39'Received'#39' header fields against DNSBL'
          'Throttle DNS blacklisted server sessions'
          'Auto-exempt sent-mail recipients from DNSBL'
          'Set Kill-Read attribute on received SPAM')
        TabOrder = 0
      end
    end
  end
  object OpenDialog: TOpenDialog
    Filter = 'Wave Files|*.wav'
    Options = [ofHideReadOnly, ofNoChangeDir, ofEnableSizing, ofDontAddToRecent]
    Left = 8
    Top = 480
  end
end
