
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "atomicx.hpp"

#define caseStatus(st) \
	case st:           \
		name = #st;    \
		break;

namespace atomicx
{
	const char *GetStatusName(Status st)
	{
		const char *name = "undefined";

		switch (st)
		{
			caseStatus(Status::none);
			caseStatus(Status::starting);

			caseStatus(Status::wait);
			caseStatus(Status::syncWait);
			caseStatus(Status::ctxSwitch);

			caseStatus(Status::sysWait);
			caseStatus(Status::syncSysWait);

			caseStatus(Status::sleep);
			caseStatus(Status::timeout);
			caseStatus(Status::halted);
			caseStatus(Status::paused);

			caseStatus(Status::locked);

			caseStatus(Status::running);
			caseStatus(Status::now);
		}

		return name;
	}

	/*
     * Tiemout functions
     */

	/*
     * timeout methods implementations
     */

	Timeout::Timeout()
	    : m_timeoutValue(0)
	{
		Set(0);
	}

    bool Timeout::CanTimeout()
    {
        return (m_timeoutValue == 0 ? false : true);
    }

	Timeout::Timeout(atomicx_time nTimeoutValue)
	    : m_timeoutValue(nTimeoutValue ? nTimeoutValue + Thread::GetTick() : 0)
	{
	}

	void Timeout::Set(atomicx_time nTimeoutValue)
	{
		m_timeoutValue = nTimeoutValue ? nTimeoutValue + Thread::GetTick() : 0;
		TRACE(DEBUG, "nTimeoutValue: " << nTimeoutValue << ", m_timeoutValue: " << m_timeoutValue);
	}

	bool Timeout::IsTimedout()
	{
		return (m_timeoutValue == 0 || m_timeoutValue > Thread::GetTick()) ? false : true;
	}

	atomicx_time Timeout::GetRemaining()
	{
		auto nNow = Thread::GetTick();

		return (m_timeoutValue && nNow < m_timeoutValue) ? m_timeoutValue - Thread::GetTick() : 0;
	}

	atomicx_time Timeout::GetDurationSince(atomicx_time startTime)
	{
		return startTime - GetRemaining();
	}

	/*
        THREAD
    */

	// Thread kernel static initializations

	Thread *Thread::m_pBegin   = nullptr;
	Thread *Thread::m_pEnd     = nullptr;
	Thread *Thread::m_pCurrent = nullptr;

	size_t Thread::m_nNodeCounter = 0;

	volatile uint8_t *Thread::m_pStartStack = nullptr;

	jmp_buf Thread::m_joinContext = {};

	/* ------------------------ */

	Status m_status = Status::starting;

	// Thread context register buffer
	jmp_buf m_context = {};

	volatile uint8_t *m_pEndStack = nullptr;

	Thread *Thread::GetCurrent()
	{
		return m_pCurrent;
	}

	// Thread methods
	bool Thread::AttachThread(Thread &thread)
	{
		if (m_pBegin == nullptr)
		{
			m_pBegin = &thread;
			m_pEnd   = m_pBegin;
		}
		else
		{
			thread.pPrev  = m_pEnd;
			m_pEnd->pNext = &thread;
			m_pEnd        = &thread;
		}

		m_nNodeCounter++;

		return true;
	}

	bool Thread::DetachThread(Thread &thread)
	{
		if (thread.pNext == nullptr && thread.pPrev == nullptr)
		{
			m_pBegin     = nullptr;
			thread.pPrev = nullptr;
		}
		else if (thread.pPrev == nullptr)
		{
			thread.pNext->pPrev = nullptr;
			m_pBegin            = thread.pNext;
		}
		else if (thread.pNext == nullptr)
		{
			thread.pPrev->pNext = nullptr;
			m_pEnd              = thread.pPrev;
		}
		else
		{
			thread.pPrev->pNext = thread.pNext;
			thread.pNext->pPrev = thread.pPrev;
		}

		m_nNodeCounter--;

		return true;
	}

	inline Thread *Thread::GetCyclicalNext()
	{
		return (m_pCurrent->pNext) == nullptr ? (m_pCurrent = m_pBegin) : m_pCurrent->pNext;
	}

	void Thread::Scheduler()
	{
		size_t nThreadCount = m_nNodeCounter;
		Thread *pThread     = m_pCurrent;
		atomicx_time tm     = GetTick();

		while (nThreadCount--)
		{
			pThread = (pThread->pNext) == nullptr ? (pThread = m_pBegin) : pThread->pNext;

			TRACE(KERNEL, pThread << "." << pThread->GetName() << ": Status: " << GetStatusName(pThread->m_status) << ", Now: " << tm
			                      << ", nextEvent: " << (int32_t)(pThread->m_nextEvent - tm) << ":" << pThread->m_nextEvent);

			if (!pThread->m_flags.noTimout && m_pCurrent->m_nextEvent >= pThread->m_nextEvent)
			{
				m_pCurrent = pThread->m_nextEvent == m_pCurrent->m_nextEvent ?
				                 pThread->m_priority > m_pCurrent->m_priority ? pThread : m_pCurrent :
				                 pThread;
			}
		}

		TRACE(KERNEL, m_pCurrent << "." << m_pCurrent->GetName() << ": LEAVING Status: " << GetStatusName(m_pCurrent->m_status)
		                         << ", Now: " << tm << ", nextEvent: " << (int32_t)(m_pCurrent->m_nextEvent - tm));

		tm = GetTick();
		if (m_pCurrent->m_nextEvent > tm)
		{
			TRACE(KERNEL, "SLEEPING: " << pThread << "." << pThread->GetName() << ", Status;" << GetStatusName(m_pCurrent->m_status)
			                           << ", tm: " << tm << ", next: " << m_pCurrent->m_nextEvent
			                           << ", sleep: " << (int32_t)(m_pCurrent->m_nextEvent - tm));

			SleepTick(m_pCurrent->m_nextEvent - tm);

			if (m_pCurrent->m_status >= Status::wait)
			 	m_pCurrent->m_status = Status::timeout;
		}
        
        m_pCurrent->m_flags.noTimout = false;
		m_pCurrent->m_late = m_pCurrent->m_nextEvent - GetTick();
	}

	void Thread::SetPriority(uint8_t value)
	{
		m_priority = value;
	}

	bool Thread::Join()
	{
		m_pCurrent = m_pEnd;

		if (m_pCurrent != nullptr)
		{
			m_pStartStack = GetStackPoint(); //&nStackPoint;

			setjmp(m_joinContext);

			if (m_nNodeCounter == 0)
			{
				return false;
			}

			// m_pCurrent = GetCyclicalNext ();
			Thread::Scheduler();

			TRACE(KERNEL, "------------------------------------");
			TRACE(KERNEL, m_pCurrent->GetName()
			                  << "_" << (size_t)m_pCurrent << ": St [" << GetStatusName(m_pCurrent->m_status) << "]");
			TRACE(KERNEL, "Stack size: " << m_pCurrent->nStackSize << ", Max: " << m_pCurrent->m_nMaxStackSize
			                             << ", Occupied: " << (100 * m_pCurrent->nStackSize) / (m_pCurrent->m_nMaxStackSize) << "%");

			if (m_pCurrent->m_status == Status::starting)
			{
				m_pCurrent->m_status = Status::running;

				m_pCurrent->run();

				m_pCurrent->m_status = Status::starting;

				longjmp(m_joinContext, 1);
			}
			else
			{
				longjmp(m_pCurrent->m_context, 1);
			}
		}

		TRACE(TRACE, "NO THREAD TO RUN.... #" << m_nNodeCounter);

		return false;
	}

	bool Thread::Yield(atomicx_time tm, Status st)
	{
		m_pCurrent->m_pEndStack = GetStackPoint();
		m_pCurrent->nStackSize  = m_pStartStack - m_pCurrent->m_pEndStack + sizeof(size_t);

		TRACE(KERNEL, "Stack size: " << m_pCurrent->nStackSize << ", Max: " << m_pCurrent->m_nMaxStackSize
		                             << ", Occupied: " << (100 * m_pCurrent->nStackSize) / (m_pCurrent->m_nMaxStackSize) << "%");

		if (st == Status::now)
		{
			tm = GetTick();
		}
		else
		{
			tm = GetTick() + (tm ? tm : m_pCurrent->m_nice);
		}

		m_pCurrent->m_status    = st;
		m_pCurrent->m_nextEvent = tm;

		if (setjmp(m_pCurrent->m_context) != 0)
		{
			m_pCurrent->nStackSize = m_pStartStack - m_pCurrent->m_pEndStack;
			memcpy((void *)m_pCurrent->m_pEndStack, (const void *)&m_pCurrent->m_stack, m_pCurrent->nStackSize);

			NOTRACE(KERNEL, (size_t)m_pCurrent << ": RETURNED from Join.");

			return true;
		}

		memcpy((void *)&m_pCurrent->m_stack, (const void *)m_pCurrent->m_pEndStack, m_pCurrent->nStackSize);

		longjmp(m_joinContext, 1);

		return false;
	}

	size_t Thread::GetThreadCount()
	{
		return m_nNodeCounter;
	}

	Thread::~Thread()
	{
		DetachThread(*this);
	}

	size_t Thread::GetStackSize()
	{
		return nStackSize;
	}

	size_t Thread::GetMaxStackSize()
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

	Status Thread::GetStatus()
	{
		return m_status;
	}

	atomicx_time Thread::GetNice()
	{
		return m_nice;
	}

	atomicx_time Thread::GetNextEvent()
	{
		return m_nextEvent;
	}

	int32_t Thread::GetLate()
	{
		return m_late;
	}

} // namespace atomicx
