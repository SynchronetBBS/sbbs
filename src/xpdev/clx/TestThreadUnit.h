//---------------------------------------------------------------------------

#ifndef TestThreadUnitH
#define TestThreadUnitH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <SyncObjs.hpp>	/* TEvent/TSimpleEvent */

#if !defined(INFINITE)  /* Missing in Kylix FT1 */
#define INFINITE -1
#endif

//---------------------------------------------------------------------------
class TTestThread : public TThread
{            
private:
protected:
	void __fastcall Execute();
public:
	TSimpleEvent*	 ChildEvent;
    TSimpleEvent*	 ParentEvent;
	__fastcall TTestThread(bool CreateSuspended);
};
//---------------------------------------------------------------------------
#endif
