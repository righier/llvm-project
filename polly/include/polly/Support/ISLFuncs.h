//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Wrappers for ISL API functions that are not exported in the C++ bindings.
//
// Instead of exposing isl_*_from_*() functions, use to_*() functions
// from GCIHelper.h. The shortened from_*() to not work well with overload
// resolution.
//
//===----------------------------------------------------------------------===//

#ifndef POLLY_SUPPORT_ISLFUNCS_H
#define POLLY_SUPPORT_ISLFUNCS_H

#include "isl/isl-noexceptions.h"

namespace polly {

static inline bool domain_is_wrapping(const isl::space &Space) {
  return isl_space_domain_is_wrapping(Space.get()) == isl_bool_true;
}

static inline bool range_is_wrapping(const isl::space &Space) {
  return isl_space_range_is_wrapping(Space.get()) == isl_bool_true;
}

// "identifty" clashes with llvm::identity
static inline isl::basic_map identity_map(isl::space Space) {
  return isl::manage(isl_basic_map_identity(Space.release()));
}

static inline bool is_map(const isl::space &Space) {
  return isl_space_is_map(Space.get()) == isl_bool_true;
}

#if 0
  static inline  isl::pw_aff get_pw_aff(const isl::pw_multi_aff& PwMultiAff, int Pos) {
    return isl::manage(isl_pw_multi_aff_get_pw_aff(PwMultiAff.get(), Pos));
  }

  static inline isl::union_map from_union_pw_aff(isl::union_pw_aff UPwAff) {
    return isl::manage(isl_union_map_from_union_pw_aff(UPwAff.release()));
  }
#endif

static inline isl::space get_space(const isl::point &P) {
  return isl::manage(isl_point_get_space(P.get()));
}

#if 0
  static inline  isl::union_set align_params(isl::union_set USet, isl::space ParamSpace) {
    return isl::manage(isl_union_set_align_params(USet.release(), ParamSpace.release()));
  }
  static inline  isl::union_map align_params(isl::union_map UMap, isl::space ParamSpace) {
    return isl::manage(isl_union_map_align_params(UMap.release(), ParamSpace.release()));
  }
#endif

static inline auto intersect_range(isl::multi_union_pw_aff MUPwAff,
                                   isl::set Set) {
  return isl::manage(
      isl_multi_union_pw_aff_intersect_range(MUPwAff.release(), Set.release()));
}

static inline auto flat_domain_product(isl::union_map UMap1,
                                       isl::union_map UMap2) {
  return isl::manage(
      isl_union_map_flat_domain_product(UMap1.release(), UMap2.release()));
}

static inline auto flat_range_product(isl::union_map UMap1,
                                      isl::union_map UMap2) {
  return isl::manage(
      isl_union_map_flat_range_product(UMap1.release(), UMap2.release()));
}

static inline auto flat_range_product(isl::basic_map BMap1,
                                      isl::basic_map BMap2) {
  return isl::manage(
      isl_basic_map_flat_range_product(BMap1.release(), BMap2.release()));
}

static inline auto get_local_space(const isl::constraint &C) {
  return isl::manage(isl_constraint_get_local_space(C.get()));
}

static inline auto get_local_space(const isl::basic_map &BMap) {
  return isl::manage(isl_basic_map_get_local_space(BMap.get()));
}

static inline isl::size dim(const isl::local_space &LS, isl::dim Dim) {
  return isl::manage(
      isl_local_space_dim(LS.get(), static_cast<enum isl_dim_type>(Dim)));
}

static inline auto get_coefficient_val(const isl::constraint &C, isl::dim Dim,
                                       int Pos) {
  return isl::manage(isl_constraint_get_coefficient_val(
      C.get(), static_cast<enum isl_dim_type>(Dim), Pos));
}

static inline auto get_constant_val(const isl::constraint &C) {
  return isl::manage(isl_constraint_get_constant_val(C.get()));
}

static inline auto set_coefficient_val(isl::constraint C, isl::dim Dim, int Pos,
                                       isl::val V) {
  return isl::manage(isl_constraint_set_coefficient_val(
      C.release(), static_cast<enum isl_dim_type>(Dim), Pos, V.release()));
}

static inline auto from_domain(isl::space Space) {
  return isl::manage(isl_space_from_domain(Space.release()));
}

static inline auto add_constraint(isl::basic_map BMap, isl::constraint C) {
  return isl::manage(isl_basic_map_add_constraint(BMap.release(), C.release()));
}

#if 0
  static inline auto set_tuple_id(isl::basic_map BMap, isl::dim Dim ,  isl::id Id) {
    return isl::manage(isl_basic_map_set_tuple_id (BMap.release(),  static_cast<enum isl_dim_type> (Dim),  Id.release()));
  }
  static inline auto set_tuple_id(isl::map Map, isl::dim Dim ,  isl::id Id) {
    return isl::manage(isl_map_set_tuple_id (Map.release(),  static_cast<enum isl_dim_type> (Dim),  Id.release()));
  }

  static inline auto project_out(isl::basic_map BMap, isl::dim Dim ,  unsigned first, unsigned n) {
    return isl::manage(isl_basic_map_project_out(BMap.release(),  static_cast<enum isl_dim_type> (Dim),first, n));
  }
#endif

static inline auto flat_product(isl::set Set1, isl::set Set2) {
  return isl::manage(isl_set_flat_product(Set1.release(), Set2.release()));
}

} // namespace polly

namespace isl {
class constraint_list;

// declarations for isl::constraint_list
inline constraint_list manage(__isl_take isl_constraint_list *ptr);
inline constraint_list manage_copy(__isl_keep isl_constraint_list *ptr);

class constraint_list {
  friend inline constraint_list manage(__isl_take isl_constraint_list *ptr);
  friend inline constraint_list
  manage_copy(__isl_keep isl_constraint_list *ptr);

  isl_constraint_list *ptr = nullptr;

  inline explicit constraint_list(__isl_take isl_constraint_list *ptr);

public:
  inline /* implicit */ constraint_list();
  inline /* implicit */ constraint_list(const constraint_list &obj);
  inline constraint_list &operator=(constraint_list obj);
  inline ~constraint_list();
  inline __isl_give isl_constraint_list *copy() const &;
  inline __isl_give isl_constraint_list *copy() && = delete;
  inline __isl_keep isl_constraint_list *get() const;
  inline __isl_give isl_constraint_list *release();
  inline bool is_null() const;
  inline isl::ctx ctx() const;
  inline void dump() const;

  inline isl::constraint_list add(isl::constraint el) const;
  static inline isl::constraint_list alloc(isl::ctx ctx, int n);
  inline isl::constraint_list clear() const;
  inline isl::constraint_list concat(isl::constraint_list list2) const;
  inline isl::constraint_list drop(unsigned int first, unsigned int n) const;
  inline stat foreach (const std::function<stat(constraint)> &fn) const;
  static inline isl::constraint_list from_constraint(isl::constraint el);
  inline isl::constraint get_at(int index) const;
  inline isl::constraint get_constraint(int index) const;
  inline isl::constraint_list insert(unsigned int pos,
                                     isl::constraint el) const;
  inline isl_size n_constraint() const;
  inline isl::constraint_list reverse() const;
  inline isl::constraint_list set_constraint(int index,
                                             isl::constraint el) const;
  inline isl::size size() const;
  inline isl::constraint_list swap(unsigned int pos1, unsigned int pos2) const;
};

// implementations for isl::constraint_list
constraint_list manage(__isl_take isl_constraint_list *ptr) {
  return constraint_list(ptr);
}
constraint_list manage_copy(__isl_keep isl_constraint_list *ptr) {
  ptr = isl_constraint_list_copy(ptr);
  return constraint_list(ptr);
}

constraint_list::constraint_list() : ptr(nullptr) {}

constraint_list::constraint_list(const constraint_list &obj) : ptr(nullptr) {
  ptr = obj.copy();
}

constraint_list::constraint_list(__isl_take isl_constraint_list *ptr)
    : ptr(ptr) {}

constraint_list &constraint_list::operator=(constraint_list obj) {
  std::swap(this->ptr, obj.ptr);
  return *this;
}

constraint_list::~constraint_list() {
  if (ptr)
    isl_constraint_list_free(ptr);
}

__isl_give isl_constraint_list *constraint_list::copy() const & {
  return isl_constraint_list_copy(ptr);
}

__isl_keep isl_constraint_list *constraint_list::get() const { return ptr; }

__isl_give isl_constraint_list *constraint_list::release() {
  isl_constraint_list *tmp = ptr;
  ptr = nullptr;
  return tmp;
}

bool constraint_list::is_null() const { return ptr == nullptr; }

isl::ctx constraint_list::ctx() const {
  return isl::ctx(isl_constraint_list_get_ctx(ptr));
}

void constraint_list::dump() const { isl_constraint_list_dump(get()); }

isl::constraint_list constraint_list::add(isl::constraint el) const {
  auto res = isl_constraint_list_add(copy(), el.release());
  return manage(res);
}

isl::constraint_list constraint_list::alloc(isl::ctx ctx, int n) {
  auto res = isl_constraint_list_alloc(ctx.release(), n);
  return manage(res);
}

isl::constraint_list constraint_list::clear() const {
  auto res = isl_constraint_list_clear(copy());
  return manage(res);
}

isl::constraint_list constraint_list::concat(isl::constraint_list list2) const {
  auto res = isl_constraint_list_concat(copy(), list2.release());
  return manage(res);
}

isl::constraint_list constraint_list::drop(unsigned int first,
                                           unsigned int n) const {
  auto res = isl_constraint_list_drop(copy(), first, n);
  return manage(res);
}

stat constraint_list::foreach (
    const std::function<stat(constraint)> &fn) const {
  struct fn_data {
    const std::function<stat(constraint)> *func;
  } fn_data = {&fn};
  auto fn_lambda = [](isl_constraint *arg_0, void *arg_1) -> isl_stat {
    auto *data = static_cast<struct fn_data *>(arg_1);
    stat ret = (*data->func)(manage(arg_0));
    return ret.release();
  };
  auto res = isl_constraint_list_foreach(get(), fn_lambda, &fn_data);
  return manage(res);
}

isl::constraint_list constraint_list::from_constraint(isl::constraint el) {
  auto res = isl_constraint_list_from_constraint(el.release());
  return manage(res);
}

isl::constraint constraint_list::get_at(int index) const {
  auto res = isl_constraint_list_get_at(get(), index);
  return manage(res);
}

isl::constraint constraint_list::get_constraint(int index) const {
  auto res = isl_constraint_list_get_constraint(get(), index);
  return manage(res);
}

isl::constraint_list constraint_list::insert(unsigned int pos,
                                             isl::constraint el) const {
  auto res = isl_constraint_list_insert(copy(), pos, el.release());
  return manage(res);
}

isl_size constraint_list::n_constraint() const {
  auto res = isl_constraint_list_n_constraint(get());
  return res;
}

isl::constraint_list constraint_list::reverse() const {
  auto res = isl_constraint_list_reverse(copy());
  return manage(res);
}

isl::constraint_list constraint_list::set_constraint(int index,
                                                     isl::constraint el) const {
  auto res = isl_constraint_list_set_constraint(copy(), index, el.release());
  return manage(res);
}

isl::size constraint_list::size() const {
  auto res = isl_constraint_list_size(get());
  return manage(res);
}

isl::constraint_list constraint_list::swap(unsigned int pos1,
                                           unsigned int pos2) const {
  auto res = isl_constraint_list_swap(copy(), pos1, pos2);
  return manage(res);
}

namespace detail {
template <typename T> class Converter;

template <> class Converter<isl::union_map> {
public:
  static isl::union_map convert(isl::union_pw_aff UPwAff) {
    return isl::manage(isl_union_map_from_union_pw_aff(UPwAff.release()));
  }

  static isl::union_map convert(isl::multi_union_pw_aff MUPwAff) {
    return isl::manage(
        isl_union_map_from_multi_union_pw_aff(MUPwAff.release()));
  }
};

template <> class Converter<isl::map> {
public:
  static isl::map convert(isl::multi_pw_aff MPwAff) {
    return isl::manage(isl_map_from_multi_pw_aff(MPwAff.release()));
  }
};

template <> class Converter<isl::set> {
public:
  static isl::set convert(isl::pw_aff PwAff) {
    return isl::manage(isl_set_from_pw_aff(PwAff.release()));
  }
};
} // namespace detail

template <typename T, typename V> static inline T convert(V v) {
  return detail::Converter<T>::convert(std::move(v));
}
} // namespace isl

namespace polly {
static inline auto get_constraint_list(const isl::basic_map &BMap) {
  return isl::manage(isl_basic_map_get_constraint_list(BMap.get()));
}

} // namespace polly

#endif /* POLLY_SUPPORT_ISLFUNCS_H */
