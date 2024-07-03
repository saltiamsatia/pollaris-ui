cmake_minimum_required(VERSION 3.5)

project(PollarisGui LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# QtCreator supports the following variables for Android, which are identical to qmake Android variables.
# Check http://doc.qt.io/qt-5/deployment-android.html for more information.
# They need to be set before the find_package(Qt6 ...) call.

#if(ANDROID)
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
#    if (ANDROID_ABI STREQUAL "armeabi-v7a")
#        set(ANDROID_EXTRA_LIBS
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libcrypto.so
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libssl.so)
#    endif()
#endif()

find_package(Qt6 COMPONENTS Core Quick LinguistTools REQUIRED)

# Pull in FC
if (DEFINED FC_LIBRARY_PATH)
    message("Using pre-built FC at ${FC_LIBRARY_PATH}")
    link_directories("${FC_LIBRARY_PATH}")
    link_directories("${FC_LIBRARY_PATH}/secp256k1")
    find_package(Threads)
    include_directories("vendor/fc/include")
    set(FC_LIBRARIES fc secp256k1 gmp boost_chrono boost_date_time boost_iostreams boost_filesystem Threads::Threads crypto ssl)
else()
    message("Compiling FC within build")
    add_subdirectory("vendor/fc")
    set(FC_LIBRARIES fc)
endif()

# Pull in vendor libraries
include_directories("vendor")

# Define the Qappa library

find_package(Qt6 REQUIRED COMPONENTS Qml)
qt_standard_project_setup(REQUIRES 6.7)
add_subdirectory("Qappa")


include_directories(cpp)
set(POLLARIS_SOURCES
    cpp/Infrastructure/typelist.hpp
    cpp/Infrastructure/reflectors.hpp

    main.cpp
    qml.qrc

    cpp/Action.cpp
    cpp/Action.hpp
    cpp/Task.cpp
    cpp/Task.hpp
    cpp/FpsTimer.hpp
    cpp/Assistant.cpp
    cpp/Assistant.hpp
    cpp/MutableTransaction.cpp
    cpp/MutableTransaction.hpp
    cpp/SignableTransaction.cpp
    cpp/SignableTransaction.hpp
    cpp/BroadcastableTransaction.cpp
    cpp/BroadcastableTransaction.hpp
    cpp/BlockchainInterface.cpp
    cpp/BlockchainInterface.hpp
    cpp/AbstractTableInterface.cpp
    cpp/AbstractTableInterface.hpp
    cpp/TableSupport.hpp
    cpp/AbstractTable.hpp
    cpp/TlsPskSession.cpp
    cpp/TlsPskSession.hpp
    cpp/Tables.hpp
    cpp/Strings.cpp
    cpp/Strings.hpp
    cpp/Enums.hpp
    cpp/Dnmx.hpp
    )

# Put KeyManager.cpp alone in a static library so it can link against fc but nothing else can
add_library(KeyManager STATIC cpp/KeyManager.cpp cpp/KeyManager.hpp)
target_link_libraries(KeyManager PRIVATE ${FC_LIBRARIES} Qt6::Core Qt6::Quick)

if(ANDROID)
    add_library(PollarisGui SHARED
      ${POLLARIS_SOURCES}
    )
else()
    add_executable(PollarisGui
      ${POLLARIS_SOURCES}
    )
endif()

target_compile_definitions(PollarisGui
  PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG> QT_MESSAGELOGCONTEXT)
target_link_libraries(PollarisGui
  PRIVATE Qappa KeyManager Qt6::Core Qt6::Quick)

