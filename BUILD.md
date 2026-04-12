# Сборка RTCW SP с SDL2 и CMake

## Требования

- CMake 3.16+
- SDL2
- OpenAL
- FreeType2
- OpenGL
- Компилятор C/C++ (GCC, Clang, MSVC)

## Установка зависимостей (Linux)

```bash
# Debian/Ubuntu
sudo apt-get install libsdl2-dev libopenal-dev libfreetype6-dev libgl1-mesa-dev

# Fedora
sudo dnf install SDL2-devel openal-devel freetype-devel mesa-libGL-devel

# Arch Linux
sudo pacman -S sdl2 openal freetype2 mesa
```

## Сборка

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## Структура сборки

После сборки будут созданы:
- `wolfsp` - основной исполняемый файл
- `gamex86_64.so` - игровая DLL (game module)
- `cgamex86_64.so` - клиентская игровая DLL (cgame module)  
- `uix86_64.so` - UI DLL (user interface module)

## Запуск

```bash
./wolfsp
```

Или скопируйте DLL в директорию с игрой:
```bash
cp *.so ../main/
cd ..
./build/wolfsp
```

## Особенности адаптации

### Удалена VM система
- Все модули (game, cgame, ui) компилируются как нативные DLL
- Используется `Sys_LoadDll` через SDL2 для загрузки модулей
- Файлы `.qvm` больше не используются

### SDL2 интеграция
- Окно и OpenGL контекст создаются через SDL2
- Ввод обрабатывается через события SDL2
- Звук через OpenAL с SDL2 бэкендом

### x64 поддержка
- Все указатели и типы данных адаптированы под 64-бит
- Удалены ассемблерные вставки x86
- Используется стандартный C для критичных функций

## Отладка

Для отладочной сборки:
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

Включите developer режим в игре:
```
/set com_developer 1
```

## Известные проблемы

1. Некоторые функции звука требуют доработки
2. Геймпад поддержка не реализована полностью
3. Требуется тестирование на разных платформах
