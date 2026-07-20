cmake_minimum_required(VERSION 3.16)

option(BUILD_VITA "Build for PS Vita" OFF)

if(BUILD_VITA)
  if(NOT DEFINED ENV{VITASDK})
    message(FATAL_ERROR "VITASDK env var must point to the Vita SDK")
  endif()
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain")
endif()

project(neonfall C)
set(CMAKE_C_STANDARD 99)

set(SRC
  src/main.c
  src/game.c
  src/render.c
  src/batch.c
)

if(BUILD_VITA)
  include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)
  set(VITA_APP_NAME "Neonfall")
  set(VITA_TITLEID "NEON00001")
  set(VITA_VERSION "01.00")

  add_executable(neonfall ${SRC} src/platform_vita.c)
  target_compile_definitions(neonfall PRIVATE VITA)
  target_compile_options(neonfall PRIVATE -O2 -Wall)
  target_link_libraries(neonfall
    vitaGL vitashark SceShaccCgExt mathneon taihen_stub
    SceShaccCg_stub SceDisplay_stub SceGxm_stub SceCtrl_stub
    SceCommonDialog_stub SceAppMgr_stub SceSysmodule_stub
    SceTouch_stub SceKernelDmacMgr_stub m
  )
  vita_create_self(eboot.bin neonfall UNSAFE)
  vita_create_vpk(neonfall.vpk ${VITA_TITLEID} eboot.bin
    VERSION ${VITA_VERSION}
    NAME ${VITA_APP_NAME}
  )
else()
  execute_process(COMMAND sdl2-config --cflags OUTPUT_VARIABLE SDL_CFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
  execute_process(COMMAND sdl2-config --libs   OUTPUT_VARIABLE SDL_LIBS   OUTPUT_STRIP_TRAILING_WHITESPACE)
  separate_arguments(SDL_CFLAGS_LIST UNIX_COMMAND "${SDL_CFLAGS}")
  separate_arguments(SDL_LIBS_LIST   UNIX_COMMAND "${SDL_LIBS}")

  add_executable(neonfall ${SRC} src/platform_desktop.c)
  target_compile_options(neonfall PRIVATE -O2 -Wall ${SDL_CFLAGS_LIST})
  target_link_options(neonfall PRIVATE ${SDL_LIBS_LIST})
  if(APPLE)
    target_link_libraries(neonfall "-framework OpenGL" m)
  else()
    target_link_libraries(neonfall GL m)
  endif()
endif()
