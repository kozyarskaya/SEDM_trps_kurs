/**
 * @file WidgetsResources.cpp
 * @brief Регистрация ресурсов (макрос Q_INIT_RESOURCE требует глобальной области).
 */
#include "WidgetsResources.h"

#include <QtGlobal>

static void initWidgetsResourcesImpl()
{
    Q_INIT_RESOURCE(widgets);
}

namespace sedm::ui {

void initWidgetsResources()
{
    ::initWidgetsResourcesImpl();
}

}  // namespace sedm::ui
