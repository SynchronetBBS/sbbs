program UserEdit;

uses
  Forms,
  MainFormUnit in 'MainFormUnit.pas' {Form1};

{$R *.RES}

begin
  Application.Initialize;
  Application.Title := 'Synchronet User Editor';
  Application.CreateForm(TForm1, Form1);
  Application.Run;
end.
