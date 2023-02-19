
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
     * Tiemout functions
    */

    /*
    * timeout methods implementations
    */

    Timeout::Timeout () : m_timeoutValue (0)
    {
        Set (0);
    }

    Timeout::Timeout (atomicx_time nTimeoutValue) : m_timeoutValue (0)
    {
        Set (nTimeoutValue);
    }

    void Timeout::Set(atomicx_time nTimeoutValue)
    {
        m_timeoutValue = nTimeoutValue ? nTimeoutValue + Thread::GetTick () : 0;
    }

    bool Timeout::IsTimedout()
    {
        return (m_timeoutValue == 0 || Thread::GetTick () < m_timeoutValue) ? false : true;
    }

    atomicx_time Timeout::GetRemaining()
    {
        auto nNow = Thread::GetTick ();

        return (nNow < m_timeoutValue) ? m_timeoutValue - nNow : 0;
    }

    atomicx_time Timeout::GetDurationSince(atomicx_time startTime)
    {
        return startTime - GetRemaining ();
    }

    /*
        THREAD 
    */

   // Thread kernel static initializations

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

    Thread* Thread::GetCurrent()
    {
        return m_pCurrent;
    }

    // Thread methods 
    bool Thread::AttachThread (Thread& thread)
    {
        if (m_pBegin == nullptr)
        {
            m_pBegin = &thread;
            m_pEnd = m_pBegin;
        }
        else
        {
            thread.pPrev = m_pEnd;
            m_pEnd->pNext = &thread;
            m_pEnd = &thread;
        }

        m_nNodeCounter++;

        return true;
    }

    bool Thread::DetachThread (Thread& thread)
    {
        if (thread.pNext == nullptr && thread.pPrev == nullptr)
        {
            m_pBegin = nullptr;
            thread.pPrev = nullptr;
        }
        else if (thread.pPrev == nullptr)
        {
            thread.pNext->pPrev = nullptr;
            m_pBegin = thread.pNext;
        }
        else if (thread.pNext == nullptr)
        {
            thread.pPrev->pNext = nullptr;
            m_pEnd = thread.pPrev;
        }
        else
        {
            thread.pPrev->pNext = thread.pNext;
            thread.pNext->pPrev = thread.pPrev;
        }

        m_nNodeCounter--;

        return true;
    }

    Thread* Thread::GetCyclicalNext()
    {
        return (m_pCurrent->pNext) == nullptr ? (m_pCurrent = m_pBegin) : m_pCurrent->pNext;
    }

    void Thread::Scheduler ()
    {
        size_t nThreadCount = m_nNodeCounter;
        Thread* pThread = m_pCurrent;
        atomicx_time tm = GetTick ();

        while (nThreadCount--)
        {
            pThread = (pThread->pNext) == nullptr ? (pThread = m_pBegin) : pThread->pNext;
            
            TRACE (KERNEL, pThread << "." << pThread->GetName() << ": Status: " << GetStatusName(pThread->m_status) << ", Now: " << tm << ", nextEvent: " << (int32_t)(pThread->m_nextEvent - tm) << ":" << pThread->m_nextEvent);
         
            if (m_pCurrent->m_nextEvent >= pThread->m_nextEvent)
            {
                m_pCurrent = pThread->m_nextEvent == m_pCurrent->m_nextEvent ? pThread->m_priority > m_pCurrent->m_priority ? pThread : m_pCurrent : pThread;
            }
        }

        TRACE (KERNEL, m_pCurrent << "." << m_pCurrent->GetName() << ": LEAVING Status: " << GetStatusName(m_pCurrent->m_status) << ", Now: " << tm << ", nextEvent: " << (int32_t)(m_pCurrent->m_nextEvent - tm));

        if (m_pCurrent->m_nextEvent > tm)
        {
            TRACE (KERNEL, "SLEEPING: " << pThread << "." << pThread->GetName() << ", Status;" << GetStatusName (m_pCurrent->m_status) << ", tm: " << tm << ", next: " << m_pCurrent->m_nextEvent << ", sleep: " << (int32_t) (m_pCurrent->m_nextEvent - tm));

            SleepTick (m_pCurrent->m_nextEvent - tm);
        }
    }

    void Thread::SetPriority (uint8_t value)
    {
        m_priority = value;
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
            Thread::Scheduler ();

            TRACE (KERNEL, "------------------------------------");
            TRACE (KERNEL, m_pCurrent->GetName () << "_" <<(size_t) m_pCurrent << ": St [" << GetStatusName (m_pCurrent->m_status) << "]");
            TRACE (KERNEL, "Stack size: " << m_pCurrent->nStackSize << ", Max: " << m_pCurrent->m_nMaxStackSize << ", Occupied: " << (100*m_pCurrent->nStackSize)/(m_pCurrent->m_nMaxStackSize) << "%");

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

        TRACE (KERNEL, "Stack size: " << m_pCurrent->nStackSize << ", Max: " << m_pCurrent->m_nMaxStackSize << ", Occupied: " << (100*m_pCurrent->nStackSize)/(m_pCurrent->m_nMaxStackSize) << "%");

        if (st == Status::now)
        {
            tm =  GetTick ();
        }
        else
        {
            tm = GetTick () + (tm ? tm : m_pCurrent->m_nice);
        }
        
        m_pCurrent->m_status = st;
        m_pCurrent->m_nextEvent = tm;

        if (setjmp (m_pCurrent->m_context) != 0)
        {
            m_pCurrent->nStackSize = m_pStartStack - m_pCurrent->m_pEndStack;
            memcpy ((void*) m_pCurrent->m_pEndStack, (const void*) &m_pCurrent->m_stack, m_pCurrent->nStackSize);

            NOTRACE(KERNEL, (size_t) m_pCurrent << ": RETURNED from Join.");

            return true;
        } 

        memcpy ((void*) &m_pCurrent->m_stack, (const void*) m_pCurrent->m_pEndStack, m_pCurrent->nStackSize);

        longjmp (m_joinContext, 1);

        m_pCurrent->m_status = Status::running;

        return false;
    }

    size_t Thread::GetThreadCount ()
    {
        return m_nNodeCounter;
    }

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

    Iterator<Thread> Thread::begin()
    {
        return Iterator<Thread>(m_pBegin);
    } 

    Iterator<Thread> Thread::end()
    {
        return Iterator<Thread>(nullptr);
    }

    Status Thread::GetStatus ()
    {
        return m_status;
    }

    atomicx_time Thread::GetNice ()
    {
        return m_nice;
    }

    atomicx_time Thread::GetNextEvent ()
    {
        return m_nextEvent;
    }

    /**
     * ------------------------------
     * SMART LOCK IMPLEMENTATION
     * ------------------------------
     */

    size_t Mutex::GetSharedLockCount ()
    {
        return nSharedLockCount;
    }

    bool Mutex::GetExclusiveLockStatus ()
    {
        return bExclusiveLock;
    }

    bool Mutex::Lock(Timeout timeout)
    {
        auto pAtomic = Thread::m_pCurrent;

        if(pAtomic == nullptr) return false;

        TRACE (LOCK, "CHECKING bExclusiveLock: " << bExclusiveLock);
        
        // Get exclusive Mutex
        while (bExclusiveLock) if  (! pAtomic->SysWait(bExclusiveLock, SYSTEM_CHANNEL,  1, timeout.GetRemaining())) return false;

        TRACE (LOCK, "Acquiring, bExclusiveLock: " << bExclusiveLock);
        
        bExclusiveLock = true;

        TRACE (LOCK, "Exclusive lock, Acquired, Waiting all reading to be done,  nSharedLockCount: " << nSharedLockCount);
        
        // Wait all shared locks to be done
        while (nSharedLockCount) if (! pAtomic->SysWait(nSharedLockCount, SYSTEM_CHANNEL, 2, timeout.GetRemaining())) return false;

        TRACE (LOCK, "All reading done.., Exlucive lock acquired.  nSharedLockCount: " << nSharedLockCount);
        
        return true;
    }

    bool Mutex::TryLock()
    {
        if (bExclusiveLock || nSharedLockCount > 0) return false;

        bExclusiveLock = true;

        return true;
    }

    void Mutex::Unlock()
    {
        auto pAtomic = Thread::m_pCurrent;

        if(pAtomic == nullptr) return;

        if (bExclusiveLock == true)
        {
            bExclusiveLock = false;

            // Notify Other locks procedures
            pAtomic->SysNotify(nSharedLockCount, SYSTEM_CHANNEL,  2, false);
            pAtomic->SysNotify(bExclusiveLock, SYSTEM_CHANNEL, 1, true);
        }
        
        TRACE (LOCK, "Unlocked, bExclusiveLock: " << bExclusiveLock << ", nSharedLockCount: " << nSharedLockCount);
    }

    bool Mutex::SharedLock(Timeout timeout)
    {
        auto pAtomic = Thread::m_pCurrent;

        if(pAtomic == nullptr) return false;

        TRACE (LOCK, "Wait Exclusive lock done, bExclusiveLock: " << bExclusiveLock << ", nSharedLockCount: " << nSharedLockCount);
        
        // Wait for exclusive Mutex
        while (bExclusiveLock > 0) if (! pAtomic->SysWait(bExclusiveLock, SYSTEM_CHANNEL, 1, timeout.GetRemaining())) { return false; }

        nSharedLockCount++;

        // Notify Other locks procedures
        pAtomic->SysNotify (nSharedLockCount, SYSTEM_CHANNEL, 2, true);

        TRACE (LOCK, "SharedLocked, bExclusiveLock: " << bExclusiveLock << ", nSharedLockCount: " << nSharedLockCount);

        return true;
    }

    bool Mutex::TrySharedLock()
    {
        if (bExclusiveLock || nSharedLockCount > 0) return false;

        nSharedLockCount++;

        return true;
    }

    void Mutex::SharedUnlock()
    {
        auto pAtomic = Thread::m_pCurrent;

        if(pAtomic == nullptr) return;

        if (nSharedLockCount)
        {
            nSharedLockCount--;

            pAtomic->SysNotify(nSharedLockCount, SYSTEM_CHANNEL, 2, true);
        }
        
        TRACE (LOCK, "SharedUnlocked, bExclusiveLock: " << bExclusiveLock << ", nSharedLockCount: " << nSharedLockCount);
    }

    size_t Mutex::IsShared()
    {
        return nSharedLockCount;
    }

    bool Mutex::IsLocked()
    {
        return bExclusiveLock;
    }

    SmartMutex::SmartMutex (Mutex& lockObj) : m_lock(lockObj)
    {}

    SmartMutex::~SmartMutex()
    {
        switch (m_lockType)
        {
            case 'L':
                m_lock.Unlock();
                break;
            case 'S':
                m_lock.SharedUnlock();
                break;
        }
    }

    bool SmartMutex::SharedLock(Timeout timeout)
    {
        bool bRet = false;

        if (m_lockType == '\0')
        {
            if (m_lock.SharedLock(timeout))
            {
                m_lockType = 'S';
                bRet = true;
            }
        }

        return bRet;
    }

    bool SmartMutex::Lock(Timeout timeout)
    {
        bool bRet = false;

        if (m_lockType == '\0')
        {
            if (m_lock.Lock(timeout))
            {
                m_lockType = 'L';
                bRet = true;
            }
        }

        return bRet;
    }

    size_t SmartMutex::IsShared()
    {
        return m_lock.IsShared();
    }

    bool SmartMutex::IsLocked()
    {
        return m_lock.IsLocked();
    }
}
