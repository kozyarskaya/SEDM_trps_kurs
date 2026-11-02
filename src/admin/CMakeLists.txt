# sedm_admin — программа администратора: реестр участников и номинаций, образцы и шифры,
# корзины, команды и дегустаторы, сеанс, мониторинг, итоги и ведомость XLSX,
# резервные копии, журнал действий.
# Окна собраны в статическую библиотеку sedm_admin_ui (для автоматических тестов интерфейса).

add_library(sedm_admin_ui STATIC
    AdminWindow.h          AdminWindow.cpp
    AdminPage.h            AdminPage.cpp
    RecordFilter.h         RecordFilter.cpp
    ParticipantsPage.h     ParticipantsPage.cpp
    ParticipantDialog.h    ParticipantDialog.cpp
    ImportDialog.h         ImportDialog.cpp
    SamplesPage.h          SamplesPage.cpp
    BasketsPage.h          BasketsPage.cpp
    TeamsPage.h            TeamsPage.cpp
    CardPrinter.h          CardPrinter.cpp
    SessionPage.h          SessionPage.cpp
    MonitoringPage.h       MonitoringPage.cpp
    ResultsPage.h          ResultsPage.cpp
    BackupPage.h           BackupPage.cpp
    AutoBackupScheduler.h  AutoBackupScheduler.cpp
    AuditDialog.h          AuditDialog.cpp
    SetupWizard.h          SetupWizard.cpp
    ${CMAKE_SOURCE_DIR}/resources/admin.qrc
)
target_include_directories(sedm_admin_ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sedm_admin_ui PUBLIC sedm_widgets Qt5::PrintSupport)
target_compile_options(sedm_admin_ui PRIVATE ${SEDM_WARNINGS})

add_executable(sedm_admin WIN32 main.cpp ${CMAKE_SOURCE_DIR}/resources/admin.rc)
target_link_libraries(sedm_admin PRIVATE sedm_admin_ui)
target_compile_definitions(sedm_admin PRIVATE SEDM_VERSION="${PROJECT_VERSION}")
target_compile_options(sedm_admin PRIVATE ${SEDM_WARNINGS})
