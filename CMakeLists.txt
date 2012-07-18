project(sixpair++)

cmake_minimum_required(VERSION 2.8)

add_library(libusb SHARED IMPORTED)

set_property(TARGET libusb PROPERTY IMPORTED_LOCATION /usr/lib64/libusb-1.0.so)

add_executable(sixpair++ main.cpp)

target_link_libraries(sixpair++ libusb)

#install(TARGETS sixpair++ RUNTIME DESTINATION bin)
