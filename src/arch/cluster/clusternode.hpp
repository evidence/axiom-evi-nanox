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

#ifndef _NANOS_CLUSTER_NODE
#define _NANOS_CLUSTER_NODE

#include "accelerator.hpp"
#include "cache.hpp"
#include "config.hpp"
#include "clusterdevice.hpp"
#include "clusterthread.hpp"
#include "simpleallocator.hpp"

namespace nanos {
namespace ext
{

   class ClusterNode : public Accelerator
   {


      private:
         // config variables
         static Atomic<int>      _deviceSeed; // Number of cluster devices assigned to threads
         unsigned int            _clusterNode; // Assigned cluster device Id

         // disable copy constructor and assignment operator
         ClusterNode( const ClusterNode &pe );
         const ClusterNode & operator= ( const ClusterNode &pe );

         DeviceCache<ClusterDevice> _cache;
         SimpleAllocator _memSegment;

      public:
         // constructors
         ClusterNode( int id ) : Accelerator( id, &SMP ), _memSegment( ( uintptr_t ) ClusterDevice::getSegmentAddr( id ), ClusterDevice::getSegmentLen( id ) ) { _clusterNode = id; }

         virtual ~ClusterNode() {}

         virtual WD & getWorkerWD () const;
         virtual WD & getMasterWD () const;
         virtual BaseThread & createThread ( WorkDescriptor &wd );

         // capability query functions
         virtual bool supportsUserLevelThreads () const { return true; }

         /* Memory space support */
         //virtual void registerDataAccessDependent( uint64_t tag, size_t size );
         //virtual void copyDataDependent( uint64_t tag, size_t size );
         //virtual void unregisterDataAccessDependent( uint64_t tag );
         //virtual void copyBackDependent( uint64_t tag, size_t size );
         virtual void* getAddressDependent( uint64_t tag );
         virtual void copyToDependent( void *dst, uint64_t tag, size_t size );

         void registerCacheAccessDependent(uint64_t a, size_t aa, bool aaa, bool aaaa);
         void unregisterCacheAccessDependent(uint64_t a, size_t aa);
         void registerPrivateAccessDependent(uint64_t a, size_t aa, bool aaa, bool aaaa);
         void unregisterPrivateAccessDependent(uint64_t a, size_t aa);

         unsigned int getClusterNodeNum();
         //static void slaveLoop ( ClusterNode *node );
         SimpleAllocator & getAllocator( void ) { return _memSegment; }
   };

}
}

#endif
