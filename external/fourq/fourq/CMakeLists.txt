# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

target_sources(fourq
  PUBLIC
    FILE_SET fourq_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}
    FILES
      ${PROJECT_SOURCE_DIR}/fourq/FourQ.h
      ${PROJECT_SOURCE_DIR}/fourq/FourQ_api.h
  PRIVATE
    FILE_SET fourq_private_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}
    FILES
      ${PROJECT_SOURCE_DIR}/fourq/FourQ_define.h
      ${PROJECT_SOURCE_DIR}/fourq/FourQ_internal.h
      ${PROJECT_SOURCE_DIR}/fourq/FourQ_params.h
      ${PROJECT_SOURCE_DIR}/fourq/FourQ_tables.h
      ${PROJECT_SOURCE_DIR}/fourq/table_lookup.h
  PRIVATE
    crypto_util.c
    eccp2_core.c
    eccp2_no_endo.c
    eccp2.c
    hash_to_curve.c
    schnorrq.c)

if(FOURQ_AMD64)
    add_subdirectory(amd64)
elseif(FOURQ_ARM64)
    add_subdirectory(arm64)
else()
    add_subdirectory(generic)
endif()

