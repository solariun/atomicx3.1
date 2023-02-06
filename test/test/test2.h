
#include "atomicx.hpp"

class tth : atomicx::Thread
{
    private:
        volatile size_t nStack [20];

    public:

    tth () : Thread (100, nStack)
    {
        // Serial.print ((size_t) this);
        // Serial.println (F(": Initiating."));
        // Serial.flush ();
    }

    virtual ~tth ()
    {
        Serial.print ((size_t) this);
        Serial.print (F(": TTH Deleting."));
        Serial.print (F(", th#:"));
        Serial.println (GetThreadCount ());
        Serial.flush ();
        
    }

    virtual void run ()
    {
        int nValue = 0;
    
        while (true)
        {
        
            Serial.print ((size_t) this);
            Serial.print (F(": TTH Vall:"));
            Serial.print (nValue++);
            Serial.print (F(", stk:"));
            Serial.println (GetStackSize ());
            Serial.flush ();

           Yield ();
        }

        Serial.print ((size_t) this);
        Serial.print (F(": Ending thread."));
        Serial.flush ();
    }

    virtual const char* GetName () final
    {
        return "TTh ";
    }
};

