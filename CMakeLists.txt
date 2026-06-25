# The minimum CMake version we rely on. 3.16 is old enough to be everywhere but
# new enough to support modern "target-based" CMake (the imported targets below).
cmake_minimum_required(VERSION 3.16)

# Declare the project and that it is a C++ project (CXX). The name becomes the
# default for some generated artifacts; it is not the executable name.
project(oms CXX)

# Use the C++17 standard, and REQUIRE it (don't silently fall back to an older
# standard if the compiler claims it can't do 17). C++17 gives us nice things
# like structured bindings and std::optional we may use later.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Tell CMake where to look for installed packages' "config files". Homebrew put
# the MySQL Connector/C++ CMake config (mysql-concpp-config.cmake) in this
# directory. Appending it here means a plain `cmake -S . -B build` just works,
# with no extra command-line flags to remember.
list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/mysql-connector-c++")

# --- OpenSSL: the keg-only fix ---------------------------------------------
# The connector depends on OpenSSL, but Homebrew's OpenSSL is "keg-only" (kept
# off the default linker path on purpose). Left alone, the connector's config
# would try to link bare `ssl`/`crypto`, which the macOS linker can't find.
# The connector documents a hook: if a target named `mysql::openssl` already
# exists when we call find_package below, it uses ours. So we build it here,
# pointing at the EXACT OpenSSL the connector itself was built against.
find_library(OSSL_SSL    ssl    PATHS /opt/homebrew/opt/openssl@3/lib NO_DEFAULT_PATH REQUIRED)
find_library(OSSL_CRYPTO crypto PATHS /opt/homebrew/opt/openssl@3/lib NO_DEFAULT_PATH REQUIRED)
add_library(mysql_openssl INTERFACE)                 # a name-only "interface" target
target_link_libraries(mysql_openssl INTERFACE ${OSSL_SSL} ${OSSL_CRYPTO})
add_library(mysql::openssl ALIAS mysql_openssl)      # the exact name the config looks for
# ---------------------------------------------------------------------------

# Find the connector package. REQUIRED means: if it's missing, stop now with a
# clear error instead of failing later with a cryptic compile/link message.
# This defines the imported target `mysql::concpp` (the X DevAPI library) and
# sets helper variables like MYSQL_CONCPP_RUNTIME_LIBRARY_DIR.
find_package(mysql-concpp REQUIRED)

# Define our executable. Headers are found via #include; only .cpp files are
# listed here as translation units to compile.
add_executable(oms
  src/main.cpp
  src/Database.cpp
  src/CustomerService.cpp
  src/ProductService.cpp
  src/OrderService.cpp)

# Link our program against the connector. Because `mysql::concpp` is a modern
# "imported target", this ONE line pulls in the right include directory (-I),
# the library to link (-l), and its own dependencies (OpenSSL). No manual paths.
# PRIVATE = only `oms` needs this; nothing that links to oms inherits it.
target_link_libraries(oms PRIVATE mysql::concpp)

# Runtime linking: the connector is a SHARED library (.dylib). At launch, macOS
# must locate it. Recording its directory as an RPATH in our binary guarantees
# that, without needing DYLD_LIBRARY_PATH environment hacks.
if(MYSQL_CONCPP_RUNTIME_LIBRARY_DIR)
  set_target_properties(oms PROPERTIES
    BUILD_RPATH "${MYSQL_CONCPP_RUNTIME_LIBRARY_DIR}"
    INSTALL_RPATH "${MYSQL_CONCPP_RUNTIME_LIBRARY_DIR}")
endif()
