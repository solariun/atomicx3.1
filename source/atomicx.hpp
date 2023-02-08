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
    
    /* *************************************************** *\
        THREAD CLASS 
    \* *************************************************** */

    /**
    * @brief General purpose timeout  facility
    */

    class Timeout
    {
        public:

            /**
             * @brief Default construct a new Timeout object
             *
             * @note    To decrease the amount of memory, Timeout does not save
             *          the start time.
             *          Special use case: if nTimeoutValue == 0, IsTimedout is always false.
             */
            Timeout ();

            /**
             * @brief Construct a new Timeout object
             *
             * @param nTimeoutValue  Timeout value to be calculated
             *
             * @note    To decrease the amount of memory, Timeout does not save
             *          the start time.
             *          Special use case: if nTimeoutValue == 0, IsTimedout is always false.
             */
            Timeout (atomicx_time nTimeoutValue);

            /**
             * @brief Set a timeout from now
             *
             * @param nTimeoutValue timeout in atomicx_time
             */
            void Set(atomicx_time nTimeoutValue);

            /**
             * @brief Check wether it has timeout
             *
             * @return true if it timeout otherwise 0
             */
            bool IsTimedout();

            /**
             * @brief Get the remaining time till timeout
             *
             * @return atomicx_time Remaining time till timeout, otherwise 0;
             */
            atomicx_time GetRemaining();

            /**
             * @brief Get the Time Since specific point in time
             *
             * @param startTime     The specific point in time
             *
             * @return atomicx_time How long since the point in time
             *
             * @note    To decrease the amount of memory, Timeout does not save
             *          the start time.
             */
            atomicx_time GetDurationSince(atomicx_time startTime);

        private:
            atomicx_time m_timeoutValue = 0;
    };

    /* *************************************************** *\
        ITERATOR CLASS 
    \* *************************************************** */

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
        Iterator(T*  ptr) : m_ptr(ptr)
        {}

        /*
        * Access operator
        */
        T& operator*()
        {
            return *m_ptr;
        }

        T* operator->()
        {
            return &m_ptr;
        }

        /*
        * Movement operator
        */
        Iterator<T>& operator++()
        {
            if (m_ptr != nullptr)
            {
                m_ptr = (T*) m_ptr->operator++ ();
            }

            return *this;
        }

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

    enum class Status : uint8_t
    {
        none =0,
        starting=1,
        wait=10,
        syncWait=11,
        ctxSwitch=12,
        sleep=13,
        timeout=14,
        halted=15,
        paused=16,
        syncSysWait=17,
        sysWait=18,
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

            caseStatus (Status::sysWait);
            caseStatus (Status::syncSysWait);

            caseStatus (Status::sleep);
            caseStatus (Status::timeout);
            caseStatus (Status::halted);
            caseStatus (Status::paused);
            
            caseStatus (Status::locked);

            caseStatus (Status::running);
            caseStatus (Status::now);
        }

        return name;
    } 

    /* *************************************************** *\
        THREAD CLASS 
    \* *************************************************** */

    template <typename T> class Iterator;

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
            
            static void Scheduller ();
            
            /* ------------------------ */

            /* Kernel ------------------ */

            Status m_status = Status::starting;

            // Thread context register buffer
            jmp_buf m_context = {};

            volatile uint8_t* m_pEndStack = nullptr;

            atomicx_time m_nice = 0;
            atomicx_time m_nextEvent = 0;

            size_t nStackSize = 0;

            size_t m_nMaxStackSize;

            volatile size_t&  m_stack; 

            /* ------------------------ */

            /* Notify payload -------- */
            void* m_pWaitEndPoint;
            size_t m_message = 0;
            size_t m_msgType = 0;
            size_t m_msgChannel = 0;

            Thread () = delete;
            /* ------------------------ */

            #define _WAIT(syncType, waitType) \
                if (tm.GetRemaining () > 0) \
                { \
                    (void) SafeNotify (var, msgChannel, message, msgType, true, syncType); \
                } \
                \
                SafeWait (var, msgChannel, message, msgType); \
                \
                Yield (tm.GetRemaining (), waitType); \
                \
                if (m_status == Status::timeout) \
                { \
                    return false; \
                }

            template<typename T> bool SysWait (T& var, size_t& msgChannel, size_t& message, size_t msgType, Timeout tm = 0)
            {
                _WAIT (Status::syncSysWait, Status::sysWait);
                
                message = m_message;
                msgChannel = m_msgChannel;
                
                return true;
            }

            #define _NOTIFY(var, msgChannel, message, msgType, tm, one, syncType, waitType) \
                if (tm.GetRemaining () > 0) \
                { \
                    SafeWait (var, msgChannel, message, msgType); \
                    \
                    Yield (tm.GetRemaining (), syncType); \
                    \
                    if (m_status == Status::timeout) return 0; \
                } \
                \
                nNotified = SafeNotify (var, msgChannel, message, msgType, one, waitType); \
                \
                Yield (0);

            template<typename T> size_t SysNotify (T& var, size_t msgChannel, size_t message, size_t msgType, Timeout tm = 0, bool one = true)
            {
                size_t nNotified = 0;

                _NOTIFY (var, msgChannel, message, msgType, tm, one, Status::syncSysWait, Status::sysWait);

                return nNotified;
            }

        protected:
            virtual void run(void) = 0;

            static bool AttachThread (Thread& thread);

            static bool DetachThread (Thread& thread);

            template<size_t N>Thread (atomicx_time nNice, volatile size_t (&stack)[N]) : 
                m_nice (nNice), 
                m_nMaxStackSize (N * sizeof (size_t)),
                m_stack (stack [0])
            {
                AttachThread (*this);
            }

            template<typename T> void SafeWait (T& var, size_t msgChannel, size_t message, size_t msgType)
            {
                m_pWaitEndPoint = static_cast<void*>(&var);

                m_message = message;
                m_msgType = msgType;
                m_msgChannel = msgChannel;
            }

            template<typename T> size_t SafeNotify (T& var, size_t msgChannel, size_t message, size_t msgType, bool one, Status stType)
            {
                size_t nNotified = 0;

                for (auto& th : *this)
                {
                    if (th.m_status == stType && th.m_pWaitEndPoint == static_cast<void*>(&var))
                    {
                        /* MessageType == 0 means accept all message from the same WaitEndPoint */
                        if (th.m_msgChannel == msgChannel  || th.m_msgChannel == 0)
                        {
                            if (th.m_msgType == msgType || th.m_msgType == 0)
                            {
                                th.m_msgChannel = msgChannel;
                                th.m_msgType = msgType;
                                th.m_message = message;

                                th.m_status = Status::now;
                                th.m_nextEvent = GetTick ();

                                nNotified++;

                                if (one == true) break;
                            }
                        }
                    }
                }

                return nNotified;
            }

            template<typename T> bool Wait (T& var, size_t msgChannel, size_t& message, size_t msgType, Timeout tm = 0)
            {
                _WAIT (Status::syncWait, Status::wait);

                message = m_message;

                return true;
            }

            template<typename T> bool Wait (T& var, size_t msgChannel, size_t& message, size_t &msgType, Timeout tm = 0)
            {
                _WAIT (Status::syncWait, Status::wait);
                
                message = m_message;
                msgType = m_msgType;
                
                return true;
            }

            template<typename T> size_t Notify (T& var, size_t msgChannel, size_t message, size_t msgType, Timeout tm = 0, bool one = true)
            {
                size_t nNotified = 0;

                _NOTIFY (var, msgChannel, message, msgType, tm, one, Status::syncWait, Status::wait);

                return nNotified;
            }

        public:

            virtual const char* GetName () = 0;
            
            virtual ~Thread ();

            size_t GetStackSize ();

            size_t GetMaxStackSize ();

            Iterator<Thread> begin();

            Iterator<Thread> end();

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
            static atomicx_time GetTick(void);

            /**
             * @brief Implement a custom sleep, usually based in the same GetTick granularity
             *
             * @param nSleep    How long custom tick to wait
             *
             * @note This function is particularly special, since it give freedom to tweak the
             *       processor power consumption if necessary
             */
            static void SleepTick(atomicx_time nSleep);

            static bool Join ();

            static bool Yield (atomicx_time tm = 0, Status st = Status::sleep);

            size_t GetThreadCount ();

            Status GetStatus ();

            atomicx_time GetNice ();
    };

}

#endif