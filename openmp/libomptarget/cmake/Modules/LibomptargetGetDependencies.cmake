#
#//===----------------------------------------------------------------------===//
#//
#// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
#// See https://llvm.org/LICENSE.txt for license information.
#// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#//
#//===----------------------------------------------------------------------===//
#

# Try to detect in the system several dependencies required by the different
# components of libomptarget. These are the dependencies we have:
#
# libelf : required by some targets to handle the ELF files at runtime.
# libffi : required to launch target kernels given function and argument 
#          pointers.
# CUDA : required to control offloading to NVIDIA GPUs.
# VEOS : required to control offloading to NEC Aurora.

include (FindPackageHandleStandardArgs)

################################################################################
# Looking for libelf...
################################################################################

find_path (
  LIBOMPTARGET_DEP_LIBELF_INCLUDE_DIR
  NAMES
    libelf.h
  PATHS
    /usr/include
    /usr/local/include
    /opt/local/include
    /sw/include
    ENV CPATH
  PATH_SUFFIXES
    libelf)

find_library (
  LIBOMPTARGET_DEP_LIBELF_LIBRARIES
  NAMES
    elf
  PATHS
    /usr/lib
    /usr/local/lib
    /opt/local/lib
    /sw/lib
    ENV LIBRARY_PATH
    ENV LD_LIBRARY_PATH)
    
set(LIBOMPTARGET_DEP_LIBELF_INCLUDE_DIRS ${LIBOMPTARGET_DEP_LIBELF_INCLUDE_DIR})
find_package_handle_standard_args(
  LIBOMPTARGET_DEP_LIBELF 
  DEFAULT_MSG
  LIBOMPTARGET_DEP_LIBELF_LIBRARIES
  LIBOMPTARGET_DEP_LIBELF_INCLUDE_DIRS)

mark_as_advanced(
  LIBOMPTARGET_DEP_LIBELF_INCLUDE_DIRS 
  LIBOMPTARGET_DEP_LIBELF_LIBRARIES)
  
################################################################################
# Looking for libffi...
################################################################################
find_package(PkgConfig)

pkg_check_modules(LIBOMPTARGET_SEARCH_LIBFFI QUIET libffi)

find_path (
  LIBOMPTARGET_DEP_LIBFFI_INCLUDE_DIR
  NAMES
    ffi.h
  HINTS
    ${LIBOMPTARGET_SEARCH_LIBFFI_INCLUDEDIR}
    ${LIBOMPTARGET_SEARCH_LIBFFI_INCLUDE_DIRS}
  PATHS
    /usr/include
    /usr/local/include
    /opt/local/include
    /sw/include
    ENV CPATH)

# Don't bother look for the library if the header files were not found.
if (LIBOMPTARGET_DEP_LIBFFI_INCLUDE_DIR)
  find_library (
      LIBOMPTARGET_DEP_LIBFFI_LIBRARIES
    NAMES
      ffi
    HINTS
      ${LIBOMPTARGET_SEARCH_LIBFFI_LIBDIR}
      ${LIBOMPTARGET_SEARCH_LIBFFI_LIBRARY_DIRS}
    PATHS
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
      ENV LIBRARY_PATH
      ENV LD_LIBRARY_PATH)
endif()

set(LIBOMPTARGET_DEP_LIBFFI_INCLUDE_DIRS ${LIBOMPTARGET_DEP_LIBFFI_INCLUDE_DIR})
find_package_handle_standard_args(
  LIBOMPTARGET_DEP_LIBFFI 
  DEFAULT_MSG
  LIBOMPTARGET_DEP_LIBFFI_LIBRARIES
  LIBOMPTARGET_DEP_LIBFFI_INCLUDE_DIRS)

mark_as_advanced(
  LIBOMPTARGET_DEP_LIBFFI_INCLUDE_DIRS 
  LIBOMPTARGET_DEP_LIBFFI_LIBRARIES)
  
################################################################################
# Looking for CUDA...
################################################################################

# How CUDA is detected should be consistent with Clang.
#
# /usr/lib/cuda is a special case for Debian/Ubuntu to have nvidia-cuda-toolkit
# work out of the box. More info on http://bugs.debian.org/882505
find_package(CUDA QUIET PATHS "/usr/lib/cuda")

# Try to get the highest Nvidia GPU architecture the system supports
if (CUDA_FOUND)
  cuda_select_nvcc_arch_flags(CUDA_ARCH_FLAGS)
  string(REGEX MATCH "sm_([0-9]+)" CUDA_ARCH_MATCH_OUTPUT ${CUDA_ARCH_FLAGS})
  if (NOT DEFINED CUDA_ARCH_MATCH_OUTPUT OR "${CMAKE_MATCH_1}" LESS 35)
    libomptarget_warning_say("Setting Nvidia GPU architecture support for OpenMP target runtime library to sm_35 by default")
    set(LIBOMPTARGET_DEP_CUDA_ARCH "35")
  else()
    set(LIBOMPTARGET_DEP_CUDA_ARCH "${CMAKE_MATCH_1}")
  endif()
endif()

set(LIBOMPTARGET_DEP_CUDA_FOUND ${CUDA_FOUND})
set(LIBOMPTARGET_DEP_CUDA_INCLUDE_DIRS ${CUDA_INCLUDE_DIRS})

mark_as_advanced(
  LIBOMPTARGET_DEP_CUDA_FOUND 
  LIBOMPTARGET_DEP_CUDA_INCLUDE_DIRS)

################################################################################
# Looking for CUDA Driver API... (needed for CUDA plugin)
################################################################################

find_library (
    LIBOMPTARGET_DEP_CUDA_DRIVER_LIBRARIES
  NAMES
    cuda
  PATHS
    /lib64)

# There is a libcuda.so in lib64/stubs that can be used for linking.
if (NOT LIBOMPTARGET_DEP_CUDA_DRIVER_LIBRARIES AND CUDA_FOUND)
  get_filename_component(CUDA_LIBDIR "${CUDA_cudart_static_LIBRARY}" DIRECTORY)
  find_library(
      LIBOMPTARGET_DEP_CUDA_DRIVER_LIBRARIES
    NAMES
      cuda
    HINTS
      "${CUDA_LIBDIR}/stubs")
endif()

find_package_handle_standard_args(
  LIBOMPTARGET_DEP_CUDA_DRIVER
  DEFAULT_MSG
  LIBOMPTARGET_DEP_CUDA_DRIVER_LIBRARIES)

mark_as_advanced(LIBOMPTARGET_DEP_CUDA_DRIVER_LIBRARIES)

################################################################################
# Looking for VEO...
################################################################################

find_path (
  LIBOMPTARGET_DEP_VEO_INCLUDE_DIR
  NAMES
    ve_offload.h
  PATHS
    /usr/include
    /usr/local/include
    /opt/local/include
    /sw/include
    /opt/nec/ve/veos/include
    ENV CPATH
  PATH_SUFFIXES
    libveo)

find_library (
  LIBOMPTARGET_DEP_VEO_LIBRARIES
  NAMES
    veo
  PATHS
    /usr/lib
    /usr/local/lib
    /opt/local/lib
    /sw/lib
    /opt/nec/ve/veos/lib64
    ENV LIBRARY_PATH
    ENV LD_LIBRARY_PATH)

find_library(
  LIBOMPTARGET_DEP_VEOSINFO_LIBRARIES
  NAMES
    veosinfo
  PATHS
    /usr/lib
    /usr/local/lib
    /opt/local/lib
    /sw/lib
    /opt/nec/ve/veos/lib64
    ENV LIBRARY_PATH
    ENV LD_LIBRARY_PATH)

set(LIBOMPTARGET_DEP_VEO_INCLUDE_DIRS ${LIBOMPTARGET_DEP_VEO_INCLUDE_DIR})
find_package_handle_standard_args(
  LIBOMPTARGET_DEP_VEO
  DEFAULT_MSG
  LIBOMPTARGET_DEP_VEO_LIBRARIES
  LIBOMPTARGET_DEP_VEOSINFO_LIBRARIES
  LIBOMPTARGET_DEP_VEO_INCLUDE_DIRS)

mark_as_advanced(
  LIBOMPTARGET_DEP_VEO_FOUND
  LIBOMPTARGET_DEP_VEO_INCLUDE_DIRS)

