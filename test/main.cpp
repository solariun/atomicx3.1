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
        volatile size_t nStack [100];

    public:

    Reader (atomicx_time nNice) : Thread (nNice, nStack)
    {
        std::cout <<  ":" << GetName () << ", Initiating thread." << std::endl << std::flush;
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

            if (mutex.SharedLock (8000))
            {
                TRACE (INFO, "Read value: [" << nValue <<  "], Stack: [" << GetStackSize () << "/" << GetMaxStackSize() << "]");
                
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
        volatile size_t nStack [100];

    public:

    Writer (atomicx_time nNice) : Thread (nNice, nStack)
    {
        std::cout <<  ":" << GetName () << ", Initiating thread." << std::endl << std::flush;
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

                TRACE (INFO, "Written value: [" << nValue << "], Stack: [" << GetStackSize () << "/" << GetMaxStackSize() << "]");

                Yield ();

                mutex.Unlock ();
            }

            Yield ();
        }

        TRACE (INFO, "Ending thread.");
    }

    virtual const char* GetName () final
    {
        return "Writer";
    }
};


int g_nice = 500;

Reader r1 (g_nice);
Reader r2 (g_nice);
// Reader r3 (g_nice);
// Reader r4 (g_nice);

Writer w1(g_nice);
//Writer w2(g_nice);

int main ()
{

    std::cout << "Beging Application" << std::endl << std::endl;

    std::cout << "-------------------------------" << std::endl;

    for (atomicx::Thread& a : w1)
    {
        std::cout << &a << " thread" << std::endl;
    }

    std::cout << "------------------------------- #:" << std::endl << std::endl;

    atomicx::Thread::Join ();
    
    std::cout << "End Application" << std::endl << std::endl;
       
    return 0;
}
