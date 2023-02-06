
#include "atomicx.hpp"

#include "test2.h"

extern void yield_in ();

class th : public atomicx::Thread
{
    private:
        volatile size_t nStack [20];

    public:

    th () : Thread (0, nStack)
    {
        // Serial.print ((size_t) this);
        // Serial.println (F(": Initiating."));
        // Serial.flush ();
    }

    virtual ~th ()
    {
        Serial.print ((size_t) this);
        Serial.println (F(": Deleting."));
        Serial.flush ();
    }

    void yield ()
    {
        yield_in ();
    }

    virtual void run ()
    {
        int nValue = 0;
        tth* th =  nullptr;

        while (true)
        {
            yield ();

            Serial.print ((size_t) this);
            Serial.print (F(":TEST Vall:"));
            Serial.print (nValue++);
            Serial.print (F(", th#:"));
            Serial.println (GetThreadCount ());
            Serial.flush ();

            if (nValue && nValue % 50 == 0) 
            {
                if (!th)
                {
                    th = new tth ();
                    delay(100);
                }
                else
                {
                    delete th;
                    th = nullptr,
                    delay(100);
                }
            }
        }

        Serial.print ((size_t) this);
        Serial.print (F(": Ending thread."));
        Serial.flush ();
    }

    virtual const char* GetName () final
    {
        return "Thread Test";
    }
};
