#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <windowsx.h>
#include <d3d9.h>
#include <dshow.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define GAME_WIDTH 640
#define GAME_HEIGHT 480

static HWND game_window;
static HWND backdrop;
static LONG_PTR windowed_style;
static WINDOWPLACEMENT windowed_placement = { sizeof(WINDOWPLACEMENT) };
static BOOL fullscreen;
static BOOL toggle_held;
static BOOL pause_held;
static BOOL aiming;
static BOOL pointer_hidden;
static HCURSOR hidden_cursor;
static WNDPROC previous_window_proc;
static volatile LONG last_weapon_update;
static volatile LONG pending_shot;
static BYTE *game_image;
static int (__cdecl *read_guns)(LONG *);
static void (__thiscall *update_weapon)(BYTE *, void *);
static HRESULT (WINAPI *create_com_object)(REFCLSID, IUnknown *, DWORD, REFIID, void **);
static HRESULT (WINAPI *render_file)(IGraphBuilder *, LPCWSTR, LPCWSTR);

static HRESULT (WINAPI *create_device)(IDirect3D9 *, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS *, IDirect3DDevice9 **);
static HRESULT (WINAPI *reset_device)(IDirect3DDevice9 *, D3DPRESENT_PARAMETERS *);
static HRESULT (WINAPI *present)(IDirect3DDevice9 *, const RECT *, const RECT *, HWND, const RGNDATA *);

#ifdef ARCADE_QA
void qa_initialize(void);
void qa_frame(IDirect3DDevice9 *device, BOOL aiming, const POINT *target);
BOOL qa_should_toggle(void);
void qa_window_changed(BOOL fullscreen, HWND window, POINT center);
void qa_movie_path(LPCWSTR path);
void qa_movie_result(HRESULT graph_result, BOOL connected);
void qa_phase(const char *label, ULONG_PTR value);
#endif

static BOOL replace_pointer(void **slot, void *value) {
    DWORD previous;
    if (!VirtualProtect(slot, sizeof *slot, PAGE_READWRITE, &previous)) return FALSE;
    *slot = value;
    VirtualProtect(slot, sizeof *slot, previous, &previous);
    return TRUE;
}

static IPin *filter_pin(IBaseFilter *filter, PIN_DIRECTION wanted) {
    IEnumPins *pins = NULL;
    if (FAILED(IBaseFilter_EnumPins(filter, &pins))) return NULL;
    IPin *pin = NULL;
    while (IEnumPins_Next(pins, 1, &pin, NULL) == S_OK) {
        PIN_DIRECTION direction;
        if (SUCCEEDED(IPin_QueryDirection(pin, &direction)) && direction == wanted) break;
        IPin_Release(pin);
        pin = NULL;
    }
    IEnumPins_Release(pins);
    return pin;
}

static void free_media_type(AM_MEDIA_TYPE *format) {
    CoTaskMemFree(format->pbFormat);
    if (format->pUnk) IUnknown_Release(format->pUnk);
}

static BOOL connect_color_converter(IGraphBuilder *graph, IPin *decoder, IPin *game_input,
        const AM_MEDIA_TYPE *original, const AM_MEDIA_TYPE *rgb) {
    IBaseFilter *converter = NULL;
    HRESULT result = create_com_object(&CLSID_Colour, NULL, CLSCTX_INPROC_SERVER,
            &IID_IBaseFilter, (void **)&converter);
    if (FAILED(result)) return FALSE;
    IPin *input = NULL, *output = NULL;
    result = IGraphBuilder_AddFilter(graph, converter, L"Arcade Color Converter");
    if (SUCCEEDED(result)) {
        input = filter_pin(converter, PINDIR_INPUT);
        output = filter_pin(converter, PINDIR_OUTPUT);
        result = input && output ? IGraphBuilder_ConnectDirect(graph, decoder, input, original) : E_FAIL;
        if (SUCCEEDED(result)) result = IGraphBuilder_ConnectDirect(graph, output, game_input, rgb);
        if (FAILED(result)) IGraphBuilder_RemoveFilter(graph, converter);
    }
    if (input) IPin_Release(input);
    if (output) IPin_Release(output);
    IBaseFilter_Release(converter);
    return SUCCEEDED(result);
}

static BOOL connect_movie_renderer(IGraphBuilder *graph) {
    IBaseFilter *game_renderer = NULL, *video_renderer = NULL;
    IPin *game_input = NULL, *video_input = NULL, *decoder = NULL;
    IEnumMediaTypes *formats = NULL;
    BOOL connected = FALSE;
    if (FAILED(IGraphBuilder_FindFilterByName(graph, L"Renderer", &game_renderer))) goto done;
    game_input = filter_pin(game_renderer, PINDIR_INPUT);
    if (!game_input) goto done;
    if (SUCCEEDED(IPin_ConnectedTo(game_input, &decoder))) {
        connected = TRUE;
        goto done;
    }
    if (FAILED(IGraphBuilder_FindFilterByName(graph, L"Video Renderer", &video_renderer))) goto done;
    video_input = filter_pin(video_renderer, PINDIR_INPUT);
    if (!video_input || FAILED(IPin_ConnectedTo(video_input, &decoder))) goto done;
    if (FAILED(IPin_EnumMediaTypes(decoder, &formats))) goto done;

    AM_MEDIA_TYPE *rgb = NULL;
    while (IEnumMediaTypes_Next(formats, 1, &rgb, NULL) == S_OK) {
        if (IsEqualGUID(&rgb->subtype, &MEDIASUBTYPE_RGB32) &&
            IsEqualGUID(&rgb->formattype, &FORMAT_VideoInfo)) break;
        free_media_type(rgb);
        CoTaskMemFree(rgb);
        rgb = NULL;
    }
    if (!rgb) goto done;

    AM_MEDIA_TYPE original = {0};
    if (SUCCEEDED(IPin_ConnectionMediaType(video_input, &original))) {
        // The game's renderer only takes RGB32 and copies its samples to a D3D texture.
        IGraphBuilder_Disconnect(graph, decoder);
        IGraphBuilder_Disconnect(graph, video_input);
        HRESULT result = IGraphBuilder_ConnectDirect(graph, decoder, game_input, rgb);
        connected = SUCCEEDED(result) || connect_color_converter(graph, decoder, game_input, &original, rgb);
        if (connected) IGraphBuilder_RemoveFilter(graph, video_renderer);
        else IGraphBuilder_ConnectDirect(graph, decoder, video_input, &original);
        free_media_type(&original);
    }
    free_media_type(rgb);
    CoTaskMemFree(rgb);
done:
    if (formats) IEnumMediaTypes_Release(formats);
    if (decoder) IPin_Release(decoder);
    if (video_input) IPin_Release(video_input);
    if (game_input) IPin_Release(game_input);
    if (video_renderer) IBaseFilter_Release(video_renderer);
    if (game_renderer) IBaseFilter_Release(game_renderer);
    return connected;
}

static void use_original_movie_audio(IGraphBuilder *graph, LPCWSTR path) {
    if (!path) return;
    const wchar_t *name = path;
    for (const wchar_t *c = path; *c; ++c) {
        if (*c == L'/' || *c == L'\\') name = c + 1;
    }
    // The replacement scene AVIs also contain the event audio the game plays from its PCM files.
    if (_wcsnicmp(name, L"scene", 5) && _wcsicmp(name, L"GAME_OVER.vid") &&
            _wcsicmp(name, L"UFO_END.vid")) return;
    IBasicAudio *audio = NULL;
    if (SUCCEEDED(IGraphBuilder_QueryInterface(graph, &IID_IBasicAudio, (void **)&audio))) {
        IBasicAudio_put_Volume(audio, -10000);
        IBasicAudio_Release(audio);
    }
}

static HRESULT WINAPI render_movie(IGraphBuilder *graph, LPCWSTR path, LPCWSTR playlist) {
#ifdef ARCADE_QA
    qa_movie_path(path);
#endif
    HRESULT result = render_file(graph, path, playlist);
    if (SUCCEEDED(result)) use_original_movie_audio(graph, path);
    BOOL connected = SUCCEEDED(result) && connect_movie_renderer(graph);
#ifdef ARCADE_QA
    qa_movie_result(result, connected);
#endif
    if (SUCCEEDED(result) && !connected) {
        IVideoWindow *video = NULL;
        if (SUCCEEDED(IGraphBuilder_QueryInterface(graph, &IID_IVideoWindow, (void **)&video))) {
            IVideoWindow_put_AutoShow(video, OAFALSE);
            IVideoWindow_put_Visible(video, OAFALSE);
            IVideoWindow_Release(video);
        }
    }
    return result;
}

static HRESULT WINAPI create_arcade_com_object(REFCLSID clsid, IUnknown *outer, DWORD context,
        REFIID interface_id, void **object) {
    HRESULT result = create_com_object(clsid, outer, context, interface_id, object);
    if (SUCCEEDED(result) && IsEqualGUID(clsid, &CLSID_FilterGraph)) {
        IGraphBuilder *graph = NULL;
        if (SUCCEEDED(IUnknown_QueryInterface((IUnknown *)*object, &IID_IGraphBuilder, (void **)&graph))) {
            if (graph->lpVtbl->RenderFile != render_movie) {
                render_file = graph->lpVtbl->RenderFile;
                replace_pointer((void **)&graph->lpVtbl->RenderFile, render_movie);
            }
            IGraphBuilder_Release(graph);
        }
    }
    return result;
}

static void __thiscall mark_weapon_frame(BYTE *weapon, void *targets) {
    // The weapon's player index at +0x34 is also passed to the cabinet recoil call.
    if (*(LONG *)(weapon + 0x34) == 0) {
        InterlockedExchange(&last_weapon_update, (LONG)GetTickCount());
    }
    update_weapon(weapon, targets);
}

static void install_weapon_hook(void) {
    BYTE *call = game_image + 0x10b3c9;
    const BYTE expected[] = {0xe8, 0x02, 0xf0, 0xff, 0xff};
    DWORD previous;
    if (memcmp(call, expected, sizeof expected) ||
        !VirtualProtect(call, sizeof expected, PAGE_EXECUTE_READWRITE, &previous)) return;
    update_weapon = (void *)(game_image + 0x10a3d0);
    uint32_t displacement = (uintptr_t)mark_weapon_frame - (uintptr_t)(call + sizeof expected);
    memcpy(call + 1, &displacement, sizeof displacement);
    FlushInstructionCache(GetCurrentProcess(), call, sizeof expected);
    VirtualProtect(call, sizeof expected, previous, &previous);
}

static BOOL player_can_aim(void) {
    DWORD last = (DWORD)InterlockedCompareExchange(&last_weapon_update, 0, 0);
    if (!last || GetTickCount() - last > 180) return FALSE;
    // The game sets this flag while it draws the cinematic into the scene.
    const BYTE *cinematic = *(BYTE **)(game_image + 0x2de5e4);
    if (cinematic && cinematic[0x0c]) return FALSE;
    // Player 1's joined flag and health are used by the original HUD.
    const BYTE *players = *(BYTE **)(game_image + 0x2de4c8);
    if (!players) return FALSE;
    HANDLE process = GetCurrentProcess();
    const BYTE *player = NULL;
    struct { LONG joined; LONG state; float health; } status;
    return ReadProcessMemory(process, players + 0x1c, &player, sizeof player, NULL) && player &&
        ReadProcessMemory(process, player, &status, sizeof status, NULL) &&
        status.joined == 1 && status.health > 0.0f;
}

static BOOL client_to_arcade(HWND window, POINT *point) {
    RECT area;
    if (!GetClientRect(window, &area) || !PtInRect(&area, *point)) return FALSE;
    point->x = (LONG)((int64_t)point->x * GAME_WIDTH / area.right);
    point->y = (LONG)((int64_t)point->y * GAME_HEIGHT / area.bottom);
    return TRUE;
}

static BOOL game_focused(void) {
    HWND foreground = GetForegroundWindow();
    return game_window && (foreground == game_window || IsChild(game_window, foreground));
}

static BOOL pointer_target(POINT *point) {
    return GetCursorPos(point) && ScreenToClient(game_window, point) && client_to_arcade(game_window, point);
}

enum { COORD_BITS = 10, COORD_MASK = (1 << COORD_BITS) - 1, SHOT = 1 << 20, RELOAD = 1 << 21 };

static void queue_shot(HWND window, LPARAM position, BOOL reload) {
    POINT point = {GET_X_LPARAM(position), GET_Y_LPARAM(position)};
    if (!aiming || !game_focused() || !client_to_arcade(window, &point)) return;
    InterlockedExchange(&pending_shot, SHOT | (reload ? RELOAD : 0) | (point.y << COORD_BITS) | point.x);
}

static int __cdecl arcade_gun_status(LONG *state) {
    int result = read_guns(state);
    LONG shot = InterlockedExchange(&pending_shot, 0);
    state[2] = 0;
    state[3] = 0;
    if (shot && aiming && game_focused()) {
        state[0] = shot & COORD_MASK;
        state[1] = (shot >> COORD_BITS) & COORD_MASK;
        state[2] = -1;
        state[3] = shot & RELOAD ? -1 : 0;
    }
    return result;
}

static HCURSOR create_hidden_cursor(void) {
    enum { SIZE = 32 };
    BYTE and_mask[SIZE * SIZE / 8];
    BYTE xor_mask[SIZE * SIZE / 8] = {0};
    memset(and_mask, 0xff, sizeof and_mask);
    return CreateCursor(GetModuleHandleW(NULL), 0, 0, SIZE, SIZE, and_mask, xor_mask);
}

static LRESULT CALLBACK arcade_window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_KILLFOCUS || (message == WM_ACTIVATEAPP && !wparam)) {
        InterlockedExchange(&pending_shot, 0);
    }
    if (message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK) queue_shot(window, lparam, FALSE);
    if (message == WM_RBUTTONDOWN || message == WM_RBUTTONDBLCLK) queue_shot(window, lparam, TRUE);
    if (message == WM_KEYDOWN && wparam == 'R' && !(lparam & (1L << 30)) && aiming && game_focused()) {
        InterlockedExchange(&pending_shot, SHOT | RELOAD);
        return 0;
    }
    if (message == WM_SETCURSOR && LOWORD(lparam) == HTCLIENT && aiming && game_focused()) {
        SetCursor(hidden_cursor);
        return TRUE;
    }
    if (message == WM_SIZE && wparam != SIZE_MINIMIZED) lparam = MAKELPARAM(GAME_WIDTH, GAME_HEIGHT);
    if (previous_window_proc) return CallWindowProcW(previous_window_proc, window, message, wparam, lparam);
    return DefWindowProcW(window, message, wparam, lparam);
}

static HCURSOR WINAPI set_game_cursor(HCURSOR cursor) {
    POINT target;
    return SetCursor(aiming && game_focused() && pointer_target(&target) ? hidden_cursor : cursor);
}

static BOOL WINAPI game_client_rect(HWND window, RECT *rect) {
    BOOL result = GetClientRect(window, rect);
    uintptr_t caller = (uintptr_t)__builtin_return_address(0) - (uintptr_t)game_image;
    // The two cursor-confinement calls still need the real desktop geometry.
    if (result && window == game_window && caller != 0x2a261 && caller != 0x2a2ad &&
            rect->right && rect->bottom) {
        rect->right = GAME_WIDTH;
        rect->bottom = GAME_HEIGHT;
    }
    return result;
}

static void draw_crosshair(IDirect3DDevice9 *device, POINT point) {
    LONG x = point.x, y = point.y;
    const D3DRECT outline[] = {
        {x - 11, y - 2, x - 3, y + 3}, {x + 4, y - 2, x + 12, y + 3},
        {x - 2, y - 11, x + 3, y - 3}, {x - 2, y + 4, x + 3, y + 12},
        {x - 1, y - 1, x + 2, y + 2},
    };
    const D3DRECT red[] = {
        {x - 10, y - 1, x - 4, y + 2}, {x + 5, y - 1, x + 11, y + 2},
        {x - 1, y - 10, x + 2, y - 4}, {x - 1, y + 5, x + 2, y + 11},
        {x, y, x + 1, y + 1},
    };
    IDirect3DDevice9_Clear(device, ARRAYSIZE(outline), outline, D3DCLEAR_TARGET, 0xff180d0d, 0, 0);
    IDirect3DDevice9_Clear(device, ARRAYSIZE(red), red, D3DCLEAR_TARGET, 0xffff5b44, 0, 0);
}

static HWND create_backdrop(void) {
    const wchar_t *class_name = L"SilentHillMacBackdrop";
    HINSTANCE instance = GetModuleHandleW(NULL);
    WNDCLASSW window_class = {0};
    window_class.lpfnWndProc = DefWindowProcW;
    window_class.hInstance = instance;
    window_class.hbrBackground = GetStockObject(BLACK_BRUSH);
    window_class.lpszClassName = class_name;
    if (!RegisterClassW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return NULL;
    return CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, class_name, L"", WS_POPUP,
            0, 0, 1, 1, NULL, NULL, instance, NULL);
}

static void restore_window(void) {
    ShowWindow(backdrop, SW_HIDE);
    SetWindowLongPtrW(game_window, GWL_STYLE, windowed_style);
    SetWindowPlacement(game_window, &windowed_placement);
    SetWindowPos(game_window, NULL, 0, 0, 0, 0,
            SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

static void toggle_fullscreen(BOOL keep_focus) {
    if (fullscreen) {
        restore_window();
        fullscreen = FALSE;
        if (keep_focus) SetForegroundWindow(game_window);
        return;
    }
    MONITORINFO display = { sizeof(MONITORINFO) };
    if (!GetMonitorInfoW(MonitorFromWindow(game_window, MONITOR_DEFAULTTONEAREST), &display) ||
        !GetWindowPlacement(game_window, &windowed_placement)) return;
    windowed_style = GetWindowLongPtrW(game_window, GWL_STYLE);
    if (!backdrop) backdrop = create_backdrop();
    if (!backdrop) return;

    RECT monitor = display.rcMonitor;
    LONG screen_width = monitor.right - monitor.left;
    LONG screen_height = monitor.bottom - monitor.top;
    LONG scale = min(screen_width / 4, screen_height / 3);
    LONG width = 4 * scale;
    LONG height = 3 * scale;
    LONG left = monitor.left + (screen_width - width) / 2;
    LONG top = monitor.top + (screen_height - height) / 2;
    SetWindowLongPtrW(game_window, GWL_STYLE, (windowed_style & ~(LONG_PTR)WS_OVERLAPPEDWINDOW) | WS_POPUP);
    SetWindowPos(backdrop, game_window, monitor.left, monitor.top, screen_width, screen_height,
            SWP_SHOWWINDOW | SWP_NOACTIVATE);
    UINT flags = SWP_FRAMECHANGED | SWP_NOOWNERZORDER;
    if (!keep_focus) flags |= SWP_NOACTIVATE;
    if (!SetWindowPos(game_window, HWND_TOP, left, top, width, height,
                flags)) {
        restore_window();
        return;
    }
    fullscreen = TRUE;
    if (keep_focus) SetForegroundWindow(game_window);
}

static HRESULT WINAPI arcade_present(IDirect3DDevice9 *device, const RECT *source, const RECT *destination,
        HWND window, const RGNDATA *dirty) {
    BOOL focused = game_focused();
    BOOL key_down = (GetAsyncKeyState('F') & 0x8000) != 0;
    BOOL toggle = focused && key_down && !toggle_held;
    toggle_held = key_down;
#ifdef ARCADE_QA
    toggle |= qa_should_toggle();
#endif
    if (toggle) {
        toggle_fullscreen(focused);
        focused = game_focused();
#ifdef ARCADE_QA
        RECT area;
        GetClientRect(game_window, &area);
        POINT center = {area.right / 2, area.bottom / 2};
        client_to_arcade(game_window, &center);
        qa_window_changed(fullscreen, game_window, center);
#endif
    }
    aiming = player_can_aim();
    POINT target;
    BOOL show_target = aiming && focused && pointer_target(&target);
    if (show_target) {
        draw_crosshair(device, target);
        if (!pointer_hidden || GetCursor() != hidden_cursor) SetCursor(hidden_cursor);
        pointer_hidden = TRUE;
    } else if (pointer_hidden && focused) {
        SetCursor(LoadCursorW(NULL, MAKEINTRESOURCEW(32512)));
        pointer_hidden = FALSE;
    }
    BOOL pause_down = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
    BOOL pause = focused && pause_down && !pause_held && aiming;
    pause_held = pause_down;
#ifdef ARCADE_QA
    qa_frame(device, aiming, show_target ? &target : NULL);
#endif
    HRESULT result = present(device, source, destination, window, dirty);
    if (pause) {
        InterlockedExchange(&pending_shot, 0);
        MessageBoxW(game_window, L"Press Esc or click OK to resume.", L"Paused", MB_OK);
    }
    return result;
}

static void fix_backbuffer_size(D3DPRESENT_PARAMETERS *parameters) {
    if (parameters->Windowed) {
        parameters->BackBufferWidth = GAME_WIDTH;
        parameters->BackBufferHeight = GAME_HEIGHT;
    }
}

static HRESULT WINAPI arcade_reset_device(IDirect3DDevice9 *device, D3DPRESENT_PARAMETERS *parameters) {
    fix_backbuffer_size(parameters);
    return reset_device(device, parameters);
}

static HRESULT WINAPI arcade_create_device(IDirect3D9 *d3d, UINT adapter, D3DDEVTYPE type, HWND window, DWORD flags,
        D3DPRESENT_PARAMETERS *parameters, IDirect3DDevice9 **device) {
#ifdef ARCADE_QA
    qa_phase("CreateDevice entered", (ULONG_PTR)window);
#endif
    fix_backbuffer_size(parameters);
    HRESULT result = create_device(d3d, adapter, type, window, flags, parameters, device);
#ifdef ARCADE_QA
    qa_phase("CreateDevice returned", result);
#endif
    if (SUCCEEDED(result)) {
        HWND target = parameters->hDeviceWindow ? parameters->hDeviceWindow : window;
        if (target && target != game_window) {
            if (previous_window_proc && IsWindow(game_window)) {
                SetWindowLongPtrW(game_window, GWLP_WNDPROC, (LONG_PTR)previous_window_proc);
            }
            game_window = target;
            if (!hidden_cursor) hidden_cursor = create_hidden_cursor();
#ifdef ARCADE_QA
            qa_phase("Hidden cursor", (ULONG_PTR)hidden_cursor);
#endif
            previous_window_proc = (WNDPROC)SetWindowLongPtrW(game_window, GWLP_WNDPROC, (LONG_PTR)arcade_window_proc);
        }
        if ((*device)->lpVtbl->Present != arcade_present) {
            present = (*device)->lpVtbl->Present;
            replace_pointer((void **)&(*device)->lpVtbl->Present, arcade_present);
        }
        if ((*device)->lpVtbl->Reset != arcade_reset_device) {
            reset_device = (*device)->lpVtbl->Reset;
            replace_pointer((void **)&(*device)->lpVtbl->Reset, arcade_reset_device);
        }
    }
    return result;
}

IDirect3D9 *WINAPI ArcadeDirect3DCreate9(UINT version) {
    if (!game_image) {
        game_image = (BYTE *)GetModuleHandleW(NULL);
        void **slot = (void **)(game_image + 0x280488);
        create_com_object = (void *)*slot;
        replace_pointer(slot, create_arcade_com_object);
        // The original window resets the cursor when the Mac gives it focus again.
        replace_pointer((void **)(game_image + 0x2802b8), set_game_cursor);
        replace_pointer((void **)(game_image + 0x2802b4), game_client_rect);
        void **gun_slot = (void **)(game_image + 0x2804d8);
        read_guns = (void *)*gun_slot;
        replace_pointer(gun_slot, arcade_gun_status);
        install_weapon_hook();
    }
#ifdef ARCADE_QA
    qa_initialize();
    qa_phase("Direct3DCreate9 entered", version);
#endif
    IDirect3D9 *d3d = Direct3DCreate9(version);
#ifdef ARCADE_QA
    qa_phase("Direct3DCreate9 returned", (ULONG_PTR)d3d);
#endif
    if (d3d && d3d->lpVtbl->CreateDevice != arcade_create_device) {
        create_device = d3d->lpVtbl->CreateDevice;
        replace_pointer((void **)&d3d->lpVtbl->CreateDevice, arcade_create_device);
    }
    return d3d;
}
