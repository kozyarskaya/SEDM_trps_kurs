# sedm_taster — программа дегустатора: вход по фамилии и PIN-коду, корзины команды,
# оценивание образцов ползунками-сотами, просмотр своих оценок.
# Окна собраны в статическую библиотеку sedm_taster_ui, чтобы их можно было
# проверять автоматическими тестами интерфейса (tests/tst_taster_ui).

add_library(sedm_taster_ui STATIC
    TasterWindow.h      TasterWindow.cpp
    LoginPage.h         LoginPage.cpp
    PinPad.h            PinPad.cpp
    BasketListPage.h    BasketListPage.cpp
    EvaluationPage.h    EvaluationPage.cpp
    ScoreRow.h          ScoreRow.cpp
    ReviewPage.h        ReviewPage.cpp
    ${CMAKE_SOURCE_DIR}/resources/taster.qrc
)
target_include_directories(sedm_taster_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sedm_taster_ui PUBLIC sedm_widgets)
target_compile_options(sedm_taster_ui PRIVATE ${SEDM_WARNINGS})

add_executable(sedm_taster WIN32 main.cpp ${CMAKE_SOURCE_DIR}/resources/taster.rc)
target_link_libraries(sedm_taster PRIVATE sedm_taster_ui)
target_compile_definitions(sedm_taster PRIVATE SEDM_VERSION="${PROJECT_VERSION}")
target_compile_options(sedm_taster PRIVATE ${SEDM_WARNINGS})
