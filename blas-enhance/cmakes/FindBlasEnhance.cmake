set(BLAS_ENHANCE_PROJECT_NAME "blas-enhance")
unset(BLAS_ENHANCE_ROOT)
find_path(BLAS_ENHANCE_ROOT NAMES ${BLAS_ENHANCE_PROJECT_NAME} HINTS ${BOLT_ROOT} $ENV{BOLT_ROOT})
set(BLAS_ENHANCE_ROOT "${BLAS_ENHANCE_ROOT}/${BLAS_ENHANCE_PROJECT_NAME}")

set(BLAS_ENHANCE_INCLUDE_DIR "${BLAS_ENHANCE_ROOT}/include")
if (USE_DYNAMIC_LIBRARY)
    set(BLAS_ENHANCE_LIBRARY "${BLAS_ENHANCE_ROOT}/lib/lib${BLAS_ENHANCE_PROJECT_NAME}.so")
else (USE_DYNAMIC_LIBRARY)
    set(BLAS_ENHANCE_LIBRARY "${BLAS_ENHANCE_ROOT}/lib/lib${BLAS_ENHANCE_PROJECT_NAME}.a")
endif (USE_DYNAMIC_LIBRARY)

if (BLAS_ENHANCE_INCLUDE_DIR AND BLAS_ENHANCE_LIBRARY)
    set(BLAS_ENHANCE_FOUND true)
endif (BLAS_ENHANCE_INCLUDE_DIR AND BLAS_ENHANCE_LIBRARY)

if (BLAS_ENHANCE_FOUND)
    include_directories(include ${BLAS_ENHANCE_INCLUDE_DIR})
    message(STATUS "Found ${BLAS_ENHANCE_PROJECT_NAME}.h: ${BLAS_ENHANCE_INCLUDE_DIR}")
    message(STATUS "Found ${BLAS_ENHANCE_PROJECT_NAME}: ${BLAS_ENHANCE_LIBRARY}")
else (BLAS_ENHANCE_FOUND)
    message(FATAL_ERROR "Could not find ${BLAS_ENHANCE_PROJECT_NAME} library")
endif (BLAS_ENHANCE_FOUND)
