
//---------------------------------------------------------------------------

#ifndef ThreadUnitH
#define ThreadUnitH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <SyncObjs.hpp>
//---------------------------------------------------------------------------
class TMyThread : public TThread
{
private:
protected:
	void __fastcall Execute();
public:
	TMemo*	Log;
    TCriticalSection* CriticalSection;
	__fastcall TMyThread(bool CreateSuspended);
};
//---------------------------------------------------------------------------
#endif
