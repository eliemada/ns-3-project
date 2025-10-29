build_lib_example(
  NAME http-cache-scenario
  SOURCE_FILES http-cache-scenario.cc
  LIBRARIES_TO_LINK
    ${libhttp-cache}
    ${libinternet}
    ${libapplications}
    ${libpoint-to-point}
)
