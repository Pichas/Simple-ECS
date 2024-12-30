project(Example LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


file(GLOB_RECURSE GAME_HEADERS CONFIGURE_DEPENDS "*.h")
file(GLOB_RECURSE GAME_SRC CONFIGURE_DEPENDS "*.cpp")

set(SOURCE_FILES
    ${GAME_HEADERS}
    ${GAME_SRC}
)


add_executable(${PROJECT_NAME}
    ${SOURCE_FILES}
)


target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        SimpleECS
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(${PROJECT_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE)

if(LINUX)
    target_compile_options(${PROJECT_NAME} PRIVATE -fsanitize=address -fsanitize=undefined)
    target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address -fsanitize=undefined)
endif()