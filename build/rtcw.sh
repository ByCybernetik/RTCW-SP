#!/bin/bash
# Скрипт запуска Return to Castle Wolfenstein

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
GAME_DIR="${HOME}/.rtcw"

# Создание директории для игры если не существует
mkdir -p "${GAME_DIR}"

# Проверка наличия файлов игры
if [ ! -d "${GAME_DIR}/main" ]; then
    echo "Ошибка: Файлы игры не найдены в ${GAME_DIR}"
    echo "Скопируйте файлы из оригинальной игры RTCW в ${GAME_DIR}"
    echo "Необходимые файлы:"
    echo "  - pak0.pk3, pak1.pk3, pak2.pk3 и т.д."
    exit 1
fi

# Запуск игры
exec "${SCRIPT_DIR}/wolfsp" \
    +set fs_basepath "${GAME_DIR}" \
    +set fs_game main \
    "$@"
