cmake_minimum_required(VERSION 3.20)

project(SimpleECS VERSION 1.4.0 LANGUAGES CXX)

option(ECS_FINAL "Final build without any debug info" OFF)
option(ECS_ENABLE_IMGUI "Enable ImGui related code" OFF)
option(ECS_ENABLE_PROFILER "Enable tracy profiler" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(H_FILES
    simple-ecs/base_system.h
    simple-ecs/components.h
    simple-ecs/entity.h
    simple-ecs/entity_iterator.h
    simple-ecs/entity_debug.cpp
    simple-ecs/entity_debug.h
    simple-ecs/filter.h
    simple-ecs/observer.h
    simple-ecs/registrant.h
    simple-ecs/registry.h
    simple-ecs/serializer.cpp
    simple-ecs/serializer.h
    simple-ecs/tools/sparse_set.h
    simple-ecs/storage.h
    simple-ecs/utils.h
    simple-ecs/world.cpp
    simple-ecs/world.h
)

set(CPP_FILES
    simple-ecs/entity_debug.cpp
    simple-ecs/serializer.cpp
    simple-ecs/world.cpp
)

set(SOURCE_FILES
    ${H_FILES}
    ${CPP_FILES}
)

add_subdirectory(external)

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)
target_link_libraries(${PROJECT_NAME} PUBLIC spdlog CT-Utils Tasks TempBuffer)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(ECS_ENABLE_PROFILER)
    target_link_libraries(${PROJECT_NAME} PUBLIC TracyClient)
endif()


if(ECS_FINAL)
    # set_target_properties(${PROJECT_NAME} PROPERTIES UNITY_BUILD ON)
    target_compile_definitions(${PROJECT_NAME} PUBLIC ECS_FINAL)
    set(ECS_ENABLE_IMGUI OFF)
endif()

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        UNITY_BUILD_MODE BATCH
        UNITY_BUILD_BATCH_SIZE 0
)

if(ECS_ENABLE_IMGUI)
    target_compile_definitions(${PROJECT_NAME} PUBLIC ECS_ENABLE_IMGUI)

    target_link_libraries(${PROJECT_NAME}
        PUBLIC
            ImGui # EntityDebugSystem uses it
            ImPlot # EntityDebugSystem uses it
    )
endif()

if (${PROJECT_IS_TOP_LEVEL})
    message("${PROJECT_NAME} example is enabled")
    add_subdirectory(example)
endif()