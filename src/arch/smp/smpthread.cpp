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

#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <assert.h>

#include "debug.hpp"
#include "instrumentationmodule_decl.hpp"
#include "instrumentation.hpp"

#include "os.hpp"
#include "pthread.hpp"

#include "basethread.hpp"
#include "schedule.hpp"

#include "smp_ult.hpp"
#include "smpprocessor.hpp"

#include "system.hpp"

#include <cxxabi.h>
//#include "clusterdevice_decl.hpp"

using namespace nanos;
using namespace nanos::ext;

SMPThread & SMPThread::stackSize( size_t size )
{
   _pthread.setStackSize( size );
   return *this;
}

void SMPThread::runDependent ()
{
   WD &work = getThreadWD();
   setCurrentWD( work );

   SMPDD &dd = ( SMPDD & ) work.activateDevice( getSMPDevice() );

   dd.execute( work );
}

void SMPThread::idle( bool debug )
{
   if ( sys.getNetwork()->getNumNodes() > 1 ) {
      if ( this->_gasnetAllowAM ) {
      sys.getNetwork()->poll(0);

      if ( !_pendingRequests.empty() ) {
         std::set<void *>::iterator it = _pendingRequests.begin();
         while ( it != _pendingRequests.end() ) {
            GetRequest *req = (GetRequest *) (*it);
            if ( req->isCompleted() ) {
               std::set<void *>::iterator toBeDeletedIt = it;
               it++;
               _pendingRequests.erase(toBeDeletedIt);
               req->clear();
               delete req;
            } else {
               it++;
            }
         }
      }
      }
   }
   getSMPDevice().tryExecuteTransfer();
}

void SMPThread::wait()
{
#ifdef NANOS_INSTRUMENTATION_ENABLED
   static InstrumentationDictionary *ID = sys.getInstrumentation()->getInstrumentationDictionary();
   static nanos_event_key_t cpuid_key = ID->getEventKey("cpuid");
   nanos_event_value_t cpuid_value = (nanos_event_value_t) 0;
   sys.getInstrumentation()->raisePointEvents(1, &cpuid_key, &cpuid_value);
#endif

   _pthread.mutexLock();

   if ( isSleeping() && !hasNextWD() && canBlock() ) {

      /* Only leave team if it's been told to */
      ThreadTeam *team = getTeam() ? getTeam() : getNextTeam();
      if ( team && isLeavingTeam() ) {
         leaveTeam();
      }

      /* Set flag */
      BaseThread::wait();

      NANOS_INSTRUMENT( InstrumentState state_stop(NANOS_STOPPED) );

      /* It is recommended to wait under a while loop to handle spurious wakeups
       * http://pubs.opengroup.org/onlinepubs/009695399/functions/pthread_cond_wait.html
       */
      while ( isSleeping() ) {
         _pthread.condWait();
      }

      NANOS_INSTRUMENT( InstrumentState state_wake(NANOS_WAKINGUP) );

      /* Unset flag */
      BaseThread::resume();

#ifdef NANOS_INSTRUMENTATION_ENABLED
      if ( sys.getSMPPlugin()->getBinding() ) {
         cpuid_value = (nanos_event_value_t) getCpuId() + 1;
      } else if ( sys.isCpuidEventEnabled() ) {
         cpuid_value = (nanos_event_value_t) sched_getcpu() + 1;
      }
      sys.getInstrumentation()->raisePointEvents(1, &cpuid_key, &cpuid_value);
#endif

      if ( getTeam() == NULL ) {
         team = getNextTeam();
         if ( team ) {
            ensure( sys.getPMInterface().isMalleable(),
                  "Only malleable prog. models should dynamically acquire a team" );
            reserve();
            sys.acquireWorker( team, this, true, false, false );
         }
      }
   }
   _pthread.mutexUnlock();

   /* Whether the thread should wait for the cpu to be free before doing some work */
   sys.getThreadManager()->waitForCpuAvailability();
   sys.getThreadManager()->returnMyCpuIfClaimed();

}

void SMPThread::wakeup()
{
   BaseThread::wakeup();
   _pthread.condSignal();
}

// This is executed in between switching stacks
void SMPThread::switchHelperDependent ( WD *oldWD, WD *newWD, void *oldState  )
{
   SMPDD & dd = ( SMPDD & )oldWD->getActiveDevice();
   dd.setState( (intptr_t *) oldState );
}

bool SMPThread::inlineWorkDependent ( WD &wd )
{
   // Now the WD will be inminently run
   wd.start(WD::IsNotAUserLevelThread);

   SMPDD &dd = ( SMPDD & )wd.getActiveDevice();

   NANOS_INSTRUMENT ( static nanos_event_key_t key = sys.getInstrumentation()->getInstrumentationDictionary()->getEventKey("user-code") );
   NANOS_INSTRUMENT ( nanos_event_value_t val = wd.getId() );
   NANOS_INSTRUMENT ( if ( wd.isRuntimeTask() ) { );
   NANOS_INSTRUMENT (    sys.getInstrumentation()->raiseOpenStateEvent ( NANOS_RUNTIME ) );
   NANOS_INSTRUMENT ( } else { );
   NANOS_INSTRUMENT (    sys.getInstrumentation()->raiseOpenStateAndBurst ( NANOS_RUNNING, key, val ) );
   NANOS_INSTRUMENT ( } );

   //if ( sys.getNetwork()->getNodeNum() > 0 ) std::cerr << "Starting wd " << wd.getId() << std::endl;

   dd.execute( wd );

   NANOS_INSTRUMENT ( if ( wd.isRuntimeTask() ) { );
   NANOS_INSTRUMENT (    sys.getInstrumentation()->raiseCloseStateEvent() );
   NANOS_INSTRUMENT ( } else { );
   NANOS_INSTRUMENT (    sys.getInstrumentation()->raiseCloseStateAndBurst ( key, val ) );
   NANOS_INSTRUMENT ( } );
   return true;
}

void SMPThread::switchTo ( WD *wd, SchedulerHelper *helper )
{
   // wd MUST have an active SMP Device when it gets here
   ensure( wd->hasActiveDevice(),"WD has no active SMP device" );
   SMPDD &dd = ( SMPDD & )wd->getActiveDevice();
   ensure( dd.hasStack(), "DD has no stack for ULT");

   ::switchStacks(
       ( void * ) getCurrentWD(),
       ( void * ) wd,
       ( void * ) dd.getState(),
       ( void * ) helper );
}

void SMPThread::exitTo ( WD *wd, SchedulerHelper *helper)
{
   // wd MUST have an active SMP Device when it gets here
   ensure( wd->hasActiveDevice(),"WD has no active SMP device" );
   SMPDD &dd = ( SMPDD & )wd->getActiveDevice();
   ensure( dd.hasStack(), "DD has no stack for ULT");

   //TODO: optimize... we don't really need to save a context in this case
   ::switchStacks(
      ( void * ) getCurrentWD(),
      ( void * ) wd,
      ( void * ) dd.getState(),
      ( void * ) helper );
}

int SMPThread::getCpuId() const {
   return _pthread.getCpuId();
}

SMPMultiThread::SMPMultiThread( WD &w, SMPProcessor *pe,
      unsigned int representingPEsCount, PE **representingPEs ) :
   SMPThread ( w, pe, pe ),
   _current( 0 ) {
    fprintf(stderr, "TH new SMPMultiThread instance=%p\n",this);
   setCurrentWD( w );
   if ( representingPEsCount > 0 ) {
      addThreadsFromPEs( representingPEsCount, representingPEs );
   }
}

void SMPMultiThread::addThreadsFromPEs(unsigned int representingPEsCount, PE **representingPEs)
{
   _threads.reserve( _threads.size() + representingPEsCount );
   for ( unsigned int i = 0; i < representingPEsCount; i++ )
   {
      _threads.push_back( &( representingPEs[ i ]->startWorker( this ) ) );
   }
}







static void decodeAndSetSched(const char *envVar, PThread& pthread) {
    char *value,*start,*end;
    int sched_policy;
    uint64_t p0=0,p1=0,p2=0;

    value=getenv(envVar);
    if (value==NULL) {
        fprintf(stderr,"WARNING '%s' environment variable not found! Not setting scheduler parameters!\n",envVar);
        return;
    }

    // DECODE

    start=value;
    end=NULL;
    sched_policy=(int)strtol(start,&end,10);
    if (start==end) {
        if (strncmp(start,"OTHER",5)==0) {
            sched_policy=SCHED_OTHER;
            end+=5;
        } else
        if (strncmp(start,"DEADLINE",8)==0) {
            sched_policy=SCHED_DEADLINE;
            end+=8;
        } else
        if (strncmp(start,"FIFO",4)==0) {
            sched_policy=SCHED_FIFO;
            end+=4;
        } else
        if (strncmp(start,"BATCH",5)==0) {
            sched_policy=SCHED_BATCH;
            end+=5;
        } else
        if (strncmp(start,"IDLE",4)==0) {
            sched_policy=SCHED_IDLE;
            end+=4;
        } else
        if (strncmp(start,"RR",2)==0) {
            sched_policy=SCHED_RR;
            end+=2;
        } else {
            fprintf(stderr,"%s='%s': unrecognized policy name or value\n",envVar,value);
            return;
        }
    }
    if (*end!='\0') {
        if (*end!=',') {
            fprintf(stderr, "%s='%s': comma expected (1)\n",envVar,value);
            return;
        }
        start=end+1;
        p0=strtol(start,&end,10);
        if (start==end) {
            fprintf(stderr, "%s='%s': unrecognized param value\n",envVar,value);
            return;
        }
    }
    if (*end!='\0') {
        if (*end!=',') {
            fprintf(stderr, "%s='%s': comma expected (2)",envVar,value);
            return;
        }
        start=end+1;
        p1=strtol(start,&end,10);
        if (start==end) {
            fprintf(stderr, "%s='%s': unrecognized param value",envVar,value);
            return;
        }
    }
    if (*end!='\0') {
        if (*end!=',') {
            fprintf(stderr, "%s='%s': comma expected (3)",envVar,value);
            return;
        }
        start=end+1;
        p2=strtol(start,&end,10);
        if (start==end) {
            fprintf(stderr, "%s='%s': unrecognized param value",envVar,value);
            return;
        }
    }
    if (*end!='\0') {
        fprintf(stderr, "%s='%s': unexpected chars after parameter",envVar,value);
        return ;
    }

    // SET
    
    pthread.setSchedParam((PThreadSchedPolicy)sched_policy,p0,p1,p2);
}

void SMPThread::initializeDependent( void ) {
    int status;
    char *name;
    name=abi::__cxa_demangle(typeid(this).name(),0,0,&status);
    fprintf(stderr,"TH SMPThread::initializeDependent() type=%s instance=%p self=0x08%lx \n",name,this,pthread_self());
    free(name);
    //if (__sync_fetch_and_add(&MYcounter,1)==0) _pthread.setSchedParam(PThreadSchedPolicy::FIFO);
    //else _pthread.setSchedParam(PThreadSchedPolicy::NORMAL);
    decodeAndSetSched("AXIOM_WRK_PARAMS",_pthread);
}

void SMPMultiThread::initializeDependent( void ) {
    int status;
    char *name;
    name=abi::__cxa_demangle(typeid(this).name(),0,0,&status);
    fprintf(stderr,"TH SMPMultiThread::initializeDependent() type=%s instance=%p self=0x08%lx \n",name,this,pthread_self());
    free(name);
    //_pthread.setSchedParam(PThreadSchedPolicy::NORMAL);
    decodeAndSetSched("AXIOM_COM_PARAMS",_pthread);
}
