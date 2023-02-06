
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>

#include "atomicx.hpp"

namespace atomicx
{

    /*
        KERNEL 
    */
    Thread* Kernel::GetCyclicalNext()
    {
        return (m_pCurrent->pNext) == nullptr ? (m_pCurrent = m_pBegin) : m_pCurrent->pNext;
    }

    bool Kernel::Join ()
    {    
        m_pCurrent = m_pEnd;

        if (m_pCurrent != nullptr )
        {
            m_pStartStack = GetStackPoint (); //&nStackPoint;

            setjmp (m_joinContext);

            if (m_nNodeCounter == 0) return false;

            m_pCurrent = GetCyclicalNext (); 

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

        return false;
    }

    bool Kernel::Yield ()
    {
        m_pCurrent->m_pEndStack = GetStackPoint (); 
        m_pCurrent->nStackSize = m_pStartStack - m_pCurrent->m_pEndStack + sizeof (size_t);

        TRACE(TRACE, "Stack size: " << m_pCurrent->nStackSize << ", Max: " << m_pCurrent->m_nMaxStackSize << ", Occupied: " << (100*m_pCurrent->nStackSize)/(m_pCurrent->m_nMaxStackSize) << "%");

        m_pCurrent->m_status = Status::sleeping;

        if (setjmp (m_pCurrent->m_context) != 0)
        {
            m_pCurrent->nStackSize = m_pStartStack - m_pCurrent->m_pEndStack;
            memcpy ((void*) m_pCurrent->m_pEndStack, (const void*) &m_pCurrent->m_stack, m_pCurrent->nStackSize);

            TRACE(TRACE, (size_t) m_pCurrent << ": RETURNED from Join.");

            return true;
        } 

        memcpy ((void*) &m_pCurrent->m_stack, (const void*) m_pCurrent->m_pEndStack, m_pCurrent->nStackSize);

        longjmp (m_joinContext, 1);

        return false;
    }

    size_t Kernel::GetThreadCount ()
    {
        return m_nNodeCounter;
    }

    /*
        THREAD 
    */

    Thread::~Thread ()
    {
        DetachThread (kernel, *this);
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