pkg_check_modules(CURL REQUIRED libcurl>=7.15)
set(NETWORK_INCLUDE_DIRS ${CURL_INCLUDE_DIRS})
set(NETWORK_LIBRARIES ${CURL_LIBRARIES})
