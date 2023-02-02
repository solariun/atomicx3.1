
#include "atomicx.hpp"

atomicx::Kernel kernel;


class tth : atomicx::Thread
{
    private:
        volatile size_t nStack [20];

    public:

    tth () : Thread (kernel, 100, nStack)
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
        Serial.println (kernel.GetThreadCount ());
        Serial.flush ();
        
    }

    atomicx::Kernel& GetKernel()
    {
        return kernel;
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

           kernel.Yield ();
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

    th () : Thread (GetKernel (), 100, nStack)
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
      kernel.Yield ();
    }

    void yield ()
    {
        yield_in ();
    }

    atomicx::Kernel& GetKernel () 
    {
        return kernel;
    }

    virtual void run ()
    {
        int nValue = 0;
        tth* th =  nullptr;

        while (true)
        {
            if (nValue && nValue % 50 == 0) 
            {
                th = new tth ();
                delay(100);
            }

            if (nValue && nValue % 100 == 0)
            {
                delete th;
                delay(100);
            }

            yield ();

            Serial.print ((size_t) this);
            Serial.print (F(":TEST Vall:"));
            Serial.print (nValue++);
            Serial.print (F(", th#:"));
            Serial.println (kernel.GetThreadCount ());
            Serial.flush ();
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

    for (auto& th : kernel)
    {
        Serial.print (__func__);
        Serial.print (": Listing thread: ");
        Serial.print (th.GetName ());
        Serial.print (", ID:");
        Serial.println ((size_t) &th);
        Serial.flush ();
    }

    Serial.println ("-------------------------------------");

    kernel.Join ();
}

void loop() {
}