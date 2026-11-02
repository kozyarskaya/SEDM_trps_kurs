# Модульные тесты СЭДМ (Qt Test). Запуск: ctest --output-on-failure

function(sedm_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE sedm_core Qt5::Test)
    target_compile_options(${name} PRIVATE ${SEDM_WARNINGS})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

sedm_add_test(tst_domain)

# Интеграционные тесты на сервере PostgreSQL (параметры сервера — tests/TestDatabase.h)
sedm_add_test(tst_services)
target_link_libraries(tst_services PRIVATE QXlsx::QXlsx)

# Тесты интерфейса программы дегустатора (платформа offscreen, тестовый сервер)
sedm_add_test(tst_taster_ui)
target_link_libraries(tst_taster_ui PRIVATE sedm_taster_ui sedm_demo_seeder)

# Тесты интерфейса программы администратора (платформа offscreen, тестовый сервер)
sedm_add_test(tst_admin_ui)
target_link_libraries(tst_admin_ui PRIVATE sedm_admin_ui sedm_demo_seeder)

# Испытания производительности на объёме ТЗ (500 образцов, 50 команд)
sedm_add_test(tst_performance)
target_link_libraries(tst_performance PRIVATE sedm_demo_seeder)
if(WIN32)
    target_link_libraries(tst_performance PRIVATE psapi)
endif()

# Снимки окон и диалогов для подробных руководств (не тест: запускается вручную,
# sedm_screens admin|taster <каталог>; тестовый сервер — как у тестов интерфейса)
add_executable(sedm_screens sedm_screens.cpp)
target_link_libraries(sedm_screens PRIVATE sedm_admin_ui sedm_taster_ui sedm_demo_seeder Qt5::Test)
target_compile_options(sedm_screens PRIVATE ${SEDM_WARNINGS})
