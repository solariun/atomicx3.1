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
        size_t nRetValue = 0;
        
        while (true)
        {
            if (Wait(nValue, 1, nRetValue, 2000))
                TRACE (INFO, "Read value: [" << nValue <<  "], RetValue: [" << nRetValue << "], Stack: [" << GetStackSize () << "/" << GetMaxStackSize() << "]");
            else
            {
                TRACE(CRITICAL,"TIMEOUT");
                exit(0);
            }
            
            //Yield (0);
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

        size_t nLocalValue = 111;
        
        //SetPriority (254);

        while (true)
        {
            {
                nValue++; nLocalValue++;

                //TRACE (INFO, "Written value: [" << nValue << "], LocalValue: [" << nLocalValue << "], Stack: [" << GetStackSize () << "/" << GetMaxStackSize() << "]");
                
                Notify(nValue, {.type = 1, .message = nLocalValue}, 2000, atomicx::Notify::one);
                
                //Yield (0);

            }
        }

        TRACE (INFO, "Ending thread.");
    }

    virtual const char* GetName () final
    {
        return "Writer";
    }
};


int g_nice = 10;

Reader r1 (g_nice);

Writer w1(g_nice);
//Writer w2(g_nice);

// Reader r2 (g_nice);
// Reader r3 (g_nice);
// Reader r4 (g_nice);

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
