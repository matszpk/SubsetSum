####
# CMakeLists.txt - maker for program
# Mateusz Szpakowski
####

CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

SET(TEST_LINK_LIBRARIES ${GLIBMM_LIBRARIES} ${OPENCL_LIBRARIES}
    ${ADDITIONAL_LIBRARIES} ${CPPUNIT_LIBRARIES})

ADD_DEFINITIONS(${CPPUNIT_CFLAGS})

ADD_EXECUTABLE(SmallestRegionTest
        SmallestRegionTest.cpp ../Int128.cpp)

TARGET_LINK_LIBRARIES(SmallestRegionTest ${TEST_LINK_LIBRARIES})

ADD_EXECUTABLE(OrderRangesTest
        OrderRangesTest.cpp ../Int128.cpp)

TARGET_LINK_LIBRARIES(OrderRangesTest ${TEST_LINK_LIBRARIES})

ADD_EXECUTABLE(HashWorkGenNHTest
        HashWorkGenNHTest.cpp
        ../Int128.cpp
        ../Queue.cpp
        ../PushPop.cpp
        ../Util.cpp)

TARGET_LINK_LIBRARIES(HashWorkGenNHTest ${TEST_LINK_LIBRARIES})

ADD_EXECUTABLE(NodeHashGenPerf NodeHashGenPerf.cpp
        ../Int128.cpp
        ../Controllers.cpp
        ../NodeHashGen.cpp
        ../Queue.cpp
        ../Util.cpp
        ../PushPop.cpp)

TARGET_LINK_LIBRARIES(NodeHashGenPerf ${LINK_LIBRARIES})

ADD_TEST(SmallestRegionTest SmallestRegionTest)
ADD_TEST(OrderRangesTest OrderRangesTest)
