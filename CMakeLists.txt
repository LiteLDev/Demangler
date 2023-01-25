cmake_minimum_required(VERSION 3.24)
project(Demangler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Release)

add_library(Demangler STATIC src/MicrosoftDemangle.cpp src/MicrosoftDemangleNodes.cpp)

target_compile_definitions(Demangler PUBLIC NDEBUG _LIB UNICODE _UNICODE)

target_compile_options(
        Demangler PUBLIC
        /permissive- /GS /GL /W3 /Gy /Zc:wchar_t /Zi /Gm- /O2 /sdl /Zc:inline
        /fp:precise /errorReport:prompt /WX- /Zc:forScope /Gd /Oi /MD /FC /EHsc /nologo /diagnostics:column
)
