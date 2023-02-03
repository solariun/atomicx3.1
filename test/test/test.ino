
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
        Serial.println (atomicx::kernel.GetThreadCount ());
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

class th : atomicx::Thread
{
    private:
        volatile size_t nStack [20];

    public:

    th () : Thread (100, nStack)
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

    void yield_in ()
    {
      Yield ();
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
            Serial.println (atomicx::kernel.GetThreadCount ());
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


th th1;
th th2;
th th3;
th th4;

void setup()
{
   
    Serial.begin (115200);

    Serial.println (F(""));
    Serial.println (F("Starting atomicx 3 demo."));
    Serial.flush ();
    delay(1000);

    Serial.println ("-------------------------------------");

    for (auto& th : atomicx::kernel)
    {
        Serial.print (__func__);
        Serial.print (": Listing thread: ");
        Serial.print (th.GetName ());
        Serial.print (", ID:");
        Serial.println ((size_t) &th);
        Serial.flush ();
    }

    Serial.println ("-------------------------------------");

    atomicx::kernel.Join ();
}

void loop() {
}