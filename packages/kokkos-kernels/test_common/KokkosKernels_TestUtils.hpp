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

#ifndef KOKKOSKERNELS_TEST_UTILS_HPP
#define KOKKOSKERNELS_TEST_UTILS_HPP

#include <random>

#include "KokkosKernels_Utils.hpp"
#include "KokkosKernels_IOUtils.hpp"
#include "Kokkos_ArithTraits.hpp"
#include "KokkosBatched_Vector.hpp"
// Make this include-able from all subdirectories
#include "../tpls/gtest/gtest/gtest.h"  //for EXPECT_**

// Simplify ETI macros
#if !defined(KOKKOSKERNELS_ETI_ONLY) && !defined(KOKKOSKERNELS_IMPL_CHECK_ETI_CALLS)
#define KOKKOSKERNELS_TEST_ALL_TYPES
#endif
#if defined(KOKKOSKERNELS_INST_LAYOUTLEFT) || defined(KOKKOSKERNELS_TEST_ALL_TYPES)
#define KOKKOSKERNELS_TEST_LAYOUTLEFT
#endif
#if defined(KOKKOSKERNELS_INST_LAYOUTRIGHT) || defined(KOKKOSKERNELS_TEST_ALL_TYPES)
#define KOKKOSKERNELS_TEST_LAYOUTRIGHT
#endif
#if defined(KOKKOSKERNELS_INST_LAYOUTSTRIDE) || defined(KOKKOSKERNELS_TEST_ALL_TYPES)
#define KOKKOSKERNELS_TEST_LAYOUTSTRIDE
#endif
#if defined(KOKKOSKERNELS_INST_FLOAT) || defined(KOKKOSKERNELS_TEST_ALL_TYPES)
#define KOKKOSKERNELS_TEST_FLOAT
#endif
#if defined(KOKKOSKERNELS_INST_DOUBLE) || defined(KOKKOSKERNELS_TEST_ALL_TYPES)
#define KOKKOSKERNELS_TEST_DOUBLE
#endif
#if defined(KOKKOSKERNELS_INST_INT) || defined(KOKKOSKERNELS_TEST_ALL_TYPES)
#define KOKKOSKERNELS_TEST_INT
#endif
#if defined(KOKKOSKERNELS_INST_COMPLEX_FLOAT) || defined(KOKKOSKERNELS_TEST_ALL_TYPES)
#define KOKKOSKERNELS_TEST_COMPLEX_FLOAT
#endif
#if defined(KOKKOSKERNELS_INST_COMPLEX_DOUBLE) || defined(KOKKOSKERNELS_TEST_ALL_TYPES)
#define KOKKOSKERNELS_TEST_COMPLEX_DOUBLE
#endif

namespace Test {

// Utility class for testing kernels with rank-1 and rank-2 views that may be
// LayoutStride. Simplifies making a LayoutStride view of a given size that is
// actually noncontiguous, and host-device transfers for checking results on
// host.
//
// Constructed with label and extent(s), and then provides 5 views as members:
//  - d_view, and a const-valued alias d_view_const
//  - h_view
//  - d_base
//  - h_base
// d_view is of type ViewType, and has the extents passed to the constructor.
// h_view is a mirror of d_view.
// d_base (and its mirror h_base) are contiguous views, so they can be
// deep-copied to each other. d_view aliases d_base, and h_view aliases h_base.
// This means that copying between d_base and h_base
//    also copies between d_view and h_view.
//
// If the Boolean template parameter 'createMirrorView' is:
// - 'true' (default value), then this utility class will use
//   Kokkos::create_mirror_view();
// - 'false', then this utility class will use Kokkos::create_mirror()
template <class ViewType, bool createMirrorView = true>
struct view_stride_adapter {
  static_assert(Kokkos::is_view_v<ViewType>, "view_stride_adapter: ViewType must be a Kokkos::View");
  static_assert(ViewType::rank >= 1 && ViewType::rank <= 2, "view_stride_adapter: ViewType must be rank 1 or rank 2");

  static constexpr bool strided = std::is_same<typename ViewType::array_layout, Kokkos::LayoutStride>::value;
  static constexpr int rank     = ViewType::rank;

  using DView = ViewType;
  using HView = typename DView::HostMirror;
  // If not strided, the base view types are the same as DView/HView.
  // But if strided, the base views have one additional dimension, so that
  // d_view/h_view have stride > 1 between consecutive elements.
  using DViewBase = std::conditional_t<
      strided, Kokkos::View<typename ViewType::data_type*, Kokkos::LayoutRight, typename ViewType::device_type>, DView>;
  using HViewBase = typename DViewBase::HostMirror;

  view_stride_adapter(const std::string& label, int m, int n = 1) {
    if constexpr (rank == 1) {
      if constexpr (strided) {
        d_base = DViewBase(label, m, 2);
        h_base = createMirrorView ? Kokkos::create_mirror_view(d_base) : Kokkos::create_mirror(d_base);
        d_view = Kokkos::subview(d_base, Kokkos::ALL(), 0);
        h_view = Kokkos::subview(h_base, Kokkos::ALL(), 0);
      } else {
        d_base = DViewBase(label, m);
        h_base = createMirrorView ? Kokkos::create_mirror_view(d_base) : Kokkos::create_mirror(d_base);
        d_view = d_base;
        h_view = h_base;
      }
    } else {
      if constexpr (strided) {
        d_base = DViewBase(label, m, n, 2);
        h_base = createMirrorView ? Kokkos::create_mirror_view(d_base) : Kokkos::create_mirror(d_base);
        d_view = Kokkos::subview(d_base, Kokkos::ALL(), Kokkos::make_pair(0, n), 0);
        h_view = Kokkos::subview(h_base, Kokkos::ALL(), Kokkos::make_pair(0, n), 0);
      } else {
        d_base = DViewBase(label, m, n);
        h_base = createMirrorView ? Kokkos::create_mirror_view(d_base) : Kokkos::create_mirror(d_base);
        d_view = d_base;
        h_view = h_base;
      }
    }
    d_view_const = d_view;
  }

  // Have both const and nonconst versions of d_view (with same underlying
  // data), since we often test BLAS with both
  DView d_view;
  typename DView::const_type d_view_const;
  HView h_view;
  DViewBase d_base;
  HViewBase h_base;
};

template <class Scalar1, class Scalar2, class Scalar3>
void EXPECT_NEAR_KK(Scalar1 val1, Scalar2 val2, Scalar3 tol, std::string msg = "") {
  typedef Kokkos::ArithTraits<Scalar1> AT1;
  typedef Kokkos::ArithTraits<Scalar3> AT3;
  EXPECT_LE((double)AT1::abs(val1 - val2), (double)AT3::abs(tol)) << msg;
}

template <class Scalar1, class Scalar2, class Scalar3>
void EXPECT_NEAR_KK_REL(Scalar1 val1, Scalar2 val2, Scalar3 tol, std::string msg = "") {
  typedef typename std::remove_reference<decltype(val1)>::type hv1_type;
  typedef typename std::remove_reference<decltype(val2)>::type hv2_type;
  const auto ahv1 = Kokkos::ArithTraits<hv1_type>::abs(val1);
  const auto ahv2 = Kokkos::ArithTraits<hv2_type>::abs(val2);
  EXPECT_NEAR_KK(val1, val2, tol * Kokkos::max(ahv1, ahv2), msg);
}

// Special overload for accurate value by value SIMD vectors comparison
template <class Scalar, int VecLen, class Tolerance>
void EXPECT_NEAR_KK_REL(const KokkosBatched::Vector<KokkosBatched::SIMD<Scalar>, VecLen>& val1,
                        const KokkosBatched::Vector<KokkosBatched::SIMD<Scalar>, VecLen>& val2, Tolerance tol,
                        std::string msg = "") {
  for (int i = 0; i < VecLen; ++i) {
    EXPECT_NEAR_KK_REL(val1[i], val2[i], tol, msg);
  }
}

template <class ViewType1, class ViewType2, class Scalar>
void EXPECT_NEAR_KK_1DVIEW(ViewType1 v1, ViewType2 v2, Scalar tol) {
  size_t v1_size = v1.extent(0);
  size_t v2_size = v2.extent(0);
  EXPECT_EQ(v1_size, v2_size);

  typename ViewType1::HostMirror h_v1 = Kokkos::create_mirror_view(v1);
  typename ViewType2::HostMirror h_v2 = Kokkos::create_mirror_view(v2);

  KokkosKernels::Impl::safe_device_to_host_deep_copy(v1.extent(0), v1, h_v1);
  KokkosKernels::Impl::safe_device_to_host_deep_copy(v2.extent(0), v2, h_v2);

  for (size_t i = 0; i < v1_size; ++i) {
    EXPECT_NEAR_KK(h_v1(i), h_v2(i), tol);
  }
}

template <class ViewType1, class ViewType2, class Scalar>
void EXPECT_NEAR_KK_REL_1DVIEW(ViewType1 v1, ViewType2 v2, Scalar tol) {
  size_t v1_size = v1.extent(0);
  size_t v2_size = v2.extent(0);
  EXPECT_EQ(v1_size, v2_size);

  typename ViewType1::HostMirror h_v1 = Kokkos::create_mirror_view(v1);
  typename ViewType2::HostMirror h_v2 = Kokkos::create_mirror_view(v2);

  KokkosKernels::Impl::safe_device_to_host_deep_copy(v1.extent(0), v1, h_v1);
  KokkosKernels::Impl::safe_device_to_host_deep_copy(v2.extent(0), v2, h_v2);

  for (size_t i = 0; i < v1_size; ++i) {
    EXPECT_NEAR_KK_REL(h_v1(i), h_v2(i), tol);
  }
}

/// This function returns a descriptive user defined failure string for
/// insertion into gtest macros such as FAIL() and EXPECT_LE(). \param file The
/// filename where the failure originated \param func The function where the
/// failure originated \param line The line number where the failure originated
/// \return a new string containing: "  > from file:func:line\n    > "
static inline const std::string kk_failure_str(std::string file, std::string func, const int line) {
  std::string failure_msg = "  > from ";
  failure_msg += (file + ":" + func + ":" + std::to_string(line) + "\n    > ");
  return std::string(failure_msg);
}

#if defined(KOKKOS_HALF_T_IS_FLOAT)
using halfScalarType = Kokkos::Experimental::half_t;
#endif  // KOKKOS_HALF_T_IS_FLOAT

#if defined(KOKKOS_BHALF_T_IS_FLOAT)
using bhalfScalarType = Kokkos::Experimental::bhalf_t;
#endif  // KOKKOS_BHALF_T_IS_FLOAT

template <class T>
class epsilon {
 public:
  constexpr static double value = std::numeric_limits<T>::epsilon();
};

using KokkosKernels::Impl::getRandomBounds;

// create_random_x_vector and create_random_y_vector can be used together to
// generate a random linear system Ax = y.
template <typename vec_t>
vec_t create_random_x_vector(vec_t& kok_x, double max_value = 10.0) {
  typedef typename vec_t::value_type scalar_t;
  auto h_x = Kokkos::create_mirror_view(kok_x);
  for (size_t j = 0; j < h_x.extent(1); ++j) {
    for (size_t i = 0; i < h_x.extent(0); ++i) {
      scalar_t r       = static_cast<scalar_t>(rand()) / static_cast<scalar_t>(RAND_MAX / max_value);
      h_x.access(i, j) = r;
    }
  }
  Kokkos::deep_copy(kok_x, h_x);
  return kok_x;
}

/// \brief SharedParamTag class used to specify how to invoke templates within
///                       batched unit tests
/// \var TA Indicates which transpose operation to apply to the A matrix
/// \var TB Indicates which transpose operation to apply to the B matrix
/// \var BL Indicates whether the batch size is in the leftmost or rightmost
///         dimension
template <typename TA, typename TB, typename BL>
struct SharedParamTag {
  using transA      = TA;
  using transB      = TB;
  using batchLayout = BL;
};

/// \brief value_type_name returns a string with the value type name
template <typename T>
std::string value_type_name() {
  return "::UnknownValueType";
}

template <>
std::string value_type_name<float>() {
  return "::Float";
}

template <>
std::string value_type_name<double>() {
  return "::Double";
}

template <>
std::string value_type_name<int>() {
  return "::Int";
}

template <>
std::string value_type_name<Kokkos::complex<float>>() {
  return "::ComplexFloat";
}

template <>
std::string value_type_name<Kokkos::complex<double>>() {
  return "::ComplexDouble";
}

/// /brief Coo matrix class for testing purposes.
/// \tparam ScalarType
/// \tparam LayoutType
/// \tparam Device
template <class ScalarType, class LayoutType, class Device>
class RandCooMat {
 private:
  using ExeSpaceType  = typename Device::execution_space;
  using RowViewTypeD  = Kokkos::View<int64_t*, LayoutType, Device>;
  using ColViewTypeD  = Kokkos::View<int64_t*, LayoutType, Device>;
  using DataViewTypeD = Kokkos::View<ScalarType*, LayoutType, Device>;
  RowViewTypeD __row_d;
  ColViewTypeD __col_d;
  DataViewTypeD __data_d;

  template <class T>
  T __getter_copy_helper(T src) {
    T dst(std::string("RandCooMat.") + typeid(T).name() + " copy", src.extent(0));
    Kokkos::deep_copy(dst, src);
    ExeSpaceType().fence();
    return dst;
  }

 public:
  std::string info;
  /// Constructs a random coo matrix with negative indices.
  /// \param m The max row id
  /// \param n The max col id
  /// \param n_tuples The number of tuples.
  /// \param min_val The minimum scalar value in the matrix.
  /// \param max_val The maximum scalar value in the matrix.
  RandCooMat(int64_t m, int64_t n, int64_t n_tuples, ScalarType min_val, ScalarType max_val) {
    uint64_t ticks = std::chrono::high_resolution_clock::now().time_since_epoch().count() % UINT32_MAX;

    info = std::string(std::string("RandCooMat<") + typeid(ScalarType).name() + ", " + typeid(LayoutType).name() +
                       ", " + typeid(ExeSpaceType).name() + std::to_string(n) +
                       "...): rand seed: " + std::to_string(ticks) + "\n");
    Kokkos::Random_XorShift64_Pool<ExeSpaceType> random(ticks);

    __row_d = RowViewTypeD("RandCooMat.RowViewType", n_tuples);
    Kokkos::fill_random(__row_d, random, -m, m);

    __col_d = ColViewTypeD("RandCooMat.ColViewType", n_tuples);
    Kokkos::fill_random(__col_d, random, -n, n);

    __data_d = DataViewTypeD("RandCooMat.DataViewType", n_tuples);
    Kokkos::fill_random(__data_d, random, min_val, max_val);

    ExeSpaceType().fence();
  }
  auto get_row() { return __getter_copy_helper(__row_d); }
  auto get_col() { return __getter_copy_helper(__col_d); }
  auto get_data() { return __getter_copy_helper(__data_d); }
};

/// /brief Cs (Compressed Sparse) matrix class for testing purposes.
/// This class is for testing purposes only and will generate a random
/// Crs / Ccs matrix when instantiated. The class is intentionally written
/// without the use of "row" and "column" member names.
/// dim1 refers to either rows for Crs matrix or columns for a Ccs matrix.
/// dim2 refers to either columns for a Crs matrix or rows for a Ccs matrix.
/// \tparam ScalarType
/// \tparam LayoutType
/// \tparam Device
template <class ScalarType, class LayoutType, class Device, typename Ordinal = int64_t,
          typename Size = KokkosKernels::default_size_type>
class RandCsMatrix {
 public:
  using value_type   = ScalarType;
  using array_layout = LayoutType;
  using device_type  = Device;
  using ordinal_type = Ordinal;
  using size_type    = Size;
  using ValViewTypeD = Kokkos::View<ScalarType*, LayoutType, Device>;
  using IdViewTypeD  = Kokkos::View<Ordinal*, LayoutType, Device>;
  using MapViewTypeD = Kokkos::View<Size*, LayoutType, Device>;

 private:
  using execution_space = typename Device::execution_space;
  Ordinal __dim2;
  Ordinal __dim1;
  Size __nnz = 0;
  MapViewTypeD __map_d;
  IdViewTypeD __ids_d;
  ValViewTypeD __vals_d;
  using MapViewTypeH = typename MapViewTypeD::HostMirror;
  using IdViewTypeH  = typename IdViewTypeD::HostMirror;
  using ValViewTypeH = typename ValViewTypeD::HostMirror;
  MapViewTypeH __map;
  IdViewTypeH __ids;
  ValViewTypeH __vals;
  bool __fully_sparse;

  /// Generates a random map where (using Ccs terminology):
  ///  1. __map(i) is in [__ids.data(), &row_ids.data()[nnz - 1]
  ///  2. __map(i) > col_map(i - 1) for i > 1
  ///  3. __map(i) == col_map(j) iff __map(i) == col_map(j) == nullptr
  ///  4. __map(i) - col_map(i - 1) is in [0, m]
  void __populate_random_cs_mat(uint64_t ticks) {
    std::srand(ticks);
    for (Ordinal col_idx = 0; col_idx < __dim1; col_idx++) {
      Ordinal r = std::rand() % (__dim2 + 1);
      if (r == 0 || __fully_sparse) {  // 100% sparse vector
        __map(col_idx) = __nnz;
      } else {  // sparse vector with r elements
        // Populate r row ids
        std::vector<Ordinal> v(r);

        for (Ordinal i = 0; i < r; i++) v.at(i) = i;

        std::shuffle(v.begin(), v.end(), std::mt19937(std::random_device()()));

        for (Ordinal i = 0; i < r; i++) __ids(i + __nnz) = v.at(i);

        // Point to new column and accumulate number of non zeros
        __map(col_idx) = __nnz;
        __nnz += r;
      }
    }

    // last entry in map points to end of id list
    __map(__dim1) = __nnz;

    // Copy to device
    Kokkos::deep_copy(__map_d, __map);
    IdViewTypeD tight_ids(Kokkos::view_alloc(Kokkos::WithoutInitializing, "RandCsMatrix.IdViewTypeD"), __nnz);
    Kokkos::deep_copy(tight_ids, Kokkos::subview(__ids, Kokkos::make_pair(0, static_cast<int>(__nnz))));
    __ids_d = tight_ids;
  }

  template <class T>
  T __getter_copy_helper(T src) {
    T dst(std::string("RandCsMatrix.") + typeid(T).name() + " copy", src.extent(0));
    Kokkos::deep_copy(dst, src);
    return dst;
  }

 public:
  std::string info;
  /// Constructs a random cs matrix.
  /// \param dim1 The first dimension: rows for Crs or columns for Ccs
  /// \param dim2 The second dimension: columns for Crs or rows for Ccs
  /// \param min_val The minimum scalar value in the matrix.
  /// \param max_val The maximum scalar value in the matrix.
  RandCsMatrix(Ordinal dim1, Ordinal dim2, ScalarType min_val, ScalarType max_val, bool fully_sparse = false) {
    __dim1         = dim1;
    __dim2         = dim2;
    __fully_sparse = fully_sparse;
    __map_d        = MapViewTypeD("RandCsMatrix.ColMapViewType", __dim1 + 1);
    __map          = Kokkos::create_mirror_view(__map_d);
    __ids_d        = IdViewTypeD("RandCsMatrix.RowIdViewType",
                                 dim2 * dim1 + 1);  // over-allocated
    __ids          = Kokkos::create_mirror_view(__ids_d);

    uint64_t ticks = std::chrono::high_resolution_clock::now().time_since_epoch().count() % UINT32_MAX;

    info = std::string(std::string("RandCsMatrix<") + typeid(ScalarType).name() + ", " + typeid(LayoutType).name() +
                       ", " + execution_space().name() + ">(" + std::to_string(dim2) + ", " + std::to_string(dim1) +
                       "...): rand seed: " + std::to_string(ticks) +
                       ", fully sparse: " + (__fully_sparse ? "true" : "false") + "\n");
    Kokkos::Random_XorShift64_Pool<Kokkos::HostSpace> random(ticks);
    __populate_random_cs_mat(ticks);

    __vals_d = ValViewTypeD("RandCsMatrix.ValViewType", __nnz + 1);
    __vals   = Kokkos::create_mirror_view(__vals_d);
    Kokkos::fill_random(__vals, random, min_val, max_val);  // random scalars
    Kokkos::fence();
    __vals(__nnz) = ScalarType(0);

    // Copy to device
    Kokkos::deep_copy(__vals_d, __vals);
  }

  // O(c), where c is a constant.
  ScalarType operator()(Size idx) { return __vals(idx); }
  Size get_nnz() { return __nnz; }
  // dimension2: This is either columns for a Crs matrix or rows for a Ccs
  // matrix.
  Ordinal get_dim2() { return __dim2; }
  // dimension1: This is either rows for Crs matrix or columns for a Ccs matrix.
  Ordinal get_dim1() { return __dim1; }
  ValViewTypeD get_vals() { return __getter_copy_helper(__vals_d); }
  IdViewTypeD get_ids() { return __getter_copy_helper(__ids_d); }
  MapViewTypeD get_map() { return __getter_copy_helper(__map_d); }
};

/// \brief Randomly shuffle the entries in each row (col) of a Crs (Ccs) or Bsr
/// matrix.
template <typename Rowptrs, typename Entries, typename Values>
void shuffleMatrixEntries(Rowptrs rowptrs, Entries entries, Values values, const size_t block_size = 1) {
  using size_type          = typename Rowptrs::non_const_value_type;
  using ordinal_type       = typename Entries::value_type;
  auto rowptrsHost         = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), rowptrs);
  auto entriesHost         = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), entries);
  auto valuesHost          = Kokkos::create_mirror_view_and_copy(Kokkos::HostSpace(), values);
  ordinal_type numRows     = rowptrsHost.extent(0) ? (rowptrsHost.extent(0) - 1) : 0;
  const size_t block_items = block_size * block_size;
  for (ordinal_type i = 0; i < numRows; i++) {
    size_type rowBegin = rowptrsHost(i);
    size_type rowEnd   = rowptrsHost(i + 1);
    for (size_type j = rowBegin; j < rowEnd - 1; j++) {
      ordinal_type swapRange = rowEnd - j;
      size_type swapOffset   = j + (rand() % swapRange);
      std::swap(entriesHost(j), entriesHost(swapOffset));
      std::swap_ranges(valuesHost.data() + j * block_items, valuesHost.data() + (j + 1) * block_items,
                       valuesHost.data() + swapOffset * block_items);
    }
  }
  Kokkos::deep_copy(entries, entriesHost);
  Kokkos::deep_copy(values, valuesHost);
}

}  // namespace Test
#endif
