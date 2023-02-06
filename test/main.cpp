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
#if 0
    atomicx_time nCurrent= Atomicx_GetTick ();
    std::cout << "Current: " << nCurrent << ", Sleep: " << nSleep << ", Thread time:" << atomicx::GetCurrent()->GetTargetTime () << ", Calculation:" << (atomicx::GetCurrent()->GetTargetTime () - nCurrent)<< std::endl << std::flush;
    ListAllThreads();
#endif

    usleep ((useconds_t)nSleep * 1000);
}

class th : public atomicx::Thread
{
    private:

        volatile size_t nStack [100];

    public:

    th () : Thread (100, nStack)
    {
        std::cout << (size_t) this << ": Initiating." << std::endl;
    }

    ~th ()
    {
        std::cout << (size_t) this << ": Deleting." << std::endl;
    }

    void yield_in ()
    {
       Yield (0, atomicx::Status::sleep);
    }

    void yield ()
    {
        yield_in ();
    }

    virtual void run ()
    {
        int nValue = 0;

        yield ();
        
        while (true)
        {
            yield ();

            TRACE (TRACE, ">>> Val: " << nValue++);
        }

        TRACE (TRACE, "Leaving thread.");
    }

    virtual const char* GetName () final
    {
        return "Thread Test";
    }
};

th th1;
th th2;

int main ()
{
    th th3;
    th th4;

    std::cout << "Beging Application" << std::endl << std::endl;

    std::cout << "-------------------------------" << std::endl;

    for (atomicx::Thread& a : th1)
    {
        std::cout << (size_t) &a << " thread" << std::endl;
    }

    std::cout << "------------------------------- #:" << std::endl << std::endl;

    atomicx::Thread::Join ();
    
    std::cout << "End Application" << std::endl << std::endl;
       
    return 0;
}