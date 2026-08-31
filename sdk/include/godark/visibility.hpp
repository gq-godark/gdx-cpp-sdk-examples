#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
  #if defined(GODARK_BUILDING_LIBRARY)
    #define GODARK_API __declspec(dllexport)
  #else
    #define GODARK_API __declspec(dllimport)
  #endif
#else
  #if defined(GODARK_BUILDING_LIBRARY)
    #if defined(__GNUC__) || defined(__clang__)
      #define GODARK_API __attribute__((visibility("default")))
    #else
      #define GODARK_API
    #endif
  #else
    #define GODARK_API
  #endif
#endif
