/*
   Copyright (c) 2009-2010, Jack Poulson
   All rights reserved.

   This file is part of Elemental.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

    - Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

    - Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

    - Neither the name of the owner nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.
*/
#include <ctime>
#include "elemental.hpp"
#include "elemental/lapack_internal.hpp"
using namespace std;
using namespace elemental;
using namespace elemental::wrappers::mpi;

void Usage()
{
    cout << "Generates random matrix then solves for its QR factorization.\n\n"
         << "  QR <r> <c> <m> <n> <nb> <correctness?> <print?>\n\n"
         << "  r: number of process rows\n"
         << "  c: number of process cols\n"
         << "  m: height of matrix\n"
         << "  n: width of matrix\n"
         << "  nb: algorithmic blocksize\n"
         << "  test correctness?: false iff 0\n"
         << "  print matrices?: false iff 0\n" << endl;
}

template<typename R>
void TestCorrectness
( bool printMatrices,
  const DistMatrix<R,MC,MR>& A,
        DistMatrix<R,MC,MR>& AOrig )
{
    const Grid& g = A.Grid();
    const int m = A.Height();
    const int n = A.Width();

    if( g.VCRank() == 0 )
    {
        cout << "  Testing orthogonality of Q...";
        cout.flush();
    }

    // Form Z := Q^H Q as an approximation to identity
    DistMatrix<R,MC,MR> Z(m,n,g);
    Z.SetToIdentity();
    lapack::UT( Left, Lower, ConjugateTranspose, 0, A, Z );
    lapack::UT( Left, Lower, Normal, 0, A, Z );

    DistMatrix<R,MC,MR> ZUpper(g);
    ZUpper.View( Z, 0, 0, n, n );

    // Form Identity
    DistMatrix<R,MC,MR> X(n,n,g);
    X.SetToIdentity();

    // Form X := I - Q^H Q
    blas::Axpy( (R)-1, ZUpper, X );

    // Compute the maximum deviance
    R myMaxDevFromIdentity = 0.;
    for( int j=0; j<X.LocalWidth(); ++j )
        for( int i=0; i<X.LocalHeight(); ++i )
            myMaxDevFromIdentity = 
                max(myMaxDevFromIdentity,abs(X.GetLocalEntry(i,j)));
    R maxDevFromIdentity;
    Reduce
    ( &myMaxDevFromIdentity, &maxDevFromIdentity, 1, MPI_MAX, 0, g.VCComm() );
    if( g.VCRank() == 0 )
        cout << "||Q^H Q - I||_oo = " << maxDevFromIdentity << endl;

    if( g.VCRank() == 0 )
    {
        cout << "  Testing if A = QR...";
        cout.flush();
    }

    // Form Q R
    DistMatrix<R,MC,MR> U( A );
    U.MakeTrapezoidal( Left, Upper );
    lapack::UT( Left, Lower, ConjugateTranspose, 0, A, U );

    // Form Q R - A
    blas::Axpy( (R)-1, AOrig, U );
    
    // Compute the maximum deviance
    R myMaxDevFromA = 0.;
    for( int j=0; j<U.LocalWidth(); ++j )
        for( int i=0; i<U.LocalHeight(); ++i )
            myMaxDevFromA = 
                max(myMaxDevFromA,abs(U.GetLocalEntry(i,j)));
    R maxDevFromA;
    Reduce
    ( &myMaxDevFromA, &maxDevFromA, 1, MPI_MAX, 0, g.VCComm() );
    if( g.VCRank() == 0 )
        cout << "||AOrig - QR||_oo = " << maxDevFromA << endl;
}

#ifndef WITHOUT_COMPLEX
template<typename R>
void TestCorrectness
( bool printMatrices,
  const DistMatrix<complex<R>,MC,MR  >& A,
  const DistMatrix<complex<R>,MD,Star>& t,
        DistMatrix<complex<R>,MC,MR  >& AOrig )
{
    typedef complex<R> C;

    const Grid& g = A.Grid();
    const int m = A.Height();
    const int n = A.Width();

    if( g.VCRank() == 0 )
    {
        cout << "  Testing orthogonality of Q...";
        cout.flush();
    }

    // Form Z := Q^H Q as an approximation to identity
    DistMatrix<C,MC,MR> Z(m,n,g);
    Z.SetToIdentity();
    lapack::UT( Left, Lower, ConjugateTranspose, 0, A, t, Z );
    lapack::UT( Left, Lower, Normal, 0, A, t, Z );
    
    DistMatrix<C,MC,MR> ZUpper(g);
    ZUpper.View( Z, 0, 0, n, n );

    // Form Identity
    DistMatrix<C,MC,MR> X(n,n,g);
    X.SetToIdentity();

    // Form X := I - Q^H Q
    blas::Axpy( (C)-1, ZUpper, X );

    // Compute the maximum deviance
    R myMaxDevFromIdentity = 0.;
    for( int j=0; j<X.LocalWidth(); ++j )
        for( int i=0; i<X.LocalHeight(); ++i )
            myMaxDevFromIdentity = 
                max(myMaxDevFromIdentity,abs(X.GetLocalEntry(i,j)));
    R maxDevFromIdentity;
    Reduce
    ( &myMaxDevFromIdentity, &maxDevFromIdentity, 1, MPI_MAX, 0, g.VCComm() );
    if( g.VCRank() == 0 )
        cout << "||Q^H Q - I||_oo = " << maxDevFromIdentity << endl;

    if( g.VCRank() == 0 )
    {
        cout << "  Testing if A = QR...";
        cout.flush();
    }

    // Form Q R
    DistMatrix<C,MC,MR> U( A );
    U.MakeTrapezoidal( Left, Upper );
    lapack::UT( Left, Lower, ConjugateTranspose, 0, A, t, U );

    // Form Q R - A
    blas::Axpy( (C)-1, AOrig, U );
    
    // Compute the maximum deviance
    R myMaxDevFromA = 0.;
    for( int j=0; j<U.LocalWidth(); ++j )
        for( int i=0; i<U.LocalHeight(); ++i )
            myMaxDevFromA = 
                max(myMaxDevFromA,abs(U.GetLocalEntry(i,j)));
    R maxDevFromA;
    Reduce
    ( &myMaxDevFromA, &maxDevFromA, 1, MPI_MAX, 0, g.VCComm() );
    if( g.VCRank() == 0 )
        cout << "||AOrig - QR||_oo = " << maxDevFromA << endl;
}
#endif // WITHOUT_COMPLEX

template<typename T>
void TestQR
( bool testCorrectness, bool printMatrices,
  int m, int n, const Grid& g );

template<>
void TestQR<double>
( bool testCorrectness, bool printMatrices,
  int m, int n, const Grid& g )
{
    typedef double R;

    double startTime, endTime, runTime, gFlops;
    DistMatrix<R,MC,MR> A(g);
    DistMatrix<R,MC,MR> AOrig(g);

    A.ResizeTo( m, n );

    A.SetToRandom();
    if( testCorrectness )
    {
        if( g.VCRank() == 0 )
        {
            cout << "  Making copy of original matrix...";
            cout.flush();
        }
        AOrig = A;
        if( g.VCRank() == 0 )
            cout << "DONE" << endl;
    }
    if( printMatrices )
        A.Print("A");

    if( g.VCRank() == 0 )
    {
        cout << "  Starting QR factorization...";
        cout.flush();
    }
    Barrier( MPI_COMM_WORLD );
    startTime = Time();
    lapack::QR( A );
    Barrier( MPI_COMM_WORLD );
    endTime = Time();
    runTime = endTime - startTime;
    gFlops = lapack::internal::QRGFlops<R>( m, n, runTime );
    if( g.VCRank() == 0 )
    {
        cout << "DONE. " << endl
             << "  Time = " << runTime << " seconds. GFlops = " 
             << gFlops << endl;
    }
    if( printMatrices )
        A.Print("A after factorization");
    if( testCorrectness )
        TestCorrectness( printMatrices, A, AOrig );
}

#ifndef WITHOUT_COMPLEX
template<>
void TestQR< complex<double> >
( bool testCorrectness, bool printMatrices,
  int m, int n, const Grid& g )
{
    typedef complex<double> C;

    double startTime, endTime, runTime, gFlops;
    DistMatrix<C,MC,MR  > A(g);
    DistMatrix<C,MD,Star> t(g);
    DistMatrix<C,MC,MR  > AOrig(g);

    A.ResizeTo( m, n );

    A.SetToRandom();
    if( testCorrectness )
    {
        if( g.VCRank() == 0 )
        {
            cout << "  Making copy of original matrix...";
            cout.flush();
        }
        AOrig = A;
        if( g.VCRank() == 0 )
            cout << "DONE" << endl;
    }
    if( printMatrices )
        A.Print("A");

    if( g.VCRank() == 0 )
    {
        cout << "  Starting QR factorization...";
        cout.flush();
    }
    Barrier( MPI_COMM_WORLD );
    startTime = Time();
    lapack::QR( A, t );
    Barrier( MPI_COMM_WORLD );
    endTime = Time();
    runTime = endTime - startTime;
    gFlops = lapack::internal::QRGFlops<C>( m, n, runTime );
    if( g.VCRank() == 0 )
    {
        cout << "DONE. " << endl
             << "  Time = " << runTime << " seconds. GFlops = " 
             << gFlops << endl;
    }
    if( printMatrices )
        A.Print("A after factorization");
    if( testCorrectness )
        TestCorrectness( printMatrices, A, t, AOrig );
}
#endif

int main( int argc, char* argv[] )
{
    int rank;
    Init( &argc, &argv );
    MPI_Comm_rank( MPI_COMM_WORLD, &rank );
    if( argc != 8 )
    {
        if( rank == 0 )
            Usage();
        Finalize();
        return 0;
    }
    try
    {
        const int r = atoi(argv[1]);
        const int c = atoi(argv[2]);
        const int m = atoi(argv[3]);
        const int n = atoi(argv[4]);
        const int nb = atoi(argv[5]);
        const bool testCorrectness = atoi(argv[6]);
        const bool printMatrices = atoi(argv[7]);
#ifndef RELEASE
        if( rank == 0 )
        {
            cout << "==========================================\n"
                 << " In debug mode! Performance will be poor! \n"
                 << "==========================================" << endl;
        }
#endif
        if( n > m )
            throw logic_error( "QR only supported when height >= width." );
        const Grid g( MPI_COMM_WORLD, r, c );
        SetBlocksize( nb );

        if( rank == 0 )
            cout << "Will test QR" << endl;

        if( rank == 0 )
        {
            cout << "---------------------\n"
                 << "Testing with doubles:\n"
                 << "---------------------" << endl;
        }
        TestQR<double>
        ( testCorrectness, printMatrices, m, n, g );

#ifndef WITHOUT_COMPLEX
        if( rank == 0 )
        {
            cout << "--------------------------------------\n"
                 << "Testing with double-precision complex:\n"
                 << "--------------------------------------" << endl;
        }
        TestQR<dcomplex>
        ( testCorrectness, printMatrices, m, n, g );
#endif
    }
    catch( exception& e )
    {
#ifndef RELEASE
        DumpCallStack();
#endif
        cerr << "Process " << rank << " caught error message:\n"
             << e.what() << endl;
    }   
    Finalize();
    return 0;
}

