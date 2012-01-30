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

#ifndef _NANOS_DEPENDABLE_OBJECT
#define _NANOS_DEPENDABLE_OBJECT
#include <stdlib.h>
#include <list>
#include <set>
#include <vector>
#include "atomic.hpp"
#include "dependableobject_decl.hpp"
#include "regionstatus_fwd.hpp"
#include "dataaccess.hpp"
#include "workdescriptor_decl.hpp"

using namespace nanos;

inline const DependableObject & DependableObject::operator= ( const DependableObject &depObj )
{
   if ( this == &depObj ) return *this; 
   _id = depObj._id;
   _numPredecessors = depObj._numPredecessors;
   _references = depObj._references;
   _successors = depObj._successors;
   _writtenRegions = depObj._writtenRegions;
   _submitted = depObj._submitted;
   _wd = depObj._wd;
   return *this;
}

inline bool DependableObject::waits ( )
{
   return false;
}

inline unsigned long DependableObject::getDescription ( )
{
   return 0;
}

inline void * DependableObject::getRelatedObject ( )
{
   return NULL;
}

inline void DependableObject::setId ( unsigned int id )
{
   _id = id;
}

inline unsigned int DependableObject::getId ()
{
   return _id;
}

inline int DependableObject::increasePredecessors ( )
{
	  return _numPredecessors++;
}

inline int DependableObject::decreasePredecessors ( DependableObject *predecessor )
{
   int  numPred = --_numPredecessors; 
   if ( predecessor != NULL ) getWD()->predecessorFinished( predecessor->getWD() );
   if ( numPred == 0 ) {
      if ( predecessor == NULL )getWD()->initMyGraphRepListNoPred();
      dependenciesSatisfied( );
   }
   return numPred;
}

inline int DependableObject::numPredecessors () const
{
   return _numPredecessors.value();
}

inline DependableObject::DependableObjectVector & DependableObject::getSuccessors ( )
{
   return _successors;
}

inline bool DependableObject::addSuccessor ( DependableObject &depObj )
{
   return _successors.insert ( &depObj ).second;
}

inline DependenciesDomain * DependableObject::getDependenciesDomain ( ) const
{
   return _domain;
}

inline void DependableObject::setDependenciesDomain ( DependenciesDomain *dependenciesDomain )
{
   _domain = dependenciesDomain;
}

inline void DependableObject::addWriteRegion ( Region const &region )
{
   _writtenRegions.push_back ( region );
}

inline DependableObject::RegionContainer const & DependableObject::getWrittenRegions ( ) const
{
   return _writtenRegions;
}

inline void DependableObject::addReadRegion ( Region const &region )
{
   _readRegions.push_back( region );
}

inline DependableObject::RegionContainer const & DependableObject::getReadRegions ( ) const
{
   return _readRegions;
}

inline void DependableObject::increaseReferences()
{
   _references++;
}

inline void DependableObject::resetReferences()
{
   _references = 1;
}


inline bool DependableObject::isSubmitted()
{
   return _submitted;
}

inline void DependableObject::submitted()
{
   _submitted = true;
   memoryFence();
}

inline Lock& DependableObject::getLock()
{
   return _objectLock;
}

inline void DependableObject::setWD( WorkDescriptor *wd )
{
   _wd = wd;
}

inline WorkDescriptor * DependableObject::getWD( void )
{
   return _wd;
}

#endif
