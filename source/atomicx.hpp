//
//  atomic.hpp
//  atomic
//
//  Created by GUSTAVO CAMPOS on 31/01/2023.


#ifndef atomic_hpp
#define atomic_hpp

#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>

/* Official version */
#define ATOMICX_VERSION "1.3.0"
#define ATOMIC_VERSION_LABEL "AtomicX v" ATOMICX_VERSION " built at " __TIMESTAMP__

typedef uint32_t atomicx_time;

// ------------------------------------------------------
// LOG FACILITIES
//
// TO USE, define -D_DEBUG=<LEVEL> where level is any of
// . those listed in DBGLevel, ex
// .    -D_DEBUG=INFO
// .     * on the example, from DEBUG to INFO will be
// .       displayed
// ------------------------------------------------------

#define STRINGIFY_2(a) #a
#define STRINGIFY(a) STRINGIFY_2(a)

#define NOTRACE(i, x) (void) #x

#ifdef _DEBUG
#include <iostream>
#define TRACE(i, x) if (DBGLevel::i <= DBGLevel::_DEBUG) std::cout << "[" << #i << "] " << this << "(" << __FUNCTION__ << ", " << __FILE_NAME__ << ":" << __LINE__ << "):  " << x << std::endl << std::flush
#else
#define TRACE(i, x) NOTRACE(i,x)
#endif

enum class DBGLevel
{
    TRACE,
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL,
};

namespace atomicx
{
    #define GetStackPoint() ({volatile uint8_t ___var = 0xBB; &___var;})

    class Thread;
    class Kernel;

    /*
    * ---------------------------------------------------------------------
    * Iterator Implementation
    * ---------------------------------------------------------------------
    */

    /**
    * @brief General purpose Iterator facility
    *
    * @tparam T
    */
    template <typename T> class Iterator
    {
    public:
        Iterator() = delete;

        /**
         * @brief Type based constructor
         *
         * @param ptr Type pointer to iterate
         */
        Iterator(T*  ptr);

        /*
        * Access operator
        */
        T& operator*();

        T* operator->();

        /*
        * Movement operator
        */
        Iterator<T>& operator++();

        /*
        * Binary operators
        */
        friend bool operator== (const Iterator& a, const Iterator& b)
        {
            return a.m_ptr == b.m_ptr;
        }


        friend bool operator!= (const Iterator& a, const Iterator& b)
        {
            return a.m_ptr != b.m_ptr;
        }


    private:
        T* m_ptr;
    };

    template <typename T> Iterator<T>::Iterator(T*  ptr) : m_ptr(ptr)
    {}

    /*
    * Access operator
    */
    template <typename T> T& Iterator<T>::operator*()
    {
        return *m_ptr;
    }

    template <typename T> T* Iterator<T>::operator->()
    {
        return &m_ptr;
    }

    /*
    * Movement operator
    */
    template <typename T> Iterator<T>& Iterator<T>::operator++()
    {
        if (m_ptr != nullptr)
        {
            m_ptr = (T*) m_ptr->operator++ ();
        }

        return *this;
    }

    struct KNode
    {
    protected:
        friend class Kernel;

        Thread* pPrev = nullptr;
        Thread* pNext = nullptr;

    public:
        Thread* operator++ ()
        {
            return pNext;
        }
    };

    static bool AttachThread (Kernel& kernel, Thread& thread);

    static bool DetachThread (Kernel& kernel, Thread& thread);

    enum class Status : uint8_t
    {
        none =0,
        starting=1,
        wait=10,
        syncWait=11,
        ctxSwitch=12,
        sleeping=13,
        timeout=14,
        halted=15,
        paused=16,
        locked=100,
        running=200,
        now=201
    };

    static const char* GetStatusName(Status st)
    {
    #define caseStatus(st) case st: name=#st; break;
        const char* name="undefined";

        switch (st)
        {
            caseStatus (Status::none);
            caseStatus (Status::starting);

            caseStatus (Status::wait);
            caseStatus (Status::syncWait);
            caseStatus (Status::ctxSwitch);
            
            caseStatus (Status::sleeping);
            caseStatus (Status::timeout);
            caseStatus (Status::halted);
            caseStatus (Status::paused);
            
            caseStatus (Status::locked);

            caseStatus (Status::running);
            caseStatus (Status::now);
        }

        return name;
    } 

    bool StaticYield (Kernel& k);

    /*
        KERNEL CLASS 
    */

    class Kernel
    {
        private:
            friend class Thread;
            friend bool AttachThread (Kernel&, Thread&);
            friend bool DetachThread (Kernel&, Thread&);

            Thread* pBegin = nullptr;
            Thread* pEnd = nullptr;
            Thread* pCurrent = nullptr;

            size_t m_nNodeCounter = 0;

        protected:
            friend class Thread;

            Thread* GetCyclicalNext();
            jmp_buf m_joinContext = {};
            volatile uint8_t* pStartStack = nullptr;

        public:

            Kernel () 
            {
                
            }

            Iterator<Thread> begin()
            {
                return Iterator<Thread>(pBegin);
            } 

            Iterator<Thread> end()
            {
                return Iterator<Thread>(nullptr);
            }

            bool Join ();

            bool Yield ();

            void Start (volatile uint8_t* start)
            {
                pStartStack = start;
            }

            size_t GetThreadCount ()
            {
                return m_nNodeCounter;
            }
    };

    Kernel kernel;

    /*
        THREAD CLASS 
    */

    class Thread : public KNode
    {
        private:
            friend bool AttachThread (Kernel&, Thread&);
            friend bool DetachThread (Kernel&, Thread&);

            Status m_status = Status::starting;

            // Thread context register buffer
            jmp_buf m_context = {};

            volatile uint8_t* pEndStack = nullptr;

            atomicx_time m_nNice = 0;

            size_t nStackSize = 0;

            size_t m_nMaxStackSize;

            volatile size_t&  m_stack; 

            Thread () = delete;
            
        protected:
            friend class Kernel;

            virtual void run(void) = 0;

            template<size_t N>Thread (atomicx_time nNice, volatile size_t (&stack)[N]) : 
                m_nNice (nNice), 
                m_nMaxStackSize (N * sizeof (size_t)),
                m_stack (stack [0])
            {
                AttachThread (kernel, *this);
            }

        public:

            virtual const char* GetName () = 0;
            
            virtual ~Thread ()
            {
                DetachThread (kernel, *this);
            }

            size_t GetStackSize ()
            {
                return nStackSize;
            }

            size_t GetMaxStackSize ()
            {
                return m_nMaxStackSize;
            }

            bool Yield ()
            {
                return kernel.Yield ();
            }
    };

    bool AttachThread (Kernel& kernel, Thread& thread)
    {
        if (kernel.pBegin == nullptr)
        {
            kernel.pBegin = &thread;
            kernel.pEnd = kernel.pBegin;
        }
        else
        {
            thread.pPrev = kernel.pEnd;
            kernel.pEnd->pNext = &thread;
            kernel.pEnd = &thread;
        }

        kernel.m_nNodeCounter++;

        return true;
    }


    static bool DetachThread (Kernel& kernel, Thread& thread)
    {
        if (thread.pNext == nullptr && thread.pPrev == nullptr)
        {
            kernel.pBegin = nullptr;
            thread.pPrev = nullptr;
        }
        else if (thread.pPrev == nullptr)
        {
            thread.pNext->pPrev = nullptr;
            kernel.pBegin = thread.pNext;
        }
        else if (thread.pNext == nullptr)
        {
            thread.pPrev->pNext = nullptr;
            kernel.pEnd = thread.pPrev;
        }
        else
        {
            thread.pPrev->pNext = thread.pNext;
            thread.pNext->pPrev = thread.pPrev;
        }

        kernel.m_nNodeCounter--;

        return true;
    }

    Thread* Kernel::GetCyclicalNext()
    {
        return (pCurrent->pNext) == nullptr ? (pCurrent = pBegin) : pCurrent->pNext;
    }

    bool Kernel::Join ()
    {    
        pCurrent = pEnd;

        if (pCurrent != nullptr )
        {
            pStartStack = GetStackPoint (); //&nStackPoint;

            setjmp (m_joinContext);

            if (m_nNodeCounter == 0) return false;

            pCurrent = GetCyclicalNext (); 

            TRACE (TRACE, "------------------------------------");
            TRACE (TRACE, pCurrent->GetName () << "_" <<(size_t) pCurrent << ": St [" << GetStatusName (pCurrent->m_status) << "]");
            TRACE (TRACE, "Stack size: " << pCurrent->nStackSize << ", Max: " << pCurrent->m_nMaxStackSize << ", Occupied: " << (100*pCurrent->nStackSize)/(pCurrent->m_nMaxStackSize) << "%");

            if (pCurrent->m_status ==  Status::starting)
            {
                pCurrent->m_status = Status::running;

                pCurrent->run ();
    
                pCurrent->m_status = Status::starting;

                longjmp (m_joinContext, 1);
            }
            else
            {
                longjmp (pCurrent->m_context, 1);
            }
        }

        return false;
    }

    bool Kernel::Yield ()
    {
        pCurrent->pEndStack = GetStackPoint (); 
        pCurrent->nStackSize = pStartStack - pCurrent->pEndStack + sizeof (size_t);

        TRACE(TRACE, "Stack size: " << pCurrent->nStackSize << ", Max: " << pCurrent->m_nMaxStackSize << ", Occupied: " << (100*pCurrent->nStackSize)/(pCurrent->m_nMaxStackSize) << "%");

        pCurrent->m_status = Status::sleeping;

        if (setjmp (pCurrent->m_context) != 0)
        {
            pCurrent->nStackSize = pStartStack - pCurrent->pEndStack;
            memcpy ((void*) pCurrent->pEndStack, (const void*) &pCurrent->m_stack, pCurrent->nStackSize);

            TRACE(TRACE, (size_t) pCurrent << ": RETURNED from Join.");

            return true;
        } 

        memcpy ((void*) &pCurrent->m_stack, (const void*) pCurrent->pEndStack, pCurrent->nStackSize);

        longjmp (m_joinContext, 1);

        return false;
    }

}

#endif