# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

target_sources(fourq
  PRIVATE
    FILE_SET amd64_private_headers
    TYPE HEADERS
    BASE_DIRS ${PROJECT_SOURCE_DIR}
    FILES
      ${PROJECT_SOURCE_DIR}/fourq/arm64/fp_arm64.h)

