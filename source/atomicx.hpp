//
//  atomic.hpp
//  atomic
//
//  Created by GUSTAVO CAMPOS on 31/01/2023.

#ifndef atomic_hpp
#define atomic_hpp

#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

#define NOTRACE(i, x) (void)#x

#ifdef _DEBUG
#include <iostream>
#define TRACE(i, x)                                                                                          \
	if (DBGLevel::i <= DBGLevel::_DEBUG)                                                                     \
	std::cout << Thread::GetCurrent() << "(" << Thread::GetCurrent()->GetName() << ")[" << #i << "] "        \
	          << "(" << __FUNCTION__ << ", " << __FILE_NAME__ << ":" << __LINE__ << "):  " << x << std::endl \
	          << std::flush
#else
#define TRACE(i, x) NOTRACE(i, x)
#endif

enum class DBGLevel
{
	CRITICAL,
	ERROR,
	WARNING,
	INFO,
	TRACE,
	DEBUG,
	LOCK,
	WAIT,
	KERNEL
};

namespace atomicx
{
#define GetStackPoint()                 \
	({                                  \
		volatile uint8_t ___var = 0xBB; \
		&___var;                        \
	})

	enum class Status : uint8_t
	{
		none        = 0,
		starting    = 1,
		ctxSwitch   = 12,
		sleep       = 13,
		timeout     = 14,
		halted      = 15,
		paused      = 16,
		locked      = 100,
		running     = 200,
		now         = 201,
		wait        = 220,
		syncWait    = 221,
		syncSysWait = 222,
		sysWait     = 223,
	};

	enum class Notify : uint8_t
	{
		one,
		all
	};

    struct Message
    {
        size_t message;
        size_t type;
    };

	const char *GetStatusName(Status st);

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
		Timeout();

		/**
         * @brief Construct a new Timeout object
         *
         * @param nTimeoutValue  Timeout value to be calculated
         *
         * @note    To decrease the amount of memory, Timeout does not save
         *          the start time.
         *          Special use case: if nTimeoutValue == 0, IsTimedout is always false.
         */
		Timeout(atomicx_time nTimeoutValue);

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
         * @brief Check wether it Can timeout
         *
         * @return true if tiemout value > 0 otherwise false
         */
        bool CanTimeout();
        
		/**
         * @brief Get the remaining time till timeout
         *
         * @return atomicx_time Remaining time till timeout, otherwise 0;
         */
		atomicx_time GetRemaining();

		/**
         * @brief Get the Time Since the specific point in time
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
     * @tparam T Type of the object used for iteration
     */
	template <typename T>
	class Iterator
	{
	public:
		Iterator() = delete;

		/**
         * @brief Type based constructor
         *
         * @param ptr Type pointer to iterate
         */
		Iterator(T *ptr)
		    : m_ptr(ptr)
		{
		}

		/*
         * Access operator
         */
		T &operator*()
		{
			return *m_ptr;
		}

		T *operator->()
		{
			return &m_ptr;
		}

		/*
         * Movement operator
         */
		Iterator<T> &operator++()
		{
			if (m_ptr != nullptr)
			{
				m_ptr = (T *)m_ptr->operator++();
			}

			return *this;
		}

		/*
         * Binary operators
         */
		friend bool operator==(const Iterator &a, const Iterator &b)
		{
			return a.m_ptr == b.m_ptr;
		}

		friend bool operator!=(const Iterator &a, const Iterator &b)
		{
			return a.m_ptr != b.m_ptr;
		}

	private:
		T *m_ptr;
	};

	struct KNode
	{
	protected:
		Thread *pPrev = nullptr;
		Thread *pNext = nullptr;

	public:
		Thread *operator++()
		{
			return pNext;
		}
	};

#define SYSTEM_CHANNEL 1


	/* *************************************************** *\
        THREAD CLASS
    \* *************************************************** */

	template <typename T>
	class Iterator;

	class Thread : public KNode
	{
	private:
		friend bool AttachThread(Thread &);
		friend bool DetachThread(Thread &);

		friend class Mutex;
		friend class SmartMutex;

		/* Kernel ------------------ */
		static Thread *m_pBegin;
		static Thread *m_pEnd;
		static Thread *m_pCurrent;

		static size_t m_nNodeCounter;

		static volatile uint8_t *m_pStartStack;

		static jmp_buf m_joinContext;

		static Thread *GetCyclicalNext();

		static void Scheduler();

		/* ------------------------ */

		/* Kernel ------------------ */

		Status m_status = Status::starting;

		// Thread context register buffer
		jmp_buf m_context = {};

		volatile uint8_t *m_pEndStack = nullptr;
        uint8_t m_priority{0};

        atomicx_time m_nice{0};
        atomicx_time m_nextEvent{0};
        int32_t m_late{0};

        size_t nStackSize{0};

		size_t m_nMaxStackSize;

		volatile size_t &m_stack;

		/* ------------------------ */

        enum class NotifyChennelType : uint16_t
        {
            KERNEL,
            MUTEX,
            USER
        };
        
		/* Notify controller-------- */
        void *m_pWaitEndPoint{nullptr};
        
        Message m_messagectl;
        
        NotifyChennelType m_msgChannel{NotifyChennelType::KERNEL};

        union
        {
            bool noTimout : 1;
            uint8_t nValue;
        } m_flags{0};
        
		Thread() = delete;
        /* ------------------------ */

	protected:

        virtual void run(void) = 0;

		static bool AttachThread(Thread &thread);

		static bool DetachThread(Thread &thread);

		void SetPriority(uint8_t value);

		template <size_t N>
		Thread(atomicx_time nNice, volatile size_t (&stack)[N])
		    : m_nice(nNice)
		    , m_nMaxStackSize(N * sizeof(size_t))
		    , m_stack(stack[0])
		{
			AttachThread(*this);
		}

        inline size_t SafeNotify(Status status, NotifyChennelType channel, void* pEndPoit, Message msg)
        {
            size_t nNotified = 0;
            
            NOTRACE(WAIT, "LOOKING: EP:" << pEndPoit << ", Status:" << GetStatusName(status) << ", type:" << msg.type << ", channel:" << (uint16_t) channel);
            
            for (auto& th : *this)
            {
                NOTRACE(WAIT, "TRYING: " << &th << ", EP:" << th.m_pWaitEndPoint << ", Status:" << GetStatusName(th.m_status) << ", type:" << th.m_messagectl.type << ", channel:" << (uint16_t) th.m_msgChannel);
                
                if (th.m_status == status && th.m_msgChannel == channel && th.m_pWaitEndPoint == pEndPoit)
                {
                    if (th.m_messagectl.type == msg.type)
                    {
                        th.m_messagectl.message = msg.message;
                        th.m_status = Status::now;
                        th.m_nextEvent = GetTick();
                        nNotified++;
                        
                        TRACE(WAIT, "EP:" << &th << ", type:" << th.m_messagectl.type << ", msg:" << th.m_messagectl.message);
                    }
                }
            }
            
            NOTRACE(WAIT, "NOTIFY: Status:" << GetStatusName(status) << ", notified:" << nNotified);
            
            return nNotified;
        }

        void SafeWait(NotifyChennelType channel, void* endPoint, size_t& nType, Timeout& tm)
        {
            m_msgChannel = channel;
            m_pWaitEndPoint = endPoint;
            m_messagectl.type = nType;
            m_flags.noTimout = !tm.CanTimeout();
        }
        
        bool GenericWait(NotifyChennelType channel, void* endPoint, size_t nType, size_t& nMessage, Timeout tm)
        {
            SafeNotify(Status::syncWait, channel, endPoint, {.type = nType, .message=0});
            Yield(0, Status::now);
            
            SafeWait(channel, endPoint, nType, tm);
            
            Yield(tm.GetRemaining(), Status::wait);
            
            if (m_status != Status::timeout)
            {
                nMessage = m_messagectl.message;
            }
            
            TRACE(WAIT,"WAIT: Status: " << GetStatusName(m_status));
            
            return m_status != Status::timeout;
        }

        inline size_t GenericNotify(NotifyChennelType channel, void* endPoint, Message msg, Timeout tm, Notify howMany)
        {
            size_t nNotified = 0;
            
            while ((nNotified = SafeNotify(Status::wait, channel, endPoint, msg) == 0 && tm.GetRemaining()))
            {
                SafeWait(channel, endPoint, msg.type, tm);
                Yield(tm.GetRemaining(), Status::syncWait);
                
                if (m_status == Status::timeout)
                {
                    TRACE(WAIT, "Timeout on syncWait");
                    return 0;
                }
            }
            
            Yield(0, Status::now);
            
            return nNotified;
        }

        template <typename T>
        size_t Notify(T& endPoint, Message msg, Timeout tm, Notify howMany = Notify::one)
        {
            return GenericNotify(NotifyChennelType::USER, (void*)&endPoint, msg, tm, howMany);
        }
        
        template <typename T>
        size_t Wait(T& endPoint, size_t nType, size_t& nMessage, Timeout tm)
        {
            return GenericWait(NotifyChennelType::USER, (void*)&endPoint, nType, nMessage, tm);
        }

	public:
		virtual const char *GetName() = 0;

		virtual ~Thread();

		size_t GetStackSize();

		size_t GetMaxStackSize();

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

		static bool Join();

		static bool Yield(atomicx_time tm = 0, Status st = Status::sleep);

		size_t GetThreadCount();

		Status GetStatus();

		atomicx_time GetNice();

		static Thread *GetCurrent();

		atomicx_time GetNextEvent();

		int32_t GetLate();
	};

} // namespace atomicx

#endif
