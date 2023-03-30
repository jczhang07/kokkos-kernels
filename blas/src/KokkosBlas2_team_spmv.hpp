//@HEADER
// ************************************************************************
//
//                        Kokkos v. 4.0
//       Copyright (2022) National Technology & Engineering
//               Solutions of Sandia, LLC (NTESS).
//
// Under the terms of Contract DE-NA0003525 with NTESS,
// the U.S. Government retains certain rights in this software.
//
// Part of Kokkos, under the Apache License v2.0 with LLVM Exceptions.
// See https://kokkos.org/LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//@HEADER
#ifndef KOKKOSBLAS2_TEAM_SPMV_HPP_
#define KOKKOSBLAS2_TEAM_SPMV_HPP_

/// \file KokkosBlas2_spmv.hpp

#include <KokkosKernels_helpers.hpp>
#include <KokkosKernels_Error.hpp>
#include <sstream>
#include <type_traits>  // requires C++11, but so does Kokkos
#include <KokkosBlas2_team_spmv_spec.hpp>

namespace KokkosBlas {
namespace Experimental {

/// \brief Sparse matrix-vector multiply: y = beta*y + alpha*A*x.
///
template <class TeamType, class ScalarType, class ValuesViewType, class IntView,
          class xViewType, class yViewType>
void KOKKOS_INLINE_FUNCTION team_spmv(
    const TeamType &team, const ScalarType &alpha, const ValuesViewType &values,
    const IntView &row_ptr, const IntView &colIndices, const xViewType &x,
    const ScalarType &beta, const yViewType &y, const int dobeta) {
#if (KOKKOSKERNELS_DEBUG_LEVEL > 0)
  static_assert(Kokkos::is_view<ValuesViewType>::value,
                "ValuesViewType must be a Kokkos::View.");
  static_assert(Kokkos::is_view<IntView>::value,
                "IntView must be a Kokkos::View.");
  static_assert(Kokkos::is_view<xViewType>::value,
                "xViewType must be a Kokkos::View.");
  static_assert(Kokkos::is_view<yViewType>::value,
                "yViewType must be a Kokkos::View.");
  static_assert(static_cast<int>(ValuesViewType::rank) == 1,
                "ValuesViewType must have rank 1.");
  static_assert(static_cast<int>(IntView::rank) == 1,
                "IntView must have rank 1.");
  static_assert(static_cast<int>(xViewType::rank) == 1,
                "xViewType must have rank 1.");
  static_assert(static_cast<int>(yViewType::rank) == 1,
                "yViewType must have rank 1.");

  // Check compatibility of dimensions at run time.
  if (values.extent(0) != colIndices.extent(0)) {
    std::ostringstream os;
    os << "KokkosBlas::spmv: Dimensions of values and colIndices do not match: "
       << "values: " << values.extent(0)
       << ", colIndices: " << colIndices.extent(0);
    KokkosKernels::Impl::throw_runtime_exception(os.str());
  }

  if (x.extent(0) != y.extent(0) || (x.extent(0) + 1) != row_ptr.extent(0)) {
    std::ostringstream os;
    os << "KokkosBlas::spmv: Dimensions of x, y, and row_ptr do not match: "
       << "x: " << x.extent(0) << ", y: " << y.extent(0)
       << ", row_ptr: " << row_ptr.extent(0);
    KokkosKernels::Impl::throw_runtime_exception(os.str());
  }
#endif  // KOKKOSKERNELS_DEBUG_LEVEL

  if (dobeta == 1)
    KokkosBlas::TeamSpmv<TeamType>::template invoke<
        ScalarType, ValuesViewType, IntView, xViewType, yViewType, 1>(
        team, alpha, values, row_ptr, colIndices, x, beta, y);
  else
    KokkosBlas::TeamSpmv<TeamType>::template invoke<
        ScalarType, ValuesViewType, IntView, xViewType, yViewType, 0>(
        team, alpha, values, row_ptr, colIndices, x, beta, y);
}

/// \brief Sparse matrix-vector multiply: y = beta*y + alpha*A*x.
///
template <class TeamType, class ScalarType, class ValuesViewType, class IntView,
          class xViewType, class yViewType>
void KOKKOS_INLINE_FUNCTION team_vector_spmv(
    const TeamType &team, const ScalarType &alpha, const ValuesViewType &values,
    const IntView &row_ptr, const IntView &colIndices, const xViewType &x,
    const ScalarType &beta, const yViewType &y, const int dobeta) {
#if (KOKKOSKERNELS_DEBUG_LEVEL > 0)
  static_assert(Kokkos::is_view<ValuesViewType>::value,
                "ValuesViewType must be a Kokkos::View.");
  static_assert(Kokkos::is_view<IntView>::value,
                "IntView must be a Kokkos::View.");
  static_assert(Kokkos::is_view<xViewType>::value,
                "xViewType must be a Kokkos::View.");
  static_assert(Kokkos::is_view<yViewType>::value,
                "yViewType must be a Kokkos::View.");
  static_assert(static_cast<int>(ValuesViewType::rank) == 1,
                "ValuesViewType must have rank 1.");
  static_assert(static_cast<int>(IntView::rank) == 1,
                "IntView must have rank 1.");
  static_assert(static_cast<int>(xViewType::rank) == 1,
                "xViewType must have rank 1.");
  static_assert(static_cast<int>(yViewType::rank) == 1,
                "yViewType must have rank 1.");

  // Check compatibility of dimensions at run time.
  if (values.extent(0) != colIndices.extent(0)) {
    std::ostringstream os;
    os << "KokkosBlas::spmv: Dimensions of values and colIndices do not match: "
       << "values: " << values.extent(0)
       << ", colIndices: " << colIndices.extent(0);
    KokkosKernels::Impl::throw_runtime_exception(os.str());
  }

  if (x.extent(0) != y.extent(0) || (x.extent(0) + 1) != row_ptr.extent(0)) {
    std::ostringstream os;
    os << "KokkosBlas::spmv: Dimensions of x, y, and row_ptr do not match: "
       << "x: " << x.extent(0) << ", y: " << y.extent(0)
       << ", row_ptr: " << row_ptr.extent(0);
    KokkosKernels::Impl::throw_runtime_exception(os.str());
  }
#endif  // KOKKOSKERNELS_DEBUG_LEVEL

  if (dobeta == 1)
    KokkosBlas::TeamVectorSpmv<TeamType>::template invoke<
        ScalarType, ValuesViewType, IntView, xViewType, yViewType, 1>(
        team, alpha, values, row_ptr, colIndices, x, beta, y);
  else
    KokkosBlas::TeamVectorSpmv<TeamType>::template invoke<
        ScalarType, ValuesViewType, IntView, xViewType, yViewType, 0>(
        team, alpha, values, row_ptr, colIndices, x, beta, y);
}

}  // namespace Experimental
}  // namespace KokkosBlas

#endif  // KOKKOS_BLAS2_MV_HPP_
