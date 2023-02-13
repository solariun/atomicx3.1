
#include "atomicx.hpp"

atomicx_time atomicx::Thread::GetTick(void)
{
    return millis();
}

void atomicx::Thread::SleepTick(atomicx_time nSleep)
{
    delay(nSleep);
}

atomicx::Mutex mutex;

uint32_t nValue = 0;

class Reader : public atomicx::Thread
{
    private:
        volatile size_t nStack [40];

    public:

    Reader () : Thread (0, nStack)
    {
        // Serial.print ((size_t) this);
        // Serial.println (F(": Initiating."));
        // Serial.flush ();
    }

    virtual ~Reader () final
    {
        Serial.print ((size_t) this);
        Serial.println (F(": Deleting."));
        Serial.flush ();
    }

    virtual void run () final 
    {
        Serial.print ((size_t) this);
        Serial.println (F(": Starting up thread."));
        Serial.flush ();

        while (Yield ())
        {
                Serial.print ((size_t) this);
                Serial.print (F(": bExclusiveLock: "));
                Serial.print (mutex.bExclusiveLock);
                Serial.print (F(", nSharedLockCount: "));
                Serial.println (mutex.nSharedLockCount);
                Serial.flush ();

            if (mutex.SharedLock (1000))
            {
                Serial.print ((size_t) this);
                Serial.print (F(": Read value: "));
                Serial.print (nValue);
                Serial.print (F(", Stack: "));
                Serial.println (GetStackSize ());

                Serial.flush ();
                
                mutex.SharedUnlock ();
            }
        }

        Serial.print ((size_t) this);
        Serial.print (F(": Ending thread."));
        Serial.flush ();
    }

    virtual const char* GetName () final
    {
        return "Reader";
    }
};

class Writer : public atomicx::Thread
{
    private:
        volatile size_t nStack [40];

    public:

    Writer () : Thread (0, nStack)
    {
        // Serial.print ((size_t) this);
        // Serial.println (F(": Initiating."));
        // Serial.flush ();
    }

    virtual ~Writer () final
    {
        Serial.print ((size_t) this);
        Serial.println (F(": Deleting."));
        Serial.flush ();
    }

    virtual void run () final 
    {
        Serial.print ((size_t) this);
        Serial.print (F(": Starting up thread."));
        Serial.flush ();

        while (true)
        {
            Serial.print ((size_t) this);
            Serial.println (F(": Acquiring lock.. "));

            Serial.print ((size_t) this);
            Serial.print (F(": bExclusiveLock: "));
            Serial.print (mutex.bExclusiveLock);
            Serial.print (F(", nSharedLockCount: "));
            Serial.println (mutex.nSharedLockCount);
            Serial.flush ();

            if (mutex.Lock (10000))
            {
                nValue++;

                Serial.print ((size_t) this);
                Serial.print (F(": Writing value: "));
                Serial.print (nValue);
                Serial.print (F(", Stack: "));
                Serial.println (GetStackSize ());

                Serial.flush ();
                
                mutex.Unlock ();
            }
        }

        Serial.print ((size_t) this);
        Serial.print (F(": Ending thread."));
        Serial.flush ();

        Yield ();
    }

    virtual const char* GetName () final
    {
        return "Writer";
    }
};

// Reader r1;
// Reader r2;
// Reader r3;
// Reader r4;

Writer w1;
Writer w2;

void setup()
{
   
    Serial.begin (115200);

    Serial.println (F(""));
    Serial.println (F("Starting atomicx 3 demo."));
    Serial.flush ();
    delay(1000);

    Serial.println ("-------------------------------------");

    for (auto& th : w1)
    {
        Serial.print (__func__);
        Serial.print (th.GetName ());
        Serial.print (", ID:");
        Serial.println ((size_t) &th);
        Serial.flush ();
    }

    Serial.println ("-------------------------------------");

    atomicx::Thread::Join ();
}

void loop() {
}