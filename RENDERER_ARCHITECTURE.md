# Архитектура рендерера Return to Castle Wolfenstein

## Обзор

Рендерер RTCW основан на движке **id Tech 3** (модифицированный Quake III Arena) с уникальными расширениями для поддержки особенностей игры (стелс, зомби, эффекты).

**Статистика кода:**
- 24 исходных файла (.c): ~28,500 строк
- 5 заголовочных файлов (.h): ~3,300 строк
- **Всего: ~32,742 строк кода**

---

## 1. Структура файлов

### Основные модули

| Файл | Строк | Назначение |
|------|-------|------------|
| `tr_init.c` | 1,420 | Инициализация рендерера, регистрация функций |
| `tr_main.c` | 1,886 | Главный цикл, управление кадрами |
| `tr_backend.c` | 1,644 | Бэкенд отрисовки, выполнение команд |
| `tr_shade.c` | 1,692 | Применение шейдеров к поверхностям |
| `tr_shader.c` | 3,307 | Парсинг и управление шейдерами |
| `tr_world.c` | 710 | Загрузка и рендеринг BSP-мира |
| `tr_bsp.c` | 2,260 | Обработка BSP-деревьев |
| `tr_model.c` | 2,135 | Загрузка моделей (MD3, MDC, MDS) |
| `tr_image.c` | 3,718 | Загрузка текстур |
| `tr_light.c` | 424 | Расчёт освещения |
| `tr_surface.c` | 1,502 | Управление поверхностями |
| `tr_curve.c` | 634 | Рендеринг кривых Безье |
| `tr_mesh.c` | 450 | Рендеринг меших |
| `tr_sky.c` | 1,030 | Небо и skybox |
| `tr_flares.c` | 543 | Эффекты бликов (lens flares) |
| `tr_shadows.c` | 348 | Тени |
| `tr_marks.c` | 822 | Декали (следы пуль, кровь) |
| `tr_cmds.c` | 556 | Командные буферы |
| `tr_font.c` | 550 | Рендеринг шрифтов |
| `tr_animation.c` | 1,488 | Анимация моделей |
| `tr_noise.c` | 99 | Генерация шума для эффектов |
| `tr_scene.c` | 542 | Управление сценой |
| `tr_shade_calc.c` | 1,232 | Вычисления для шейдеров |
| `tr_cmesh.c` | 446 | Кастомные меши |

### Заголовочные файлы

| Файл | Строк | Назначение |
|------|-------|------------|
| `tr_local.h` | 1,856 | Внутренние структуры и функции рендерера |
| `tr_public.h` | 183 | Публичный API для игры |
| `qgl.h` | 617 | OpenGL обёртка и расширения |
| `qgl_linked.h` | 364 | Линковка OpenGL функций |
| `anorms256.h` | 284 | Таблица нормалей для моделей |

---

## 2. Архитектурные компоненты

### 2.1. Глобальное состояние (trGlobals_t)

**Расположение:** `tr_local.h:944-1047`

Центральная структура, хранящая всё состояние рендерера:

```c
typedef struct {
    qboolean registered;                    // Флаг загрузки
    int visCount;                           // Счётчик посещения кластеров
    int frameCount;                         // Счётчик кадров
    int sceneCount;                         // Счётчик сцен
    int viewCount;                          // Счётчик видов (порталы)
    
    world_t *world;                         // Загруженный мир
    image_t *defaultImage;                  // Текстура по умолчанию
    image_t *lightmaps[MAX_LIGHTMAPS];      // Lightmap'ы
    
    shader_t *shaders[MAX_SHADERS];         // Все шейдеры
    shader_t *sortedShaders[MAX_SHADERS];   // Отсортированные шейдеры
    
    model_t *models[MAX_MOD_KNOWN];         // Все модели
    image_t *images[MAX_DRAWIMAGES];        // Все текстуры
    skin_t *skins[MAX_SKINS];               // Скины
    
    trRefdef_t refdef;                      // Текущее определение вида
    viewParms_t viewParms;                  // Параметры вида
    orientationr_t or;                      // Ориентация камеры
    
    float identityLight;                    // Множитель света
    int overbrightBits;                     // Overbright биты
    
    frontEndCounters_t pc;                  // Счётчики производительности
} trGlobals_t;
```

**Ключевые особенности:**
- Двойная буферизация для SMP (`SMP_FRAMES = 2`)
- Быстрые таблицы (`sinTable`, `fogTable` и др.)
- Кэширование ресурсов (модели, текстуры, шейдеры)

---

### 2.2. Система шейдеров

**Расположение:** `tr_local.h:361-429`, `tr_shader.c`

Шейдеры RTCW — это **скриптовые определения**, а не GPU-шейдеры в современном понимании. Поддерживают до 8 проходов:

```c
typedef struct shader_s {
    char name[MAX_QPATH];                   // Имя шейдера
    int lightmapIndex;                      // Индекс lightmap
    float sort;                             // Приоритет сортировки
    
    qboolean isSky;                         // Это небо
    skyParms_t sky;                         // Параметры неба
    fogParms_t fogParms;                    // Параметры тумана
    
    cullType_t cullType;                    // Тип отсечения
    qboolean polygonOffset;                 // Смещение полигонов
    
    int numDeforms;                         // Деформации вершин
    deformStage_t deforms[MAX_SHADER_DEFORMS];
    
    int numUnfoggedPasses;                  // Количество проходов
    shaderStage_t *stages[MAX_SHADER_STAGES]; // До 8 проходов
    
    void (*optimalStageIteratorFunc)(void); // Оптимизированный итератор
} shader_t;
```

#### Этапы шейдера (shaderStage_t)

Каждый проход включает:

```c
typedef struct {
    qboolean active;
    textureBundle_t bundle[NUM_TEXTURE_BUNDLES]; // 2 текстурных блока
    
    waveForm_t rgbWave;                     // Волновая функция цвета
    colorGen_t rgbGen;                      // Генератор цвета
    
    waveForm_t alphaWave;                   // Волновая функция альфы
    alphaGen_t alphaGen;                    // Генератор альфы
    
    unsigned stateBits;                     // GLS_xxxx mask (состояние OpenGL)
    acff_t adjustColorsForFog;              // Коррекция цвета для тумана
} shaderStage_t;
```

#### Типы генераторов

**Цвет (colorGen_t):**
- `CGEN_IDENTITY` — белый (1,1,1)
- `CGEN_VERTEX` — цвет из вершины
- `CGEN_LIGHTING_DIFFUSE` — диффузное освещение
- `CGEN_WAVEFORM` — программная генерация
- `CGEN_FOG` — цвет тумана

**Альфа (alphaGen_t):**
- `AGEN_ENTITY` — из сущности
- `AGEN_VERTEX` — из вершины
- `AGEN_LIGHTING_SPECULAR` — спекулярное освещение
- `AGEN_WAVEFORM` — волновая функция
- `AGEN_PORTAL` — для порталов

#### Деформации вершин (deform_t)

- `DEFORM_WAVE` — волновая деформация
- `DEFORM_BULGE` — эффект "распирания" (для желе)
- `DEFORM_MOVE` — перемещение
- `DEFORM_AUTOSPRITE` — автоматический спрайт
- `DEFORM_PROJECTION_SHADOW` — проекционные тени
- `DEFORM_TEXT0-7` — деформация текста

#### Модификации текстур (texMod_t)

- `TMOD_SCROLL` — скроллинг
- `TMOD_SCALE` — масштабирование
- `TMOD_ROTATE` — вращение
- `TMOD_TRANSFORM` — матричное преобразование
- `TMOD_TURBULENT` — турбулентность
- `TMOD_STRETCH` — растяжение

---

### 2.3. Система отрисовки (Front-end / Back-end)

#### Front-end (трассировка сцены)

**Файлы:** `tr_main.c`, `tr_scene.c`, `tr_world.c`, `tr_model.c`

Задачи:
1. Обход BSP-дерева
2. Отсечение невидимых поверхностей (PVS, frustum culling)
3. Сбор draw calls в список `drawSurf_t`
4. Сортировка по шейдерам и прозрачности
5. Подготовка данных для бэкенда

**Структура drawSurf_t:**
```c
typedef struct drawSurf_s {
    unsigned sort;                      // Упакованные данные для сортировки
    surfaceType_t *surface;             // Поверхность (face, grid, md3, etc.)
} drawSurf_t;
```

**Битовая упаковка sort (32 бита):**
```
Биты 22-31: Индекс шейдера (10 бит = 1024 шейдера)
Биты 11-21: Номер сущности (11 бит = 2048 сущностей)
Биты 2-6:   Номер тумана (5 бит = 32 тумана)
Биты 0-1:   Индекс dlight map (2 бита = 4 источника)
```

#### Back-end (отрисовка)

**Файлы:** `tr_backend.c`, `tr_shade.c`

Задачи:
1. Установка состояния OpenGL
2. Применение шейдеров к поверхностям
3. Отправка геометрии в GPU
4. Синхронизация для SMP

**Ключевая функция:** `RB_ExecuteRenderCommands()`

---

### 2.4. Тесселятор (shaderCommands_t)

**Расположение:** `tr_local.h:1406-1434`

Промежуточный буфер для подготовки геометрии:

```c
typedef struct shaderCommands_s {
    glIndex_t indexes[SHADER_MAX_INDEXES];      // Индексы (24,000)
    vec4_t xyz[SHADER_MAX_VERTEXES];            // Вершины (4,000)
    vec4_t normal[SHADER_MAX_VERTEXES];         // Нормали
    vec2_t texCoords[SHADER_MAX_VERTEXES][2];   // UV координаты
    color4ub_t vertexColors[SHADER_MAX_VERTEXES]; // Цвета вершин
    int vertexDlightBits[SHADER_MAX_VERTEXES];  // Биты dlight
    
    stageVars_t svars;                          // Переменные этапов
    shader_t *shader;                           // Текущий шейдер
    float shaderTime;                           // Время для анимации
    int fogNum;                                 // Туман
    int numIndexes;                             // Количество индексов
    int numVertexes;                            // Количество вершин
} shaderCommands_t;
```

**Лимиты:**
- `SHADER_MAX_VERTEXES = 4000`
- `SHADER_MAX_INDEXES = 24,000` (6 × вершины)

---

### 2.5. Типы поверхностей

**Расположение:** `tr_local.h:568-584`

```c
typedef enum {
    SF_BAD,
    SF_SKIP,                // Игнорировать
    SF_FACE,                // Грань BSP
    SF_GRID,                // Кривая Безье
    SF_TRIANGLES,           // Статические треугольники
    SF_POLY,                // Полигон от игры
    SF_MD3,                 // MD3 модель
    SF_MDC,                 // MDC модель (сжатая)
    SF_MDS,                 // MDS модель (скелетная)
    SF_FLARE,               // Блик
    SF_ENTITY,              // Сущность (лучи, молнии)
    SF_DISPLAY_LIST         // Display list OpenGL
} surfaceType_t;
```

Для каждого типа есть функция рендеринга в таблице `rb_surfaceTable[]`.

---

### 2.6. OpenGL абстракция

**Файл:** `qgl.h`

Все вызовы OpenGL идут через функциональные указатели `qgl*`:

```c
extern void (APIENTRY *qglBegin)(GLenum mode);
extern void (APIENTRY *qglVertex3fv)(const GLfloat *v);
extern void (APIENTRY *qglTexCoord2fv)(const GLfloat *v);
extern void (APIENTRY *qglColor4fv)(const GLfloat *v);
extern void (APIENTRY *qglDrawElements)(GLenum mode, GLsizei count, ...);
```

**Преимущества:**
- Динамическая загрузка расширений
- Логирование вызовов
- Минидрайверы (замена реализации)
- Кроссплатформенность (Windows, Linux, macOS)

**Поддерживаемые расширения:**
- `GL_ARB_multitexture` — мультитекстурирование
- `GL_EXT_compiled_vertex_array` — компилированные массивы
- `GL_EXT_texture_filter_anisotropic` — анизотропная фильтрация
- `GL_ATI_pn_triangles` — тесселяция TruForm
- `GL_NV_fog_distance` — туман по расстоянию
- `GL_EXT_texture_compression_s3tc` — сжатие S3TC

---

### 2.7. Освещение

#### Статическое (Lightmaps)

- Предварительно рассчитанные lightmap'ы
- Хранятся в BSP
- Наложение через мультитекстурирование
- Режим: `GL_MODULATE`

#### Динамическое (Dlights)

**Структура:**
```c
typedef struct dlight_s {
    vec3_t origin;              // Позиция
    vec3_t color;               // Цвет (0-1)
    float radius;               // Радиус
    shader_t *dlshader;         // Шейдер для проекции
    qboolean forced;            // Всегда активен
} dlight_t;
```

**Особенности RTCW:**
- Проекционное освещение через текстуру (`dlightImage`)
- Поддержка "важных" источников (`forced`)
- Затухание по квадрату расстояния

#### Световая сетка (Light Grid)

- 3D сетка образцов освещения в мире
- Интерполяция для динамических объектов
- Хранит ambient и directed свет
- Используется для освещения персонажей и предметов

---

### 2.8. Туман (Fog)

**Расположение:** `tr_local.h:522-533`

```c
typedef struct {
    int originalBrushNumber;
    vec3_t bounds[2];
    unsigned colorInt;                  // Упакованный цвет
    float tcScale;                      // Масштаб UV
    fogParms_t parms;                   // Параметры
    qboolean hasSurface;                // Есть ли поверхность
    float surface[4];                   // Плоскость поверхности
} fog_t;
```

**Типы тумана:**
- Глобальный (на весь уровень)
- Локальный (в объёмах)
- Подводный
- Портальный

**Реализация:**
- Таблица `fogTable[256]` для расчёта плотности
- Генерация 3D текстурных координат
- Отрисовка в отдельном проходе

---

### 2.9. Небо (Sky)

**Файл:** `tr_sky.c`

Двухслойная система:
1. **Outer box** — далёкие облака (без параллакса)
2. **Inner box** — ближние облака (с параллаксом)

**Параметры:**
```c
typedef struct {
    float cloudHeight;                  // Высота облаков
    image_t *outerbox[6];               // Дальний слой
    image_t *innerbox[6];               // Ближний слой
} skyParms_t;
```

**Оптимизации:**
- Отсечение по направлению взгляда
- Рендеринг после непрозрачных объектов
- Поддержка солнца (`sunShader`, `sunflareShader`)

---

### 2.10. Модели

#### Форматы

**MD3 (Quake III):**
- Несколько LOD уровней
- Вершинная анимация
- Теги для прикрепления объектов

**MDC (сжатый MD3):**
- Сжатие вершин для экономии памяти
- Быстрая декомпрессия при загрузке

**MDS (скелетный):**
- Скелетная анимация
- Поддержка мешей (Ragdoll)
- Расширенные теги

#### Загрузка
**Файл:** `tr_model.c`

```c
typedef struct model_s {
    char name[MAX_QPATH];
    modtype_t type;                     // MOD_BRUSH, MOD_MESH, MOD_MDS...
    bmodel_t *bmodel;                   // Для BSP
    md3Header_t *md3[MD3_MAX_LODS];     // Для MD3
    mdsHeader_t *mds;                   // Для MDS
    int numLods;
} model_t;
```

---

### 2.11. Декали (Marks)

**Файл:** `tr_marks.c`

Следы от пуль, крови, взрывов:

**Процесс:**
1. Определение поверхности попадания
2. Проекция декали на поверхность
3. Разбиение полигонов
4. Добавление в список рендеринга

**Ограничения:**
- Максимум полигонов на декаль
- Время жизни (исчезают со временем)
- Сортировка перед прозрачными объектами

---

### 2.12. Эффекты

#### Блики (Flares)
**Файл:** `tr_flares.c`

- Lens flare эффекты для источников света
- Проверка видимости (occlusion test)
- Плавное появление/исчезновение
- Настройка через `r_flareSize`, `r_flareFade`

#### Corona
Кольцевые ореолы вокруг ярких источников:
```c
typedef struct corona_s {
    vec3_t origin;
    vec3_t color;
    float scale;
    int id;
    int flags;                          // Видимость
} corona_t;
```

#### ZombieFX (уникально для RTCW)
Специальные эффекты для зомби:
- Искажение меша
- Пульсирующее свечение
- Частицы

---

## 3. Конвейер рендеринга

### 3.1. Последовательность кадров

```
RE_BeginFrame()
├── Очистка буферов
├── Обновление cvars
└── Сброс счётчиков

RE_RenderScene(&refdef)
├── R_SetupFrame() — подготовка кадра
├── R_MirrorViewBySurface() — отражения
├── R_AddWorldSurfaces() — мир
│   ├── PVS отсечение
│   ├── Frustum culling
│   └── Добавление в drawSurfs
├── R_AddEntities() — сущности
├── R_AddPolys() — полигоны
├── R_AddDecals() — декали
├── R_AddFlares() — блики
├── R_SortDrawSurfs() — сортировка
└── R_DrawWorld() — отрисовка
    ├── RB_ExecuteRenderCommands()
    │   ├── Установка состояния
    │   ├── RB_BeginSurface()
    │   ├── Применение шейдера
    │   └── RB_EndSurface()
    └── SwapBuffers()

RE_EndFrame()
└── GLimp_EndFrame()
```

### 3.2. Сортировка поверхностей

Приоритеты (от меньшего к большему):
1. **SS_PORTAL** — порталы, зеркала
2. **SS_ENVIRONMENT** — небо
3. **SS_OPAQUE** — непрозрачные объекты
4. **SS_DECAL** — декали
5. **SS_SEE_THROUGH** — решётки, лестницы
6. **SS_FOG** — туман
7. **SS_BLEND0-6** — прозрачность (alpha blending)
8. **SS_STENCIL_SHADOW** — тени
9. **SS_NEAREST** — ближайшая прозрачность (кровь)

---

## 4. Оптимизации

### 4.1. Отсечение

- **PVS (Potentially Visible Set)** — предвычисленная видимость кластеров
- **Frustum culling** — отсечение вне поля зрения
- **Backface culling** — отсечение задних граней
- **Occlusion culling** — для порталов

### 4.2. Состояние OpenGL

Минимизация переключений:
- Сортировка по шейдерам
- Кэширование привязок текстур
- Ленивое обновление состояний

### 4.3. SMP (Symmetric Multi-Processing)

**Поддержка двух CPU:**
- Front-end работает на одном ядре
- Back-end — на другом
- Двойная буферизация всех структур
- Синхронизация через `GLimp_RendererSleep()` / `GLimp_WakeRenderer()`

### 4.4. Vertex Arrays

Использование расширений:
- `GL_EXT_compiled_vertex_array`
- `GL_LOCK_ARRAYS_EXT`
- Индексированные треугольные списки

---

## 5. Уникальные особенности RTCW

### 5.1. Расширения движка

**В сравнении с Quake III:**
- Увеличено количество entity num (10 → 11 бит)
- Поддержка MDC/MDS моделей
- ZombieFX система
- Corona эффекты
- Height-based fog
- Улучшенная система частиц
- Стелс-механики (видимость игрока)

### 5.2. Специфичные cvars

```
r_flareSize         — размер бликов
r_flareFade         — затухание бликов
r_drawSun           — рисование солнца (0/1/2)
r_dynamiclight      — динамические огни
r_wolffog           — фирменный туман Wolfenstein
r_portalsky         — небо в порталах
r_cache             — кэширование ресурсов
r_ati_truform_*     — настройки TruForm
```

---

## 6. Требования для портирования на Vulkan

### 6.1. Критические изменения

1. **Замена OpenGL абстракции:**
   - Переписать `qgl.h` → `qvk.h`
   - Все `qgl*()` → `qvk*()`
   - Управление памятью Vulkan (VkDeviceMemory)

2. **Конвейер рендеринга:**
   - Создание `VkPipeline` для каждого типа шейдера
   - Динамические состояния вместо фиксированных
   - Управление `VkDescriptorSet`

3. **Буферы:**
   - `VkBuffer` для вершин, индексов, uniform
   - `VkCommandBuffer` для записи команд
   - Синхронизация (semaphores, fences)

4. **Шейдеры:**
   - Парсинг старых шейдеров → SPIR-V
   - Эмуляция фиксированного конвейера
   - Push constants для параметров

5. **Swapchain:**
   - Замена WGL/GLX на `VkSurfaceKHR`
   - Управление `VkImageView`
   - Present timing

### 6.2. Оценка сложности

| Компонент | Сложность | Строк кода | Примечания |
|-----------|-----------|------------|------------|
| Инициализация Vulkan | Средняя | ~1,500 | Instance, Device, Queues |
| Swapchain | Низкая | ~500 | Surface, Present |
| Pipeline management | Высокая | ~3,000 | Shader compilation, layouts |
| Descriptor sets | Высокая | ~2,000 | Binding models |
| Command buffers | Средняя | ~1,500 | Recording, submission |
| Resource upload | Средняя | ~1,000 | Staging buffers |
| Sync primitives | Высокая | ~1,000 | Semaphores, fences |
| Shader translation | Очень высокая | ~5,000+ | Parser + codegen |
| **Итого** | | **~15,500** | ~47% кода рендерера |

---

## 7. Заключение

Рендерер RTCW представляет собой классическую архитектуру движка конца 1990-х — начала 2000-х годов с:

✅ **Сильными сторонами:**
- Чёткое разделение front-end / back-end
- Мощная скриптовая система шейдеров
- Поддержка SMP
- Хорошая модульность

⚠️ **Слабыми сторонами:**
- Фиксированный конвейер OpenGL
- Глобальное состояние
- Ограниченная пакетная обработка
- Нет многопоточности beyond SMP

**Для портирования на Vulkan** потребуется переписать ~47% кода рендерера,重点关注 shader pipeline, descriptor management и синхронизацию. Нейросеть может помочь с шаблонным кодом, но архитектурные решения требуют участия опытного разработчика.
