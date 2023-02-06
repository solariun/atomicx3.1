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
#define TRACE(i, x) if (DBGLevel::i <= DBGLevel::_DEBUG) std::cout << "[" << #i << "] "  << "(" << __FUNCTION__ << ", " << __FILE_NAME__ << ":" << __LINE__ << "):  " << x << std::endl << std::flush
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
        Thread* pPrev = nullptr;
        Thread* pNext = nullptr;

    public:
        Thread* operator++ ()
        {
            return pNext;
        }
    };

    static bool AttachThread (Thread& thread);

    static bool DetachThread (Thread& thread);

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

    /*
        KERNEL internals
    */

    namespace Kernel 
    {
        static Thread* m_pBegin = nullptr;
        static Thread* m_pEnd = nullptr;
        static Thread* m_pCurrent = nullptr;

        static size_t m_nNodeCounter = 0;

        static volatile uint8_t* m_pStartStack = nullptr;

        static jmp_buf m_joinContext = {};

        static Thread* GetCyclicalNext();


    }

    /*
        THREAD CLASS 
    */

    class Thread : public KNode
    {
        private:
            friend bool AttachThread (Thread&);
            friend bool DetachThread (Thread&);

            /* Kernel ------------------ */
            static Thread* m_pBegin;
            static Thread* m_pEnd;
            static Thread* m_pCurrent;

            static size_t m_nNodeCounter;

            static volatile uint8_t* m_pStartStack;

            static jmp_buf m_joinContext;

            static Thread* GetCyclicalNext();
            
            /* ------------------------ */

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
            virtual void run(void) = 0;

            static bool AttachThread (Thread& thread)
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

            static bool DetachThread (Thread& thread)
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

            template<size_t N>Thread (atomicx_time nNice, volatile size_t (&stack)[N]) : 
                m_nNice (nNice), 
                m_nMaxStackSize (N * sizeof (size_t)),
                m_stack (stack [0])
            {
                AttachThread (*this);
            }

        public:

            virtual const char* GetName () = 0;
            
            virtual ~Thread ();

            size_t GetStackSize ();

            size_t GetMaxStackSize ();

            Iterator<Thread> begin()
            {
                return Iterator<Thread>(m_pBegin);
            } 

            Iterator<Thread> end()
            {
                return Iterator<Thread>(nullptr);
            }

            /*
            * ATTENTION: GetTick and SleepTick MUST be ported from user
            *
            * crete functions with the following prototype on your code,
            * for example, see test/main.cpp
            *
            *   atomicx_time atomicx::GetTick (void) { <code> }
            *   void atomicx::SleepTick(atomicx_time nSleep) { <code> }
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

            static bool Join ();

            static bool Yield ();

            size_t GetThreadCount ();
    };


}

#endif