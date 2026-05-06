function(godark_example name)
    add_executable(${name} ${ARGN})
    target_compile_features(${name} PRIVATE cxx_std_20)
    target_link_libraries(${name} PRIVATE godark::godark)
endfunction()

# --- WebSocket trading / market data (public godark.hpp) ---
godark_example(quickstart quickstart.cpp)
godark_example(e2e_trading_smoke e2e_trading_smoke.cpp)
godark_example(market_data_example market_data_example.cpp)
godark_example(full_trader_example full_trader_example.cpp)

# REST + encrypted orders (public rest_client.hpp).
godark_example(full_trader_rest full_trader_rest.cpp)
