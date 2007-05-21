SET(DIR "${CMAKE_CURRENT_SOURCE_DIR}/util")
TRY_RUN(SLANG_RUN_RESULT SLANG_COMPILE_RESULT
	${CMAKE_CURRENT_BINARY_DIR}
	${DIR}/slang-version.c
	OUTPUT_VARIABLE SLANG_JUNK)

STRING(REGEX MATCH "SLANG_VERSION=([0-9]+)" SLANG_VERSION "${SLANG_JUNK}")
#STRING(LENGTH <string> <output variable>)
#STRING(SUBSTRING <string> <begin> <length> <output variable>)
STRING(LENGTH "${SLANG_VERSION}" LEN)
MATH(EXPR LEN "${LEN} - 14")
STRING(SUBSTRING "${SLANG_VERSION}" 14 ${LEN} SLANG_VERSION)
