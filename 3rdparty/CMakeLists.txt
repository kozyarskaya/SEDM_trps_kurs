# Сторонние библиотеки СЭДМ.
#
# QXlsx 1.5.1.1 (https://github.com/QtExcel/QXlsx, лицензия MIT) —
#   чтение реестра участников и запись итоговой ведомости в формате XLSX.
# crypt_blowfish 1.3 (https://www.openwall.com/crypt/, общественное достояние) —
#   хэширование пароля администратора и PIN-кодов алгоритмом bcrypt.

set(BUILD_SHARED_LIBS OFF CACHE BOOL "QXlsx — статическая библиотека" FORCE)
set(QT_VERSION_MAJOR 5)
add_subdirectory(QXlsx)

add_library(crypt_blowfish STATIC crypt_blowfish/crypt_blowfish.c crypt_blowfish/crypt_blowfish.h)
target_include_directories(crypt_blowfish PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/crypt_blowfish)
