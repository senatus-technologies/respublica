# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

target_sources(fourq
  PRIVATE
    FILE_SET generic_private_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}
    FILES
      ${PROJECT_SOURCE_DIR}/fourq/generic/fp.h)

