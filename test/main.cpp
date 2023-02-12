//
//  main.cpp
//  atomic
//
//  Created by GUSTAVO CAMPOS on 29/08/2021.
//

#include "atomicx.hpp"

#include <stdio.h>
#include <sys/time.h>
#include <unistd.h>

#include <string.h>
#include <stdint.h>

#include <stdlib.h>
#include <iostream>

atomicx_time atomicx::Thread::GetTick (void)
{
    struct timeval tp;
    gettimeofday (&tp, NULL);

    return (atomicx_time)tp.tv_sec * 1000 + tp.tv_usec / 1000;
}

size_t nCounter = 0;

void atomicx::Thread::SleepTick(atomicx_time nSleep)
{
    usleep ((useconds_t)nSleep * 1000);
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
        TRACE (INFO, "Initiating thread.");
    }

    virtual ~Reader () final
    {
        TRACE (INFO, "Deleting");
    }


    virtual void run () final 
    {
        TRACE (INFO, "Starting thread.");

        while (Yield ())
        {
            TRACE (INFO, "bExclusiveLock: [" << mutex.bExclusiveLock << "], nSharedLockCount: [" << mutex.nSharedLockCount << "]");

            if (mutex.SharedLock (10000))
            {
                TRACE (INFO, "Read value: [" << nValue << "], Stack: [" << GetStackSize () << "]");
                
                mutex.SharedUnlock ();
            }
        }

        TRACE (INFO, "Ending thread.");
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
        TRACE (INFO, "Initiating thread.");
    }

    virtual ~Writer () final
    {
        TRACE (INFO, "Deleting");
    }

    virtual void run () final 
    {
        TRACE (INFO, "Starting thread.");

        while (true)
        {
            TRACE (INFO, "Acquiring exclusive lock.");

            TRACE (INFO, "bExclusiveLock: [" << mutex.bExclusiveLock << "], nSharedLockCount: [" << mutex.nSharedLockCount << "]");

            if (mutex.Lock (10000))
            {
                nValue++;

                TRACE (INFO, "Written value: [" << nValue << "], Stack: [" << GetStackSize () << "]");

                mutex.Unlock ();
            }
        }

        TRACE (INFO, "Ending thread.");

        Yield ();
    }

    virtual const char* GetName () final
    {
        return "Writer";
    }
};

Reader r1;

Writer w1;

int main ()
{

    std::cout << "Beging Application" << std::endl << std::endl;

    std::cout << "-------------------------------" << std::endl;

    for (atomicx::Thread& a : r1)
    {
        std::cout << (size_t) &a << " thread" << std::endl;
    }

    std::cout << "------------------------------- #:" << std::endl << std::endl;

    atomicx::Thread::Join ();
    
    std::cout << "End Application" << std::endl << std::endl;
       
    return 0;
}
