/**
 * @file CoreResources.h
 * @brief Регистрация ресурсов статической библиотеки sedm_core.
 */
#pragma once

namespace sedm {

/// Регистрирует ресурсы ядра (сценарий схемы БД). Вызывается один раз в main(),
/// так как ресурсы статической библиотеки не подключаются автоматически.
void initCoreResources();

}  // namespace sedm
