/*
//@HEADER
// ************************************************************************
//
//               KokkosKernels 0.9: Linear Algebra and Graph Kernels
//                 Copyright 2017 Sandia Corporation
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
// Questions? Contact Siva Rajamanickam (srajama@sandia.gov)
//
// ************************************************************************
//@HEADER
*/
#include "KokkosKernels_BitUtils.hpp"
namespace KokkosSparse{

namespace Impl{


template <typename HandleType,
typename a_row_view_t_, typename a_lno_nnz_view_t_, typename a_scalar_nnz_view_t_,
typename b_lno_row_view_t_, typename b_lno_nnz_view_t_, typename b_scalar_nnz_view_t_  >
template <typename a_row_view_t, typename a_nnz_view_t,
          typename b_original_row_view_t,
          typename b_compressed_row_view_t, typename b_nnz_view_t,
          typename c_row_view_t, //typename nnz_lno_temp_work_view_t,
          typename pool_memory_space>
struct KokkosSPGEMM
  <HandleType, a_row_view_t_, a_lno_nnz_view_t_, a_scalar_nnz_view_t_,
    b_lno_row_view_t_, b_lno_nnz_view_t_, b_scalar_nnz_view_t_>::
  StructureC{
  const nnz_lno_t numrows; //num rows in A

  const a_row_view_t row_mapA; //A row pointers
  const a_nnz_view_t entriesA; // A column indices

  const b_original_row_view_t row_pointer_begins_B;
  const b_compressed_row_view_t row_pointer_ends_B;
  b_nnz_view_t entriesSetIndicesB;
  b_nnz_view_t entriesSetsB;

  c_row_view_t rowmapC;
  //nnz_lno_temp_work_view_t entriesSetIndicesC;
  //nnz_lno_temp_work_view_t entriesSetsC;


  const nnz_lno_t pow2_hash_size;
  const nnz_lno_t pow2_hash_func;
  const nnz_lno_t MaxRoughNonZero;

  const size_t shared_memory_size;
  const int vector_size;
  pool_memory_space m_space;
  const KokkosKernels::Impl::ExecSpaceType my_exec_space;


  const int unit_memory; //begins, nexts, and keys. No need for vals yet.
  const int suggested_team_size;
  const int thread_memory;
  nnz_lno_t shmem_key_size;
  nnz_lno_t shared_memory_hash_func;
  nnz_lno_t shmem_hash_size;
  nnz_lno_t team_row_chunk_size;

  /**
   * \brief constructor
   * \param m_: input row size of A
   * \param row_mapA_: row pointers of A
   * \param entriesA_: col indices of A
   * \param row_ptr_begins_B_: beginning of the rows of B
   * \param row_ptr_ends_B_:end of the rows of B
   * \param entriesSetIndicesB_: column set indices of B
   * \param entriesSetsB_: columns sets of B
   * \param rowmapC_: output rowmap C
   * \param hash_size_: global hashmap hash size.
   * \param MaxRoughNonZero_: max flops for row.
   * \param sharedMemorySize_: shared memory size.
   * \param suggested_team_size_: suggested team size
   * \param team_row_chunk_size_: suggested team chunk size
   * \param my_exec_space_ : execution space.
   */
  StructureC(
      const nnz_lno_t m_,
      const a_row_view_t row_mapA_,
      const a_nnz_view_t entriesA_,
      const b_original_row_view_t row_ptr_begins_B_,
      const b_compressed_row_view_t row_ptr_ends_B_,
      const b_nnz_view_t entriesSetIndicesB_,
      const b_nnz_view_t entriesSetsB_,
      c_row_view_t rowmapC_,
      const nnz_lno_t hash_size_,
      const nnz_lno_t MaxRoughNonZero_,
      const size_t sharedMemorySize_,
      const int suggested_team_size_,
      const nnz_lno_t team_row_chunk_size_,
      const int vector_size_,
      pool_memory_space mpool_,
      const KokkosKernels::Impl::ExecSpaceType my_exec_space_
      ,bool KOKKOSKERNELS_VERBOSE_
      ):
        numrows(m_),
        row_mapA (row_mapA_),
        entriesA(entriesA_),
        row_pointer_begins_B(row_ptr_begins_B_),
        row_pointer_ends_B(row_ptr_ends_B_),
        entriesSetIndicesB(entriesSetIndicesB_),
        entriesSetsB(entriesSetsB_),
        rowmapC(rowmapC_),
        //entriesSetIndicesC(),
        //entriesSetsC(),
        pow2_hash_size(hash_size_),
        pow2_hash_func(hash_size_ - 1),
        MaxRoughNonZero(MaxRoughNonZero_),
        shared_memory_size(sharedMemorySize_),
        vector_size (vector_size_),
        m_space(mpool_),
        my_exec_space(my_exec_space_),

        //unit memory for a hashmap entry. assuming 1 begin, 1 next, 1 key 1 value.
        unit_memory(sizeof(nnz_lno_t) * 2 + sizeof(nnz_lno_t) * 2),
        suggested_team_size(suggested_team_size_),
        thread_memory((shared_memory_size /8 / suggested_team_size_) * 8),
        shmem_key_size(),
        shared_memory_hash_func(),
        shmem_hash_size(1),
        team_row_chunk_size(team_row_chunk_size_)
  {

    //how many keys I can hold?
    //thread memory - 3 needed entry for size.
    shmem_key_size = ((thread_memory - sizeof(nnz_lno_t) * 3) / unit_memory);

    //put the hash size closest power of 2.
    //we round down here, because we want to store more keys,
    //conflicts are cheaper.
    while (shmem_hash_size * 2 <=  shmem_key_size){
      shmem_hash_size = shmem_hash_size * 2;
    }
    //for and opeation we get -1.
    shared_memory_hash_func = shmem_hash_size - 1;

    //increase the key size wit the left over from hash size.
    shmem_key_size = shmem_key_size + ((shmem_key_size - shmem_hash_size) ) / 3;
    //round it down to 2, because of some alignment issues.
    shmem_key_size = (shmem_key_size >> 1) << 1;

    if (KOKKOSKERNELS_VERBOSE_){

      std::cout << "\tStructureC "
                << " thread_memory:" << thread_memory
                << " unit_memory:" << unit_memory
                << " adjusted hashsize:" << shmem_hash_size
                << " adjusted shmem_key_size:" << shmem_key_size
                << " using "<< (shmem_key_size * 3  + shmem_hash_size) * sizeof (nnz_lno_t) +    sizeof(nnz_lno_t) * 3
                << " of thread_memory: " << thread_memory
                << std::endl;
          }
  }

  KOKKOS_INLINE_FUNCTION
  size_t get_thread_id(const size_t row_index) const{
    switch (my_exec_space){
    default:
      return row_index;
#if defined( KOKKOS_HAVE_SERIAL )
    case KokkosKernels::Impl::Exec_SERIAL:
      return 0;
#endif
#if defined( KOKKOS_HAVE_OPENMP )
    case KokkosKernels::Impl::Exec_OMP:
      return Kokkos::OpenMP::hardware_thread_id();
#endif
#if defined( KOKKOS_HAVE_PTHREAD )
    case KokkosKernels::Impl::Exec_PTHREADS:
      return Kokkos::Threads::hardware_thread_id();
#endif
#if defined( KOKKOS_HAVE_QTHREAD)
    case KokkosKernels::Impl::Exec_QTHREADS:
      return Kokkos::Qthread::hardware_thread_id();
#endif
#if defined( KOKKOS_ENABLE_CUDA )
    case KokkosKernels::Impl::Exec_CUDA:
      return row_index;
#endif
    }
  }

  KOKKOS_INLINE_FUNCTION
  void operator()(const MultiCoreDenseAccumulatorTag&, const team_member_t & teamMember) const {
    const nnz_lno_t team_row_begin = teamMember.league_rank() * team_row_chunk_size;
    const nnz_lno_t team_row_end = KOKKOSKERNELS_MACRO_MIN(team_row_begin + team_row_chunk_size, numrows);

    //dense accumulators
    nnz_lno_t *indices = NULL;
    nnz_lno_t *sets = NULL;
    volatile nnz_lno_t * tmp = NULL;

    size_t tid = get_thread_id(team_row_begin + teamMember.team_rank());
    while (tmp == NULL){
      tmp = (volatile nnz_lno_t * )( m_space.allocate_chunk(tid));
    }

    //we need as much as column size for sets.
    sets = (nnz_lno_t *) tmp;
    tmp += MaxRoughNonZero; //this is set as column size before calling dense accumulators.
    //indices only needs max row size.
    indices = (nnz_lno_t *) tmp;


    Kokkos::parallel_for(Kokkos::TeamThreadRange(teamMember, team_row_begin, team_row_end), [&] (const nnz_lno_t& row_index)
    {
      nnz_lno_t index_cnt = 0;
      const size_type col_begin = row_mapA[row_index];
      const nnz_lno_t col_size = row_mapA[row_index + 1] - col_begin;

      //traverse columns of A
      for (nnz_lno_t colind = 0; colind < col_size; ++colind){
        size_type a_col = colind + col_begin;

        nnz_lno_t rowB = entriesA[a_col];
        size_type rowBegin = row_pointer_begins_B(rowB);
        nnz_lno_t left_work = row_pointer_ends_B(rowB ) - rowBegin;

        //traverse columns of B
        for (nnz_lno_t i = 0; i < left_work; ++i){

          const size_type adjind = i + rowBegin;
          nnz_lno_t b_set_ind = entriesSetIndicesB[adjind];
          nnz_lno_t b_set = entriesSetsB[adjind];

          //if sets are not set before, add this to indices.
          if (sets[b_set_ind] == 0){
            indices[index_cnt++] = b_set_ind;
          }
          //make a union.
          sets[b_set_ind] |= b_set;
        }
      }
      nnz_lno_t num_el = 0;
      for (nnz_lno_t ii = 0; ii < index_cnt; ++ii){
        nnz_lno_t set_ind = indices[ii];
        nnz_lno_t c_rows = sets[set_ind];
        sets[set_ind] = 0;

        nnz_lno_t num_el2 = KokkosKernels::Impl::pop_count(c_rows);
/*
        //count number of set bits
        nnz_lno_t num_el2 = 0;
        for (; c_rows; num_el2++) {
          c_rows = c_rows & (c_rows - 1); // clear the least significant bit set
        }
        */
        num_el += num_el2;
      }
      rowmapC(row_index) = num_el;
    }
    );

    m_space.release_chunk(indices);
  }

  //this one will be cuckoo hashing.
  KOKKOS_INLINE_FUNCTION
  void operator()(const MultiCoreTag3&, const team_member_t & teamMember) const {
    const nnz_lno_t team_row_begin = teamMember.league_rank() * team_row_chunk_size;
    const nnz_lno_t team_row_end = KOKKOSKERNELS_MACRO_MIN(team_row_begin + team_row_chunk_size, numrows);

    //get memory from memory pool.
    volatile nnz_lno_t * tmp = NULL;
    size_t tid = get_thread_id(team_row_begin + teamMember.team_rank());
    while (tmp == NULL){
      tmp = (volatile nnz_lno_t * )( m_space.allocate_chunk(tid));
    }

    nnz_lno_t *hash_ids = (nnz_lno_t *) (tmp);
    tmp += pow2_hash_size;
    nnz_lno_t *hash_values = (nnz_lno_t *) (tmp);

    Kokkos::parallel_for(Kokkos::TeamThreadRange(teamMember, team_row_begin, team_row_end), [&] (const nnz_lno_t& row_index){
      nnz_lno_t globally_used_hash_count = 0;
      nnz_lno_t used_hash_size = 0;
      const size_type col_begin = row_mapA[row_index];
      const nnz_lno_t col_size = row_mapA[row_index + 1] - col_begin;
      //traverse columns of A.
      for (nnz_lno_t colind = 0; colind < col_size; ++colind){
        size_type a_col = colind + col_begin;
        nnz_lno_t rowB = entriesA[a_col];

        size_type rowBegin = row_pointer_begins_B(rowB);
        nnz_lno_t left_work = row_pointer_ends_B(rowB ) - rowBegin;
        //traverse columns of B
        for (nnz_lno_t i = 0; i < left_work; ++i){

          const size_type adjind = i + rowBegin;

          nnz_lno_t b_set_ind = entriesSetIndicesB[adjind];
          nnz_lno_t b_set = entriesSetsB[adjind];
          nnz_lno_t hash = (b_set_ind * HASHSCALAR) & pow2_hash_func;

          while (true){
            if (hash_ids[hash] == -1){
            	hash_ids[hash] = b_set_ind;
            	hash_values[hash] = b_set;
            	break;
            }
            else if (hash_ids[hash] == b_set_ind){
            	hash_values[hash] = hash_values[hash] | b_set;
            	break;
            }
            else {
            	hash = (hash + 1) & pow2_hash_func;
            }
          }
        }
      }

      //when done with all insertions, traverse insertions and get the size.
      nnz_lno_t num_el = 0;
      for (nnz_lno_t ii = 0; ii < pow2_hash_size; ++ii){
    	  if (hash_ids[ii] != -1){
    		  hash_ids[ii] = -1;
    		  nnz_lno_t c_rows = hash_values[ii];
    		  nnz_lno_t num_el2 = KokkosKernels::Impl::pop_count(c_rows);

    		  //the number of set bits.
    		  /*
    		  for (; c_rows; num_el2++) {
    			  c_rows = c_rows & (c_rows - 1); // clear the least significant bit set
    		  }
    		  */
    		  num_el += num_el2;
    	  }
      }

      //set the row size.
      rowmapC(row_index) = num_el;
    });

    m_space.release_chunk(hash_ids);
  }

  //this one will be cuckoo hashing with tracking.
  KOKKOS_INLINE_FUNCTION
  void operator()(const MultiCoreTag4&, const team_member_t & teamMember) const {



    const nnz_lno_t team_row_begin = teamMember.league_rank() * team_row_chunk_size;
    const nnz_lno_t team_row_end = KOKKOSKERNELS_MACRO_MIN(team_row_begin + team_row_chunk_size, numrows);


    //get memory from memory pool.
    volatile nnz_lno_t * tmp = NULL;
    size_t tid = get_thread_id(team_row_begin + teamMember.team_rank());
    while (tmp == NULL){
      tmp = (volatile nnz_lno_t * )( m_space.allocate_chunk(tid));
    }



    nnz_lno_t *used_indices = (nnz_lno_t *) (tmp);
    tmp += MaxRoughNonZero;
    nnz_lno_t *hash_ids = (nnz_lno_t *) (tmp);
    tmp += pow2_hash_size;
    nnz_lno_t *hash_values = (nnz_lno_t *) (tmp);



    Kokkos::parallel_for(Kokkos::TeamThreadRange(teamMember, team_row_begin, team_row_end), [&] (const nnz_lno_t& row_index){
      nnz_lno_t used_count = 0;

      nnz_lno_t globally_used_hash_count = 0;
      nnz_lno_t used_hash_size = 0;
      const size_type col_begin = row_mapA[row_index];
      const nnz_lno_t col_size = row_mapA[row_index + 1] - col_begin;
      //traverse columns of A.
      for (nnz_lno_t colind = 0; colind < col_size; ++colind){
        size_type a_col = colind + col_begin;
        nnz_lno_t rowB = entriesA[a_col];

        size_type rowBegin = row_pointer_begins_B(rowB);
        nnz_lno_t left_work = row_pointer_ends_B(rowB ) - rowBegin;
        //traverse columns of B
        for (nnz_lno_t i = 0; i < left_work; ++i){

          const size_type adjind = i + rowBegin;

          nnz_lno_t b_set_ind = entriesSetIndicesB[adjind];
          nnz_lno_t b_set = entriesSetsB[adjind];
          nnz_lno_t hash = (b_set_ind * HASHSCALAR) & pow2_hash_func;

          while (true){
            if (hash_ids[hash] == -1){
            	used_indices[used_count++] = hash;
            	hash_ids[hash] = b_set_ind;
            	hash_values[hash] = b_set;
            	break;
            }
            else if (hash_ids[hash] == b_set_ind){
            	hash_values[hash] = hash_values[hash] | b_set;
            	break;
            }
            else {
            	hash = (hash + 1) & pow2_hash_func;
            }
          }
        }
      }

      //when done with all insertions, traverse insertions and get the size.
      nnz_lno_t num_el = 0;
      for (nnz_lno_t ii = 0; ii < used_count; ++ii){

    	  nnz_lno_t used_index = used_indices[ii];
    	  nnz_lno_t c_rows = hash_values[used_index];
    	  hash_ids[used_index] = -1;


		  nnz_lno_t num_el2 = KokkosKernels::Impl::pop_count(c_rows);
/*
    	  //the number of set bits.
    	  for (; c_rows; num_el2++) {
    		  c_rows = c_rows & (c_rows - 1); // clear the least significant bit set
    	  }
    	  */
    	  num_el += num_el2;
      }
      rowmapC(row_index) = num_el;
    });

    m_space.release_chunk(used_indices);
  }


  //this one will be cuckoo hashing with tracking.
  KOKKOS_INLINE_FUNCTION
  void operator()(const MultiCoreTag5&, const team_member_t & teamMember) const {



    const nnz_lno_t team_row_begin = teamMember.league_rank() * team_row_chunk_size;
    const nnz_lno_t team_row_end = KOKKOSKERNELS_MACRO_MIN(team_row_begin + team_row_chunk_size, numrows);


    //get memory from memory pool.
    volatile nnz_lno_t * tmp = NULL;
    size_t tid = get_thread_id(team_row_begin + teamMember.team_rank());
    while (tmp == NULL){
      tmp = (volatile nnz_lno_t * )( m_space.allocate_chunk(tid));
    }



    nnz_lno_t *used_indices = (nnz_lno_t *) (tmp);
    tmp += MaxRoughNonZero;
    nnz_lno_t *hash_ids = (nnz_lno_t *) (tmp);
    tmp += pow2_hash_size;
    nnz_lno_t *hash_values = (nnz_lno_t *) (tmp);



    Kokkos::parallel_for(Kokkos::TeamThreadRange(teamMember, team_row_begin, team_row_end), [&] (const nnz_lno_t& row_index){
      nnz_lno_t used_count = 0;

      nnz_lno_t globally_used_hash_count = 0;
      nnz_lno_t used_hash_size = 0;
      const size_type col_begin = row_mapA[row_index];
      const nnz_lno_t col_size = row_mapA[row_index + 1] - col_begin;
      //traverse columns of A.
      for (nnz_lno_t colind = 0; colind < col_size; ++colind){
        size_type a_col = colind + col_begin;
        nnz_lno_t rowB = entriesA[a_col];

        size_type rowBegin = row_pointer_begins_B(rowB);
        nnz_lno_t left_work = row_pointer_ends_B(rowB ) - rowBegin;
        //traverse columns of B
        for (nnz_lno_t i = 0; i < left_work; ++i){

          const size_type adjind = i + rowBegin;

          nnz_lno_t b_set_ind = entriesSetIndicesB[adjind];
          nnz_lno_t b_set = entriesSetsB[adjind];

          nnz_lno_t hash = b_set_ind;
          hash = ((hash >> 16) ^ hash) * UINT32_C(0x45d9f3b);
          hash = ((hash >> 16) ^ hash) * UINT32_C(0x45d9f3b);
          hash = ((hash >> 16) ^ hash);
          hash = hash & pow2_hash_func;

          //nnz_lno_t hash = (b_set_ind * HASHSCALAR) & pow2_hash_func;

          while (true){
            if (hash_ids[hash] == -1){
            	used_indices[used_count++] = hash;
            	hash_ids[hash] = b_set_ind;
            	hash_values[hash] = b_set;
            	break;
            }
            else if (hash_ids[hash] == b_set_ind){
            	hash_values[hash] = hash_values[hash] | b_set;
            	break;
            }
            else {
            	hash = (hash + 1) & pow2_hash_func;
            }
          }
        }
      }

      //when done with all insertions, traverse insertions and get the size.
      nnz_lno_t num_el = 0;
      for (nnz_lno_t ii = 0; ii < used_count; ++ii){

    	  nnz_lno_t used_index = used_indices[ii];
    	  nnz_lno_t c_rows = hash_values[used_index];
    	  hash_ids[used_index] = -1;


		  nnz_lno_t num_el2 = KokkosKernels::Impl::pop_count(c_rows);
/*
    	  //the number of set bits.
    	  for (; c_rows; num_el2++) {
    		  c_rows = c_rows & (c_rows - 1); // clear the least significant bit set
    	  }
    	  */
    	  num_el += num_el2;
      }
      rowmapC(row_index) = num_el;
    });

    m_space.release_chunk(used_indices);
  }
  KOKKOS_INLINE_FUNCTION
  void operator()(const MultiCoreTag&, const team_member_t & teamMember) const {



    const nnz_lno_t team_row_begin = teamMember.league_rank() * team_row_chunk_size;
    const nnz_lno_t team_row_end = KOKKOSKERNELS_MACRO_MIN(team_row_begin + team_row_chunk_size, numrows);


    //get memory from memory pool.
    volatile nnz_lno_t * tmp = NULL;
    size_t tid = get_thread_id(team_row_begin + teamMember.team_rank());
    while (tmp == NULL){
      tmp = (volatile nnz_lno_t * )( m_space.allocate_chunk(tid));
    }

    //set first to globally used hash indices.
    nnz_lno_t *globally_used_hash_indices = (nnz_lno_t *) tmp;
    tmp += pow2_hash_size;

    //create hashmap accumulator.
    KokkosKernels::Experimental::HashmapAccumulator<nnz_lno_t,nnz_lno_t,nnz_lno_t> hm2;

    //set memory for hash begins.
    hm2.hash_begins = (nnz_lno_t *) (tmp);
    tmp += pow2_hash_size ;

    hm2.hash_nexts = (nnz_lno_t *) (tmp);
    tmp += MaxRoughNonZero;

    //holds the keys
    hm2.keys = (nnz_lno_t *) (tmp);
    tmp += MaxRoughNonZero;
    hm2.values = (nnz_lno_t *) (tmp);

    hm2.hash_key_size = pow2_hash_size;
    hm2.max_value_size = MaxRoughNonZero;

    Kokkos::parallel_for(Kokkos::TeamThreadRange(teamMember, team_row_begin, team_row_end), [&] (const nnz_lno_t& row_index){
      nnz_lno_t globally_used_hash_count = 0;
      nnz_lno_t used_hash_size = 0;
      const size_type col_begin = row_mapA[row_index];
      const nnz_lno_t col_size = row_mapA[row_index + 1] - col_begin;
      //traverse columns of A.
      for (nnz_lno_t colind = 0; colind < col_size; ++colind){
        size_type a_col = colind + col_begin;
        nnz_lno_t rowB = entriesA[a_col];

        size_type rowBegin = row_pointer_begins_B(rowB);
        nnz_lno_t left_work = row_pointer_ends_B(rowB ) - rowBegin;
        //traverse columns of B
        for (nnz_lno_t i = 0; i < left_work; ++i){

          const size_type adjind = i + rowBegin;

          nnz_lno_t b_set_ind = entriesSetIndicesB[adjind];
          nnz_lno_t b_set = entriesSetsB[adjind];
          nnz_lno_t hash = b_set_ind & pow2_hash_func;

          //insert it to first hash.
          hm2.sequential_insert_into_hash_mergeOr_TrackHashes(
              hash,
              b_set_ind, b_set,
              &used_hash_size,
              hm2.max_value_size,&globally_used_hash_count,
              globally_used_hash_indices
          );
        }
      }

      //when done with all insertions, traverse insertions and get the size.
      nnz_lno_t num_el = 0;
      for (nnz_lno_t ii = 0; ii < used_hash_size; ++ii){
        nnz_lno_t c_rows = hm2.values[ii];
		nnz_lno_t num_el2 = KokkosKernels::Impl::pop_count(c_rows);
/*
        //the number of set bits.
        for (; c_rows; num_el2++) {
          c_rows = c_rows & (c_rows - 1); // clear the least significant bit set
        }
        */
        num_el += num_el2;
      }

      //clear the begins.
      for (int i = 0; i < globally_used_hash_count; ++i){
        nnz_lno_t dirty_hash = globally_used_hash_indices[i];
        hm2.hash_begins[dirty_hash] = -1;
      }
      //set the row size.
      rowmapC(row_index) = num_el;
    });

    m_space.release_chunk(globally_used_hash_indices);
  }


  KOKKOS_INLINE_FUNCTION
  void operator()(const GPUTag&, const team_member_t & teamMember) const {


    nnz_lno_t row_index = teamMember.league_rank()  * teamMember.team_size()+ teamMember.team_rank();
    if (row_index >= numrows) return;


    //printf("row:%d\n", row_index);

    //int thread_memory = ((shared_memory_size/ 4 / teamMember.team_size())) * 4;
    char *all_shared_memory = (char *) (teamMember.team_shmem().get_shmem(shared_memory_size));

    //nnz_lno_t *alloc_global_memory = NULL;
    nnz_lno_t *globally_used_hash_indices = NULL;

    //shift it to the thread private part
    all_shared_memory += thread_memory * teamMember.team_rank();

    //used_hash_sizes hold the size of 1st and 2nd level hashes
    volatile nnz_lno_t *used_hash_sizes = (volatile nnz_lno_t *) (all_shared_memory);
    all_shared_memory += sizeof(nnz_lno_t) * 2;

    nnz_lno_t *globally_used_hash_count = (nnz_lno_t *) (all_shared_memory);

    all_shared_memory += sizeof(nnz_lno_t) ;
    //int unit_memory = sizeof(nnz_lno_t) * 2 + sizeof(nnz_lno_t) * 2;
    //nnz_lno_t shmem_key_size = (thread_memory - sizeof(nnz_lno_t) * 3) / unit_memory;

    nnz_lno_t * begins = (nnz_lno_t *) (all_shared_memory);
    all_shared_memory += sizeof(nnz_lno_t) * shmem_hash_size;

    //poins to the next elements
    nnz_lno_t * nexts = (nnz_lno_t *) (all_shared_memory);
    all_shared_memory += sizeof(nnz_lno_t) * shmem_key_size;

    //holds the keys
    nnz_lno_t * keys = (nnz_lno_t *) (all_shared_memory);
    all_shared_memory += sizeof(nnz_lno_t) * shmem_key_size;
    nnz_lno_t * vals = (nnz_lno_t *) (all_shared_memory);

    //printf("begins:%ld, nexts:%ld, keys:%ld, vals:%ld\n", begins, nexts, keys, vals);
    //return;
    //first level hashmap
    KokkosKernels::Experimental::HashmapAccumulator<nnz_lno_t,nnz_lno_t,nnz_lno_t>
      hm(shmem_hash_size, shmem_key_size, begins, nexts, keys, vals);

    KokkosKernels::Experimental::HashmapAccumulator<nnz_lno_t,nnz_lno_t,nnz_lno_t> hm2;

    //initialize begins.
    Kokkos::parallel_for(
        Kokkos::ThreadVectorRange(teamMember, shmem_hash_size),
        [&] (int i) {
      begins[i] = -1;
    });

    //initialize hash usage sizes
    Kokkos::single(Kokkos::PerThread(teamMember),[&] () {
      used_hash_sizes[0] = 0;
      used_hash_sizes[1] = 0;
      globally_used_hash_count[0] = 0;
    });

    bool is_global_alloced = false;

    const size_type col_end = row_mapA[row_index + 1];
    const size_type col_begin = row_mapA[row_index];
    const nnz_lno_t col_size = col_end - col_begin;

    for (nnz_lno_t colind = 0; colind < col_size; ++colind){
      size_type a_col = colind + col_begin;

      nnz_lno_t rowB = entriesA[a_col];
      size_type rowBegin = row_pointer_begins_B(rowB);

      nnz_lno_t left_work = row_pointer_ends_B(rowB) - rowBegin;

      while (left_work){
        nnz_lno_t work_to_handle = KOKKOSKERNELS_MACRO_MIN(vector_size, left_work);

        nnz_lno_t b_set_ind = -1, b_set = -1;
        nnz_lno_t hash = -1;
        Kokkos::parallel_for(
            Kokkos::ThreadVectorRange(teamMember, work_to_handle),
            [&] (nnz_lno_t i) {
          const size_type adjind = i + rowBegin;
          b_set_ind = entriesSetIndicesB[adjind];
          b_set = entriesSetsB[adjind];
          //hash = b_set_ind % shmem_key_size;
          hash = b_set_ind & shared_memory_hash_func;
        });


        int num_unsuccess = hm.vector_atomic_insert_into_hash_mergeOr(
            teamMember, vector_size,
            hash, b_set_ind, b_set,
            used_hash_sizes,
            shmem_key_size);


        int overall_num_unsuccess = 0;

        Kokkos::parallel_reduce( Kokkos::ThreadVectorRange(teamMember, vector_size),
            [&] (const int threadid, int &overall_num_unsuccess_) {
          overall_num_unsuccess_ += num_unsuccess;
        }, overall_num_unsuccess);


        if (overall_num_unsuccess){

          //printf("row:%d\n", row_index);
          if (!is_global_alloced){
            volatile nnz_lno_t * tmp = NULL;
            size_t tid = get_thread_id(row_index);
            while (tmp == NULL){
              Kokkos::single(Kokkos::PerThread(teamMember),[&] (volatile nnz_lno_t * &memptr) {
                memptr = (volatile nnz_lno_t * )( m_space.allocate_chunk(tid));
              }, tmp);
            }
            is_global_alloced = true;

            globally_used_hash_indices = (nnz_lno_t *) tmp;
            tmp += pow2_hash_size ;

            hm2.hash_begins = (nnz_lno_t *) (tmp);
            tmp += pow2_hash_size ;

            //poins to the next elements
            hm2.hash_nexts = (nnz_lno_t *) (tmp);
            tmp += MaxRoughNonZero;

            //holds the keys
            hm2.keys = (nnz_lno_t *) (tmp);
            tmp += MaxRoughNonZero;
            hm2.values = (nnz_lno_t *) (tmp);

            hm2.hash_key_size = pow2_hash_size;
            hm2.max_value_size = MaxRoughNonZero;
          }

          nnz_lno_t hash_ = -1;
          if (num_unsuccess) hash_ = b_set_ind & pow2_hash_func;

          //int insertion =
          hm2.vector_atomic_insert_into_hash_mergeOr_TrackHashes(
              teamMember, vector_size,
              hash_,b_set_ind,b_set,
              used_hash_sizes + 1, hm2.max_value_size
              ,globally_used_hash_count, globally_used_hash_indices
              );

        }
        left_work -= work_to_handle;
        rowBegin += work_to_handle;
      }
    }

    Kokkos::single(Kokkos::PerThread(teamMember),[&] () {
      if (used_hash_sizes[0] > shmem_key_size) used_hash_sizes[0] = shmem_key_size;
    });

    /*
    Kokkos::single(Kokkos::PerThread(teamMember),[&] () {
      if (used_hash_sizes[1] > hm2.max_value_size) used_hash_sizes[1] = hm2.max_value_size;
    });
    */

    nnz_lno_t num_elements = 0;

    nnz_lno_t num_compressed_elements = used_hash_sizes[0];

    Kokkos::parallel_reduce( Kokkos::ThreadVectorRange(teamMember, num_compressed_elements),
        [&] (const nnz_lno_t ii, nnz_lno_t &num_nnz_in_row) {
      nnz_lno_t c_rows = hm.values[ii];
      nnz_lno_t num_el = 0;
      for (; c_rows; num_el++) {
        c_rows &= c_rows - 1; // clear the least significant bit set
      }
      num_nnz_in_row += num_el;
    }, num_elements);


    if (is_global_alloced){
      nnz_lno_t num_global_elements = 0;
      nnz_lno_t num_compressed_elements_ = used_hash_sizes[1];
      Kokkos::parallel_reduce( Kokkos::ThreadVectorRange(teamMember, num_compressed_elements_),
          [&] (const nnz_lno_t ii, nnz_lno_t &num_nnz_in_row) {
        nnz_lno_t c_rows = hm2.values[ii];
        nnz_lno_t num_el = 0;
        for (; c_rows; num_el++) {
          c_rows &= c_rows - 1; // clear the least significant bit set
        }
        num_nnz_in_row += num_el;
      }, num_global_elements);


      //now thread leaves the memory as it finds. so there is no need to initialize the hash begins
      nnz_lno_t dirty_hashes = globally_used_hash_count[0];
      Kokkos::parallel_for(
          Kokkos::ThreadVectorRange(teamMember, dirty_hashes),
          [&] (nnz_lno_t i) {
        nnz_lno_t dirty_hash = globally_used_hash_indices[i];
        hm2.hash_begins[dirty_hash] = -1;
      });


      Kokkos::single(Kokkos::PerThread(teamMember),[&] () {
        m_space.release_chunk(globally_used_hash_indices);
      });
      num_elements += num_global_elements;
    }

    rowmapC(row_index) = num_elements;
  }

  size_t team_shmem_size (int team_size) const {
    return shared_memory_size;
  }

};



template <typename HandleType,
typename a_row_view_t_, typename a_lno_nnz_view_t_, typename a_scalar_nnz_view_t_,
typename b_lno_row_view_t_, typename b_lno_nnz_view_t_, typename b_scalar_nnz_view_t_  >
template <typename a_row_view_t, typename a_nnz_view_t,
          typename b_oldrow_view_t, typename b_row_view_t>
struct KokkosSPGEMM
  <HandleType, a_row_view_t_, a_lno_nnz_view_t_, a_scalar_nnz_view_t_,
    b_lno_row_view_t_, b_lno_nnz_view_t_, b_scalar_nnz_view_t_>::
  PredicMaxRowNNZ{
  nnz_lno_t m; //num rows
  a_row_view_t row_mapA;  //row pointers of a
  a_nnz_view_t entriesA;  //col
  b_oldrow_view_t row_begins_B;
  b_row_view_t row_end_indices_B;
  const size_type min_val;
  nnz_lno_t team_row_chunk_size;

  /**
   * \brief Constructor
   * \param m_: num rows in A.
   * \param row_mapA_: row pointers of A
   * \param entriesA_: col indices of A
   * \param row_begins_B_: row begin indices of B
   * \param row_end_indices_B_: row end indices of B
   * \param team_row_chunk_size_: the number of rows assigned to each team.
   */
  PredicMaxRowNNZ(
      nnz_lno_t m_,
      a_row_view_t row_mapA_,
      a_nnz_view_t entriesA_,

      b_oldrow_view_t row_begins_B_,
      b_row_view_t row_end_indices_B_,
      nnz_lno_t team_row_chunk_size_):
        m(m_),
        row_mapA(row_mapA_), entriesA(entriesA_),
        row_begins_B(row_begins_B_),
        row_end_indices_B(row_end_indices_B_),
        min_val(((std::numeric_limits<size_type>::lowest()))),
        team_row_chunk_size(team_row_chunk_size_){}

  KOKKOS_INLINE_FUNCTION
  void operator()(const team_member_t & teamMember, size_type &overal_max) const {
    //get the range of rows for team.
    const nnz_lno_t team_row_begin = teamMember.league_rank() * team_row_chunk_size;
    const nnz_lno_t team_row_end = KOKKOSKERNELS_MACRO_MIN(team_row_begin + team_row_chunk_size, m);

    Kokkos::parallel_for(Kokkos::TeamThreadRange(teamMember, team_row_begin, team_row_end), [&] (const nnz_lno_t& row_index)
    {

      const size_type col_begin = row_mapA[row_index];
      const size_type col_end = row_mapA[row_index + 1];
      const nnz_lno_t left_work = col_end - col_begin;

      size_type max_num_results_in_row = 0;

      //get the size of the rows of B, pointed by row of A
      Kokkos::parallel_reduce(
          Kokkos::ThreadVectorRange(teamMember, left_work),
          [&] (nnz_lno_t i, size_type & valueToUpdate) {
        const size_type adjind = i + col_begin;
        const nnz_lno_t colIndex = entriesA[adjind];
        valueToUpdate += row_end_indices_B (colIndex) - row_begins_B(colIndex);
      },
      max_num_results_in_row);
      //set max.
      if (overal_max < max_num_results_in_row) {
        overal_max = max_num_results_in_row;
      }
    });
  }

  KOKKOS_INLINE_FUNCTION
  void join (volatile size_type& dst,const volatile size_type& src) const {
    if (dst < src) { dst = src;}
  }


  KOKKOS_INLINE_FUNCTION
  void init (size_type& dst) const
  {
    dst = min_val;
  }
};

template <typename HandleType,
typename a_row_view_t_, typename a_lno_nnz_view_t_, typename a_scalar_nnz_view_t_,
typename b_lno_row_view_t_, typename b_lno_nnz_view_t_, typename b_scalar_nnz_view_t_  >
struct KokkosSPGEMM
  <HandleType, a_row_view_t_, a_lno_nnz_view_t_, a_scalar_nnz_view_t_,
    b_lno_row_view_t_, b_lno_nnz_view_t_, b_scalar_nnz_view_t_>::
  PredicMaxRowNNZIntersection{
  const nnz_lno_t m,k; //num rows
  const size_type * row_mapA;  //row pointers of a
  const nnz_lno_t * entriesA;  //col
  const size_type * row_begins_B;
  const size_type * row_end_indices_B;
  const size_type min_val;
  const nnz_lno_t team_row_chunk_size;
  nnz_lno_t * min_result_row_for_each_row;

  /**
   * \brief Constructor
   * \param m_: num rows in A.
   * \param row_mapA_: row pointers of A
   * \param entriesA_: col indices of A
   * \param row_begins_B_: row begin indices of B
   * \param row_end_indices_B_: row end indices of B
   * \param team_row_chunk_size_: the number of rows assigned to each team.
   */
  PredicMaxRowNNZIntersection(
      const nnz_lno_t m_, const nnz_lno_t k_,
      const size_type * row_mapA_,
      const nnz_lno_t * entriesA_,

      const size_type * row_begins_B_,
      const size_type * row_end_indices_B_,
      const nnz_lno_t team_row_chunk_size_,
      nnz_lno_t * min_result_row_for_each_row_):
        m(m_), k(k_),
        row_mapA(row_mapA_), entriesA(entriesA_),
        row_begins_B(row_begins_B_),
        row_end_indices_B(row_end_indices_B_),
        min_val(((std::numeric_limits<size_type>::lowest()))),
        team_row_chunk_size(team_row_chunk_size_),
        min_result_row_for_each_row(min_result_row_for_each_row_){}

  KOKKOS_INLINE_FUNCTION
  void operator()(const team_member_t & teamMember, nnz_lno_t &overal_max) const {
    //get the range of rows for team.
    const nnz_lno_t team_row_begin = teamMember.league_rank() * team_row_chunk_size;
    const nnz_lno_t team_row_end = KOKKOSKERNELS_MACRO_MIN(team_row_begin + team_row_chunk_size, m);

    Kokkos::parallel_for(Kokkos::TeamThreadRange(teamMember, team_row_begin, team_row_end), [&] (const nnz_lno_t& row_index)
    {

      const size_type col_begin = row_mapA[row_index];
      const size_type col_end = row_mapA[row_index + 1];
      const nnz_lno_t left_work = col_end - col_begin;
      nnz_lno_t min_num_result_row = -1;
      if (left_work){
        nnz_lno_t min_num_results_in_row = this->k;
        for (nnz_lno_t i = 0; i< left_work; ++i){

          const size_type adjind = i + col_begin;
          const nnz_lno_t colIndex = entriesA[adjind];
          nnz_lno_t rowsize = row_end_indices_B [colIndex] - row_begins_B[colIndex];
          if (min_num_results_in_row > rowsize){
            min_num_results_in_row = rowsize;
            min_num_result_row = colIndex;
          }
          //max_num_results_in_row = KOKKOSKERNELS_MACRO_MIN(max_num_results_in_row, rowsize);

        }
        /*
      //get the size of the rows of B, pointed by row of A
      Kokkos::parallel_reduce(
          Kokkos::ThreadVectorRange(teamMember, left_work),
          [&] (nnz_lno_t i, nnz_lno_t & valueToUpdate) {
        const size_type adjind = i + col_begin;
        const nnz_lno_t colIndex = entriesA[adjind];
        nnz_lno_t rowsize = row_end_indices_B (colIndex) - row_begins_B(colIndex);
        std::cout << "adjind:" << adjind << " valueToUpdate:" << valueToUpdate << " rowsize:" << rowsize << std::endl;
        valueToUpdate = KOKKOSKERNELS_MACRO_MIN(valueToUpdate, rowsize);
      },
        [&] (nnz_lno_t& val, const nnz_lno_t& src)
        {std::cout << "val:" << val << " src:" << src << std::endl;

        val = KOKKOSKERNELS_MACRO_MIN(val, src);},
      max_num_results_in_row);
         */
        //set max.
        if (overal_max < min_num_results_in_row) {
          overal_max = min_num_results_in_row;
        }
      }
      min_result_row_for_each_row[row_index] = min_num_result_row;
    });
  }

  KOKKOS_INLINE_FUNCTION
  void join (volatile size_type& dst,const volatile size_type& src) const {
    if (dst < src) { dst = src;}
  }


  KOKKOS_INLINE_FUNCTION
  void init (size_type& dst) const
  {
    dst = min_val;
  }
};

template <typename HandleType,
typename a_row_view_t_, typename a_lno_nnz_view_t_, typename a_scalar_nnz_view_t_,
typename b_lno_row_view_t_, typename b_lno_nnz_view_t_, typename b_scalar_nnz_view_t_  >
template <typename a_r_view_t, typename a_nnz_view_t,
            typename b_original_row_view_t,
            typename b_compressed_row_view_t, typename b_nnz_view_t,
            typename c_row_view_t>
void KokkosSPGEMM
  <HandleType,
      a_row_view_t_, a_lno_nnz_view_t_, a_scalar_nnz_view_t_,
      b_lno_row_view_t_, b_lno_nnz_view_t_, b_scalar_nnz_view_t_>::
  symbolic_c(
    nnz_lno_t m,
    a_r_view_t row_mapA_,
    a_nnz_view_t entriesA_,

    b_original_row_view_t old_row_mapB,
    b_compressed_row_view_t row_mapB_,
    b_nnz_view_t entriesSetIndex,
    b_nnz_view_t entriesSets,

    c_row_view_t rowmapC,
    nnz_lno_t maxNumRoughNonzeros
){
  typedef KokkosKernels::Impl::UniformMemoryPool< MyTempMemorySpace, nnz_lno_t> pool_memory_space;

  //get the number of rows and nonzeroes of B.
  nnz_lno_t brows = row_mapB_.dimension_0() - 1;
  size_type bnnz =  entriesSetIndex.dimension_0();

  //get the SPGEMMAlgorithm to run.
  //SPGEMMAlgorithm spgemm_algorithm = this->handle->get_spgemm_handle()->get_algorithm_type();

  KokkosKernels::Impl::ExecSpaceType my_exec_space = this->handle->get_handle_exec_space();
  size_type compressed_b_size = bnnz;
#ifdef KOKKOSKERNELS_ANALYZE_COMPRESSION
  //TODO: DELETE BELOW
  {
std::cout << "\t\t!!!!DELETE THIS PART!!!! PRINTING STATS HERE!!!!!" << std::endl;
      KokkosKernels::Impl::kk_reduce_diff_view <b_original_row_view_t, b_compressed_row_view_t, MyExecSpace>
  (brows, old_row_mapB, row_mapB_, compressed_b_size);
      std::cout << "\tcompressed_b_size:" << compressed_b_size << " bnnz:" << bnnz << std::endl;
      std::cout << "Given compressed maxNumRoughNonzeros:" << maxNumRoughNonzeros << std::endl;
      nnz_lno_t r_maxNumRoughZeros = this->getMaxRoughRowNNZ(a_row_cnt, row_mapA, entriesA, old_row_mapB,row_mapB_ );
      std::cout << "compressed r_maxNumRoughZeros:" << r_maxNumRoughZeros << std::endl;

      size_t compressed_flops = 0;
      size_t original_flops = 0;
      size_t compressd_max_flops= 0;
      size_t original_max_flops = 0;
      for (int i = 0; i < a_row_cnt; ++i){
  int arb = row_mapA(i);
        int are = row_mapA(i + 1);
        size_t compressed_row_flops = 0;
  size_t original_row_flops = 0;
  for (int j = arb; j < are; ++j){
          int ae = entriesA(j);
          compressed_row_flops += row_mapB_(ae) - old_row_mapB(ae);
    original_row_flops += old_row_mapB(ae + 1) - old_row_mapB(ae);
        }
        if (compressed_row_flops > compressd_max_flops) compressd_max_flops = compressed_row_flops;
        if (original_row_flops > original_max_flops) original_max_flops = original_row_flops;
        compressed_flops += compressed_row_flops;
        original_flops += original_row_flops;
      }
std::cout   << "original_flops:" << original_flops
    << " compressed_flops:" << compressed_flops
    << " FLOP_REDUCTION:" << double(compressed_flops) / original_flops
    << std::endl;
std::cout   << "original_max_flops:" << original_max_flops
    << " compressd_max_flops:" << compressd_max_flops
    << " MEM_REDUCTION:" << double(compressd_max_flops) / original_max_flops * 2
    << std::endl;
      std::cout   << "\tOriginal_B_SIZE:" << bnnz
    << " Compressed_b_size:" << compressed_b_size
    << std::endl;
std::cout << " AR AC ANNZ BR BC BNNZ original_flops compressed_flops FLOP_REDUCTION original_max_flops compressd_max_flops MEM_REDUCTION riginal_B_SIZE Compressed_b_size B_SIZE_REDUCTION" <<  std::endl;
std::cout << " " << a_row_cnt << " " << b_row_cnt << " " << entriesA.dimension_0() << " " << b_row_cnt << " " << b_col_cnt << " " << entriesB.dimension_0() << " " <<  original_flops << " " << compressed_flops << " " << double(compressed_flops) / original_flops << " " << original_max_flops << " " << compressd_max_flops << " " << double(compressd_max_flops) / original_max_flops * 2 << " " << bnnz << " " << compressed_b_size <<" "<< double(compressed_b_size) / bnnz  << std::endl;
  }
  //TODO DELETE ABOVE
#endif
  if (my_exec_space == KokkosKernels::Impl::Exec_CUDA){
KokkosKernels::Impl::kk_reduce_diff_view <b_original_row_view_t, b_compressed_row_view_t, MyExecSpace> (brows, old_row_mapB, row_mapB_, compressed_b_size);
      if (KOKKOSKERNELS_VERBOSE){
  std::cout << "\tcompressed_b_size:" << compressed_b_size << " bnnz:" << bnnz << std::endl;
}
  }
  int suggested_vector_size = this->handle->get_suggested_vector_size(brows, compressed_b_size);

  //this kernel does not really work well if the vector size is less than 4.
  if (suggested_vector_size < 4 && my_exec_space == KokkosKernels::Impl::Exec_CUDA){
      if (KOKKOSKERNELS_VERBOSE){
        std::cout << "\tsuggested_vector_size:" << suggested_vector_size << " setting it to 4 for Structure kernel" << std::endl;
      }
      suggested_vector_size = 4;
  }
  int suggested_team_size = this->handle->get_suggested_team_size(suggested_vector_size);
  nnz_lno_t team_row_chunk_size = this->handle->get_team_work_size(suggested_team_size,concurrency, a_row_cnt);

  //round up maxNumRoughNonzeros to closest power of 2.
  nnz_lno_t min_hash_size = 1;
  while (maxNumRoughNonzeros > min_hash_size){
    min_hash_size *= 2;
  }

  //set the chunksize.
  size_t chunksize = 1;
  //initizalize value for the mem pool
  int pool_init_val = -1;
  if (this->spgemm_algorithm == SPGEMM_KK_TRACKED_CUCKOO || this->spgemm_algorithm == SPGEMM_KK_TRACKED_CUCKOO_F ){
	  chunksize = min_hash_size ; //this is for keys
	  chunksize += min_hash_size ; //this is for the values
	  chunksize += maxNumRoughNonzeros ; //this is for hash values
  }
  else if (this->spgemm_algorithm == SPGEMM_KK_CUCKOO){
	  chunksize = min_hash_size ; //this is for keys
	  chunksize += min_hash_size ; //this is for the values
  }
  else {
	  chunksize = min_hash_size ; //this is for used hash indices
	  chunksize += min_hash_size ; //this is for the hash begins
	  chunksize += maxNumRoughNonzeros ; //this is for hash nexts
	  chunksize += maxNumRoughNonzeros ; //this is for hash keys
	  chunksize += maxNumRoughNonzeros ; //this is for hash values
  }
  //if KKSPEED are used on CPU, or KKMEMSPEED is run with threads less than 32
  //than we use dense accumulators.
  if ((   spgemm_algorithm == SPGEMM_KK_MEMSPEED  &&
      concurrency <=  sizeof (nnz_lno_t) * 8 &&
      my_exec_space != KokkosKernels::Impl::Exec_CUDA)
      ||
      (   spgemm_algorithm == SPGEMM_KK_SPEED &&
          my_exec_space != KokkosKernels::Impl::Exec_CUDA)){

    nnz_lno_t col_size = this->b_col_cnt / (sizeof (nnz_lno_t) * 8)+ 1;

    nnz_lno_t max_row_size = KOKKOSKERNELS_MACRO_MIN(col_size, maxNumRoughNonzeros);
    chunksize = col_size + max_row_size;
    //if speed is set, and exec space is cpu, then  we use dense accumulators.
    //or if memspeed is set, and concurrency is not high, we use dense accumulators.
    maxNumRoughNonzeros = col_size;
    pool_init_val = 0;
    if (KOKKOSKERNELS_VERBOSE){
      std::cout << "\tDense Acc - COLS:" << col_size << " max_row_size:" << max_row_size << std::endl;
    }
  }


  nnz_lno_t num_chunks = concurrency / suggested_vector_size;

  KokkosKernels::Impl::PoolType my_pool_type = KokkosKernels::Impl::OneThread2OneChunk;
  if (my_exec_space == KokkosKernels::Impl::Exec_CUDA) {
    my_pool_type = KokkosKernels::Impl::ManyThread2OneChunk;
  }


#if defined( KOKKOS_ENABLE_CUDA )
  if (my_exec_space == KokkosKernels::Impl::Exec_CUDA) {

    size_t free_byte ;
    size_t total_byte ;
    cudaMemGetInfo( &free_byte, &total_byte ) ;
    size_t required_size = size_t (num_chunks) * chunksize * sizeof(nnz_lno_t);
    if (KOKKOSKERNELS_VERBOSE)
      std::cout << "\tmempool required size:" << required_size << " free_byte:" << free_byte << " total_byte:" << total_byte << std::endl;
    if (required_size + num_chunks > free_byte){
      num_chunks = ((((free_byte - num_chunks)* 0.5) /8 ) * 8) / sizeof(nnz_lno_t) / chunksize;
    }
    {
      nnz_lno_t min_chunk_size = 1;
      while (min_chunk_size * 2 <= num_chunks) {
        min_chunk_size *= 2;
      }
      num_chunks = min_chunk_size;
    }
  }
#endif
  if (KOKKOSKERNELS_VERBOSE){
    std::cout << "\tPool Size (MB):" << (num_chunks * chunksize * sizeof(nnz_lno_t)) / 1024. / 1024. << " num_chunks:" << num_chunks << " chunksize:" << chunksize << std::endl;
  }
  Kokkos::Impl::Timer timer1;
  pool_memory_space m_space(num_chunks, chunksize, pool_init_val,  my_pool_type);
  MyExecSpace::fence();

  if (KOKKOSKERNELS_VERBOSE){
    std::cout << "\tPool Alloc Time:" << timer1.seconds() << std::endl;
  }

  StructureC<a_r_view_t, a_nnz_view_t,
  b_original_row_view_t, b_compressed_row_view_t, b_nnz_view_t,
  c_row_view_t, /* nnz_lno_temp_work_view_t,*/ pool_memory_space>
  sc(
      m,
      row_mapA_,
      entriesA_,
      old_row_mapB,
      row_mapB_,
      entriesSetIndex,
      entriesSets,
      rowmapC,
      min_hash_size,
      maxNumRoughNonzeros,
      shmem_size,
      suggested_team_size,
      team_row_chunk_size,
      suggested_vector_size,
      m_space,
      my_exec_space,KOKKOSKERNELS_VERBOSE
   );

  if (KOKKOSKERNELS_VERBOSE){
    std::cout << "\tStructureC vector_size:" << suggested_vector_size
        << " team_size:" << suggested_team_size
        << " chunk_size:" << team_row_chunk_size
        << " shmem_size:" << shmem_size << std::endl;
  }

  timer1.reset();

  if (my_exec_space == KokkosKernels::Impl::Exec_CUDA) {
    Kokkos::parallel_for( gpu_team_policy_t(m / suggested_team_size + 1 , suggested_team_size, suggested_vector_size), sc);
  }
  else {
	  if (( spgemm_algorithm == SPGEMM_KK_MEMSPEED  &&
			  concurrency <=  sizeof (nnz_lno_t) * 8)  ||
			  spgemm_algorithm == SPGEMM_KK_SPEED){

		  if (use_dynamic_schedule){
			  Kokkos::parallel_for( dynamic_multicore_dense_team_count_policy_t(m / team_row_chunk_size + 1 , suggested_team_size, suggested_vector_size), sc);
		  }
		  else {
			  Kokkos::parallel_for( multicore_dense_team_count_policy_t(m / team_row_chunk_size + 1 , suggested_team_size, suggested_vector_size), sc);
		  }
	  }
	  else if (this->spgemm_algorithm == SPGEMM_KK_TRACKED_CUCKOO){
		  if (use_dynamic_schedule){
			  Kokkos::parallel_for( dynamic_multicore_team_policy4_t(m / team_row_chunk_size + 1 , suggested_team_size, suggested_vector_size), sc);
		  }
		  else {

			  Kokkos::parallel_for( multicore_team_policy4_t(m / team_row_chunk_size + 1 , suggested_team_size, suggested_vector_size), sc);
		  }
	  }
	  else if (this->spgemm_algorithm == SPGEMM_KK_TRACKED_CUCKOO_F){
		  if (use_dynamic_schedule){
			  Kokkos::parallel_for( dynamic_multicore_team_policy5_t(m / team_row_chunk_size + 1 , suggested_team_size, suggested_vector_size), sc);
		  }
		  else {

			  Kokkos::parallel_for( multicore_team_policy5_t(m / team_row_chunk_size + 1 , suggested_team_size, suggested_vector_size), sc);
		  }
	  }
	  else if (this->spgemm_algorithm == SPGEMM_KK_CUCKOO){
		  if (use_dynamic_schedule){
			  Kokkos::parallel_for( dynamic_multicore_team_policy3_t(m / team_row_chunk_size + 1 , suggested_team_size, suggested_vector_size), sc);
		  }
		  else {

			  Kokkos::parallel_for( multicore_team_policy3_t(m / team_row_chunk_size + 1 , suggested_team_size, suggested_vector_size), sc);
		  }
	  }
	  else {
		  if (use_dynamic_schedule){
			  Kokkos::parallel_for( dynamic_multicore_team_policy_t(m / team_row_chunk_size + 1 , suggested_team_size, suggested_vector_size), sc);
		  }
		  else {
			  Kokkos::parallel_for( multicore_team_policy_t(m / team_row_chunk_size + 1 , suggested_team_size, suggested_vector_size), sc);
		  }
	  }
  }
  MyExecSpace::fence();

  if (KOKKOSKERNELS_VERBOSE){
    std::cout << "\tStructureC Kernel time:" << timer1.seconds() << std::endl<< std::endl;
  }
  //if we need to find the max nnz in a row.
  {
    Kokkos::Impl::Timer timer1_;
    size_type c_max_nnz = 0;
    KokkosKernels::Impl::view_reduce_max<c_row_view_t, MyExecSpace>(m, rowmapC, c_max_nnz);
    MyExecSpace::fence();
    this->handle->get_spgemm_handle()->set_max_result_nnz(c_max_nnz);

    if (KOKKOSKERNELS_VERBOSE){
      std::cout << "\tReduce Max Row Size Time:" << timer1_.seconds() << std::endl;
    }
  }

  KokkosKernels::Impl::kk_exclusive_parallel_prefix_sum<c_row_view_t, MyExecSpace>(m+1, rowmapC);
  MyExecSpace::fence();


  auto d_c_nnz_size = Kokkos::subview(rowmapC, m);
  auto h_c_nnz_size = Kokkos::create_mirror_view (d_c_nnz_size);
  Kokkos::deep_copy (h_c_nnz_size, d_c_nnz_size);
  typename c_row_view_t::non_const_value_type c_nnz_size = h_c_nnz_size();
  this->handle->get_spgemm_handle()->set_c_nnz(c_nnz_size);


  if (spgemm_algorithm == SPGEMM_KK_COLOR ||
      spgemm_algorithm == SPGEMM_KK_MULTICOLOR ||
      spgemm_algorithm == SPGEMM_KK_MULTICOLOR2){

    if (KOKKOSKERNELS_VERBOSE){
      std::cout << "\tCOLORING PHASE"<<  std::endl;
    }

    nnz_lno_temp_work_view_t entryIndicesC_; //(Kokkos::ViewAllocateWithoutInitializing("entryIndicesC_"), c_nnz_size);

  timer1.reset();
  entryIndicesC_ = nnz_lno_temp_work_view_t (Kokkos::ViewAllocateWithoutInitializing("entryIndicesC_"), c_nnz_size);
  //calculate the structure.
  NonzeroesC<
  a_r_view_t, a_nnz_view_t,
  b_original_row_view_t, b_compressed_row_view_t, b_nnz_view_t,
  c_row_view_t, nnz_lno_temp_work_view_t,
  pool_memory_space>
  nnzc_( m,
      row_mapA_,
      entriesA_,
      old_row_mapB,
      row_mapB_,
      entriesSetIndex,
      entriesSets,
      rowmapC,
      entryIndicesC_,
      min_hash_size,
      maxNumRoughNonzeros,
      shmem_size,suggested_vector_size,m_space,
      my_exec_space);

  if (my_exec_space == KokkosKernels::Impl::Exec_CUDA) {
    Kokkos::parallel_for( gpu_team_policy_t(m / suggested_team_size + 1 , suggested_team_size, suggested_vector_size), nnzc_);
  }
  else {
    if (use_dynamic_schedule){
      Kokkos::parallel_for( dynamic_multicore_team_policy_t(m / suggested_team_size + 1 , suggested_team_size, suggested_vector_size), nnzc_);
    }
    else {
      Kokkos::parallel_for( multicore_team_policy_t(m / suggested_team_size + 1 , suggested_team_size, suggested_vector_size), nnzc_);
    }
  }

  MyExecSpace::fence();


    if (KOKKOSKERNELS_VERBOSE){
      std::cout << "\t\tCOLORING-NNZ-FILL-TIME:" << timer1.seconds() <<  std::endl;
    }

    nnz_lno_t original_num_colors, num_colors_in_one_step, num_multi_color_steps;
    nnz_lno_persistent_work_host_view_t h_color_xadj;
    nnz_lno_persistent_work_view_t color_adj, vertex_colors_to_store;

    //distance-2 color
    this->d2_color_c_matrix(
        rowmapC, entryIndicesC_,
        original_num_colors, h_color_xadj, color_adj , vertex_colors_to_store,
        num_colors_in_one_step, num_multi_color_steps, spgemm_algorithm);

    std::cout << "original_num_colors:" << original_num_colors << " num_colors_in_one_step:" << num_colors_in_one_step << " num_multi_color_steps:" << num_multi_color_steps << std::endl;
    timer1.reset();

    //sort the color indices.
    for (nnz_lno_t i = 0; i < num_multi_color_steps; ++i){
      //sort the ones that have more than 32 rows.
      if (h_color_xadj(i+1) - h_color_xadj(i) <= 32) continue;
      auto sv = Kokkos::subview(color_adj,Kokkos::pair<nnz_lno_t, nnz_lno_t> (h_color_xadj(i), h_color_xadj(i+1)));
      //KokkosKernels::Impl::print_1Dview(sv, i ==47);
      //TODO for some reason kokkos::sort is failing on views with size 56 and 112.
      //for now we use std::sort. Delete below comment, and delete the code upto fence.
      //Kokkos::sort(sv);
      //
      auto h_sv = Kokkos::create_mirror_view (sv);
      Kokkos::deep_copy(h_sv,sv);
      auto* p_sv = h_sv.ptr_on_device();
      std::sort (p_sv, p_sv + h_color_xadj(i+1) - h_color_xadj(i));
      Kokkos::deep_copy(sv,h_sv);
      MyExecSpace::fence();
    }

    if (KOKKOSKERNELS_VERBOSE){
      std::cout << "\t\tCOLOR-SORT-TIME:" << timer1.seconds() <<  std::endl;
    }
    this->handle->get_spgemm_handle()->set_color_xadj(
        original_num_colors,
        h_color_xadj, color_adj, vertex_colors_to_store,
        num_colors_in_one_step, num_multi_color_steps);
    this->handle->get_spgemm_handle()->set_c_column_indices(entryIndicesC_);
  }

}


template <typename HandleType,
typename a_row_view_t_, typename a_lno_nnz_view_t_, typename a_scalar_nnz_view_t_,
typename b_lno_row_view_t_, typename b_lno_nnz_view_t_, typename b_scalar_nnz_view_t_  >
template <typename a_r_view_t, typename a_n_view_t, typename b_oldrow_view_t, typename b_r_view_t>

size_t KokkosSPGEMM
  <HandleType,
      a_row_view_t_, a_lno_nnz_view_t_, a_scalar_nnz_view_t_,
      b_lno_row_view_t_, b_lno_nnz_view_t_, b_scalar_nnz_view_t_>::
    getMaxRoughRowNNZ(
        nnz_lno_t m,
    a_r_view_t row_mapA_,
    a_n_view_t entriesA_,

    b_oldrow_view_t row_pointers_begin_B,
    b_r_view_t row_pointers_end_B)
    {

  //get the execution space type.
  //KokkosKernels::Impl::ExecSpaceType my_exec_space = this->handle->get_handle_exec_space();
  int suggested_vector_size = this->handle->get_suggested_vector_size(m, entriesA_.dimension_0());
  int suggested_team_size = this->handle->get_suggested_team_size(suggested_vector_size);
  nnz_lno_t team_row_chunk_size = this->handle->get_team_work_size(suggested_team_size, this->concurrency , m);

  PredicMaxRowNNZ<a_r_view_t, a_n_view_t, b_oldrow_view_t, b_r_view_t>
  pcnnnz(
      m,
      row_mapA_,
      entriesA_,
      row_pointers_begin_B,
      row_pointers_end_B,
      team_row_chunk_size );


  typename b_oldrow_view_t::non_const_value_type rough_size = 0;
  Kokkos::parallel_reduce( team_policy_t(m / team_row_chunk_size  + 1 , suggested_team_size, suggested_vector_size), pcnnnz, rough_size);
  MyExecSpace::fence();
  return rough_size;
    }


template <typename HandleType,
typename a_row_view_t_, typename a_lno_nnz_view_t_, typename a_scalar_nnz_view_t_,
typename b_lno_row_view_t_, typename b_lno_nnz_view_t_, typename b_scalar_nnz_view_t_  >

struct KokkosSPGEMM
  <HandleType, a_row_view_t_, a_lno_nnz_view_t_, a_scalar_nnz_view_t_,
    b_lno_row_view_t_, b_lno_nnz_view_t_, b_scalar_nnz_view_t_>::
  PredicMaxRowNNZ_p{
  const nnz_lno_t m; //num rows
  const size_type * row_mapA;  //row pointers of a
  const nnz_lno_t * entriesA;  //col
  const size_type * row_begins_B;
  const size_type * row_end_indices_B;
  const size_type min_val;
  const nnz_lno_t team_row_chunk_size;

  /**
   * \brief Constructor
   * \param m_: num rows in A.
   * \param row_mapA_: row pointers of A
   * \param entriesA_: col indices of A
   * \param row_begins_B_: row begin indices of B
   * \param row_end_indices_B_: row end indices of B
   * \param team_row_chunk_size_: the number of rows assigned to each team.
   */
  PredicMaxRowNNZ_p(
      const nnz_lno_t m_,
      const size_type * row_mapA_,
      const nnz_lno_t * entriesA_,

      const size_type * row_begins_B_,
      const size_type * row_end_indices_B_,
      nnz_lno_t team_row_chunk_size_):
        m(m_),
        row_mapA(row_mapA_), entriesA(entriesA_),
        row_begins_B(row_begins_B_),
        row_end_indices_B(row_end_indices_B_),
        min_val(((std::numeric_limits<size_type>::lowest()))),
        team_row_chunk_size(team_row_chunk_size_){}

  KOKKOS_INLINE_FUNCTION
  void operator()(const team_member_t & teamMember, size_type &overal_max) const {
    //get the range of rows for team.
    const nnz_lno_t team_row_begin = teamMember.league_rank() * team_row_chunk_size;
    const nnz_lno_t team_row_end = KOKKOSKERNELS_MACRO_MIN(team_row_begin + team_row_chunk_size, m);

    //TODO MD: here do I need a reduce as well?
    Kokkos::parallel_for(Kokkos::TeamThreadRange(teamMember, team_row_begin, team_row_end), [&] (const nnz_lno_t& row_index)
    {

      const size_type col_begin = row_mapA[row_index];
      const size_type col_end = row_mapA[row_index + 1];
      const nnz_lno_t left_work = col_end - col_begin;

      size_type max_num_results_in_row = 0;

      //get the size of the rows of B, pointed by row of A
      Kokkos::parallel_reduce(
          Kokkos::ThreadVectorRange(teamMember, left_work),
          [&] (nnz_lno_t i, size_type & valueToUpdate) {
        const size_type adjind = i + col_begin;
        const nnz_lno_t colIndex = entriesA[adjind];
        valueToUpdate += row_end_indices_B [colIndex] - row_begins_B[colIndex];
      },
      max_num_results_in_row);
      //set max.
      if (overal_max < max_num_results_in_row) {
        overal_max = max_num_results_in_row;
      }
    });
  }

  KOKKOS_INLINE_FUNCTION
  void join (volatile size_type& dst,const volatile size_type& src) const {
    if (dst < src) { dst = src;}
  }


  KOKKOS_INLINE_FUNCTION
  void init (size_type& dst) const
  {
    dst = min_val;
  }
};

template <typename HandleType,
typename a_row_view_t_, typename a_lno_nnz_view_t_, typename a_scalar_nnz_view_t_,
typename b_lno_row_view_t_, typename b_lno_nnz_view_t_, typename b_scalar_nnz_view_t_  >
size_t KokkosSPGEMM
  <HandleType,
      a_row_view_t_, a_lno_nnz_view_t_, a_scalar_nnz_view_t_,
      b_lno_row_view_t_, b_lno_nnz_view_t_, b_scalar_nnz_view_t_>::
      getMaxRoughRowNNZ_p(
          const nnz_lno_t m,const  size_type annz,
          const size_type * row_mapA_,
          const nnz_lno_t * entriesA_,

          const size_type * row_pointers_begin_B,
          const size_type * row_pointers_end_B) {

  int suggested_vector_size = this->handle->get_suggested_vector_size(m, annz);
  int suggested_team_size = this->handle->get_suggested_team_size(suggested_vector_size);
  nnz_lno_t team_row_chunk_size = this->handle->get_team_work_size(suggested_team_size, this->concurrency , m);

  PredicMaxRowNNZ_p
  pcnnnz(
      m,
      row_mapA_,
      entriesA_,
      row_pointers_begin_B,
      row_pointers_end_B,
      team_row_chunk_size );


  size_type rough_size = 0;
  Kokkos::parallel_reduce( team_policy_t(m / team_row_chunk_size  + 1 , suggested_team_size, suggested_vector_size), pcnnnz, rough_size);
  MyExecSpace::fence();
  return rough_size;
}

template <typename HandleType,
typename a_row_view_t_, typename a_lno_nnz_view_t_, typename a_scalar_nnz_view_t_,
typename b_lno_row_view_t_, typename b_lno_nnz_view_t_, typename b_scalar_nnz_view_t_  >
size_t KokkosSPGEMM
  <HandleType,
      a_row_view_t_, a_lno_nnz_view_t_, a_scalar_nnz_view_t_,
      b_lno_row_view_t_, b_lno_nnz_view_t_, b_scalar_nnz_view_t_>::
    getMaxRoughRowNNZIntersection_p(
        const nnz_lno_t m,const  size_type annz,
        const size_type * row_mapA_,
        const nnz_lno_t * entriesA_,

        const size_type * row_pointers_begin_B,
        const size_type * row_pointers_end_B,
        nnz_lno_t * min_result_row_for_each_row
        )
    {

  //get the execution space type.
  //KokkosKernels::Impl::ExecSpaceType my_exec_space = this->handle->get_handle_exec_space();
  int suggested_vector_size = this->handle->get_suggested_vector_size(m, annz);
  int suggested_team_size = this->handle->get_suggested_team_size(suggested_vector_size);
  nnz_lno_t team_row_chunk_size = this->handle->get_team_work_size(suggested_team_size, this->concurrency , m);

  //KokkosKernels::Impl::print_1Dview(row_mapA_);
  //KokkosKernels::Impl::print_1Dview(entriesA_);
  //KokkosKernels::Impl::print_1Dview(row_pointers_begin_B);

  //KokkosKernels::Impl::print_1Dview(row_pointers_end_B);


  PredicMaxRowNNZIntersection
  pcnnnz(
      m, this->b_col_cnt,
      row_mapA_,
      entriesA_,
      row_pointers_begin_B,
      row_pointers_end_B,
      team_row_chunk_size,  min_result_row_for_each_row);


  nnz_lno_t rough_size = 0;
  Kokkos::parallel_reduce( team_policy_t(m / team_row_chunk_size  + 1 , suggested_team_size, suggested_vector_size), pcnnnz, rough_size);
  MyExecSpace::fence();
  return rough_size;
    }


template <typename HandleType,
typename a_row_view_t_, typename a_lno_nnz_view_t_, typename a_scalar_nnz_view_t_,
typename b_lno_row_view_t_, typename b_lno_nnz_view_t_, typename b_scalar_nnz_view_t_  >
template <typename a_row_view_t, typename a_nnz_view_t,
          typename b_original_row_view_t,
          typename b_compressed_row_view_t, typename b_nnz_view_t,
          typename c_row_view_t, typename nnz_lno_temp_work_view_t,
          typename pool_memory_space>
struct KokkosSPGEMM
  <HandleType, a_row_view_t_, a_lno_nnz_view_t_, a_scalar_nnz_view_t_,
    b_lno_row_view_t_, b_lno_nnz_view_t_, b_scalar_nnz_view_t_>::
  NonzeroesC{
  nnz_lno_t numrows;

  a_row_view_t row_mapA;
  a_nnz_view_t entriesA;

  b_original_row_view_t old_row_mapB;
  b_compressed_row_view_t row_mapB;
  b_nnz_view_t entriesSetIndicesB;
  b_nnz_view_t entriesSetsB;

  c_row_view_t rowmapC;
  nnz_lno_temp_work_view_t entriesSetIndicesC;
  nnz_lno_temp_work_view_t entriesSetsC;


  const nnz_lno_t pow2_hash_size;
  const nnz_lno_t pow2_hash_func;
  const nnz_lno_t MaxRoughNonZero;

  const size_t shared_memory_size;
  int vector_size;
  pool_memory_space m_space;
  const KokkosKernels::Impl::ExecSpaceType my_exec_space;

  /**
   * \brief Constructor.
   */

  NonzeroesC(
      nnz_lno_t m_,
      a_row_view_t row_mapA_,
      a_nnz_view_t entriesA_,

      b_original_row_view_t old_row_mapB_,
      b_compressed_row_view_t row_mapB_,
      b_nnz_view_t entriesSetIndicesB_,
      b_nnz_view_t entriesSetsB_,

      c_row_view_t rowmapC_,
      nnz_lno_temp_work_view_t entriesSetIndicesC_,

      const nnz_lno_t hash_size_,
      const nnz_lno_t MaxRoughNonZero_,
      const size_t sharedMemorySize_,
      const int vector_size_,
      pool_memory_space mpool_,
      const KokkosKernels::Impl::ExecSpaceType my_exec_space_):
        numrows(m_),

        row_mapA (row_mapA_),
        entriesA(entriesA_),

        old_row_mapB(old_row_mapB_),
        row_mapB(row_mapB_),
        entriesSetIndicesB(entriesSetIndicesB_),
        entriesSetsB(entriesSetsB_),

        rowmapC(rowmapC_),
        entriesSetIndicesC(entriesSetIndicesC_),
        entriesSetsC(),


        pow2_hash_size(hash_size_),
        pow2_hash_func(hash_size_ - 1),
        MaxRoughNonZero(MaxRoughNonZero_),

        shared_memory_size(sharedMemorySize_),
        vector_size (vector_size_), m_space(mpool_), my_exec_space(my_exec_space_)
        {}

  KOKKOS_INLINE_FUNCTION
  size_t get_thread_id(const size_t row_index) const{
    switch (my_exec_space){
    default:
      return row_index;
#if defined( KOKKOS_HAVE_SERIAL )
    case KokkosKernels::Impl::Exec_SERIAL:
      return 0;
#endif
#if defined( KOKKOS_HAVE_OPENMP )
    case KokkosKernels::Impl::Exec_OMP:
      return Kokkos::OpenMP::hardware_thread_id();
#endif
#if defined( KOKKOS_HAVE_PTHREAD )
    case KokkosKernels::Impl::Exec_PTHREADS:
      return Kokkos::Threads::hardware_thread_id();
#endif
#if defined( KOKKOS_HAVE_QTHREAD)
    case KokkosKernels::Impl::Exec_QTHREADS:
      return Kokkos::Qthread::hardware_thread_id();
#endif
#if defined( KOKKOS_ENABLE_CUDA )
    case KokkosKernels::Impl::Exec_CUDA:
      return row_index;
#endif
    }
  }

  KOKKOS_INLINE_FUNCTION
  void operator()(const MultiCoreTag&, const team_member_t & teamMember) const {

    nnz_lno_t row_index = teamMember.league_rank()  * teamMember.team_size()+ teamMember.team_rank();
    if (row_index >= numrows) return;
    //get row index.

    nnz_lno_t *globally_used_hash_indices = NULL;
    nnz_lno_t globally_used_hash_count = 0;
    nnz_lno_t used_hash_size = 0;
    KokkosKernels::Experimental::HashmapAccumulator<nnz_lno_t,nnz_lno_t,nnz_lno_t> hm2;

    volatile nnz_lno_t * tmp = NULL;
    size_t tid = get_thread_id(row_index);
    while (tmp == NULL){
      tmp = (volatile nnz_lno_t * )( m_space.allocate_chunk(tid));
    }

    globally_used_hash_indices = (nnz_lno_t *) tmp;
    tmp += pow2_hash_size ;

    hm2.hash_begins = (nnz_lno_t *) (tmp);
    tmp += pow2_hash_size ;

    //poins to the next elements
    hm2.hash_nexts = (nnz_lno_t *) (tmp);
    tmp += MaxRoughNonZero;

    //holds the keys
    hm2.keys = (nnz_lno_t *) (tmp);
    tmp += MaxRoughNonZero;
    hm2.values = (nnz_lno_t *) (tmp);

    hm2.hash_key_size = pow2_hash_size;
    hm2.max_value_size = MaxRoughNonZero;


    {
      const size_type col_begin = row_mapA[row_index];
      const nnz_lno_t col_size = row_mapA[row_index + 1] - col_begin;

      for (nnz_lno_t colind = 0; colind < col_size; ++colind){
        size_type a_col = colind + col_begin;

        nnz_lno_t rowB = entriesA[a_col];
        size_type rowBegin = old_row_mapB(rowB);

        nnz_lno_t left_work = row_mapB(rowB ) - rowBegin;

        for (nnz_lno_t i = 0; i < left_work; ++i){

          const size_type adjind = i + rowBegin;
          nnz_lno_t b_set_ind = entriesSetIndicesB[adjind];
          nnz_lno_t b_set = entriesSetsB[adjind];
          nnz_lno_t hash = b_set_ind & pow2_hash_func;

          hm2.sequential_insert_into_hash_mergeOr_TrackHashes(
              hash,b_set_ind,b_set,
              &used_hash_size, hm2.max_value_size
              ,&globally_used_hash_count, globally_used_hash_indices
          );
        }
      }

      int set_size = sizeof(nnz_lno_t) * 8;
      nnz_lno_t num_el = rowmapC(row_index);
      for (nnz_lno_t ii = 0; ii < used_hash_size; ++ii){
        nnz_lno_t c_rows_setind = hm2.keys[ii];
        nnz_lno_t c_rows = hm2.values[ii];

        int current_row = 0;
        nnz_lno_t unit = 1;

        while (c_rows){
          if (c_rows & unit){
            //insert indices.
            entriesSetIndicesC(num_el++) = set_size * c_rows_setind + current_row;
          }
          current_row++;
          c_rows = c_rows & ~unit;
          unit = unit << 1;
        }
      }
      for (int i = 0; i < globally_used_hash_count; ++i){
        nnz_lno_t dirty_hash = globally_used_hash_indices[i];
        hm2.hash_begins[dirty_hash] = -1;
      }
    }
    m_space.release_chunk(globally_used_hash_indices);
  }

  KOKKOS_INLINE_FUNCTION
  void operator()(const GPUTag&, const team_member_t & teamMember) const {

    nnz_lno_t row_index = teamMember.league_rank()  * teamMember.team_size()+ teamMember.team_rank();
    if (row_index >= numrows) return;


    //printf("row:%d\n", row_index);

    int thread_memory = ((shared_memory_size/ 4 / teamMember.team_size())) * 4;
    char *all_shared_memory = (char *) (teamMember.team_shmem().get_shmem(shared_memory_size));

    //nnz_lno_t *alloc_global_memory = NULL;
    nnz_lno_t *globally_used_hash_indices = NULL;

    //shift it to the thread private part
    all_shared_memory += thread_memory * teamMember.team_rank();

    //used_hash_sizes hold the size of 1st and 2nd level hashes
    volatile nnz_lno_t *used_hash_sizes = (volatile nnz_lno_t *) (all_shared_memory);
    all_shared_memory += sizeof(nnz_lno_t) * 2;

    nnz_lno_t *globally_used_hash_count = (nnz_lno_t *) (all_shared_memory);

    all_shared_memory += sizeof(nnz_lno_t) ;
    int unit_memory = sizeof(nnz_lno_t) * 2 + sizeof(nnz_lno_t) * 2;
    nnz_lno_t shared_memory_hash_size = (thread_memory - sizeof(nnz_lno_t) * 3) / unit_memory;

    nnz_lno_t * begins = (nnz_lno_t *) (all_shared_memory);
    all_shared_memory += sizeof(nnz_lno_t) * shared_memory_hash_size;

    //poins to the next elements
    nnz_lno_t * nexts = (nnz_lno_t *) (all_shared_memory);
    all_shared_memory += sizeof(nnz_lno_t) * shared_memory_hash_size;

    //holds the keys
    nnz_lno_t * keys = (nnz_lno_t *) (all_shared_memory);
    all_shared_memory += sizeof(nnz_lno_t) * shared_memory_hash_size;
    nnz_lno_t * vals = (nnz_lno_t *) (all_shared_memory);

    //printf("begins:%ld, nexts:%ld, keys:%ld, vals:%ld\n", begins, nexts, keys, vals);
    //return;
    //first level hashmap
    KokkosKernels::Experimental::HashmapAccumulator<nnz_lno_t,nnz_lno_t,nnz_lno_t>
      hm(shared_memory_hash_size, shared_memory_hash_size, begins, nexts, keys, vals);

    KokkosKernels::Experimental::HashmapAccumulator<nnz_lno_t,nnz_lno_t,nnz_lno_t> hm2;

    //initialize begins.
    Kokkos::parallel_for(
        Kokkos::ThreadVectorRange(teamMember, shared_memory_hash_size),
        [&] (int i) {
      begins[i] = -1;
    });

    //initialize hash usage sizes
    Kokkos::single(Kokkos::PerThread(teamMember),[&] () {
      used_hash_sizes[0] = 0;
      used_hash_sizes[1] = 0;
      globally_used_hash_count[0] = 0;
    });

    bool is_global_alloced = false;

    const size_type col_end = row_mapA[row_index + 1];
    const size_type col_begin = row_mapA[row_index];
    const nnz_lno_t col_size = col_end - col_begin;

    for (nnz_lno_t colind = 0; colind < col_size; ++colind){
      size_type a_col = colind + col_begin;

      nnz_lno_t rowB = entriesA[a_col];
      size_type rowBegin = old_row_mapB(rowB);

      nnz_lno_t left_work = row_mapB(rowB ) - rowBegin;

      while (left_work){
        nnz_lno_t work_to_handle = KOKKOSKERNELS_MACRO_MIN(vector_size, left_work);

        nnz_lno_t b_set_ind = -1, b_set = -1;
        nnz_lno_t hash = -1;
        Kokkos::parallel_for(
            Kokkos::ThreadVectorRange(teamMember, work_to_handle),
            [&] (nnz_lno_t i) {
          const size_type adjind = i + rowBegin;
          b_set_ind = entriesSetIndicesB[adjind];
          b_set = entriesSetsB[adjind];
          hash = b_set_ind % shared_memory_hash_size;
        });


        int num_unsuccess = hm.vector_atomic_insert_into_hash_mergeOr(
            teamMember, vector_size,
            hash, b_set_ind, b_set,
            used_hash_sizes,
            shared_memory_hash_size);


        int overall_num_unsuccess = 0;

        Kokkos::parallel_reduce( Kokkos::ThreadVectorRange(teamMember, vector_size),
            [&] (const int threadid, int &overall_num_unsuccess_) {
          overall_num_unsuccess_ += num_unsuccess;
        }, overall_num_unsuccess);


        if (overall_num_unsuccess){

          //printf("row:%d\n", row_index);
          if (!is_global_alloced){
            volatile nnz_lno_t * tmp = NULL;
            size_t tid = get_thread_id(row_index);
            while (tmp == NULL){
              Kokkos::single(Kokkos::PerThread(teamMember),[&] (volatile nnz_lno_t * &memptr) {
                memptr = (volatile nnz_lno_t * )( m_space.allocate_chunk(tid));
              }, tmp);
            }
            is_global_alloced = true;

            globally_used_hash_indices = (nnz_lno_t *) tmp;
            tmp += pow2_hash_size ;

            hm2.hash_begins = (nnz_lno_t *) (tmp);
            tmp += pow2_hash_size ;

            //poins to the next elements
            hm2.hash_nexts = (nnz_lno_t *) (tmp);
            tmp += MaxRoughNonZero;

            //holds the keys
            hm2.keys = (nnz_lno_t *) (tmp);
            tmp += MaxRoughNonZero;
            hm2.values = (nnz_lno_t *) (tmp);

            hm2.hash_key_size = pow2_hash_size;
            hm2.max_value_size = MaxRoughNonZero;
          }

          nnz_lno_t hash_ = -1;
          if (num_unsuccess) hash_ = b_set_ind & pow2_hash_func;

          //int insertion =
          hm2.vector_atomic_insert_into_hash_mergeOr_TrackHashes(
              teamMember, vector_size,
              hash_,b_set_ind,b_set,
              used_hash_sizes + 1, hm2.max_value_size
              ,globally_used_hash_count, globally_used_hash_indices
              );

        }
        left_work -= work_to_handle;
        rowBegin += work_to_handle;
      }
    }

    Kokkos::single(Kokkos::PerThread(teamMember),[&] () {
      if (used_hash_sizes[0] > shared_memory_hash_size) used_hash_sizes[0] = shared_memory_hash_size;
    });

    nnz_lno_t num_compressed_elements = used_hash_sizes[0];
    used_hash_sizes[0] = 0;
    size_type row_begin = rowmapC(row_index);
    int set_size = sizeof(nnz_lno_t) * 8;
    Kokkos::parallel_for( Kokkos::ThreadVectorRange(teamMember, num_compressed_elements),
        [&] (const nnz_lno_t ii) {
      nnz_lno_t c_rows_setind = hm.keys[ii];
      nnz_lno_t c_rows = hm.values[ii];

      int current_row = 0;
      nnz_lno_t unit = 1;

      while (c_rows){
        if (c_rows & unit){

          size_type wind = Kokkos::atomic_fetch_add(used_hash_sizes, 1);
          entriesSetIndicesC(wind + row_begin) = set_size * c_rows_setind + current_row;
        }
        current_row++;
        c_rows = c_rows & ~unit;
        unit = unit << 1;
      }

    });


    if (is_global_alloced){
      nnz_lno_t num_compressed_elements_ = used_hash_sizes[1];
      Kokkos::parallel_for( Kokkos::ThreadVectorRange(teamMember, num_compressed_elements_),
          [&] (const nnz_lno_t ii) {
        nnz_lno_t c_rows_setind = hm2.keys[ii];
        nnz_lno_t c_rows = hm2.values[ii];

        int current_row = 0;
        nnz_lno_t unit = 1;

        while (c_rows){
          if (c_rows & unit){

            size_type wind = Kokkos::atomic_fetch_add(used_hash_sizes, 1);
            entriesSetIndicesC(wind + row_begin) = set_size * c_rows_setind + current_row;
          }
          current_row++;
          c_rows = c_rows & ~unit;
          unit = unit << 1;
        }
      });

      //now thread leaves the memory as it finds. so there is no need to initialize the hash begins
      nnz_lno_t dirty_hashes = globally_used_hash_count[0];
      Kokkos::parallel_for(
          Kokkos::ThreadVectorRange(teamMember, dirty_hashes),
          [&] (nnz_lno_t i) {
        nnz_lno_t dirty_hash = globally_used_hash_indices[i];
        hm2.hash_begins[dirty_hash] = -1;
      });


      Kokkos::single(Kokkos::PerThread(teamMember),[&] () {
        m_space.release_chunk(globally_used_hash_indices);
      });
    }

  }

  size_t team_shmem_size (int team_size) const {
    return shared_memory_size;
  }

};
}
}
