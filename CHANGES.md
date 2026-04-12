# Адаптация RTCW для современных Linux - Обзор изменений

## Резюме

Код движка Return to Castle Wolfenstein адаптирован для сборки и запуска на современных Linux системах с использованием:
- **CMake** вместо устаревшей системы сборки Cons
- **GLFW 3.x** для создания окна, обработки ввода и управления OpenGL контекстом
- **PulseAudio** для вывода звука
- **x64 архитектура** без использования QVM (прямая компиляция в native code)

## Созданные файлы

### 1. CMakeLists.txt
Основной файл сборки CMake который:
- Определяет все модули проекта (client, game, cgame, ui, botlib, renderer)
- Находит и линкует зависимости (GLFW, PulseAudio, FreeType, JPEG, OpenGL)
- Создаёт исполняемый файл `wolfsp` и разделяемые библиотеки `.so`
- Поддерживает опции сборки через `-D` флаги

### 2. src/unix/linux_glimp_glfw.c
Заменяет старый `linux_glimp.c` (X11/GLX):
- Инициализация GLFW и создание OpenGL 3.3 Core Profile контекста
- Обработка ввода клавиатуры и мыши через callback функции GLFW
- Маппинг клавиш GLFW на коды клавиш Quake engine
- Поддержка полноэкранного и оконного режима
- Функции буфера обмена через GLFW
- Удалена зависимость от X11, GLX, XF86VMode, XF86DGA

### 3. src/unix/linux_snd_pulse.c
Заменяет старый `linux_snd.c` (OSS/ALSA direct):
- Инициализация PulseAudio через libpulse-simple
- Поддержка 8/16 бит, моно/стерео режимов
- Правильная настройка sample rate (8000-48000 Hz)
- DMA буфер совместимый с оригинальным движком
- Корректная обработка ошибок PulseAudio

### 4. src/unix/unix_main_glfw.c
Заменяет старый `unix_main.c`:
- Точка входа `main()` с правильной инициализацией
- Системные функции для x64 Linux
- Определение домашнего каталога `~/.rtcw`
- Интеграция с GLFW для основного цикла
- Удалены TTY консоль и устаревшие функции

### 5. scripts/rtcw.sh.in
Шаблон скрипта запуска:
- Проверка наличия файлов игры
- Установка правильных путей
- Передача параметров командной строки

### 6. README_LINUX.md
Полная документация:
- Инструкция по установке зависимостей для разных дистрибутивов
- Параметры сборки CMake
- Инструкция по установке файлов игры
- Параметры запуска
- Решение известных проблем

## Ключевые изменения в архитектуре

### Удалённые зависимости
| Компонент | Старая реализация | Новая реализация |
|-----------|------------------|------------------|
| Окно/OpenGL | X11 + GLX | GLFW 3.x |
| Ввод | X11 events | GLFW callbacks |
| Звук | OSS/ALSA direct | PulseAudio |
| Сборка | Cons/make | CMake |
| VM | QVM bytecode | Native x64 .so |

### Удаление QVM
Оригинальный код использовал виртуальную машину Quake (QVM) для:
- `game.so` - игровая логика
- `cgame.so` - client-side prediction
- `ui.so` - интерфейс

В новой реализации эти модули компилируются напрямую в native x64 code:
- Лучшая производительность (нет overhead VM)
- Проще отладка (native debugging)
- Меньше кода (нет VM интерпретатора)

### OpenGL Context
Старый код:
```c
// X11 + GLX
XOpenDisplay()
glXChooseVisual()
glXCreateContext()
```

Новый код:
```c
// GLFW
glfwInit()
glfwCreateWindow()
glfwMakeContextCurrent()
gladLoadGLLoader(glfwGetProcAddress)
```

### Звуковая система
Старый код:
```c
// OSS /dev/dsp
open("/dev/dsp", O_WRONLY)
ioctl(fd, SNDCTL_DSP_SETFMT, &fmt)
```

Новый код:
```c
// PulseAudio
pa_simple_new()
pa_simple_write()
pa_simple_drain()
```

## Совместимость

### Поддерживаемые дистрибутивы
- Ubuntu 20.04+
- Debian 11+
- Fedora 33+
- Arch Linux
- openSUSE Tumbleweed

### Требования к железу
- CPU: x86_64 с SSE2
- GPU: OpenGL 3.3+ support
- RAM: 512 MB minimum
- Место на диске: ~2 GB (с файлами игры)

## Сборка и запуск

```bash
# Установка зависимостей (Ubuntu)
sudo apt install cmake build-essential libglfw3-dev libpulse-dev libfreetype6-dev libjpeg-dev

# Сборка
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Запуск
./wolfsp +set fs_basepath ~/.rtcw +set fs_game main
```

## Дальнейшие улучшения

Возможные дальнейшие модернизации:
1. **Wayland поддержка** - GLFW уже поддерживает Wayland
2. **Pipewire** - замена PulseAudio (совместим через pulse compat layer)
3. **Modern OpenGL** - переход на OpenGL 4.x с шейдерами
4. **High DPI** - поддержка 4K дисплеев
5. **Steam integration** - SteamOS/Linux совместимость

## Лицензия

Все изменения распространяются под GPL v3+ согласно лицензии оригинального кода id Software.
