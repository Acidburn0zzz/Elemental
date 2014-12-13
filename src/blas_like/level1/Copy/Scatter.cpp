/*
   Copyright (c) 2009-2014, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
#include "El.hpp"

namespace El {
namespace copy {

template<typename T>
void Scatter
( const DistMatrix<T,CIRC,CIRC>& A,
        AbstractDistMatrix<T>& B )
{
    DEBUG_ONLY(CallStackEntry cse("copy::Scatter"))
    AssertSameGrids( A, B );

    const Int m = A.Height();
    const Int n = A.Width();
    const Int colStride = B.ColStride();
    const Int rowStride = B.RowStride();
    B.Resize( m, n );
    if( A.CrossSize() != 1 )
        LogicError("Non-trivial cross teams not yet supported");
    // TODO: Broadcast over the redundant communicator and use mpi::Translate
    //       rank to determine whether a process is the root of the broadcast
    if( A.RedundantSize() != 1 )
        LogicError("Non-trivial redundant teams not yet supported");

    const Int pkgSize = mpi::Pad(MaxLength(m,colStride)*MaxLength(n,rowStride));
    const Int recvSize = pkgSize;
    const Int sendSize = B.DistSize()*pkgSize;

    // Translate the root of A into the DistComm of B (if possible)
    const Int root = A.Root();
    const Int target = mpi::Translate( A.CrossComm(), root, B.DistComm() ); 
    if( target == mpi::UNDEFINED )
        return;

    std::vector<T> buffer;
    T* recvBuf=0; // some compilers (falsely) warn otherwise
    if( A.CrossRank() == root )
    {
        buffer.resize( sendSize+recvSize );
        T* sendBuf = &buffer[0];
        recvBuf    = &buffer[sendSize];

        // Pack the send buffer
        copy::util::StridedPack
        ( m, n,
          B.ColAlign(), colStride,
          B.RowAlign(), rowStride,
          A.LockedBuffer(), A.LDim(),
          sendBuf,          pkgSize );

        // Scatter from the root
        mpi::Scatter
        ( sendBuf, pkgSize, recvBuf, pkgSize, target, B.DistComm() );
    }
    else
    {
        buffer.resize( recvSize );
        recvBuf = &buffer[0];

        // Perform the receiving portion of the scatter from the non-root
        mpi::Scatter
        ( static_cast<T*>(0), pkgSize,
          recvBuf,            pkgSize, target, B.DistComm() );
    }

    // Unpack
    copy::util::InterleaveMatrix
    ( B.LocalHeight(), B.LocalWidth(),
      recvBuf,    1, B.LocalHeight(),
      B.Buffer(), 1, B.LDim() );
}

#define PROTO(T) \
  template void Scatter \
  ( const DistMatrix<T,CIRC,CIRC>& A, \
          AbstractDistMatrix<T>& B );

#include "El/macros/Instantiate.h"

} // namespace copy
} // namespace El