cmake_minimum_required(VERSION 3.10)

# Project name
project(shmCpp)

# Generate compile_commands.json for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Request C++11 standard features for compiling tests
set(CMAKE_CXX_STANDARD 11)

# Make sure the compiler looks here on MacOS
if(APPLE)
    include_directories(/usr/local/include)
    link_directories(/usr/local/lib)
endif()

# Include the include/ directory for downstream projects
include_directories(${PROJECT_SOURCE_DIR}/include)


### TESTING ###

# Enable `make test`
enable_testing()

# Get test source files
file(GLOB TEST_SRCS ${PROJECT_SOURCE_DIR}/test/*.cpp)

# Run through each test file
foreach(testSrc ${TEST_SRCS})
    # Get extension-less file name
    get_filename_component(testFileName ${testSrc} NAME_WE)
    # Make test name
    set(testName ${testFileName})
    # Add target
    add_executable(${testName} ${testSrc})
    # Link to realtime library on Linux
    if (UNIX AND NOT APPLE)
        target_link_libraries(${testName} rt)
    endif()
    # Put test executables in their own directory
    set_target_properties(${testName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/test/bin)
endforeach(testSrc)

# Add test to execution
add_test(
    NAME shmCpp_test
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/test/bin
    COMMAND shmCpp_test_1
)
