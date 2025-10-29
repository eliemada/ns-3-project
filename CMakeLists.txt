cmake_minimum_required(VERSION 3.16)

set(source_files
  model/http-header.cc
  model/http-client-app.cc
  model/http-cache-app.cc
  model/http-origin-app.cc
)

set(header_files
  model/http-header.h
  model/http-client-app.h
  model/http-cache-app.h
  model/http-origin-app.h
)

build_lib(
  LIBNAME http-cache
  SOURCE_FILES ${source_files}
  HEADER_FILES ${header_files}
  LIBRARIES_TO_LINK
    ${libinternet}
    ${libapplications}
)

if(NS3_ENABLE_EXAMPLES)
  add_subdirectory(examples)
endif()
