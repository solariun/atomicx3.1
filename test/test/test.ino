
#include "atomicx.hpp"

#include "test.h"

atomicx_time atomicx::Kernel::GetTick(void)
{
    return millis();
}

void atomicx::Kernel::SleepTick(atomicx_time nSleep)
{
    delay(nSleep);
}

void yield_in ()
{
    atomicx::kernel.Yield ();
}

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