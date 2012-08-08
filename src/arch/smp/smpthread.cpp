/*************************************************************************************/
/*      Copyright 2009 Barcelona Supercomputing Center                               */
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

#include "smpprocessor.hpp"
#include "schedule.hpp"
#include "debug.hpp"
#include "system.hpp"
#include <iostream>
#include <sched.h>
#include <unistd.h>
#include "smp_ult.hpp"
#include "instrumentation.hpp"
#ifdef GPU_DEV
#include "gpuprocessor_decl.hpp"
#endif

using namespace nanos;
using namespace nanos::ext;

void * smp_bootthread ( void *arg )
{
   SMPThread *self = static_cast<SMPThread *>( arg );

   self->run();

   pthread_exit ( 0 );
}

// TODO: detect at configure
#ifndef PTHREAD_STACK_MIN
#define PTHREAD_STACK_MIN 16384
#endif

void SMPThread::start ()
{
   pthread_attr_t attr;
   pthread_attr_init(&attr);

   // user-defined stack size
   if ( _stackSize > 0 ) {
     if ( _stackSize < PTHREAD_STACK_MIN ) {
       warning("specified thread stack too small, adjusting it to minimum size");
       _stackSize = PTHREAD_STACK_MIN;
     }

     if (pthread_attr_setstacksize( &attr, _stackSize ) )
       warning("couldn't set pthread stack size stack");
   }

   if ( pthread_create( &_pth, &attr, smp_bootthread, this ) )
      fatal( "couldn't create thread" );
}

void SMPThread::runDependent ()
{
   WD &work = getThreadWD();
   setCurrentWD( work );

   SMPDD &dd = ( SMPDD & ) work.activateDevice( SMP );

   dd.getWorkFct()( work.getData() );
}

void SMPThread::join ()
{
   pthread_join( _pth,NULL );
   joined();
}

void SMPThread::bind( void )
{
   cpu_set_t cpu_set;
   long ncpus = sysconf( _SC_NPROCESSORS_ONLN );
   int cpu_idx = ( getCpuId() * sys.getBindingStride() ) + sys.getBindingStart();
   int cpu_id = ( (cpu_idx + ( cpu_idx/ncpus) ) % ncpus);
   
   // If using the socket scheduler...
   if ( sys.getDefaultSchedule() == "socket" )
   {
      #ifdef GPU_DEV
         int gpuCount = nanos::ext::GPUConfig::getGPUCount();
         if ( gpuCount > 0 )
         {
            if ( sys.getBindingStride() == 1 && sys.getBindingStart() == 0 )
            {
               fprintf( stderr, "ID %d, cpu_id/coresPerSocket = %d\n", cpu_id,(cpu_id +1)/ sys.getCoresPerSocket()  );
               GPUThread* gpuThread = dynamic_cast<GPUThread*>( this );
               if ( gpuThread == NULL )
                  fprintf( stderr, "CPU thread %d has now affinity to %d\n", cpu_id, cpu_id + (( cpu_id +1 ) / sys.getCoresPerSocket() ) );
               else{
                  // Try to guess the gpu number
                  // TODO (gmiranda): assume the device id matches the gpu thread number
                  int gpuIndex = gpuThread->getGPUDevice();
                  fprintf( stderr, "GPU thread %d goes to %d\n", cpu_id, sys.getCoresPerSocket()*(1 + gpuIndex ) - 1);
               }
            }
            else 
            {
               warning( "Cannot split GPU threads over the available sockets "
                  << "when using non-default binding stride and/or offset." );
            }
         }
      #endif
      
      // Set the number of socket
      int socket = cpu_id / sys.getCoresPerSocket();
      
      if ( socket >= sys.getNumSockets() ) {
         warning( "cpu id " << cpu_id << " is in socket #" << socket <<
                 ", while there are only " << sys.getNumSockets() << " sockets." );
      }
      
      setSocket( socket );
   }

   ensure( ( ( cpu_id >= 0 ) && ( cpu_id < ncpus ) ), "invalid value for cpu id" );
   CPU_ZERO( &cpu_set );
   CPU_SET( cpu_id, &cpu_set );
   verbose( "Binding thread " << getId() << " to cpu " << cpu_id );
   sched_setaffinity( ( pid_t ) 0, sizeof( cpu_set ), &cpu_set );
}

void SMPThread::yield()
{
   if (sched_yield() != 0)
      warning("sched_yield call returned an error");
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
   NANOS_INSTRUMENT ( sys.getInstrumentation()->raiseOpenStateAndBurst ( NANOS_RUNNING, key, val ) );
   ( dd.getWorkFct() )( wd.getData() );
   NANOS_INSTRUMENT ( sys.getInstrumentation()->raiseCloseStateAndBurst ( key ) );
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

