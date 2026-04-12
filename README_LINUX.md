# Инструкция по сборке Return to Castle Wolfenstein для современных Linux

## Требования

### Системные требования
- **ОС:** Linux (x86_64)
- **Компилятор:** GCC 9+ или Clang 10+
- **CMake:** 3.16+
- **OpenGL:** 3.3+ с поддержкой GLSL

### Зависимости

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install -y \
    cmake \
    build-essential \
    libglfw3-dev \
    libpulse-dev \
    libfreetype6-dev \
    libjpeg-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev
```

#### Fedora/RHEL
```bash
sudo dnf install -y \
    cmake \
    gcc-c++ \
    glfw-devel \
    pulseaudio-libs-devel \
    freetype-devel \
    libjpeg-turbo-devel \
    mesa-libGL-devel \
    mesa-libGLU-devel
```

#### Arch Linux
```bash
sudo pacman -S --noconfirm \
    cmake \
    base-devel \
    glfw-x11 \
    pulseaudio \
    freetype2 \
    libjpeg-turbo \
    mesa \
    glu
```

## Сборка

### Быстрая сборка
```bash
cd /workspace
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Опции сборки

| Опция | Описание | По умолчанию |
|-------|----------|--------------|
| `BUILD_CLIENT` | Собрать клиент игры | ON |
| `BUILD_SERVER` | Собрать выделенный сервер | OFF |
| `BUILD_GAME` | Собрать game.so модуль | ON |
| `BUILD_CGAME` | Собрать cgame.so модуль | ON |
| `BUILD_UI` | Собрать ui.so модуль | ON |
| `USE_PULSEAUDIO` | Использовать PulseAudio | ON |
| `USE_GLFW` | Использовать GLFW | ON |

Пример сборки только клиента:
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_CLIENT=ON \
    -DBUILD_SERVER=OFF \
    -DBUILD_GAME=ON \
    -DBUILD_CGAME=ON \
    -DBUILD_UI=ON
```

### Отладочная сборка
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

## Установка файлов игры

1. Создайте директорию для файлов игры:
```bash
mkdir -p ~/.rtcw/main
```

2. Скопируйте файлы из оригинальной игры RTCW:
```
~/.rtcw/main/pak0.pk3
~/.rtcw/main/pak1.pk3
~/.rtcw/main/pak2.pk3
~/.rtcw/main/pak3.pk3
~/.rtcw/main/pak4.pk3
```

3. Или используйте символические ссылки:
```bash
ln -s /path/to/rtcw/Main/* ~/.rtcw/main/
```

## Запуск игры

### Через скрипт
```bash
./build/rtcw.sh
```

### Напрямую
```bash
./build/wolfsp +set fs_basepath ~/.rtcw +set fs_game main
```

### Параметры запуска

| Параметр | Описание |
|----------|----------|
| `+set r_mode N` | Разрешение экрана (N=-1 для custom) |
| `+set r_customwidth W` | Пользовательская ширина |
| `+set r_customheight H` | Пользовательская высота |
| `+set r_fullscreen 0/1` | Оконный/полноэкранный режим |
| `+set s_initsound 0/1` | Включить/выключить звук |
| `+set com_maxfps N` | Ограничение FPS |
| `+set g_debug 0/1` | Отладочный режим игры |

Пример:
```bash
./wolfsp +set r_mode -1 +set r_customwidth 1920 +set r_customheight 1080 +set r_fullscreen 1
```

## Структура проекта

```
/workspace/
├── CMakeLists.txt          # Основной файл сборки CMake
├── src/
│   ├── unix/
│   │   ├── linux_glimp_glfw.c    # OpenGL инициализация (GLFW)
│   │   ├── linux_snd_pulse.c     # Звуковая система (PulseAudio)
│   │   └── unix_main_glfw.c      # Точка входа
│   ├── client/             # Клиентский код
│   ├── renderer/           # Рендерер
│   ├── game/               # Игровая логика
│   ├── cgame/              # Client-side game logic
│   ├── ui/                 # Интерфейс
│   └── ...
└── scripts/
    └── rtcw.sh.in          # Шаблон скрипта запуска
```

## Известные проблемы и решения

### Проблема: Ошибка "Failed to initialize GLFW"
**Решение:** Убедитесь, что установлен корректный драйвер OpenGL и запущен X-сервер или Wayland.

### Проблема: Нет звука
**Решение:** Проверьте, что PulseAudio запущен:
```bash
pulseaudio --check || pulseaudio --start
```

### Проблема: Низкая производительность
**Решение:** 
- Убедитесь, что используется дискретная видеокарта
- Отключите VSync: `+set r_vsync 0`
- Уменьшите разрешение текстуры

### Проблема: Игра не находит файлы
**Решение:** Проверьте путь к файлам игры:
```bash
ls -la ~/.rtcw/main/*.pk3
```

## Отладка

### Включение логов
```bash
./wolfsp +set developer 1 +set com_demoshow 0
```

### GDB отладка
```bash
gdb ./wolfsp
(gdb) run
```

### Valgrind проверка утечек
```bash
valgrind --leak-check=full ./wolfsp
```

## Лицензия

Исходный код распространяется под лицензией GPL v3+.
Смотрите файл LICENSE для деталей.

## Поддержка

Для сообщений об ошибках и предложений создавайте issues на GitHub.
