/**
 * @file CoreResources.cpp
 * @brief Регистрация ресурсов ядра (макрос Q_INIT_RESOURCE требует глобальной области).
 */
#include "CoreResources.h"

#include <QtGlobal>

static void initCoreResourcesImpl()
{
    Q_INIT_RESOURCE(core);
}

namespace sedm {

void initCoreResources()
{
    ::initCoreResourcesImpl();
}

}  // namespace sedm
