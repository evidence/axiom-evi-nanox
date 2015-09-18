#ifndef CLUSTERPLUGIN_DECL_H
#define CLUSTERPLUGIN_DECL_H

#include "plugin.hpp"
#include "system_decl.hpp"
#include "clusternode_decl.hpp"
#include "gasnetapi_decl.hpp"

namespace nanos {
namespace ext {

class ClusterPlugin : public ArchPlugin
{
      GASNetAPI _gasnetApi;

      unsigned int _numPinnedSegments;
      void ** _pinnedSegmentAddrList;
      std::size_t * _pinnedSegmentLenList;
      unsigned int _extraPEsCount;
      std::string _conduit;
      std::size_t _nodeMem;
      bool _allocFit;
      bool _allowSharedThd;
      bool _unalignedNodeMem;
      int _gpuPresend;
      int _smpPresend;
      System::CachePolicyType _cachePolicy;
      std::vector<ext::ClusterNode *> *_nodes;
      ext::SMPProcessor *_cpu;
      ext::SMPMultiThread *_clusterThread;
      std::size_t _gasnetSegmentSize;

   public:
      ClusterPlugin();
      virtual void config( Config& cfg );
      virtual void init();

      void prepare( Config& cfg );
      void addSegments( unsigned int numSegments, void **segmentAddr, size_t *segmentSize );
      void * getSegmentAddr( unsigned int idx );
      std::size_t getSegmentLen( unsigned int idx );
      void addPinnedSegments( unsigned int numSegments, void **segmentAddr, size_t *segmentSize );
      void * getPinnedSegmentAddr( unsigned int idx ) const;
      std::size_t getPinnedSegmentLen( unsigned int idx ) const;
      std::size_t getNodeMem() const;
      int getGpuPresend() const;
      int getSmpPresend() const;
      System::CachePolicyType getCachePolicy ( void ) const;
      RemoteWorkDescriptor * getRemoteWorkDescriptor( int archId );
      bool getAllocFit() const;
      bool unalignedNodeMemory() const;
      std::size_t getGASNetSegmentSize() const;


      virtual void startSupportThreads();
      virtual void startWorkerThreads( std::map<unsigned int, BaseThread *> &workers);
      virtual void finalize();

      virtual ProcessingElement * createPE( unsigned id, unsigned uid );
      virtual unsigned getNumThreads() const; 
      void addPEs( std::map<unsigned int, ProcessingElement *> &pes ) const;
      virtual void addDevices( DeviceList &devices ) const {}
      virtual unsigned int getNumPEs() const;
      virtual unsigned int getMaxPEs() const;
      virtual unsigned int getNumWorkers() const;
      virtual unsigned int getMaxWorkers() const;
};

}
}

#endif /* CLUSTERPLUGIN_DECL_H */
