#!/bin/bash
file_path=${PATH_TO_DATA}"/"${DATA_FILE}
chmod 700 $file_path
${FULL_EXE_NAME} $file_path ${PATTERN} ${BOUND} &
