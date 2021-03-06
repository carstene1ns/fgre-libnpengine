/* 
 * libnpengine: Nitroplus script interpreter
 * Copyright (C) 2014-2016,2018 Mislav Blažević <krofnica996@gmail.com>
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
#ifndef WINDOW_HPP
#define WINDOW_HPP

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <list>
using namespace std;

class Texture;
class NSBInterpreter;
class Window
{
public:
    Window(const char* WindowTitle, const int Width, const int Height);
    virtual ~Window();

    static void PushMoveCursorEvent(int X, int Y);

    void Run();
    void Exit();
    void Select(bool Enable);
    void AddTexture(Texture* pTexture);
    void RemoveTexture(Texture* pTexture);
    void MoveCursor(int32_t X, int32_t Y);
    bool IsRunning_() { return IsRunning; }
    void SetFullscreen(Uint32 flags);
    void DrawTextures(uint32_t Diff);

    const int WIDTH;
    const int HEIGHT;
protected:
    void HandleEvent(SDL_Event& Event);

    NSBInterpreter* pInterpreter;
private:
    void Draw();

    uint32_t LastDrawTime;
    bool IsRunning;
    bool EventLoop;
    SDL_Window* SDLWindow;
    SDL_GLContext GLContext;
    list<Texture*> Textures;
};

#endif
