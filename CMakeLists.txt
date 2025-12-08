cmake_minimum_required(VERSION 3.20)
project(SFML_EXAMPLES)
set (CMAKE_CXX_STANDARD 20)

set(SFML_STATIC_LIBRARIES TRUE)

if(MSVC)
    set(SFML_DIR "C:/Program Files/SFML_3.0.2/lib/cmake/SFML")
endif(MSVC)

    set(DEFS "PATH_IMG=\"${CMAKE_CURRENT_SOURCE_DIR}/img/\"")
    add_compile_definitions(${DEFS})

find_package(SFML 3 COMPONENTS Window Graphics System REQUIRED)
link_libraries(SFML::Graphics SFML::Window SFML::System)



add_executable (ATC_Simulation 
    "ATC_Simulation/main.cpp" 
    "ATC_Simulation/TourControle.cpp" 
    "ATC_Simulation/Avion.cpp"
)

# Support pour le multithreading
find_package(Threads REQUIRED)
target_link_libraries(ATC_Simulation PRIVATE Threads::Threads)
