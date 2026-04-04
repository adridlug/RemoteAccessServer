cmake_minimum_required(VERSION 3.16)

project(remote-access-client LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(remote-access-client
  remote_client.c
  helpers.c
  socks5_proxy.c
  ssh_client_impl.c
  ssh_server_impl.c
  http_parser.c
)

set_target_properties(remote-access-client PROPERTIES OUTPUT_NAME "client")

target_include_directories(remote-access-client PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}"
)

find_package(libssh CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(PThreads4W REQUIRED)

target_link_libraries(remote-access-client PRIVATE
  ssh
  PThreads4W::PThreads4W
  OpenSSL::Crypto
  ws2_32
  user32
  crypt32
  shell32
  advapi32
  netapi32
  urlmon
  wininet
)

target_compile_options(remote-access-client PRIVATE /W4)