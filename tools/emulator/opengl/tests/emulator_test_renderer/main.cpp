/*
* Copyright (C) 2011 The Android Open Source Project
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
#undef HAVE_MALLOC_H
#include <SDL.h>
#include <SDL_syswm.h>
#include <stdio.h>
#include <string.h>
#include "libOpenglRender/render_api.h"
#include <EventInjector.h>

static int convert_keysym(int sym); // forward

#ifdef _WIN32
#include <winsock2.h>
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
#else
int main(int argc, char *argv[])
#endif
{
    int portNum = 22468;
    int winWidth = 320;
    int winHeight = 480;
    int width, height;
    int mouseDown = 0;
    const char* env = getenv("ANDROID_WINDOW_SIZE");
    FBNativeWindowType windowId = NULL;
    EventInjector* injector;
    int consolePort = 5554;

    if (env && sscanf(env, "%dx%d", &width, &height) == 2) {
        winWidth = width;
        winHeight = height;
    }

    //
    // Inialize SDL window
    //
    if (SDL_Init(SDL_INIT_NOPARACHUTE | SDL_INIT_VIDEO)) {
        fprintf(stderr,"SDL init failed: %s\n", SDL_GetError());
        return -1;
    }

    SDL_Surface *surface = SDL_SetVideoMode(winWidth, winHeight, 32, SDL_SWSURFACE);
    if (surface == NULL) {
        fprintf(stderr,"Failed to set video mode: %s\n", SDL_GetError());
        return -1;
    }

    SDL_SysWMinfo  wminfo;
    memset(&wminfo, 0, sizeof(wminfo));
    SDL_GetWMInfo(&wminfo);
#ifdef _WIN32
    windowId = wminfo.window;
    WSADATA  wsaData;
    int      rc = WSAStartup( MAKEWORD(2,2), &wsaData);
    if (rc != 0) {
            printf( "could not initialize Winsock\n" );
    }
#elif __linux__
    windowId = wminfo.info.x11.window;
#elif __APPLE__
    windowId = wminfo.nsWindowPtr;
#endif

    printf("initializing renderer process\n");

    //
    // initialize OpenGL renderer to render in our window
    //
    bool inited = initOpenGLRenderer(windowId, 0, 0,
                                     winWidth, winHeight, portNum);
    if (!inited) {
        return -1;
    }
    printf("renderer process started\n");

    injector = new EventInjector(consolePort);

    // Just wait until the window is closed
    SDL_Event ev;

    for (;;) {
        injector->wait(1000/15);
        injector->poll();

        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_MOUSEBUTTONDOWN:
                if (!mouseDown) {
                    injector->sendMouseDown(ev.button.x, ev.button.y);
                    mouseDown = 1;
                }
		break;
            case SDL_MOUSEBUTTONUP:
                if (mouseDown) {
                    injector->sendMouseUp(ev.button.x,ev.button.y);
                    mouseDown = 0;
                }
                break;
            case SDL_MOUSEMOTION:
                if (mouseDown)
                    injector->sendMouseMotion(ev.button.x,ev.button.y);
                break;

            case SDL_KEYDOWN:
#ifdef __APPLE__
                /* special code to deal with Command-Q properly */
                if (ev.key.keysym.sym == SDLK_q &&
                    ev.key.keysym.mod & KMOD_META) {
                  goto EXIT;
                }
#endif
		injector->sendKeyDown(convert_keysym(ev.key.keysym.sym));
                break;
            case SDL_KEYUP:
                injector->sendKeyUp(convert_keysym(ev.key.keysym.sym));
                break;
            case SDL_QUIT:
                goto EXIT;
            }
        }
    }
EXIT:
    //
    // stop the renderer
    //
    printf("stopping the renderer process\n");
    stopOpenGLRenderer();

    return 0;
}

static int convert_keysym(int sym)
{
#define  EE(x,y)   SDLK_##x, EventInjector::KEY_##y,
    static const int keymap[] = {
        EE(LEFT,LEFT)
        EE(RIGHT,RIGHT)
        EE(DOWN,DOWN)
        EE(UP,UP)
        EE(RETURN,ENTER)
        EE(F1,SOFT1)
        EE(ESCAPE,BACK)
        EE(HOME,HOME)
        -1
    };
    int nn;
    for (nn = 0; keymap[nn] >= 0; nn += 2) {
        if (keymap[nn] == sym)
            return keymap[nn+1];
    }
    return sym;
}
