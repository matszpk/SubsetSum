####
# CMakeLists.txt - maker for program
# Mateusz Szpakowski
####

CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

SET(LINK_LIBRARIES ${GLIBMM_LIBRARIES} ${OPENCL_LIBRARIES}
        ${GTHREAD_LIBRARIES} ${POPT_LIBRARIES} ${ADDITIONAL_LIBRARIES})

IF(HAVE_OPENCL)
    SET(CL_SOURCES NaiveKernel.cl
        Naive64BitKernel.cl
        HashKernel.cl
        HashKernelGroups.cl
        HashKernelBigHash.cl
        HashKernelBHGroups.cl)
    FOREACH(CLSRC IN LISTS CL_SOURCES)
        STRING(REGEX REPLACE "\\." "_" CLSRC_CPP "${CLSRC}")
        SET(CLSRC_CPP "${PROJECT_BINARY_DIR}/${CLSRC_CPP}.cpp")
        
        ADD_CUSTOM_COMMAND(OUTPUT "${CLSRC_CPP}"
                COMMAND xxd -i "${CLSRC}" "${CLSRC_CPP}"
                WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/src"
                DEPENDS "${PROJECT_SOURCE_DIR}/src/${CLSRC}"
                COMMENT "Creating inline ${CLSRC}")
        
        LIST(APPEND CLCPP_SOURCES "${CLSRC_CPP}")
    ENDFOREACH(CLSRC IN LISTS CL_SOURCES)
ENDIF(HAVE_OPENCL)

IF(HAVE_SSE2)
    LIST(APPEND CPU_SRCS NaiveMethodSSE2.cpp)
    SET_SOURCE_FILES_PROPERTIES(NaiveMethodSSE2.cpp PROPERTIES COMPILE_FLAGS "-msse2")
ENDIF(HAVE_SSE2)
IF(HAVE_SSE4)
    LIST(APPEND CPU_SRCS NaiveMethodSSE4.cpp)
    SET_SOURCE_FILES_PROPERTIES(NaiveMethodSSE4.cpp PROPERTIES COMPILE_FLAGS "-msse4")
ENDIF(HAVE_SSE4)

ADD_EXECUTABLE(SubsetSum
    main.cpp
    Int128.cpp
    Controllers.cpp
    HashWorkers.cpp
    NaiveWorkers.cpp
    NodeHashGen.cpp
    PushPop.cpp
    Queue.cpp
    Util.cpp
    X86CpuId.cpp
    ${CPU_SRCS}
    ${CLCPP_SOURCES})

TARGET_LINK_LIBRARIES(SubsetSum ${LINK_LIBRARIES})

INSTALL(TARGETS SubsetSum RUNTIME DESTINATION bin)

IF(BUILD_TESTS)
    ADD_SUBDIRECTORY(tests)
ENDIF(BUILD_TESTS)