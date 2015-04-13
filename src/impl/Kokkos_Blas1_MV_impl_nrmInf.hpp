/*
//@HEADER
// ************************************************************************
//
//          Kokkos: Node API and Parallel Node Kernels
//              Copyright (2008) Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
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
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact Michael A. Heroux (maherou@sandia.gov)
//
// ************************************************************************
//@HEADER
*/
#ifndef KOKKOS_BLAS1_MV_IMPL_NRMINF_HPP_
#define KOKKOS_BLAS1_MV_IMPL_NRMINF_HPP_

#include <TpetraKernels_config.h>
#include <Kokkos_Core.hpp>
#include <Kokkos_InnerProductSpaceTraits.hpp>

namespace KokkosBlas {
namespace Impl {

//
// nrmInf
//

/// \brief Inf-norm functor for single vectors.
///
/// \tparam RV 0-D output View
/// \tparam XV 1-D input View
/// \tparam SizeType Index type.  Use int (32 bits) if possible.
template<class RV, class XV, class SizeType = typename XV::size_type>
struct V_NrmInf_Functor
{
  typedef typename XV::execution_space              execution_space;
  typedef SizeType                                        size_type;
  typedef typename XV::non_const_value_type             xvalue_type;
  typedef Kokkos::Details::InnerProductSpaceTraits<xvalue_type> IPT;
  typedef Kokkos::Details::ArithTraits<typename IPT::mag_type>   AT;
  typedef typename IPT::mag_type                         value_type;

  RV m_r;
  XV m_x;

  V_NrmInf_Functor (const RV& r, const XV& x) :
    m_r (r), m_x (x)
  {}

  KOKKOS_INLINE_FUNCTION
  void operator() (const size_type& i, value_type& update) const
  {
    const typename IPT::mag_type tmp = IPT::norm (m_x(i));
    if (update < tmp) {
      update = tmp;
    }
  }

  KOKKOS_INLINE_FUNCTION void init (value_type& update) const
  {
    update = AT::zero ();
  }

  KOKKOS_INLINE_FUNCTION void
  join (volatile value_type& update,
        const volatile value_type& source) const
  {
    if (update < source) {
      update = source;
    }
  }

  // On device, write the reduction result to the output View.
  KOKKOS_INLINE_FUNCTION void final (const value_type& dst) const
  {
    m_r() = dst;
  }
};

/// \brief Inf-norm functor for multivectors.
///
/// \tparam RV 1-D output View
/// \tparam XMV 2-D input View
/// \tparam SizeType Index type.  Use int (32 bits) if possible.
template<class RV, class XMV, class SizeType = typename XMV::size_type>
struct MV_NrmInf_Functor {
  typedef typename XMV::execution_space             execution_space;
  typedef SizeType                                        size_type;
  typedef typename XMV::non_const_value_type            xvalue_type;
  typedef Kokkos::Details::InnerProductSpaceTraits<xvalue_type> IPT;
  typedef Kokkos::Details::ArithTraits<typename IPT::mag_type>   AT;
  typedef typename IPT::mag_type                       value_type[];

  const size_type value_count;
  RV norms_;
  XMV X_;

  MV_NrmInf_Functor (const RV& norms, const XMV& X) :
    value_count (X.dimension_1 ()), norms_ (norms), X_ (X)
  {}

  KOKKOS_INLINE_FUNCTION void
  operator() (const size_type& i, value_type update) const
  {
#ifdef KOKKOS_HAVE_PRAGMA_IVDEP
#pragma ivdep
#endif
#ifdef KOKKOS_HAVE_PRAGMA_VECTOR
#pragma vector always
#endif
    for (size_type j = 0; j < value_count; ++j) {
      const typename IPT::mag_type tmp = IPT::norm (X_(i,j));
      if (update[j] < tmp) {
        update[j] = tmp;
      }
    }
  }

  KOKKOS_INLINE_FUNCTION void
  init (value_type update) const
  {
#ifdef KOKKOS_HAVE_PRAGMA_IVDEP
#pragma ivdep
#endif
#ifdef KOKKOS_HAVE_PRAGMA_VECTOR
#pragma vector always
#endif
    for (size_type k = 0; k < value_count; ++k) {
      update[k] = AT::zero ();
    }
  }

  KOKKOS_INLINE_FUNCTION void
  join (volatile value_type update,
        const volatile value_type source) const
  {
#ifdef KOKKOS_HAVE_PRAGMA_IVDEP
#pragma ivdep
#endif
#ifdef KOKKOS_HAVE_PRAGMA_VECTOR
#pragma vector always
#endif
    for (size_type k = 0; k < value_count; ++k) {
      if (update[k] < source[k]) {
        update[k] = source[k];
      }
    }
  }

  // On device, write the reduction result to the output View.
  KOKKOS_INLINE_FUNCTION void
  final (const value_type dst) const
  {
#ifdef KOKKOS_HAVE_PRAGMA_IVDEP
#pragma ivdep
#endif
#ifdef KOKKOS_HAVE_PRAGMA_VECTOR
#pragma vector always
#endif
    for (size_type k = 0; k < value_count; ++k) {
      norms_(k) = dst[k];
    }
  }
};

//! Implementation of KokkosBlas::nrmInf for multivectors.
template<class RT, class RL, class RD, class RM, class RS,
         class XT, class XL, class XD, class XM, class XS>
struct NrmInf_MV {
  typedef Kokkos::View<RT,RL,RD,RM,RS> RV;
  typedef Kokkos::View<XT,XL,XD,XM,XS> XMV;
  typedef typename XMV::execution_space execution_space;
  typedef typename XMV::size_type size_type;

  /// \brief Compute the inf-norm(s) of the column(s) of the
  ///   multivector (2-D View) X, and store result(s) in r.
  static void nrmInf (const RV& r, const XMV& X)
  {
    const size_type numRows = X.dimension_0 ();
    const size_type numCols = X.dimension_1 ();

    // int is generally faster than size_t, but check for overflow first.
    if (numRows < static_cast<size_type> (INT_MAX) &&
        numRows * numCols < static_cast<size_type> (INT_MAX)) {
      typedef MV_NrmInf_Functor<RV, XMV, int> functor_type;
      Kokkos::RangePolicy<execution_space, int> policy (0, numRows);
      functor_type op (r, X);
      Kokkos::parallel_reduce (policy, op);
    }
    else {
      typedef MV_NrmInf_Functor<RV, XMV, size_type> functor_type;
      Kokkos::RangePolicy<execution_space, size_type> policy (0, numRows);
      functor_type op (r, X);
      Kokkos::parallel_reduce (policy, op);
    }
  }

  //! Compute the inf-norm of X(:,X_col), and store result in r(r_col).
  static void nrmInf (const RV& r, const size_t r_col, const XMV& X, const size_t X_col)
  {
    using Kokkos::ALL;
    using Kokkos::subview;
    typedef Kokkos::View<typename RV::value_type,
      typename RV::array_layout,
      typename RV::device_type,
      typename RV::memory_traits,
      typename RV::specialize> RV0D;
    typedef Kokkos::View<typename XMV::const_value_type*,
      typename XMV::array_layout,
      typename XMV::device_type,
      typename XMV::memory_traits,
      typename XMV::specialize> XMV1D;

    const size_type numRows = X.dimension_0 ();
    const size_type numCols = X.dimension_1 ();

    // int is generally faster than size_t, but check for overflow first.
    if (numRows < static_cast<size_type> (INT_MAX) &&
        numRows * numCols < static_cast<size_type> (INT_MAX)) {
      typedef V_NrmInf_Functor<RV0D, XMV1D, int> functor_type;
      Kokkos::RangePolicy<execution_space, int> policy (0, numRows);
      functor_type op (subview (r, r_col), subview (X, ALL (), X_col));
      Kokkos::parallel_reduce (policy, op);
    }
    else {
      typedef V_NrmInf_Functor<RV0D, XMV1D, size_type> functor_type;
      Kokkos::RangePolicy<execution_space, size_type> policy (0, numRows);
      functor_type op (subview (r, r_col), subview (X, ALL (), X_col));
      Kokkos::parallel_reduce (policy, op);
    }
  }
};

// Full specializations for cases of interest for Tpetra::MultiVector.
//
// Currently, we include specializations for Scalar = double,
// LayoutLeft (which is what Tpetra::MultiVector uses at the moment),
// and all execution spaces.  This may change in the future.  The
// output 1-D View _always_ uses the execution space's default array
// layout, which is what Tpetra::MultiVector wants for the output
// argument of normInf().

#ifdef KOKKOS_HAVE_SERIAL
template<>
struct NrmInf_MV<double*,
                 Kokkos::Serial::array_layout,
                 Kokkos::Device<Kokkos::Serial, Kokkos::HostSpace>,
                 Kokkos::MemoryTraits<Kokkos::Unmanaged>,
                 Kokkos::Impl::ViewDefault,
                 const double**,
                 Kokkos::LayoutLeft,
                 Kokkos::Device<Kokkos::Serial, Kokkos::HostSpace>,
                 Kokkos::MemoryTraits<Kokkos::Unmanaged>,
                 Kokkos::Impl::ViewDefault> {
  typedef double* RT;
  typedef Kokkos::Serial::array_layout RL;
  typedef Kokkos::Device<Kokkos::Serial, Kokkos::HostSpace> RD;
  typedef Kokkos::MemoryTraits<Kokkos::Unmanaged> RM;
  typedef Kokkos::Impl::ViewDefault RS;
  typedef Kokkos::View<RT,RL,RD,RM,RS> RV;

  typedef const double** XT;
  typedef Kokkos::LayoutLeft XL;
  typedef Kokkos::Device<Kokkos::Serial, Kokkos::HostSpace> XD;
  typedef Kokkos::MemoryTraits<Kokkos::Unmanaged> XM;
  typedef Kokkos::Impl::ViewDefault XS;
  typedef Kokkos::View<XT,XL,XD,XM,XS> XMV;

  typedef XMV::execution_space execution_space;
  typedef XMV::size_type size_type;

  static void nrmInf (const RV& r, const XMV& X);
  static void nrmInf (const RV& r, const size_t r_col, const XMV& X, const size_t X_col);
};
#endif // KOKKOS_HAVE_SERIAL

#ifdef KOKKOS_HAVE_OPENMP
template<>
struct NrmInf_MV<double*,
                 Kokkos::OpenMP::array_layout,
                 Kokkos::Device<Kokkos::OpenMP, Kokkos::HostSpace>,
                 Kokkos::MemoryTraits<Kokkos::Unmanaged>,
                 Kokkos::Impl::ViewDefault,
                 const double**,
                 Kokkos::LayoutLeft,
                 Kokkos::Device<Kokkos::OpenMP, Kokkos::HostSpace>,
                 Kokkos::MemoryTraits<Kokkos::Unmanaged>,
                 Kokkos::Impl::ViewDefault> {
  typedef double* RT;
  typedef Kokkos::OpenMP::array_layout RL;
  typedef Kokkos::Device<Kokkos::OpenMP, Kokkos::HostSpace> RD;
  typedef Kokkos::MemoryTraits<Kokkos::Unmanaged> RM;
  typedef Kokkos::Impl::ViewDefault RS;
  typedef Kokkos::View<RT,RL,RD,RM,RS> RV;

  typedef const double** XT;
  typedef Kokkos::LayoutLeft XL;
  typedef Kokkos::Device<Kokkos::OpenMP, Kokkos::HostSpace> XD;
  typedef Kokkos::MemoryTraits<Kokkos::Unmanaged> XM;
  typedef Kokkos::Impl::ViewDefault XS;
  typedef Kokkos::View<XT,XL,XD,XM,XS> XMV;

  typedef XMV::execution_space execution_space;
  typedef XMV::size_type size_type;

  static void nrmInf (const RV& r, const XMV& X);
  static void nrmInf (const RV& r, const size_t r_col, const XMV& X, const size_t X_col);
};
#endif // KOKKOS_HAVE_OPENMP

#ifdef KOKKOS_HAVE_PTHREAD
template<>
struct NrmInf_MV<double*,
                 Kokkos::Threads::array_layout,
                 Kokkos::Device<Kokkos::Threads, Kokkos::HostSpace>,
                 Kokkos::MemoryTraits<Kokkos::Unmanaged>,
                 Kokkos::Impl::ViewDefault,
                 const double**,
                 Kokkos::LayoutLeft,
                 Kokkos::Device<Kokkos::Threads, Kokkos::HostSpace>,
                 Kokkos::MemoryTraits<Kokkos::Unmanaged>,
                 Kokkos::Impl::ViewDefault> {
  typedef double* RT;
  typedef Kokkos::Threads::array_layout RL;
  typedef Kokkos::Device<Kokkos::Threads, Kokkos::HostSpace> RD;
  typedef Kokkos::MemoryTraits<Kokkos::Unmanaged> RM;
  typedef Kokkos::Impl::ViewDefault RS;
  typedef Kokkos::View<RT,RL,RD,RM,RS> RV;

  typedef const double** XT;
  typedef Kokkos::LayoutLeft XL;
  typedef Kokkos::Device<Kokkos::Threads, Kokkos::HostSpace> XD;
  typedef Kokkos::MemoryTraits<Kokkos::Unmanaged> XM;
  typedef Kokkos::Impl::ViewDefault XS;
  typedef Kokkos::View<XT,XL,XD,XM,XS> XMV;

  typedef XMV::execution_space execution_space;
  typedef XMV::size_type size_type;

  static void nrmInf (const RV& r, const XMV& X);
  static void nrmInf (const RV& r, const size_t r_col, const XMV& X, const size_t X_col);
};
#endif // KOKKOS_HAVE_PTHREAD

#ifdef KOKKOS_HAVE_CUDA
template<>
struct NrmInf_MV<double*,
                 Kokkos::Cuda::array_layout,
                 Kokkos::Device<Kokkos::Cuda, Kokkos::CudaSpace>,
                 Kokkos::MemoryTraits<Kokkos::Unmanaged>,
                 Kokkos::Impl::ViewDefault,
                 const double**,
                 Kokkos::LayoutLeft,
                 Kokkos::Device<Kokkos::Cuda, Kokkos::CudaSpace>,
                 Kokkos::MemoryTraits<Kokkos::Unmanaged>,
                 Kokkos::Impl::ViewDefault> {
  typedef double* RT;
  typedef Kokkos::Cuda::array_layout RL;
  typedef Kokkos::Device<Kokkos::Cuda, Kokkos::CudaSpace> RD;
  typedef Kokkos::MemoryTraits<Kokkos::Unmanaged> RM;
  typedef Kokkos::Impl::ViewDefault RS;
  typedef Kokkos::View<RT,RL,RD,RM,RS> RV;

  typedef const double** XT;
  typedef Kokkos::LayoutLeft XL;
  typedef Kokkos::Device<Kokkos::Cuda, Kokkos::CudaSpace> XD;
  typedef Kokkos::MemoryTraits<Kokkos::Unmanaged> XM;
  typedef Kokkos::Impl::ViewDefault XS;
  typedef Kokkos::View<XT,XL,XD,XM,XS> XMV;

  typedef XMV::execution_space execution_space;
  typedef XMV::size_type size_type;

  static void nrmInf (const RV& r, const XMV& X);
  static void nrmInf (const RV& r, const size_t r_col, const XMV& X, const size_t X_col);
};
#endif // KOKKOS_HAVE_CUDA

#ifdef KOKKOS_HAVE_CUDA
template<>
struct NrmInf_MV<double*,
                 Kokkos::Cuda::array_layout,
                 Kokkos::Device<Kokkos::Cuda, Kokkos::CudaUVMSpace>,
                 Kokkos::MemoryTraits<Kokkos::Unmanaged>,
                 Kokkos::Impl::ViewDefault,
                 const double**,
                 Kokkos::LayoutLeft,
                 Kokkos::Device<Kokkos::Cuda, Kokkos::CudaUVMSpace>,
                 Kokkos::MemoryTraits<Kokkos::Unmanaged>,
                 Kokkos::Impl::ViewDefault> {
  typedef double* RT;
  typedef Kokkos::Cuda::array_layout RL;
  typedef Kokkos::Device<Kokkos::Cuda, Kokkos::CudaUVMSpace> RD;
  typedef Kokkos::MemoryTraits<Kokkos::Unmanaged> RM;
  typedef Kokkos::Impl::ViewDefault RS;
  typedef Kokkos::View<RT,RL,RD,RM,RS> RV;

  typedef const double** XT;
  typedef Kokkos::LayoutLeft XL;
  typedef Kokkos::Device<Kokkos::Cuda, Kokkos::CudaUVMSpace> XD;
  typedef Kokkos::MemoryTraits<Kokkos::Unmanaged> XM;
  typedef Kokkos::Impl::ViewDefault XS;
  typedef Kokkos::View<XT,XL,XD,XM,XS> XMV;

  typedef XMV::execution_space execution_space;
  typedef XMV::size_type size_type;

  static void nrmInf (const RV& r, const XMV& X);
  static void nrmInf (const RV& r, const size_t r_col, const XMV& X, const size_t X_col);
};
#endif // KOKKOS_HAVE_CUDA

} // namespace Impl
} // namespace KokkosBlas

#endif // KOKKOS_BLAS1_MV_IMPL_NRMINF_HPP_