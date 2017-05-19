#ifndef __KOKKOSKERNELS_LU_TEAM_IMPL_HPP__
#define __KOKKOSKERNELS_LU_TEAM_IMPL_HPP__


/// \author Kyungjoo Kim (kyukim@sandia.gov)

#include "KokkosKernels_Util.hpp"
#include "KokkosKernels_LU_Team_Internal.hpp"

namespace KokkosKernels {
  namespace Batched {
    namespace Experimental {
      ///
      /// Team Impl
      /// =========

      namespace Team {
        
        ///
        /// LU no piv
        ///

        template<typename MemberType>
        template<typename AViewType>
        KOKKOS_INLINE_FUNCTION
        int
        LU<MemberType,Algo::LU::Unblocked>::
        invoke(const MemberType &member, const AViewType &A) {
          return LU_Internal<Algo::LU::Unblocked>::invoke(member,
                                                          A.dimension_0(), A.dimension_1(),
                                                          A.data(), A.stride_0(), A.stride_1());
        }
    
        template<typename MemberType>
        template<typename AViewType>
        KOKKOS_INLINE_FUNCTION
        int
        LU<MemberType,Algo::LU::Blocked>::
        invoke(const MemberType &member, const AViewType &A) {
          return LU_Internal<Algo::LU::Blocked>::invoke(member, 
                                                        A.dimension_0(), A.dimension_1(),
                                                        A.data(), A.stride_0(), A.stride_1());
        }

      }
    }
  }
}
#endif
