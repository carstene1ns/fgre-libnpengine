/* 
 * libnpengine: Nitroplus script interpreter
 * Copyright (C) 2014 Mislav Blažević <krofnica996@gmail.com>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * */
#include "Window.hpp"
#include "Texture.hpp"

Window::Window(const char* WindowTitle, const int Width, const int Height) : WIDTH(Width), HEIGHT(Height), IsRunning(true)
{
    SDL_Init(SDL_INIT_VIDEO);
    SDLWindow = SDL_CreateWindow(WindowTitle, 0, 0, WIDTH, HEIGHT, SDL_WINDOW_OPENGL);
    GLContext = SDL_GL_CreateContext(SDLWindow);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glViewport(0, 0, WIDTH, HEIGHT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WIDTH, HEIGHT, 0, -1, 1);
}

Window::~Window()
{
    SDL_GL_DeleteContext(GLContext);
    SDL_DestroyWindow(SDLWindow);
    SDL_Quit();
}

void Window::Run()
{
    SDL_Event Event;
    while (IsRunning)
    {
        while (SDL_PollEvent(&Event))
            HandleEvent(Event);

        Draw();
        RunInterpreter();
        SDL_Delay(16);
    }
}

void Window::HandleEvent(SDL_Event Event)
{
    switch (Event.type)
    {
    case SDL_QUIT:
        IsRunning = false;
        break;
    default:
        break;
    }
}

void Window::Draw()
{
    glClear(GL_COLOR_BUFFER_BIT);
    for (auto i = Textures.begin(); i != Textures.end(); ++i)
        (*i)->Draw();
    SDL_GL_SwapWindow(SDLWindow);
}

void Window::AddTexture(Texture* pTexture)
{
    Textures.push_back(pTexture);
}
