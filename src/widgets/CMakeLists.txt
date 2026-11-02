# sedm_widgets — общие элементы интерфейса программ администратора и дегустатора:
#   тема оформления Г1, логотип, значки, ползунок-сота, окна сообщений,
#   подключения к серверу, справки и «О программе».

add_library(sedm_widgets STATIC
    Theme.h             Theme.cpp
    Icons.h             Icons.cpp
    LogoWidget.h        LogoWidget.cpp
    HoneySlider.h       HoneySlider.cpp
    Messages.h          Messages.cpp
    ConnectionDialog.h  ConnectionDialog.cpp
    HelpWindow.h        HelpWindow.cpp
    AboutDialog.h       AboutDialog.cpp
    Localization.h      Localization.cpp
    WidgetsResources.h  WidgetsResources.cpp
    ${CMAKE_SOURCE_DIR}/resources/widgets.qrc
)

target_include_directories(sedm_widgets PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sedm_widgets PUBLIC sedm_core Qt5::Widgets Qt5::Svg)
target_compile_options(sedm_widgets PRIVATE ${SEDM_WARNINGS})
