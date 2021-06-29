/*
//@HEADER
// ************************************************************************
//
//                        Kokkos v. 3.0
//       Copyright (2020) National Technology & Engineering
//               Solutions of Sandia, LLC (NTESS).
//
// Under the terms of Contract DE-NA0003525 with NTESS,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY NTESS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NTESS OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Jennifer Loe (jloe@sandia.gov)
//
// ************************************************************************
//@HEADER
*/

#include<math.h>
#include"KokkosKernels_IOUtils.hpp"
#include<Kokkos_Core.hpp>
#include<Kokkos_Random.hpp>
#include<KokkosBlas.hpp>
#include<KokkosBlas3_trsm.hpp>
#include<KokkosSparse_spmv.hpp>

#include"gmres.hpp"

int main(int /*argc*/, char ** /*argv[]*/) {

  typedef Kokkos::complex<double>           ST;
  typedef int                               OT;
  typedef Kokkos::DefaultExecutionSpace     EXSP;

  //TODO: Should these really be layout left?
  using ViewVectorType = Kokkos::View<ST*,Kokkos::LayoutLeft, EXSP>;
  using ViewHostVectorType = Kokkos::View<ST*,Kokkos::LayoutLeft, Kokkos::HostSpace>;
  using ViewMatrixType = Kokkos::View<ST**,Kokkos::LayoutLeft, EXSP>; 

  std::string filename("young1c.mtx"); // example matrix
  std::string ortho("CGS2"); //orthog type
  int m = 50; //Max subspace size before restarting.
  double convTol = 1e-08; //Relative residual convergence tolerance.
  int cycLim = 60;

  std::cout << "File to process is: " << filename << std::endl;
  std::cout << "Convergence tolerance is: " << convTol << std::endl;

  //Initialize Kokkos AFTER parsing parameters:
  Kokkos::initialize();
  {

  // Read in a matrix Market file and use it to test the Kokkos Operator.
  KokkosSparse::CrsMatrix<ST, OT, EXSP> A = 
    KokkosKernels::Impl::read_kokkos_crst_matrix<KokkosSparse::CrsMatrix<ST, OT, EXSP>>(filename.c_str()); 

  int n = A.numRows();
  ViewVectorType X("X",n); //Solution and initial guess
  ViewVectorType Wj("Wj",n); //For checking residuals at end.
  ViewVectorType B(Kokkos::ViewAllocateWithoutInitializing("B"),n);//right-hand side vec

  // Make rhs random.
  /*int rand_seed = std::rand();
  Kokkos::Random_XorShift64_Pool<> pool(rand_seed); //initially used seed 12371
  Kokkos::fill_random(B, pool, -1,1);*/

  // Make rhs ones so that results are repeatable:
  Kokkos::deep_copy(B,1.0);

  std::cout << "Testing GMRES with CGS2 ortho:" << std::endl;
  GmresStats solveStats = gmres<ST, Kokkos::LayoutLeft, EXSP>(A, B, X, convTol, m, cycLim, ortho);

  // Double check residuals at end of solve:
  double nrmB = KokkosBlas::nrm2(B);
  KokkosSparse::spmv("N", 1.0, A, X, 0.0, Wj); // wj = Ax
  KokkosBlas::axpy(-1.0, Wj, B); // b = b-Ax. 
  double endRes = KokkosBlas::nrm2(B)/nrmB;
  std::cout << "Verify from main: Ending residual is " << endRes << std::endl;
  std::cout << "Number of iterations is: " << solveStats.numIters << std::endl;
  std::cout << "Diff of residual from main - residual from solver: " << solveStats.minRelRes - endRes << std::endl;
  std::cout << "Convergence flag is : " << solveStats.convFlag() << std::endl;
  
  if( solveStats.numIters < 2690 && solveStats.numIters > 2650 && endRes < convTol){
    std::cout << "Test CGS2 Passed!" << std::endl;
    }

  ortho = "MGS";
  Kokkos::deep_copy(X,0.0);

  std::cout << "Testing GMRES with MGS ortho:" << std::endl;
  solveStats = gmres<ST, Kokkos::LayoutLeft, EXSP>(A, B, X, convTol, m, cycLim, ortho);

  // Double check residuals at end of solve:
  nrmB = KokkosBlas::nrm2(B);
  KokkosSparse::spmv("N", 1.0, A, X, 0.0, Wj); // wj = Ax
  KokkosBlas::axpy(-1.0, Wj, B); // b = b-Ax. 
  endRes = KokkosBlas::nrm2(B)/nrmB;
  std::cout << "Verify from main: Ending residual is " << endRes << std::endl;
  std::cout << "Number of iterations is: " << solveStats.numIters << std::endl;
  std::cout << "Diff of residual from main - residual from solver: " << solveStats.minRelRes - endRes << std::endl;
  std::cout << "Convergence flag is : " << solveStats.convFlag() << std::endl;
  
  if( solveStats.numIters < 2690 && solveStats.numIters > 2650 && endRes < convTol){
    std::cout << "Test MGS Passed!" << std::endl;
    }

  }
  Kokkos::finalize();

}

