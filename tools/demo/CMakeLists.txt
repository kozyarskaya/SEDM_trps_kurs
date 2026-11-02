# sedm_demo — консольная утилита: демонстрационная база СЭДМ для показа программы
# и испытаний производительности (заполнение через прикладные сервисы ядра).

add_library(sedm_demo_seeder STATIC DemoSeeder.h DemoSeeder.cpp)
target_include_directories(sedm_demo_seeder PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sedm_demo_seeder PUBLIC sedm_core)
target_compile_options(sedm_demo_seeder PRIVATE ${SEDM_WARNINGS})

add_executable(sedm_demo main.cpp)
target_link_libraries(sedm_demo PRIVATE sedm_demo_seeder)
target_compile_options(sedm_demo PRIVATE ${SEDM_WARNINGS})
