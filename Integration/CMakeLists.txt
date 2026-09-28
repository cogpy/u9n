# u9n integration layer: GTAngelEcho + EchoSelf-48M ported onto the Deep Tree Echo reservoir.
add_library(u9n_integration INTERFACE)
target_include_directories(u9n_integration INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(u9n_integration INTERFACE Eigen3::Eigen)

add_executable(DeepTreeEchoAGIEval eval/agi_eval.cpp)
target_link_libraries(DeepTreeEchoAGIEval PRIVATE u9n_integration)

if(BUILD_TESTING)
    add_executable(U9nIntegrationTests tests/integration_tests.cpp)
    target_link_libraries(U9nIntegrationTests PRIVATE u9n_integration)
    add_test(NAME U9nIntegrationTests COMMAND U9nIntegrationTests)
    set_tests_properties(U9nIntegrationTests PROPERTIES LABELS "unit;integration")
endif()
