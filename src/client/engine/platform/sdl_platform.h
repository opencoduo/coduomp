#ifndef CODUOMP_SDL_PLATFORM_H
#define CODUOMP_SDL_PLATFORM_H

#include "../q_shared.h"

#include <stddef.h>

#if !defined(_WIN32)

qboolean CoduoSDL_Init(void);
void CoduoSDL_Shutdown(void);
qboolean CoduoSDL_CreateOpenGLWindow(int32_t width, int32_t height,
                                     int32_t colorBits, int32_t depthBits,
                                     int32_t stencilBits,
                                     int32_t windowMode);
void CoduoSDL_DestroyOpenGLWindow(void);
void CoduoSDL_GetDesktopMode(int32_t *width, int32_t *height,
                             int32_t *refreshRate);
qboolean coduomp_sdl_get_native_display_mode_compat(int32_t *width,
                                                     int32_t *height,
                                                     int32_t *refreshRate);
qboolean coduomp_sdl_display_mode_available_compat(int32_t width,
                                                    int32_t height);
void coduomp_sdl_window_size_for_drawable_compat(int32_t *width,
                                                  int32_t *height);
void CoduoSDL_GetFramebufferSize(int32_t *width, int32_t *height);
void coduomp_sdl_get_window_size_compat(int32_t *width, int32_t *height);
qboolean coduomp_sdl_get_window_gamma_ramp(uint16_t red[256],
                                           uint16_t green[256],
                                           uint16_t blue[256]);
qboolean coduomp_sdl_set_window_gamma_ramp(const uint16_t red[256],
                                           const uint16_t green[256],
                                           const uint16_t blue[256]);
const char *coduomp_sdl_error_compat(void);
void CoduoSDL_GetOpenGLFormat(int32_t *colorBits, int32_t *depthBits,
                              int32_t *stencilBits);
void CoduoSDL_GetOpenGLSymbol(const char *name, void *destination,
                              size_t destinationSize);
void CoduoSDL_SwapWindow(void);
void CoduoSDL_SetSwapInterval(int32_t interval);
void CoduoSDL_PumpEvents(void);
void CoduoSDL_SetRelativeMouse(qboolean active);
qboolean CoduoSDL_HasOpenGLWindow(void);
void CoduoSDL_ShowErrorDialog(const char *message, const char *title);
char *coduomp_sdl_get_clipboard_text_compat(void);

#endif

#endif
