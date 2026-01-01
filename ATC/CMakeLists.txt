cmake_minimum_required(VERSION 3.16)

project(ATC VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SFML_DIR "C:/Program Files/SFML_3.0.2/lib/cmake/SFML")

list(APPEND CMAKE_PREFIX_PATH "C:/Program Files/SFML_3.0.2")

set(SFML_STATIC_LIBRARIES TRUE)

find_package(SFML 3 COMPONENTS Graphics Audio Window System Network REQUIRED)

add_executable(ATC
    main.cpp
    avion.cpp
    monde.cpp
    textures_jeu.cpp
    tour_controle.cpp
)


target_include_directories(ATC PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(ATC PRIVATE SFML::Graphics SFML::Audio SFML::Window SFML::System SFML::Network)

add_custom_command(TARGET ATC POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:ATC>/assets"
)
