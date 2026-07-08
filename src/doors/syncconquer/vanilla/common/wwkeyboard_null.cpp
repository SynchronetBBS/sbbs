// Null keyboard backend: satisfies CreateWWKeyboardClass() for headless
// builds with no video backend (video_null). No input is ever produced.
// Local SyncConquer addition -- not upstream. See ../PROVENANCE.md.
#include "wwkeyboard.h"

class WWKeyboardClassNull : public WWKeyboardClass
{
public:
    virtual ~WWKeyboardClassNull()
    {
    }

    virtual void Fill_Buffer_From_System(void)
    {
    }

    virtual KeyASCIIType To_ASCII(unsigned short key)
    {
        return KA_NONE;
    }
};

WWKeyboardClass* CreateWWKeyboardClass(void)
{
    return new WWKeyboardClassNull;
}
