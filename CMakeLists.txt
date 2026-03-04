cmake_minimum_required(VERSION 3.10)
project(PolynomialOptics)

# C++ 표준 설정
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. 헤더 전용 라이브러리 설정 (INTERFACE 사용)
add_library(PolyOptics INTERFACE)

# 2. 헤더 파일 경로들 지정
# 사용자가 #include <TruncPoly/xxx.hh> 식으로 호출하므로 
# 이 폴더들이 들어있는 루트 디렉터리와 include 폴더를 지정합니다.
target_include_directories(PolyOptics INTERFACE 
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# 3. 예제 파일 빌드 (실제 소스 파일)
# 예제 파일들은 루트 디렉터리에 있으므로 바로 추가합니다.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Example_BasicArithmetic.cpp")
    add_executable(Example_BasicArithmetic Example_BasicArithmetic.cpp)
    target_link_libraries(Example_BasicArithmetic PRIVATE PolyOptics)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Example_PostprocessImage.cpp")
    add_executable(Example_PostprocessImage Example_PostprocessImage.cpp)
    target_link_libraries(Example_PostprocessImage PRIVATE PolyOptics)
endif()