##===- source/Plugins/Platform/Makefile --------------------*- Makefile -*-===##
#
#                     The LLVM Compiler Infrastructure
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
##===----------------------------------------------------------------------===##

LLDB_LEVEL := ../../..

include $(LLDB_LEVEL)/../../Makefile.config

PARALLEL_DIRS := gdb-server MacOSX Linux FreeBSD POSIX Windows Kalimba Android

# ifeq ($(HOST_OS),Darwin)
#   DIRS += MacOSX
# endif
#
# ifeq ($(HOST_OS),Linux)
#   DIRS += Linux
# endif
#
# ifeq ($(HOST_OS),FreeBSD)
#   DIRS += FreeBSD
# endif
# 
# ifeq ($(HOST_OS),GNU/kFreeBSD)
#   DIRS += FreeBSD
# endif

include $(LLDB_LEVEL)/Makefile
