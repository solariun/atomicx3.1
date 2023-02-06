
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>

#include "atomicx.hpp"

namespace atomicx
{

    Thread* Thread::m_pBegin = nullptr;
    Thread* Thread::m_pEnd = nullptr;
    Thread* Thread::m_pCurrent = nullptr;

    size_t Thread::m_nNodeCounter = 0;

    volatile uint8_t* Thread::m_pStartStack = nullptr;

    jmp_buf Thread::m_joinContext = {};
    
    /* ------------------------ */

    Status m_status = Status::starting;

    // Thread context register buffer
    jmp_buf m_context = {};

    volatile uint8_t* m_pEndStack = nullptr;

    /*
        KERNEL 
    */
    Thread* Thread::GetCyclicalNext()
    {
        return (m_pCurrent->pNext) == nullptr ? (m_pCurrent = m_pBegin) : m_pCurrent->pNext;
    }

    void Thread::Scheduller ()
    {
        size_t nThreadCount = m_nNodeCounter;
        Thread* pThread = m_pCurrent;
        atomicx_time tm = GetTick ();

        while (nThreadCount--)
        {
            pThread = (pThread->pNext) == nullptr ? (pThread = m_pBegin) : pThread->pNext;

            if (pThread->m_status == Status::now)
            {
                pThread->m_nextEvent = tm;
                m_pCurrent = pThread;
                break;
            }
            
            if (pThread->m_nextEvent < m_pCurrent->m_nextEvent)
            {
                m_pCurrent = pThread;
            }
        }

        if (m_pCurrent->m_nextEvent > tm)
        {
            TRACE (TRACE, "Status;" << GetStatusName (m_pCurrent->m_status) << ", tm: " << tm << ", next: " << m_pCurrent->m_nextEvent << ", sleep: " << (m_pCurrent->m_nextEvent - tm));

            SleepTick (m_pCurrent->m_nextEvent - tm);
        }
    }

    bool Thread::Join ()
    {    
        m_pCurrent = m_pEnd;

        if (m_pCurrent != nullptr )
        {
            m_pStartStack = GetStackPoint (); //&nStackPoint;

            setjmp (m_joinContext);

            if (m_nNodeCounter == 0) return false;

            //m_pCurrent = GetCyclicalNext (); 
            Scheduller ();

            TRACE (TRACE, "------------------------------------");
            TRACE (TRACE, m_pCurrent->GetName () << "_" <<(size_t) m_pCurrent << ": St [" << GetStatusName (m_pCurrent->m_status) << "]");
            TRACE (TRACE, "Stack size: " << m_pCurrent->nStackSize << ", Max: " << m_pCurrent->m_nMaxStackSize << ", Occupied: " << (100*m_pCurrent->nStackSize)/(m_pCurrent->m_nMaxStackSize) << "%");

            if (m_pCurrent->m_status ==  Status::starting)
            {
                m_pCurrent->m_status = Status::running;

                m_pCurrent->run ();
    
                m_pCurrent->m_status = Status::starting;

                longjmp (m_joinContext, 1);
            }
            else
            {
                longjmp (m_pCurrent->m_context, 1);
            }
        }

        TRACE (TRACE, "NO THREAD TO RUN.... #" << m_nNodeCounter);

        return false;
    }

    bool Thread::Yield (atomicx_time tm, Status st)
    {
        m_pCurrent->m_pEndStack = GetStackPoint (); 
        m_pCurrent->nStackSize = m_pStartStack - m_pCurrent->m_pEndStack + sizeof (size_t);

        TRACE (TRACE, "Stack size: " << m_pCurrent->nStackSize << ", Max: " << m_pCurrent->m_nMaxStackSize << ", Occupied: " << (100*m_pCurrent->nStackSize)/(m_pCurrent->m_nMaxStackSize) << "%");

        if (st == Status::now)
        {
            tm = 0;
        }
        else
        {
            tm = GetTick () + (tm ? tm : m_pCurrent->m_nNice);
        }
        
        m_pCurrent->m_status = st;
        m_pCurrent->m_nextEvent = tm;

        if (setjmp (m_pCurrent->m_context) != 0)
        {
            m_pCurrent->nStackSize = m_pStartStack - m_pCurrent->m_pEndStack;
            memcpy ((void*) m_pCurrent->m_pEndStack, (const void*) &m_pCurrent->m_stack, m_pCurrent->nStackSize);

            NOTRACE(TRACE, (size_t) m_pCurrent << ": RETURNED from Join.");

            return true;
        } 

        memcpy ((void*) &m_pCurrent->m_stack, (const void*) m_pCurrent->m_pEndStack, m_pCurrent->nStackSize);

        longjmp (m_joinContext, 1);

        return false;
    }

    size_t Thread::GetThreadCount ()
    {
        return m_nNodeCounter;
    }

    /*
        THREAD 
    */

    Thread::~Thread ()
    {
        DetachThread (*this);
    }

    size_t Thread::GetStackSize ()
    {
        return nStackSize;
    }

    size_t Thread::GetMaxStackSize ()
    {
        return m_nMaxStackSize;
    }
}