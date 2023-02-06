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

            Thread* m_pBegin = nullptr;
            Thread* m_pEnd = nullptr;
            Thread* m_pCurrent = nullptr;

            size_t m_nNodeCounter = 0;

        protected:
            friend class Thread;

            volatile uint8_t* m_pStartStack = nullptr;

            jmp_buf m_joinContext = {};

            Thread* GetCyclicalNext();

        public:

            /*
            * ATTENTION: GetTick and SleepTick MUST be ported from user
            *
            * crete functions with the following prototype on your code,
            * for example, see test/main.cpp
            *
            *   atomicx_time atomicx::Kernel::GetTick (void) { <code> }
            *   void atomicx::Kernel::SleepTick(atomicx_time nSleep) { <code> }
            */

            /**
             * @brief Implement the custom Tick acquisition
             *
             * @return atomicx_time
             */
            atomicx_time GetTick(void);

            /**
             * @brief Implement a custom sleep, usually based in the same GetTick granularity
             *
             * @param nSleep    How long custom tick to wait
             *
             * @note This function is particularly special, since it give freedom to tweak the
             *       processor power consumption if necessary
             */
            void SleepTick(atomicx_time nSleep);
            
            Kernel () 
            {
                
            }

            Iterator<Thread> begin()
            {
                return Iterator<Thread>(m_pBegin);
            } 

            Iterator<Thread> end()
            {
                return Iterator<Thread>(nullptr);
            }

            bool Join ();

            bool Yield ();

            size_t GetThreadCount ();
    };

    static Kernel kernel;

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

            volatile uint8_t* m_pEndStack = nullptr;

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

            static bool Yield ()
            {
                return kernel.Yield ();
            }
            virtual const char* GetName () = 0;
            
            virtual ~Thread ();

            size_t GetStackSize ();

            size_t GetMaxStackSize ();
    };

    static bool AttachThread (Kernel& kernel, Thread& thread)
    {
        if (kernel.m_pBegin == nullptr)
        {
            kernel.m_pBegin = &thread;
            kernel.m_pEnd = kernel.m_pBegin;
        }
        else
        {
            thread.pPrev = kernel.m_pEnd;
            kernel.m_pEnd->pNext = &thread;
            kernel.m_pEnd = &thread;
        }

        kernel.m_nNodeCounter++;

        return true;
    }


    static bool DetachThread (Kernel& kernel, Thread& thread)
    {
        if (thread.pNext == nullptr && thread.pPrev == nullptr)
        {
            kernel.m_pBegin = nullptr;
            thread.pPrev = nullptr;
        }
        else if (thread.pPrev == nullptr)
        {
            thread.pNext->pPrev = nullptr;
            kernel.m_pBegin = thread.pNext;
        }
        else if (thread.pNext == nullptr)
        {
            thread.pPrev->pNext = nullptr;
            kernel.m_pEnd = thread.pPrev;
        }
        else
        {
            thread.pPrev->pNext = thread.pNext;
            thread.pNext->pPrev = thread.pPrev;
        }

        kernel.m_nNodeCounter--;

        return true;
    }

}

#endif