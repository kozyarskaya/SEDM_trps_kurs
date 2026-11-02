# sedm_core — ядро СЭДМ (статическая библиотека, без графического интерфейса):
#   domain/    — классы предметной области и её правила;
#   security/  — хэширование bcrypt;
#   data/      — подключение к PostgreSQL, схема БД, репозитории;
#   services/  — прикладные сервисы (сценарии работы администратора и дегустатора);
#   io/        — импорт реестра (CSV, XLSX), экспорт ведомости и журнала, резервные копии.

set(SEDM_DOMAIN_SOURCES
    domain/Criterion.h          domain/Criterion.cpp
    domain/DomainError.h        domain/DomainError.cpp
    domain/Nomination.h         domain/Nomination.cpp
    domain/Participant.h        domain/Participant.cpp
    domain/Sample.h             domain/Sample.cpp
    domain/Basket.h             domain/Basket.cpp
    domain/Team.h               domain/Team.cpp
    domain/Account.h            domain/Account.cpp
    domain/Taster.h             domain/Taster.cpp
    domain/Evaluation.h         domain/Evaluation.cpp
    domain/BasketProgress.h     domain/BasketProgress.cpp
    domain/TastingSession.h     domain/TastingSession.cpp
    domain/AuditEntry.h
    domain/SampleResult.h       domain/SampleResult.cpp
    domain/RankingCalculator.h  domain/RankingCalculator.cpp
    domain/CipherGenerator.h    domain/CipherGenerator.cpp
    domain/LoginGenerator.h     domain/LoginGenerator.cpp
    domain/PinGenerator.h       domain/PinGenerator.cpp
)

set(SEDM_SECURITY_SOURCES
    security/PasswordHasher.h   security/PasswordHasher.cpp
)

set(SEDM_DATA_SOURCES
    data/DbError.h                data/DbError.cpp
    data/ConnectionSettings.h     data/ConnectionSettings.cpp
    data/Database.h               data/Database.cpp
    data/SqlUtil.h
    data/SqlScript.h              data/SqlScript.cpp
    data/SchemaInstaller.h        data/SchemaInstaller.cpp
    data/NominationRepository.h   data/NominationRepository.cpp
    data/ParticipantRepository.h  data/ParticipantRepository.cpp
    data/SampleRepository.h       data/SampleRepository.cpp
    data/BasketRepository.h       data/BasketRepository.cpp
    data/TeamRepository.h         data/TeamRepository.cpp
    data/TasterRepository.h       data/TasterRepository.cpp
    data/EvaluationRepository.h   data/EvaluationRepository.cpp
    data/SessionRepository.h      data/SessionRepository.cpp
    data/AuditRepository.h        data/AuditRepository.cpp
    data/ReportRepository.h       data/ReportRepository.cpp
)

set(SEDM_IO_SOURCES
    io/CsvReader.h            io/CsvReader.cpp
    io/CsvWriter.h            io/CsvWriter.cpp
    io/ParticipantImporter.h  io/ParticipantImporter.cpp
    io/ResultExporter.h       io/ResultExporter.cpp
    io/DraftStore.h           io/DraftStore.cpp
)

set(SEDM_SERVICE_SOURCES
    services/AuditService.h       services/AuditService.cpp
    services/AuthService.h        services/AuthService.cpp
    services/RegistryService.h    services/RegistryService.cpp
    services/SampleService.h      services/SampleService.cpp
    services/BasketService.h      services/BasketService.cpp
    services/TeamService.h        services/TeamService.cpp
    services/SessionService.h     services/SessionService.cpp
    services/TastingService.h     services/TastingService.cpp
    services/MonitoringService.h  services/MonitoringService.cpp
    services/ResultService.h      services/ResultService.cpp
    services/BackupService.h      services/BackupService.cpp
)

add_library(sedm_core STATIC
    AppPaths.h AppPaths.cpp
    CoreResources.h CoreResources.cpp
    ${SEDM_DOMAIN_SOURCES}
    ${SEDM_SECURITY_SOURCES}
    ${SEDM_DATA_SOURCES}
    ${SEDM_IO_SOURCES}
    ${SEDM_SERVICE_SOURCES}
    ${CMAKE_SOURCE_DIR}/resources/core.qrc
)

target_include_directories(sedm_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/domain
    ${CMAKE_CURRENT_SOURCE_DIR}/security
    ${CMAKE_CURRENT_SOURCE_DIR}/data
    ${CMAKE_CURRENT_SOURCE_DIR}/io
    ${CMAKE_CURRENT_SOURCE_DIR}/services)

target_link_libraries(sedm_core
    PUBLIC  Qt5::Core Qt5::Sql
    PRIVATE crypt_blowfish QXlsx::QXlsx)
if(WIN32)
    target_link_libraries(sedm_core PRIVATE crypt32)   # DPAPI для пароля подключения
endif()

target_compile_options(sedm_core PRIVATE ${SEDM_WARNINGS})
