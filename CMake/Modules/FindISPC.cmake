#
# Copyright (c) 2008-2015 the Urho3D project.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#

# Find ISPC binary
#
#  ISPC_DIR
#  ISPC_BIN
#

if (NOT ISPC_DIR)
    if (DEFINED ENV{ISPC_DIR})
        file (TO_CMAKE_PATH $ENV{ISPC_DIR} ISPC_DIR)
    endif ()
endif ()

find_path (ISPC_DIR NAMES ispc ispc.exe DOC "ISPC directory")

include (FindPackageHandleStandardArgs)
find_package_handle_standard_args (ISPC REQUIRED_VARS ISPC_DIR FAIL_MESSAGE "Could NOT find ISPC")
if (ISPC_FOUND)
    if (WIN32)
        set (ISPC_BIN ${ISPC_DIR}/ispc)
    else ()
        set (ISPC_BIN ${ISPC_DIR}/ispc.exe)
    endif ()
endif ()

mark_as_advanced (ISPC_DIR ISPC_BIN)
