//---------------------------------------------------------------------------

#ifndef TestThreadUnitH
#define TestThreadUnitH
//---------------------------------------------------------------------------
#include <classes.hpp>
#include <syncobjs.hpp>	/* TEvent/TSimpleEvent */
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
    __fastcall ~TTestThread();
};
//---------------------------------------------------------------------------
#endif
