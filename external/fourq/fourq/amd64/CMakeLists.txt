# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

# Source files in this directory
if(USE_ASM)
  if(USE_AVX2)
    target_sources(fourq
      PRIVATE
        FILE_SET amd64_private_headers
        TYPE HEADERS
        BASE_DIRS ${PROJECT_SOURCE_DIR}
        FILES
          ${PROJECT_SOURCE_DIR}/fourq/amd64/fp_x64.h
      PRIVATE
        consts.c
        fp2_1271_AVX2.S)
  else()
    target_sources(fourq
      PRIVATE
        FILE_SET amd64_private_headers
        TYPE HEADERS
        BASE_DIRS ${PROJECT_SOURCE_DIR}
        FILES
          ${PROJECT_SOURCE_DIR}/fourq/amd64/fp_x64.h
      PRIVATE
        fp2_1271.S)
    endif()
endif()

