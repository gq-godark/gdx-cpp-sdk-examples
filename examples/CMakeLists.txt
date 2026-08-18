function(godark_example name)
    add_executable(${name} ${ARGN})
    target_compile_features(${name} PRIVATE cxx_std_20)
    target_link_libraries(${name} PRIVATE godark::godark)
endfunction()

# --- Trading examples (public godark.hpp) ---
godark_example(quickstart quickstart.cpp)
godark_example(full_trader_example full_trader_example.cpp)
godark_example(rest_client_example rest_client_example.cpp)
