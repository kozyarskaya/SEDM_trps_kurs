/**
 * @file WidgetsResources.h
 * @brief Регистрация ресурсов библиотеки sedm_widgets.
 */
#pragma once

namespace sedm::ui {

/// Регистрирует ресурсы тем оформления и значков (ресурсы статической
/// библиотеки нужно подключать явно, один раз в main()).
void initWidgetsResources();

}  // namespace sedm::ui
