#define VLA_IMPLEMENTATION
#include "vla.h"

#include "khash.h"

#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#define CLAY_IMPLEMENTATION
#include "clay.h"
#include "clay_renderer_raylib.c"

#define TAIL_SIZE 32
#define FV_WIDTH 32
#define FV_HEIGHT 18

#define RAYLIB_VECTOR2_TO_CLAY_VECTOR2(vector) (Clay_Vector2) { .x = vector.x, .y = vector.y }

KHASH_MAP_INIT_STR(fonts, Font)

Vla clayVla = {0};
bool debugEnabled = false;

typedef struct {
    Clay_Vector2 clickOrigin;
    Clay_Vector2 positionOrigin;
    bool mouseDown;
} ScrollbarData;

ScrollbarData scrollbarData = {0};

void init_clay();

void handle_clay_errors(Clay_ErrorData errorData) {
    printf("%s", errorData.errorText.chars);

    if (errorData.errorType == CLAY_ERROR_TYPE_ELEMENTS_CAPACITY_EXCEEDED) {
        Clay_SetMaxElementCount(Clay_GetMaxElementCount() * 2);
    } else if (errorData.errorType == CLAY_ERROR_TYPE_TEXT_MEASUREMENT_CAPACITY_EXCEEDED) {
        Clay_SetMaxMeasureTextCacheWordCount(Clay_GetMaxMeasureTextCacheWordCount() * 2);
    }

    vla_grow_to(&clayVla, Clay_MinMemorySize());
    init_clay();
}

void init_clay() {
    Clay_Arena clayArena = (Clay_Arena){
        .memory = clayVla.base,
        .capacity = clayVla.comitted,
    };

    Clay_Dimensions clayDimensions = {
        .width = GetScreenWidth(),
        .height = GetScreenHeight(),
    };
    Clay_Initialize(clayArena, clayDimensions,
                    (Clay_ErrorHandler){handle_clay_errors, NULL});
}

Clay_RenderCommandArray DrawUi() {
    Clay_BeginLayout();
    {
        Clay_String str = CLAY_STRING_CONST("Hello world");
        CLAY_TEXT(str,
                  CLAY_TEXT_CONFIG({.fontSize = 24,
                                    .textColor = {0, 0, 0, 255},
                                    .textAlignment = CLAY_TEXT_ALIGN_RIGHT}));
    }
    return Clay_EndLayout(GetFrameTime());
}

const uint32_t FONT_ID_BODY_24 = 0;
const uint32_t FONT_ID_BODY_16 = 1;

khash_t(fonts) *fontMap;

int main(void) {
    clayVla = vla_init(VLA_MB(256));
    uint64_t initialClayAlloc = Clay_MinMemorySize();
    vla_grow_to(&clayVla, initialClayAlloc);

    init_clay();

    Clay_Raylib_Initialize(TAIL_SIZE * FV_WIDTH, TAIL_SIZE * FV_HEIGHT, "raylib tesrting", FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI);

    // fonts
    fontMap = kh_init(fonts);
    int ret;
    khiter_t k = kh_put(fonts, fontMap, "Roboto-Regular", &ret);
    Font fonts[2];
    fonts[FONT_ID_BODY_24] = LoadFontEx("resources/Roboto-Regular.ttf", 32, 0, 400);LoadFontEx("resources/Roboto-Regular.ttf", 32, 0, 400);

    SetTextureFilter(fonts[FONT_ID_BODY_24].texture, TEXTURE_FILTER_BILINEAR);

    fonts[FONT_ID_BODY_16] =
        LoadFontEx("resources/Roboto-Regular.ttf", 32, 0, 400);
    SetTextureFilter(fonts[FONT_ID_BODY_16].texture, TEXTURE_FILTER_BILINEAR);
    Clay_SetMeasureTextFunction(Raylib_MeasureText, fonts);

    // Main game loop
    while (!WindowShouldClose()) {
        // debug mode
        if (IsKeyPressed(KEY_D)) {
            debugEnabled = !debugEnabled;
            Clay_SetDebugModeEnabled(debugEnabled);
        }

        Vector2 mouseWheelDelta = GetMouseWheelMoveV();
        Clay_Vector2 mousePosition = RAYLIB_VECTOR2_TO_CLAY_VECTOR2(GetMousePosition());

        Clay_SetLayoutDimensions((Clay_Dimensions) { (float)GetScreenWidth(), (float)GetScreenHeight() });

        Clay_SetPointerState(mousePosition, IsMouseButtonDown(0) && !scrollbarData.mouseDown);
        if (!IsMouseButtonDown(0)) {
          scrollbarData.mouseDown = false;
        }

        if (IsMouseButtonDown(0) && !scrollbarData.mouseDown &&
            Clay_PointerOver(Clay_GetElementId(CLAY_STRING("ScrollBar")))) {
          Clay_ScrollContainerData scrollContainerData =
              Clay_GetScrollContainerData(
                  Clay_GetElementId(CLAY_STRING("MainContent")));
          scrollbarData.clickOrigin = mousePosition;
          scrollbarData.positionOrigin = *scrollContainerData.scrollPosition;
          scrollbarData.mouseDown = true;
        } else if (scrollbarData.mouseDown) {
          Clay_ScrollContainerData scrollContainerData =
              Clay_GetScrollContainerData(
                  Clay_GetElementId(CLAY_STRING("MainContent")));
          if (scrollContainerData.contentDimensions.height > 0) {
            Clay_Vector2 ratio = (Clay_Vector2){
                scrollContainerData.contentDimensions.width /
                    scrollContainerData.scrollContainerDimensions.width,
                scrollContainerData.contentDimensions.height /
                    scrollContainerData.scrollContainerDimensions.height,
            };
            if (scrollContainerData.config.vertical) {
              scrollContainerData.scrollPosition->y =
                  scrollbarData.positionOrigin.y +
                  (scrollbarData.clickOrigin.y - mousePosition.y) * ratio.y;
            }
            if (scrollContainerData.config.horizontal) {
              scrollContainerData.scrollPosition->x =
                  scrollbarData.positionOrigin.x +
                  (scrollbarData.clickOrigin.x - mousePosition.x) * ratio.x;
            }
          }
        }

        Clay_UpdateScrollContainers(true, RAYLIB_VECTOR2_TO_CLAY_VECTOR2(mouseWheelDelta), GetFrameTime());

        Clay_RenderCommandArray renderCommands = DrawUi();

        BeginDrawing();
        {
            ClearBackground(RAYWHITE);

            for (int i = 0; i < FV_WIDTH; i++) {
                DrawLine(TAIL_SIZE * i, 0, TAIL_SIZE * i, TAIL_SIZE * FV_HEIGHT, BLACK);
            }

            for (int i = 0; i < FV_HEIGHT; i++) {
                DrawLine(0, TAIL_SIZE * i, TAIL_SIZE * FV_WIDTH, TAIL_SIZE * i, BLACK);
            }

            /* DrawFPS(10, 10); */

            Clay_Raylib_Render(renderCommands, fonts);
        }
        EndDrawing();
    }

    Clay_Raylib_Close();
    return 0;
}
