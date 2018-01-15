/*************************************************************************************/
/*      Copyright 2015 Barcelona Supercomputing Center                               */
/*                                                                                   */
/*      This file is part of the NANOS++ library.                                    */
/*                                                                                   */
/*      NANOS++ is free software: you can redistribute it and/or modify              */
/*      it under the terms of the GNU Lesser General Public License as published by  */
/*      the Free Software Foundation, either version 3 of the License, or            */
/*      (at your option) any later version.                                          */
/*                                                                                   */
/*      NANOS++ is distributed in the hope that it will be useful,                   */
/*      but WITHOUT ANY WARRANTY; without even the implied warranty of               */
/*      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                */
/*      GNU Lesser General Public License for more details.                          */
/*                                                                                   */
/*      You should have received a copy of the GNU Lesser General Public License     */
/*      along with NANOS++.  If not, see <http://www.gnu.org/licenses/>.             */
/*************************************************************************************/

#include "pthread.hpp"
#include "os.hpp"
#include "basethread_decl.hpp"
#include "instrumentation.hpp"
#include <iostream>
#include <sched.h>
#include <unistd.h>
#include <signal.h>
#include <assert.h>
#include <linux/sched.h>
#include <sys/syscall.h>
#include <sched.h>
#include <string.h>
#include <errno.h>
#include <cxxabi.h>

/*
 * START linux specific
 */

#include <sys/syscall.h>

#define gettid() syscall(SYS_gettid)

struct sched_attr {
	__u32 size;

	__u32 sched_policy;
	__u64 sched_flags;

	/* SCHED_NORMAL, SCHED_BATCH */
	__s32 sched_nice;

	/* SCHED_FIFO, SCHED_RR */
	__u32 sched_priority;

	/* SCHED_DEADLINE (nsec) */
	__u64 sched_runtime;
	__u64 sched_deadline;
	__u64 sched_period;
 };

static int sched_setattr(pid_t pid, const struct sched_attr *attr, unsigned int flags) {
	return syscall(SYS_sched_setattr, pid, attr, flags);
 }

//static int sched_getattr(pid_t pid, struct sched_attr *attr, unsigned int size, unsigned int flags) {
//	return syscall(SYS_sched_getattr, pid, attr, size, flags);
// }

/*
 * END linux specific
 */

// TODO: detect at configure
#ifndef PTHREAD_STACK_MIN
#define PTHREAD_STACK_MIN 16384
#endif


using namespace nanos;

struct thread_info_t {
    BaseThread *bt;
    PThread *pt;
};

void * os_bootthread ( void *arg )
{
   struct thread_info_t *info = (struct thread_info_t *)arg ;
   BaseThread *self = info->bt;
   PThread *pt=info->pt;
   free(arg);
#ifdef NANOS_RESILIENCY_ENABLED
   self->setupSignalHandlers();
#endif
   pt->_tid=gettid(); // corsa critica???
   {
       int status;
       char *name;
       name=abi::__cxa_demangle(typeid(*self).name(),0,0,&status);
       fprintf(stderr,"PTH new OS pthread (self=0x08%lx tid=0x08%x instance=%p) running type=%s instance=%p\n",pthread_self(),pt->_tid, pt, name, self);
       free(name);
   }

   self->run();

   self->finish();
   pthread_exit ( 0 );

   // We should never get here!
   return NULL;
}

// Main thread does not get initializaed through start()
void PThread::initMain ()
{
   _pth = pthread_self();
   _tid = gettid();

   fprintf(stderr,"PTH main OS pthread self=0x%08lx tid=0x%08x instance=%p\n", _pth, _tid, this);

   if ( pthread_cond_init( &_condWait, NULL ) < 0 )
      fatal( "couldn't create pthread condition wait" );

   if ( pthread_mutex_init(&_mutexWait, NULL) < 0 )
      fatal( "couldn't create pthread mutex wait" );

}

void PThread::start ( BaseThread * th )
{
   struct thread_info_t *pinfo=(struct thread_info_t *)malloc(sizeof(struct thread_info_t));
   pthread_attr_t attr;
   pthread_attr_init( &attr );

   //! \note Checking user-defined stack size
   if ( _stackSize > 0 ) {
      if ( _stackSize < PTHREAD_STACK_MIN ) {
         warning("Specified thread stack size (" << _stackSize << " bytes) too small, adjusting to " << PTHREAD_STACK_MIN << " bytes");
         _stackSize = PTHREAD_STACK_MIN;
      }
      if (pthread_attr_setstacksize( &attr, _stackSize ) )
         warning( "Couldn't set pthread stack size stack" );
   }
 
   verbose( "Creating thread with " << _stackSize << " bytes of stack size" );

   pinfo->bt=th;
   pinfo->pt=this;
   if ( pthread_create( &_pth, &attr, os_bootthread, pinfo ) )
      fatal( "Couldn't create thread" );

   {
       pid_t mytid=_tid;
       fprintf(stderr,"PTH instance=%p => self=0x%08lx tid=0x%08x %s\n",this,_pth,mytid,mytid==0?"(DANGER tid id ZERO!)":"");
   }
      
   if ( pthread_cond_init( &_condWait, NULL ) < 0 )
      fatal( "Couldn't create pthread condition wait" );

   if ( pthread_mutex_init(&_mutexWait, NULL) < 0 )
      fatal( "Couldn't create pthread mutex wait" );

}

void PThread::finish ()
{
   NANOS_INSTRUMENT ( static InstrumentationDictionary *ID = sys.getInstrumentation()->getInstrumentationDictionary(); )
   NANOS_INSTRUMENT ( static nanos_event_key_t cpuid_key = ID->getEventKey("cpuid"); )
   NANOS_INSTRUMENT ( nanos_event_value_t cpuid_value =  (nanos_event_value_t) 0; )
   NANOS_INSTRUMENT ( sys.getInstrumentation()->raisePointEvents(1, &cpuid_key, &cpuid_value); )

   if ( pthread_mutex_destroy( &_mutexWait ) < 0 )
      fatal( "couldn't destroy pthread mutex wait" );

   if ( pthread_cond_destroy( &_condWait ) < 0 )
      fatal( "couldn't destroy pthread condition wait" );
}

void PThread::join ()
{
   if ( pthread_join( _pth, NULL ) )
      fatal( "Thread cannot be joined" );
}

void PThread::bind()
{
   int cpu_id = _core->getBindingId();
   cpu_set_t cpu_set;
   CPU_ZERO( &cpu_set );
   CPU_SET( cpu_id, &cpu_set );
   verbose( "Binding thread " << getMyThreadSafe()->getId() << " to cpu " << cpu_id );
   int res=pthread_setaffinity_np( _pth, sizeof(cpu_set_t), &cpu_set );
   if (res!=0) {
       warning0("pthread set affinity ERROR for thread " << getMyThreadSafe()->getId() << "to cpu "<<cpu_id);
   }

   NANOS_INSTRUMENT ( static InstrumentationDictionary *ID = sys.getInstrumentation()->getInstrumentationDictionary(); )
   NANOS_INSTRUMENT ( static nanos_event_key_t cpuid_key = ID->getEventKey("cpuid"); )
   NANOS_INSTRUMENT ( static nanos_event_key_t numa_key = ID->getEventKey("thread-numa-node"); )
   
   NANOS_INSTRUMENT ( nanos_event_key_t keys[2]; )
   NANOS_INSTRUMENT ( keys[0] = cpuid_key )
   NANOS_INSTRUMENT ( keys[1] = numa_key )
   
   NANOS_INSTRUMENT ( nanos_event_value_t values[2]; )
   NANOS_INSTRUMENT ( values[0] = (nanos_event_value_t) cpu_id + 1; )
   NANOS_INSTRUMENT ( values[1] = (nanos_event_value_t) _core->getNumaNode() + 1; )
   NANOS_INSTRUMENT ( sys.getInstrumentation()->raisePointEvents(2, keys, values); )
}

void PThread::yield()
{
   if ( sched_yield() != 0 )
      warning("sched_yield call returned an error");
}

void PThread::mutexLock()
{
   pthread_mutex_lock( &_mutexWait );
}

void PThread::mutexUnlock()
{
   pthread_mutex_unlock( &_mutexWait );
}

void PThread::condWait()
{
   pthread_cond_wait( &_condWait, &_mutexWait );
}

void PThread::condSignal()
{
   pthread_cond_signal( &_condWait );
}

void PThread::setSchedParam(PThreadSchedPolicy pol) {
    uint64_t p0=0,p1=0,p2=0;
    switch (pol) {
        case PThreadSchedPolicy::IDLE:
            break;
        case PThreadSchedPolicy::OTHER:
        case PThreadSchedPolicy::BATCH:
            p0=0;
            break;
        case PThreadSchedPolicy::FIFO:
        case PThreadSchedPolicy::RR:
            p0=1;;
            break;
        case PThreadSchedPolicy::DEADLINE:
            // dummy :-(
            // msec
            p0=100;
            p1=995;
            p2=1000;
            break;
    }
    setSchedParam(pol,p0,p1,p2);
}

void PThread::setSchedParam(PThreadSchedPolicy policy, uint64_t p0, uint64_t p1, uint64_t p2) {
    struct sched_attr attr;
    pid_t mytid=_tid;
    memset(&attr,0,sizeof(struct sched_attr));
    attr.size=sizeof(struct sched_attr);
    attr.sched_policy=policy;
    switch (policy) {
        case PThreadSchedPolicy::IDLE:
            break;
        case PThreadSchedPolicy::OTHER:
        case PThreadSchedPolicy::BATCH:
            attr.sched_nice=p0;
            break;
        case PThreadSchedPolicy::FIFO:
        case PThreadSchedPolicy::RR:
            attr.sched_priority=p0;
            break;
        case PThreadSchedPolicy::DEADLINE:
            attr.sched_runtime=p0*1000*1000;
            attr.sched_deadline=p1*1000*1000;
            attr.sched_period=p2*1000*1000;
            break;
    }
    fprintf(stderr,"SCHED: calling pthread_setschedparam()... (instance=%p tid=0x%08x policy=%d p0=%ld p1=%ld p2=%ld errno=%d)\n",this,mytid,policy,p0,p1,p2,errno);
    if (sched_setattr(mytid,&attr,0)!=0) {
        fprintf(stderr,"SCHED: pthread_setschedparam() error! (instance=%p tid=0x%08x)\n",this,mytid);
    }
    fprintf(stderr,"SCHED: pthread_setschedparam() OK (instance=%p tid=0x%08x)\n",this,mytid);
}

#ifdef NANOS_RESILIENCY_ENABLED

void PThread::setupSignalHandlers ()
{
   /* Set up the structure to specify task-recovery. */
   struct sigaction recovery_action;
   recovery_action.sa_sigaction = &taskExecutionHandler;
   sigemptyset(&recovery_action.sa_mask);
   recovery_action.sa_flags = SA_SIGINFO // Provides context information to the handler.
                            | SA_RESTART; // Resume system calls interrupted by the signal.

   debug0("Resiliency: handling synchronous signals raised in tasks' context.");
   /* Program synchronous signals to use the default recovery handler.
    * Synchronous signals are: SIGILL, SIGTRAP, SIGBUS, SIGFPE, SIGSEGV, SIGSTKFLT (last one is no longer used)
    */
   fatal_cond0(sigaction(SIGILL, &recovery_action, NULL) != 0, "Signal setup (SIGILL) failed");
   fatal_cond0(sigaction(SIGTRAP, &recovery_action, NULL) != 0, "Signal setup (SIGTRAP) failed");
   fatal_cond0(sigaction(SIGBUS, &recovery_action, NULL) != 0, "Signal setup (SIGBUS) failed");
   fatal_cond0(sigaction(SIGFPE, &recovery_action, NULL) != 0, "Signal setup (SIGFPE) failed");
   fatal_cond0(sigaction(SIGSEGV, &recovery_action, NULL) != 0, "Signal setup (SIGSEGV) failed");

}

void taskExecutionHandler ( int sig, siginfo_t* si, void* context ) throw(TaskExecutionException)
{
   /*
    * In order to prevent the signal to be raised inside the handler,
    * the kernel blocks it until the handler returns.
    *
    * As we are exiting the handler before return (throwing an exception),
    * we must unblock the signal or that signal will not be available to catch
    * in the future (this is done in at the catch clause).
    */
   throw TaskExecutionException(getMyThreadSafe()->getCurrentWD(), *si, *(ucontext_t*)context);
}
#endif
