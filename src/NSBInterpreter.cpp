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
#include "NSBInterpreter.hpp"
#include "NSBContext.hpp"
#include "Texture.hpp"
#include "Image.hpp"
#include "Window.hpp"
#include "Movie.hpp"
#include "Text.hpp"
#include "Scrollbar.hpp"
#include "nsbmagic.hpp"
#include "nsbconstants.hpp"
#include "scriptfile.hpp"
#include "npafile.hpp"
#include "fscommon.hpp"
#include "buffer.hpp"
#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <memory>
#include <algorithm>

#define NSB_ERROR(MSG1, MSG2) cout << __PRETTY_FUNCTION__ << ": " << MSG1 << " " << MSG2 << endl;
#define NSB_VARARGS 0xFF

extern "C" { void gst_init(int* argc, char** argv[]); }

NSBInterpreter::NSBInterpreter(Window* pWindow) :
pDebuggerThread(nullptr),
LogCalls(false),
DbgStepping(false),
RunInterpreter(true),
SkipHack(false),
pWindow(pWindow),
pContext(nullptr),
Builtins(MAGIC_UNK119 + 1, {nullptr, 0})
{
    gst_init(nullptr, nullptr);
    srand(time(0));

    Builtins[MAGIC_FUNCTION_DECLARATION] = { &NSBInterpreter::FunctionDeclaration, 0 };
    Builtins[MAGIC_CALL_FUNCTION] = { &NSBInterpreter::CallFunction, 0 };
    Builtins[MAGIC_CALL_SCENE] = { &NSBInterpreter::CallScene, 0 };
    Builtins[MAGIC_CALL_CHAPTER] = { &NSBInterpreter::CallChapter, 0 };
    Builtins[MAGIC_CMP_LOGICAL_AND] = { &NSBInterpreter::CmpLogicalAnd, 2 };
    Builtins[MAGIC_CMP_LOGICAL_OR] = { &NSBInterpreter::CmpLogicalOr, 2 };
    Builtins[MAGIC_LOGICAL_GREATER_EQUAL] = { &NSBInterpreter::LogicalGreaterEqual, 2 };
    Builtins[MAGIC_LOGICAL_LESS_EQUAL] = { &NSBInterpreter::LogicalLessEqual, 2 };
    Builtins[MAGIC_CMP_GREATER] = { &NSBInterpreter::CmpGreater, 2 };
    Builtins[MAGIC_CMP_LESS] = { &NSBInterpreter::CmpLess, 2 };
    Builtins[MAGIC_CMP_EQUAL] = { &NSBInterpreter::CmpEqual, 2 };
    Builtins[MAGIC_LOGICAL_NOT_EQUAL] = { &NSBInterpreter::LogicalNotEqual, 2 };
    Builtins[MAGIC_LOGICAL_NOT] = { &NSBInterpreter::LogicalNot, 1 };
    Builtins[MAGIC_ADD_EXPRESSION] = { &NSBInterpreter::AddExpression, 2 };
    Builtins[MAGIC_SUB_EXPRESSION] = { &NSBInterpreter::SubExpression, 2 };
    Builtins[MAGIC_MUL_EXPRESSION] = { &NSBInterpreter::MulExpression, 2 };
    Builtins[MAGIC_DIV_EXPRESSION] = { &NSBInterpreter::DivExpression, 2 };
    Builtins[MAGIC_MOD_EXPRESSION] = { &NSBInterpreter::ModExpression, 2 };
    Builtins[MAGIC_INCREMENT] = { &NSBInterpreter::Increment, 1 };
    Builtins[MAGIC_DECREMENT] = { &NSBInterpreter::Decrement, 1 };
    Builtins[MAGIC_LITERAL] = { &NSBInterpreter::Literal, 0 };
    Builtins[MAGIC_ASSIGN] = { &NSBInterpreter::Assign, 1 };
    Builtins[MAGIC_VARIABLE] = { &NSBInterpreter::Get, 0 };
    Builtins[MAGIC_SCOPE_BEGIN] = { &NSBInterpreter::ScopeBegin, 0 };
    Builtins[MAGIC_SCOPE_END] = { &NSBInterpreter::ScopeEnd, 0 };
    Builtins[MAGIC_RETURN] = { &NSBInterpreter::Return, 0 };
    Builtins[MAGIC_END_FUNCTION] = { &NSBInterpreter::Return, 0 };
    Builtins[MAGIC_END_SCENE] = { &NSBInterpreter::Return, 0 };
    Builtins[MAGIC_END_CHAPTER] = { &NSBInterpreter::Return, 0 };
    Builtins[MAGIC_IF] = { &NSBInterpreter::If, 1 };
    Builtins[MAGIC_WHILE] = { &NSBInterpreter::While, 1 };
    Builtins[MAGIC_WHILE_END] = { &NSBInterpreter::WhileEnd, 0 };
    Builtins[MAGIC_SELECT] = { &NSBInterpreter::Select, 0 };
    Builtins[MAGIC_SELECT_END] = { &NSBInterpreter::SelectEnd, 0 };
    Builtins[MAGIC_SELECT_BREAK_END] = { &NSBInterpreter::SelectBreakEnd, 0 };
    Builtins[MAGIC_BREAK] = { &NSBInterpreter::Break, 0 };
    Builtins[MAGIC_JUMP] = { &NSBInterpreter::Jump, 0 };
    Builtins[MAGIC_ADD_ASSIGN] = { &NSBInterpreter::AddAssign, 1 };
    Builtins[MAGIC_SUB_ASSIGN] = { &NSBInterpreter::SubAssign, 1 };
    Builtins[MAGIC_WRITE_FILE] = { &NSBInterpreter::WriteFile, 2 };
    Builtins[MAGIC_READ_FILE] = { &NSBInterpreter::ReadFile, 1 };
    Builtins[MAGIC_CREATE_TEXTURE] = { &NSBInterpreter::CreateTexture, 5 };
    Builtins[MAGIC_IMAGE_HORIZON] = { &NSBInterpreter::ImageHorizon, 1 };
    Builtins[MAGIC_IMAGE_VERTICAL] = { &NSBInterpreter::ImageVertical, 1 };
    Builtins[MAGIC_TIME] = { &NSBInterpreter::Time, 0 };
    Builtins[MAGIC_STR_STR] = { &NSBInterpreter::StrStr, 2 };
    Builtins[MAGIC_EXIT] = { &NSBInterpreter::Exit, 0 };
    Builtins[MAGIC_CURSOR_POSITION] = { &NSBInterpreter::CursorPosition, 2 };
    Builtins[MAGIC_MOVE_CURSOR] = { &NSBInterpreter::MoveCursor, 2 };
    Builtins[MAGIC_POSITION] = { &NSBInterpreter::Position, 3 };
    Builtins[MAGIC_WAIT] = { &NSBInterpreter::Wait, 1 };
    Builtins[MAGIC_WAIT_KEY] = { &NSBInterpreter::WaitKey, NSB_VARARGS };
    Builtins[MAGIC_NEGA_EXPRESSION] = { &NSBInterpreter::NegaExpression, 1 };
    Builtins[MAGIC_SYSTEM] = { &NSBInterpreter::System, 3 };
    Builtins[MAGIC_STRING] = { &NSBInterpreter::String, NSB_VARARGS };
    Builtins[MAGIC_VARIABLE_VALUE] = { &NSBInterpreter::VariableValue, NSB_VARARGS };
    Builtins[MAGIC_CREATE_PROCESS] = { &NSBInterpreter::CreateProcess, 5 };
    Builtins[MAGIC_COUNT] = { &NSBInterpreter::Count, 1 };
    Builtins[MAGIC_ARRAY] = { &NSBInterpreter::Array, NSB_VARARGS };
    Builtins[MAGIC_SUB_SCRIPT] = { &NSBInterpreter::SubScript, 0 };
    Builtins[MAGIC_ASSOC_ARRAY] = { &NSBInterpreter::AssocArray, NSB_VARARGS };
    Builtins[MAGIC_MODULE_FILE_NAME] = { &NSBInterpreter::ModuleFileName, 0 };
    Builtins[MAGIC_REQUEST] = { &NSBInterpreter::Request, 2 };
    Builtins[MAGIC_SET_VERTEX] = { &NSBInterpreter::SetVertex, 3 };
    Builtins[MAGIC_ZOOM] = { &NSBInterpreter::Zoom, 6 };
    Builtins[MAGIC_MOVE] = { &NSBInterpreter::Move, 6 };
    Builtins[MAGIC_SET_SHADE] = { &NSBInterpreter::SetShade, 2 };
    Builtins[MAGIC_DRAW_TO_TEXTURE] = { &NSBInterpreter::DrawToTexture, 4 };
    Builtins[MAGIC_CREATE_RENDER_TEXTURE] = { &NSBInterpreter::CreateRenderTexture, 4 };
    Builtins[MAGIC_DRAW_TRANSITION] = { &NSBInterpreter::DrawTransition, 8 };
    Builtins[MAGIC_CREATE_COLOR] = { &NSBInterpreter::CreateColor, 7 };
    Builtins[MAGIC_LOAD_IMAGE] = { &NSBInterpreter::LoadImage, 2 };
    Builtins[MAGIC_FADE] = { &NSBInterpreter::Fade, 5 };
    Builtins[MAGIC_DELETE] = { &NSBInterpreter::Delete, 1 };
    Builtins[MAGIC_CLEAR_PARAMS] = { &NSBInterpreter::ClearParams, 0 };
    Builtins[MAGIC_SET_LOOP] = { &NSBInterpreter::SetLoop, 2 };
    Builtins[MAGIC_SET_VOLUME] = { &NSBInterpreter::SetVolume, 4 };
    Builtins[MAGIC_SET_LOOP_POINT] = { &NSBInterpreter::SetLoopPoint, 3 };
    Builtins[MAGIC_CREATE_SOUND] = { &NSBInterpreter::CreateSound, 3 };
    Builtins[MAGIC_REMAIN_TIME] = { &NSBInterpreter::RemainTime, 1 };
    Builtins[MAGIC_CREATE_MOVIE] = { &NSBInterpreter::CreateMovie, 8 };
    Builtins[MAGIC_DURATION_TIME] = { &NSBInterpreter::DurationTime, 1 };
    Builtins[MAGIC_SET_FREQUENCY] = { &NSBInterpreter::SetFrequency, 4 };
    Builtins[MAGIC_SET_PAN] = { &NSBInterpreter::SetPan, 4 };
    Builtins[MAGIC_SET_ALIAS] = { &NSBInterpreter::SetAlias, 2 };
    Builtins[MAGIC_CREATE_NAME] = { &NSBInterpreter::CreateName, 1 };
    Builtins[MAGIC_CREATE_WINDOW] = { &NSBInterpreter::CreateWindow, 7 };
    Builtins[MAGIC_CREATE_CHOICE] = { &NSBInterpreter::CreateChoice, NSB_VARARGS };
    Builtins[MAGIC_CASE] = { &NSBInterpreter::Case, 0 };
    Builtins[MAGIC_CASE_END] = { &NSBInterpreter::CaseEnd, 0 };
    Builtins[MAGIC_SET_NEXT_FOCUS] = { &NSBInterpreter::SetNextFocus, 3 };
    Builtins[MAGIC_PASSAGE_TIME] = { &NSBInterpreter::PassageTime, 1 };
    Builtins[MAGIC_PARSE_TEXT] = { &NSBInterpreter::ParseText, 0 };
    Builtins[MAGIC_LOAD_TEXT] = { &NSBInterpreter::LoadText, 7 };
    Builtins[MAGIC_WAIT_TEXT] = { &NSBInterpreter::WaitText, 2 };
    Builtins[MAGIC_LOCK_VIDEO] = { &NSBInterpreter::LockVideo, 1 };
    Builtins[MAGIC_SAVE] = { &NSBInterpreter::Save, 1 };
    Builtins[MAGIC_DELETE_SAVE_FILE] = { &NSBInterpreter::DeleteSaveFile, 1};
    Builtins[MAGIC_CONQUEST] = { &NSBInterpreter::Conquest, 3 };
    Builtins[MAGIC_CLEAR_SCORE] = { &NSBInterpreter::ClearScore, 1 };
    Builtins[MAGIC_CLEAR_BACKLOG] = { &NSBInterpreter::ClearBacklog, 0 };
    Builtins[MAGIC_SET_FONT] = { &NSBInterpreter::SetFont, 6 };
    Builtins[MAGIC_SET_SHORTCUT] = { &NSBInterpreter::SetShortcut, 2 };
    Builtins[MAGIC_CREATE_CLIP_TEXTURE] = { &NSBInterpreter::CreateClipTexture, 9 };
    Builtins[MAGIC_EXIST_SAVE] = { &NSBInterpreter::ExistSave, 1 };
    Builtins[MAGIC_WAIT_ACTION] = { &NSBInterpreter::WaitAction, NSB_VARARGS };
    Builtins[MAGIC_LOAD] = { &NSBInterpreter::Load, 1 };
    Builtins[MAGIC_SET_BACKLOG] = { &NSBInterpreter::SetBacklog, 3 };
    Builtins[MAGIC_CREATE_TEXT] = { &NSBInterpreter::CreateText, 7 };
    Builtins[MAGIC_AT_EXPRESSION] = { &NSBInterpreter::AtExpression, 1 };
    Builtins[MAGIC_RANDOM] = { &NSBInterpreter::Random, 1 };
    Builtins[MAGIC_CREATE_EFFECT] = { &NSBInterpreter::CreateEffect, 7 };
    Builtins[MAGIC_SET_TONE] = { &NSBInterpreter::SetTone, 2 };
    Builtins[MAGIC_DATE_TIME] = { &NSBInterpreter::DateTime, 6};
    Builtins[MAGIC_SHAKE] = { &NSBInterpreter::Shake, 9};
    Builtins[MAGIC_MOVIE_PLAY] = { &NSBInterpreter::MoviePlay, 2};
    Builtins[MAGIC_SET_STREAM] = { &NSBInterpreter::SetStream, 2};
    Builtins[MAGIC_WAIT_PLAY] = { &NSBInterpreter::WaitPlay, 2};
    Builtins[MAGIC_WAIT_FADE] = { &NSBInterpreter::WaitFade, 2};
    Builtins[MAGIC_SOUND_AMPLITUDE] = { &NSBInterpreter::SoundAmplitude, 2};
    Builtins[MAGIC_ROTATE] = { &NSBInterpreter::Rotate, 7};
    Builtins[MAGIC_MESSAGE] = { &NSBInterpreter::Message, 4};
    Builtins[MAGIC_INTEGER] = { &NSBInterpreter::Integer, 1};
    Builtins[MAGIC_CREATE_SCROLLBAR] = { &NSBInterpreter::CreateScrollbar, 14};
    Builtins[MAGIC_SET_SCROLLBAR_VALUE] = { &NSBInterpreter::SetScrollbarValue, 2};
    Builtins[MAGIC_SET_SCROLLBAR_WHEEL_AREA] = { &NSBInterpreter::SetScrollbarWheelArea, 5};
    Builtins[MAGIC_SCROLLBAR_VALUE] = { &NSBInterpreter::ScrollbarValue, 1};
    Builtins[MAGIC_CREATE_STENCIL] = { &NSBInterpreter::CreateStencil, 7};
    Builtins[MAGIC_CREATE_MASK] = { &NSBInterpreter::CreateMask, 6};

    pContext = new NSBContext("__main__");
    pContext->Start();
    Threads.push_back(pContext);
}

NSBInterpreter::~NSBInterpreter()
{
    if (pDebuggerThread)
        pDebuggerThread->join();

    delete pDebuggerThread;
    for (NSBContext* pContext : Threads)
        if (pContext->GetName() == "__main__" || pContext->GetName() == "UNK")
            delete pContext;
}

void NSBInterpreter::ExecuteLocalScript(const string& Filename)
{
    ScriptFile* pScript = new ScriptFile(Filename, ScriptFile::NSS);
    for (const string& i : pScript->GetIncludes())
        sResourceMgr->GetScriptFile(i);
    pContext->Call(pScript, "chapter.main");
}

void NSBInterpreter::ExecuteScript(const string& Filename)
{
    CallScript(Filename, "chapter.main");
}

void NSBInterpreter::ExecuteScriptThread(const string& Filename)
{
    NSBContext* pThread = new NSBContext("UNK");
    AddThread(pThread);
    if (ScriptFile* pScript = sResourceMgr->GetScriptFile(Filename))
        pThread->Call(pScript, "chapter.main");
}

void NSBInterpreter::Run(int NumCommands)
{
    for (int i = 0; i < NumCommands; ++i)
        RunCommand();
}

void NSBInterpreter::RunCommand()
{
    if (Threads.empty())
        Exit();

    if (!RunInterpreter)
        return;

    ThreadsModified = false;
    for (auto i = Threads.begin(); i != Threads.end(); ++i)
    {
        pContext = *i;
        pContext->TryWake();

        while (pContext->IsActive() && !pContext->IsStarving() && !pContext->IsSleeping() && pContext->Advance() != MAGIC_CLEAR_PARAMS)
        {
            if (pContext->GetName() == "__main__")
                DebuggerTick();

            if (pContext->GetMagic() < Builtins.size())
                 Call(pContext->GetMagic());
        }

        ClearParams();
        if (pContext->IsStarving())
        {
            ObjectHolder.Delete(pContext->GetName());
            RemoveThread(pContext);
        }

        if (ThreadsModified)
            break;
    }
}

void NSBInterpreter::Update(uint32_t Diff)
{
    for (NSBContext* pContext : Threads)
        pContext->Update(Diff);
}

void NSBInterpreter::PushEvent(const SDL_Event& Event)
{
    Events.push(Event);
}

void NSBInterpreter::HandleEvent(const SDL_Event& Event)
{
    /*
      TODO:
      $SYSTEM_mousewheel_down
      $SYSTEM_mousewheel_up
      _onetime
    */
    switch (Event.type)
    {
    case SDL_MOUSEBUTTONDOWN:
        ProcessButton(Event.button.button, "true");
        for (auto pContext : Threads)
            pContext->OnClick();
        break;
    case SDL_MOUSEBUTTONUP:
        ProcessButton(Event.button.button, "false");
        break;
    case SDL_KEYDOWN:
        if (Event.key.keysym.sym == SDLK_F1)
            SkipHack = !SkipHack;

        ProcessKey(Event.key.keysym.sym, "true");
        for (NSBShortcut& Shortcut : Shortcuts)
            if (Shortcut.Key == Event.key.keysym.sym)
                CallScriptThread(Shortcut.Script, "chapter.main");
        break;
    case SDL_KEYUP:
        ProcessKey(Event.key.keysym.sym, "false");
        break;
    }
}

void NSBInterpreter::ProcessButton(int button, const string& Val)
{
    if (button == SDL_BUTTON_LEFT)
        SetString("$SYSTEM_l_button_down", Val);
    else if (button == SDL_BUTTON_RIGHT)
        SetString("$SYSTEM_r_button_down", Val);
}

void NSBInterpreter::ProcessKey(int Key, const string& Val)
{
    static const map<int, string> keydown =
    {
        { SDLK_RCTRL, "$SYSTEM_keydown_ctrl" },
        { SDLK_KP_ENTER, "$SYSTEM_keydown_enter" },
        { SDLK_RETURN, "$SYSTEM_keydown_enter" },
        { SDLK_END, "$SYSTEM_keydown_end" },
        { SDLK_ESCAPE, "$SYSTEM_keydown_esc" },
        { SDLK_LEFT, "$SYSTEM_keydown_left" },
        { SDLK_RIGHT, "$SYSTEM_keydown_right" },
        { SDLK_DOWN, "$SYSTEM_keydown_up" },
        { SDLK_UP, "$SYSTEM_keydown_down" },
        { SDLK_ESCAPE, "$SYSTEM_keydown_esc" },
        { SDLK_HOME, "$SYSTEM_keydown_home" },
        { SDLK_PAGEDOWN, "$SYSTEM_keydown_pagedown" },
        { SDLK_PAGEUP, "$SYSTEM_keydown_pageup" },
        { SDLK_RSHIFT, "$SYSTEM_keydown_shift" },
        { SDLK_SPACE, "$SYSTEM_keydown_space" },
        { SDLK_TAB, "$SYSTEM_keydown_tab" }
    };
    if (Key >= SDLK_a && Key <= SDLK_z)
    {
        char c = 'a' + Key - SDLK_a;
        string Identifier = "$SYSTEM_keydown_" + string(1, c);
        SetString(Identifier, Val);
    }
    auto it = keydown.find(Key);
    if (it != keydown.end())
        SetString(it->second, Val);
}

void NSBInterpreter::FunctionDeclaration()
{
    Params.Begin(pContext->GetNumParams() - 1);
    for (int i = 1; i < pContext->GetNumParams(); ++i)
        Assign_(i);
}

void NSBInterpreter::CallFunction()
{
    CallFunction_(pContext, pContext->GetParam(0));
}

void NSBInterpreter::CallScene()
{
    CallScriptSymbol("scene.");
}

void NSBInterpreter::CallChapter()
{
    CallScriptSymbol("chapter.");
}

void NSBInterpreter::CmpLogicalAnd()
{
    BoolBinaryOp(logical_and<bool>());
}

void NSBInterpreter::CmpLogicalOr()
{
    BoolBinaryOp(logical_or<bool>());
}

void NSBInterpreter::LogicalGreaterEqual()
{
    if (Params.Top()->IsFloat() || Params.TTop()->IsFloat())
        FloatBinaryOp(greater_equal<float>());
    else
        IntBinaryOp(greater_equal<int32_t>());
}

void NSBInterpreter::CmpGreater()
{
    if (Params.Top()->IsFloat() || Params.TTop()->IsFloat())
        FloatBinaryOp(greater<float>());
    else
        IntBinaryOp(greater<int32_t>());
}

void NSBInterpreter::CmpLess()
{
    if (Params.Top()->IsFloat() || Params.TTop()->IsFloat())
        FloatBinaryOp(less<float>());
    else
        IntBinaryOp(less<int32_t>());
}

void NSBInterpreter::LogicalLessEqual()
{
    if (Params.Top()->IsFloat() || Params.TTop()->IsFloat())
        FloatBinaryOp(less_equal<float>());
    else
        IntBinaryOp(less_equal<int32_t>());
}

void NSBInterpreter::CmpEqual()
{
    Variable* pLhs = PopVar();
    Variable* pRhs = PopVar();

    bool Equal = false;
    if (pLhs->IsInt() && pRhs->IsInt())
        Equal = pLhs->ToInt() == pRhs->ToInt();
    else if (pLhs->IsString() && pRhs->IsString())
        Equal = pLhs->ToString() == pRhs->ToString();
    else if (pLhs->ToFloat() && pRhs->ToFloat())
        Equal = pLhs->ToFloat() == pRhs->ToFloat();

    Variable::Destroy(pLhs);
    Variable::Destroy(pRhs);
    PushVar(Variable::MakeInt(Equal));
}

void NSBInterpreter::LogicalNotEqual()
{
    CmpEqual();
    Call(MAGIC_LOGICAL_NOT);
}

void NSBInterpreter::LogicalNot()
{
    PushVar(Variable::MakeInt(!PopBool()));
}

void NSBInterpreter::AddExpression()
{
    if (Params.Top()->IsFloat() || Params.TTop()->IsFloat())
        FloatBinaryOp(plus<float>());
    else
    {
        Variable* pLhs = PopVar();
        Variable* pRhs = PopVar();
        PushVar(Variable::Add(pLhs, pRhs));
    }
}

void NSBInterpreter::SubExpression()
{
    if (Params.Top()->IsFloat() || Params.TTop()->IsFloat())
        FloatBinaryOp(minus<float>());
    else
        IntBinaryOp(minus<int32_t>());
}

void NSBInterpreter::MulExpression()
{
    if (Params.Top()->IsFloat() || Params.TTop()->IsFloat())
        FloatBinaryOp(multiplies<float>());
    else
        IntBinaryOp(multiplies<int32_t>());
}

void NSBInterpreter::DivExpression()
{
    if (Params.Top()->IsFloat() || Params.TTop()->IsFloat())
        FloatBinaryOp(divides<float>());
    else
        IntBinaryOp(divides<int32_t>());
}

void NSBInterpreter::ModExpression()
{
    IntBinaryOp(modulus<int32_t>());
}

void NSBInterpreter::Increment()
{
    if (Params.Top()->Name == "$SW_PHONE_SENDMAILNO")
    {
        Variable* pVar = PopVar();
        int32_t Index = Nsb::ConstantToValue<Nsb::PhoneMail>(pVar->ToString());
        string Constant = Nsb::ValueToConstant<Nsb::PhoneMail>(Index + 1);
        pVar->Set(Constant);
        PushVar(pVar);
    }
    else
        IntUnaryOp([](int32_t a) { return ++a; });
}

void NSBInterpreter::Decrement()
{
    IntUnaryOp([](int32_t a) { return --a; });
}

void NSBInterpreter::Literal()
{
    const string& Type = pContext->GetParam(0);
    const string& Val = pContext->GetParam(1);
    if (Type == "STRING")
    {
        if (Variable* pVar = VariableHolder.Read(Val))
            PushVar(pVar);
        else
            PushString(Val);
    }
    else if (Type == "INT")
        PushInt(stoi(Val));
    else if (Type == "FLOAT")
        PushFloat(stof(Val));
}

void NSBInterpreter::Assign()
{
    if (pContext->GetParam(0) == "__array_variable__")
    {
        Params.Begin(1);
        Variable* pVar = PopVar();
        Variable* pLit = PopVar();
        pVar->Set(pLit);
        Variable::Destroy(pLit);
    }
    else
        Assign_(0);
}

void NSBInterpreter::Get()
{
    PushVar(GetVar(pContext->GetParam(0)));
}

void NSBInterpreter::ScopeBegin()
{
}

void NSBInterpreter::ScopeEnd()
{
}

void NSBInterpreter::Return()
{
    pContext->Return();
}

void NSBInterpreter::If()
{
    if (!PopBool())
        Jump();
}

void NSBInterpreter::While()
{
    pContext->PushBreak();
    If();
}

void NSBInterpreter::WhileEnd()
{
    pContext->PopBreak();
}

/*
 * Select an event for next iteration of event loop. (see: Case)
 * Valid events are: Mouse Up/Down/Wheel, Arrow Up/Down/Right/Left
 * */
void NSBInterpreter::Select()
{
    pWindow->Select(true);
    if (SelectEvent())
        pContext->PushBreak();
}

void NSBInterpreter::SelectEnd()
{
    SelectEvent();
}

void NSBInterpreter::SelectBreakEnd()
{
    pContext->PopBreak();
    pWindow->Select(false);
    Events = queue<SDL_Event>();
}

void NSBInterpreter::Break()
{
    pContext->Break();
}

void NSBInterpreter::Jump()
{
    pContext->Jump(pContext->GetParam(0));
}

Variable* NSBInterpreter::PopVar()
{
    return Params.Pop();
}

Texture* NSBInterpreter::PopTexture()
{
    return Get<Texture>(PopString());
}

GLTexture* NSBInterpreter::PopGLTexture()
{
    return Get<GLTexture>(PopString());
}

Playable* NSBInterpreter::PopPlayable()
{
    return Get<Playable>(PopString());
}

Scrollbar* NSBInterpreter::PopScrollbar()
{
    return Get<Scrollbar>(PopString());
}

int32_t NSBInterpreter::PopInt()
{
    Variable* pVar = PopVar();
    int32_t Val = 0;
    if (pVar->IsInt())
        Val = pVar->ToInt();
    else
        Val = Nsb::ConstantToValue<Nsb::Null>(boost::algorithm::to_lower_copy(pVar->ToString()));
    Variable::Destroy(pVar);
    return Val;
}

float NSBInterpreter::PopFloat()
{
    Variable* pVar = PopVar();
    float Val = pVar->ToFloat();
    Variable::Destroy(pVar);
    return Val;
}

string NSBInterpreter::PopString()
{
    Variable* pVar = PopVar();
    string Val = pVar->ToString();
    Variable::Destroy(pVar);
    return Val;
}

NSBPosition NSBInterpreter::PopPos()
{
    const int32_t WIDTH = pWindow->WIDTH;
    const int32_t HEIGHT = pWindow->HEIGHT;
    static const size_t SPECIAL_POS_NUM = 19;
    static const PosFunc SpecialPosTable[SPECIAL_POS_NUM] =
    {
        [WIDTH] (int32_t x) { return WIDTH; }, // OutRight
        [] (int32_t x) { return -x; }, // OutLeft
        [] (int32_t y) { return -y; }, // OutTop
        [HEIGHT] (int32_t y) { return HEIGHT; }, // OutBottom

        [WIDTH] (int32_t x) { return WIDTH - x; }, // InRight
        [] (int32_t x) { return 0; }, // InLeft
        [] (int32_t y) { return 0; }, // InTop
        [HEIGHT] (int32_t y) { return HEIGHT - y; }, // InBottom

        [WIDTH] (int32_t x) { return WIDTH - (x / 2); }, // OnRight
        [] (int32_t x) { return -(x / 2); }, // OnLeft
        [] (int32_t y) { return -(y / 2); }, // OnTop
        [HEIGHT] (int32_t y) { return HEIGHT - (y / 2); }, // OnBottom

        [] (int32_t x) { return x; }, // Right
        [] (int32_t x) { return 0; }, // Left
        [] (int32_t y) { return 0; }, // Top
        [] (int32_t y) { return y; }, // Bottom

        [WIDTH] (int32_t x) { return (WIDTH - x) / 2; }, // Center
        [HEIGHT] (int32_t y) { return (HEIGHT - y) / 2; }, // Middle
        [] (int32_t xy) { return numeric_limits<int32_t>::max(); } // Auto
    };
    static const string SpecialPos[SPECIAL_POS_NUM] =
    {
        "outright", "outleft", "outtop", "outbottom",
        "inright", "inleft", "intop", "inbottom",
        "onright", "onleft", "ontop", "onbottom",
        "right", "left", "top", "bottom",
        "center", "middle", "auto"
    };

    NSBPosition Position;
    Variable* pVar = PopVar();
    Position.Relative = pVar->Relative;
    if (pVar->IsInt())
    {
        int32_t Val = pVar->ToInt();
        Position.Func = [Val] (int32_t) { return Val; };
    }
    else
    {
        string Str = pVar->ToString();
        transform(Str.begin(), Str.end(), Str.begin(), ::tolower);
        size_t i = -1;
        while (++i < SPECIAL_POS_NUM)
            if (Str == SpecialPos[i])
                Position.Func = SpecialPosTable[i];
    }
    Variable::Destroy(pVar);
    return Position;
}

NSBPosition NSBInterpreter::PopRelative()
{
    NSBPosition Position;
    Variable* pVar = PopVar();
    Position.Relative = pVar->Relative;
    int32_t Val = pVar->ToInt();
    Position.Func = [Val] (int32_t) { return Val; };
    Variable::Destroy(pVar);
    return Position;
}

uint32_t NSBInterpreter::PopColor()
{
    uint32_t Color = 0;
    Variable* pVar = PopVar();
    if (pVar->IsString())
    {
        string Str = boost::algorithm::to_lower_copy(pVar->ToString());
        if (Nsb::IsValidConstant<Nsb::Color>(Str))
            Color = Nsb::ConstantToValue<Nsb::Color>(Str);
        else
        {
            size_t i = Str.find_first_of("0123456789abcdef");
            if (i != Str.npos && Str.size() - i >= 6)
                Color = stoi(Str.substr(i, 6), nullptr, 16) | (0xFF << 24);
        }
    }
    else
        Color = stoi(to_string(pVar->ToInt()), nullptr, 16) | (0xFF << 24);

    Variable::Destroy(pVar);
    return Color;
}

int32_t NSBInterpreter::PopRequest()
{
    return Nsb::ConstantToValue<Nsb::Request>(PopString());
}

int32_t NSBInterpreter::PopTone()
{
    return Nsb::ConstantToValue<Nsb::Tone>(PopString());
}

int32_t NSBInterpreter::PopEffect()
{
    return Nsb::ConstantToValue<Nsb::Effect>(PopString());
}

int32_t NSBInterpreter::PopShade()
{
    return Nsb::ConstantToValue<Nsb::Shade>(PopString());
}

int32_t NSBInterpreter::PopTempo()
{
    return Nsb::ConstantToValue<Nsb::Tempo>(PopString());
}

bool NSBInterpreter::PopBool()
{
    Variable* pVar = PopVar();
    bool ret = ToBool(pVar);
    Variable::Destroy(pVar);
    return ret;
}

string NSBInterpreter::PopSave()
{
    boost::format Fmt("%s/%04d.npf");
    Fmt % GetString("#SYSTEM_save_path");
    Fmt % PopInt();
    return Fmt.str();
}

void NSBInterpreter::PushFloat(float Float)
{
    PushVar(Variable::MakeFloat(Float));
}

void NSBInterpreter::PushInt(int32_t Int)
{
    PushVar(Variable::MakeInt(Int));
}

void NSBInterpreter::PushString(const string& Str)
{
    PushVar(Variable::MakeString(Str));
}

void NSBInterpreter::PushVar(Variable* pVar)
{
    Params.Push(pVar);
}

void NSBInterpreter::Assign_(int Index)
{
    SetVar(pContext->GetParam(Index), PopVar());
}

void NSBInterpreter::IntUnaryOp(function<int32_t(int32_t)> Func)
{
    PushVar(PopVar()->IntUnaryOp(Func));
}

void NSBInterpreter::IntBinaryOp(function<int32_t(int32_t, int32_t)> Func)
{
    int32_t lhs = PopInt();
    int32_t rhs = PopInt();
    PushInt(Func(lhs, rhs));
}

void NSBInterpreter::FloatBinaryOp(function<float(float, float)> Func)
{
    float lhs = PopFloat();
    float rhs = PopFloat();
    PushFloat(Func(lhs, rhs));
}

void NSBInterpreter::BoolBinaryOp(function<bool(bool, bool)> Func)
{
    bool lhs = PopBool();
    bool rhs = PopBool();
    PushInt(Func(lhs, rhs));
}

void NSBInterpreter::CallFunction_(NSBContext* pThread, const string& Symbol)
{
    if (!pThread->Call(pContext->GetScript(), string("function.") + Symbol))
        NSB_ERROR("Failed to call function", Symbol);
}

void NSBInterpreter::CallScriptSymbol(const string& Prefix)
{
    string ScriptName = GetString(pContext->GetParam(0)), Symbol;
    size_t i = ScriptName.find("->");
    if (i != string::npos)
    {
        Symbol = ScriptName.substr(i + 2);
        ScriptName.erase(i);
    }
    else
        Symbol = "main";
    CallScript(ScriptName == "@" ? pContext->GetScriptName() : ScriptName, Prefix + Symbol);
}

void NSBInterpreter::CallScript(const string& Filename, const string& Symbol)
{
    if (ScriptFile* pScript = sResourceMgr->GetScriptFile(Filename))
        pContext->Call(pScript, Symbol);
}

void NSBInterpreter::CallScriptThread(const string& Filename, const string& Symbol)
{
    NSBContext* pThread = new NSBContext("UNK");
    AddThread(pThread);
    if (ScriptFile* pScript = sResourceMgr->GetScriptFile(Filename))
        pThread->Call(pScript, Symbol);
}

void NSBInterpreter::Call(uint16_t Magic)
{
    uint8_t NumParams = Builtins[Magic].NumParams;
    Params.Begin(NumParams == NSB_VARARGS ? pContext->GetNumParams() : NumParams);

    if (Builtins[Magic].Func)
        (this->*Builtins[Magic].Func)();
}

bool NSBInterpreter::SelectEvent()
{
    if (!Events.empty())
    {
        Event = Events.front();
        Events.pop();
        return true;
    }
    else
    {
        pContext->Rewind();
        pContext->Wait(20);
        return false;
    }
}

void NSBInterpreter::AddThread(NSBContext* pThread)
{
    pThread->Start();
    Threads.push_back(pThread);
    ThreadsModified = true;
}

void NSBInterpreter::RemoveThread(NSBContext* pThread)
{
    Threads.remove(pThread);
    ThreadsModified = true;
}

int32_t NSBInterpreter::GetInt(const string& Name)
{
    return GetVar(Name)->ToInt();
}

bool NSBInterpreter::GetBool(const string& Name)
{
    return ToBool(GetVar(Name));
}

string NSBInterpreter::GetString(const string& Name)
{
    if (Name[0] != '$' && Name[0] != '#')
        return Name;
    return GetVar(Name)->ToString();
}

bool NSBInterpreter::ToBool(Variable* pVar)
{
    if (pVar->IsString())
    {
        int32_t Val = Nsb::ConstantToValue<Nsb::Boolean>(pVar->ToString());
        if (Val != -1)
            return static_cast<bool>(Val);
    }
    return static_cast<bool>(pVar->ToInt());
}

Variable* NSBInterpreter::GetVar(const string& Name)
{
    if (Variable* pVar = VariableHolder.Read(Name))
        return pVar;

    Variable* pVar = Variable::MakeNull(Name);
    VariableHolder.Write(Name, pVar);
    return pVar;
}

Object* NSBInterpreter::GetObject(const string& Name)
{
    return ObjectHolder.Read(Name);
}

void NSBInterpreter::OnVariableChanged(const string& Name)
{
    if (Name == "#SYSTEM_window_full")
        pWindow->SetFullscreen(GetBool("#SYSTEM_window_full") ? SDL_WINDOW_FULLSCREEN : 0);
}

void NSBInterpreter::SetVar(const string& Name, Variable* pVar)
{
    GetVar(Name)->Set(pVar);
    OnVariableChanged(Name);
    Variable::Destroy(pVar);
}

void NSBInterpreter::SetInt(const string& Name, int32_t Val)
{
    SetVar(Name, Variable::MakeInt(Val));
}

void NSBInterpreter::SetString(const string& Name, const string& Val)
{
    SetVar(Name, Variable::MakeString(Val));
}

void NSBInterpreter::AddAssign()
{
    Variable* pVar = GetVar(pContext->GetParam(0));
    Variable* pTemp = Variable::Add(pVar, PopVar());
    pVar->Set(pTemp);
    Variable::Destroy(pTemp);
}

void NSBInterpreter::SubAssign()
{
    Variable* pVar = GetVar(pContext->GetParam(0));
    Variable* pTemp = Variable::MakeInt(pVar->ToInt() - PopInt());
    pVar->Set(pTemp);
    Variable::Destroy(pTemp);
}

void NSBInterpreter::WriteFile()
{
    string Filename = PopString();
    string Data = NpaFile::FromUtf8(PopString());
    uint32_t Size = Data.size();
    fs::WriteFileDirectory(Filename, NpaFile::Encrypt(&Data[0], Size), Size);
}

void NSBInterpreter::ReadFile()
{
    string Filename = PopString();
    uint32_t Size;
    char* pData = fs::ReadFile(Filename, Size);
    NpaFile::Decrypt(pData, Size);
    PushString(NpaFile::ToUtf8(pData, Size));
    delete[] pData;
}

void NSBInterpreter::CreateTexture()
{
    string Handle = PopString();
    int32_t Priority = PopInt();
    NSBPosition X = PopPos();
    NSBPosition Y = PopPos();
    string Source = PopString();

    Texture* pTexture = new Texture;
    if (Source == "VIDEO")
        ;
    else if (Source == "SCREEN")
        pTexture->CreateFromScreen(pWindow);
    else if (Source.size() < 4 || Source[Source.size() - 4] != '.')
    {
        if (GLTexture* pSource = Get<GLTexture>(Source))
            pTexture->CreateFromGLTexture(pSource);
        else if (Image* pSource = Get<Image>(Source))
            pTexture->CreateFromImage(pSource);
    }
    else
        pTexture->CreateFromFile(Source);

    pTexture->SetVertex(pTexture->GetWidth() / 2, pTexture->GetHeight() / 2);
    pTexture->Move(X(pTexture->GetWidth()), Y(pTexture->GetHeight()));
    pTexture->SetPriority(Priority);

    pWindow->AddTexture(pTexture);
    ObjectHolder.Write(Handle, pTexture);
}

void NSBInterpreter::ImageHorizon()
{
    Texture* pTexture = PopTexture();
    PushInt(pTexture ? pTexture->GetWidth() : 0);
}

void NSBInterpreter::ImageVertical()
{
    Texture* pTexture = PopTexture();
    PushInt(pTexture ? pTexture->GetHeight() : 0);
}

void NSBInterpreter::Time()
{
    PushInt(time(0));
}

void NSBInterpreter::StrStr()
{
    string Haystack = PopString();
    string Needle = PopString();
    PushInt(Haystack.find(Needle) + 1);
}

void NSBInterpreter::Exit()
{
    // TODO: save # variables in 0000.npf
    pWindow->Exit();
}

void NSBInterpreter::CursorPosition()
{
    int32_t X, Y;
    SDL_PumpEvents();
    SDL_GetMouseState(&X, &Y);
    SetInt(PopString(), X);
    SetInt(PopString(), Y);
}

void NSBInterpreter::MoveCursor()
{
    int32_t X = PopInt();
    int32_t Y = PopInt();
    pWindow->MoveCursor(X, Y);
}

void NSBInterpreter::Position()
{
    Texture* pTexture = PopTexture();
    SetInt(pContext->GetParam(1), pTexture->GetX());
    SetInt(pContext->GetParam(2), pTexture->GetY());
}

void NSBInterpreter::Wait()
{
    pContext->Wait(PopInt());
}

void NSBInterpreter::WaitKey()
{
    pContext->WaitKey(pContext->GetNumParams() == 1 ? PopInt() : -1);
}

void NSBInterpreter::NegaExpression()
{
    IntUnaryOp(negate<int32_t>());
}

void NSBInterpreter::System()
{
    static const string OpenStr = "OPEN:";
    string Command = PopString();
    string Parameters = PopString();
    string Directory = PopString();
    if (Command.substr(0, OpenStr.size()) != OpenStr)
        return;

    Command = Command.substr(OpenStr.size());

    if (fork() == 0)
        execlp("/usr/bin/xdg-open", "/usr/bin/xdg-open", Command.c_str(), NULL);
}

void NSBInterpreter::String()
{
    boost::format Fmt(PopString());
    for (int i = 1; i < pContext->GetNumParams(); ++i)
    {
        Variable* pVar = PopVar();
        if (pVar->IsInt())
            Fmt % pVar->ToInt();
        else if (pVar->IsString())
            Fmt % pVar->ToString();
        else if (pVar->IsFloat())
            Fmt % pVar->ToFloat();
        Variable::Destroy(pVar);
    }
    PushString(Fmt.str());
}

void NSBInterpreter::VariableValue()
{
    string Type = PopString();
    string Name = PopString();

    // Check if it's array index. Not sure about this...
    if (Name.size() > 3 && Name[Name.size() - 3] == '[' && Name.back() == ']' && isdigit(Name[Name.size() - 2]))
        PushVar(GetVar(Name.substr(0, Name.size() - 3) + "/" + to_string(Name[Name.size() - 2])));
    else if (pContext->GetNumParams() == 3)
        SetVar(Type + Name, PopVar());
    else if (pContext->GetNumParams() == 2)
        PushVar(GetVar(Type + Name));
    else
        assert(false && "This will trigger when we get new season of Haruhi");
}

void NSBInterpreter::CreateProcess()
{
    string Handle = PopString();
    /*int32_t unk1 = */PopInt();
    /*int32_t unk2 = */PopInt();
    /*int32_t unk3 = */PopInt();
    string Symbol = PopString();

    NSBContext* pThread = new NSBContext(Handle);
    CallFunction_(pThread, Symbol);
    AddThread(pThread);
    ObjectHolder.Write(Handle, pThread);
}

void NSBInterpreter::Count()
{
    Variable* pArr = PopVar();
    int SCount = count(pArr->Name.begin(), pArr->Name.end(), '/') + 1;
    int32_t Size = 0;
    for (auto& i : VariableHolder.Cache)
    {
        string Prefix = i.first.substr(0, pArr->Name.size());
        int Count = count(i.first.begin(), i.first.end(), '/');
        if (Prefix == pArr->Name && SCount == Count)
            Size++;
    }
    PushInt(Size);
}

void NSBInterpreter::Array()
{
    Variable* pArr = PopVar();
    if (!pArr)
    {
        pArr = Variable::MakeNull(pContext->GetParam(0));
        VariableHolder.Write(pContext->GetParam(0), pArr);
    }
    for (int i = 1; i < pContext->GetNumParams(); ++i)
    {
        string Name = pArr->Name + "/" + to_string(i - 1);
        Variable* pVar = Variable::MakeCopy(PopVar(), Name);
        VariableHolder.Write(Name, pVar);
    }
}

void NSBInterpreter::SubScript()
{
    Variable* pArr = GetVar(pContext->GetParam(0));
    int32_t Depth = stoi(pContext->GetParam(1));
    Params.Begin(Depth);
    while (Depth --> 0)
    {
        Variable* pVar = PopVar();
        int Index = pVar->IsInt() ? pVar->ToInt() : pArr->Assoc[pVar->ToString()];
        pArr = GetVar(pArr->Name + "/" + to_string(Index));
        Variable::Destroy(pVar);
    }
    PushVar(pArr);
}

void NSBInterpreter::AssocArray()
{
    Variable* pArr = PopVar();
    for (int i = 1; i < pContext->GetNumParams(); ++i)
        pArr->Assoc[PopString()] = i - 1;
}

void NSBInterpreter::ModuleFileName()
{
    string Name = pContext->GetScriptName();
    PushString(Name.substr(4, Name.size() - 8)); // Remove nss/ and .nsb
}

void NSBInterpreter::Request()
{
    string Handle = PopString();
    int32_t Request = PopRequest();

    ObjectHolder.Execute(Handle, [Request] (Object** ppObject)
    {
        if (Object* pObject = *ppObject)
            pObject->Request(Request);
    });
}

void NSBInterpreter::SetVertex()
{
    Texture* pTexture = PopTexture();
    NSBPosition X = PopPos();
    NSBPosition Y = PopPos();

    if (pTexture)
        pTexture->SetVertex(X(pTexture->GetWidth(), pTexture->GetOX()), Y(pTexture->GetHeight(), pTexture->GetOY()));
}

void NSBInterpreter::Zoom()
{
    string Handle = PopString();
    int32_t Time = PopInt();
    NSBPosition XScale = PopRelative();
    NSBPosition YScale = PopRelative();
    /*int32_t Tempo = */PopTempo();
    bool Wait = PopBool();

    ObjectHolder.Execute(Handle, [&] (Object** ppObject)
    {
        if (Texture* pTexture = dynamic_cast<Texture*>(*ppObject))
            pTexture->Zoom(Time, XScale(0, pTexture->GetXScale()), YScale(0, pTexture->GetYScale()));
    });

    if (Wait)
        pContext->Wait(Time);
}

void NSBInterpreter::Move()
{
    string Handle = PopString();
    int32_t Time = PopInt();
    NSBPosition X = PopPos();
    NSBPosition Y = PopPos();
    /*int32_t Tempo = */PopTempo();
    bool Wait = PopBool();

    ObjectHolder.Execute(Handle, [&] (Object** ppObject)
    {
        if (Texture* pTexture = dynamic_cast<Texture*>(*ppObject))
            pTexture->Move(X(pTexture->GetWidth(), pTexture->GetMX()), Y(pTexture->GetHeight(), pTexture->GetMY()), Time);
    });

    if (Wait)
        pContext->Wait(Time);
}

void NSBInterpreter::SetShade()
{
    if (Texture* pTexture = PopTexture())
        pTexture->SetShade(PopShade());
}

void NSBInterpreter::DrawToTexture()
{
    GLTexture* pTexture = PopGLTexture();
    int32_t X = PopInt();
    int32_t Y = PopInt();
    string Filename = PopString();

    if (pTexture)
        pTexture->Draw(X, Y, Filename);
}

void NSBInterpreter::CreateRenderTexture()
{
    string Handle = PopString();
    int32_t Width = PopInt();
    int32_t Height = PopInt();
    uint32_t Color = PopColor();

    GLTexture* pTexture = new GLTexture;
    pTexture->CreateFromColor(Width, Height, Color);
    ObjectHolder.Write(Handle, pTexture);
}

void NSBInterpreter::DrawTransition()
{
    Texture* pTexture = PopTexture();
    int32_t Time = PopInt();
    int32_t Start = PopInt();
    int32_t End = PopInt();
    int32_t Boundary = PopInt();
    /*int32_t Tempo = */PopTempo();
    string Filename = PopString();
    bool Wait = PopBool();

    if (pTexture)
        pTexture->DrawTransition(Time, Start, End, Boundary, Filename);

    if (Wait)
        pContext->Wait(Time);
}

void NSBInterpreter::CreateColor()
{
    string Handle = PopString();
    int32_t Priority = PopInt();
    NSBPosition X = PopPos();
    NSBPosition Y = PopPos();
    int32_t Width = PopInt();
    int32_t Height = PopInt();
    uint32_t Color = PopColor();

    Texture* pTexture = new Texture;
    pTexture->CreateFromColor(Width, Height, Color);
    pTexture->SetVertex(Width / 2, Height / 2);
    pTexture->Move(X(Width), Y(Height));
    pTexture->SetPriority(Priority);

    pWindow->AddTexture(pTexture);
    ObjectHolder.Write(Handle, pTexture);
}

void NSBInterpreter::LoadImage()
{
    string Handle = PopString();
    string Filename = PopString();

    Image* pImage = new Image;
    if (Filename == "SCREEN")
        pImage->LoadScreen(pWindow);
    else
        pImage->LoadImage(Filename);
    ObjectHolder.Write(Handle, pImage);
}

void NSBInterpreter::Fade()
{
    string Handle = PopString();
    int32_t Time = PopInt();
    int32_t Opacity = PopInt();
    /*int32_t Tempo = */PopTempo();
    bool Wait = PopBool();

    ObjectHolder.Execute(Handle, [Time, Opacity] (Object** ppObject)
    {
        if (Texture* pTexture = dynamic_cast<Texture*>(*ppObject))
            pTexture->Fade(Time, Opacity);
    });

    if (Wait)
        pContext->Wait(Time);
}

void NSBInterpreter::Delete()
{
    string Handle = PopString();

    ObjectHolder.Execute(Handle, [this] (Object** ppObject)
    {
        if (Object* pObject = *ppObject)
        {
            if (pObject->Lock)
                return;

            if (NSBContext* pThread = dynamic_cast<NSBContext*>(pObject))
                RemoveThread(pThread);

            delete pObject;
            *ppObject = nullptr;
        }
    });
}

void NSBInterpreter::ClearParams()
{
    Params.Reset();
}

void NSBInterpreter::SetLoop()
{
    if (Playable* pPlayable = PopPlayable())
        pPlayable->SetLoop(PopBool());
}

void NSBInterpreter::SetVolume()
{
    string Handle = PopString();
    int32_t Time = PopInt();
    int32_t Volume = PopInt();
    /*int32_t Tempo = */PopTempo();

    ObjectHolder.Execute(Handle, [Time, Volume] (Object** ppObject)
    {
        if (Playable* pPlayable = dynamic_cast<Playable*>(*ppObject))
            pPlayable->SetVolume(Time, Volume);
    });
}

void NSBInterpreter::SetLoopPoint()
{
    string Handle = PopString();
    int32_t Begin = PopInt();
    int32_t End = PopInt();

    if (Playable* pPlayable = Get<Playable>(Handle))
        pPlayable->SetLoopPoint(Begin, End);
}

void NSBInterpreter::CreateSound()
{
    string Handle = PopString();
    string Type = PopString();
    string File = PopString();

    if (File.substr(File.size() - 4) != ".ogg")
        File += ".ogg";

    Resource Res = sResourceMgr->GetResource(File);
    if (!Res.IsValid())
        return;

    int32_t Volume = 0;
    if (Type == "BGM")
        Volume = GetInt("#SYSTEM_sound_volume_bgm");
    else if (Type == "SE")
        Volume = GetInt("#SYSTEM_sound_volume_se");
    else if (Type == "VOICE")
        Volume = GetInt("#SYSTEM_sound_volume_voice");
    else
        return;

    Playable* pPlayable = new Playable(Res);
    pPlayable->SetVolume(0, Volume);
    ObjectHolder.Write(Handle, pPlayable);
}

void NSBInterpreter::RemainTime()
{
    Playable* pPlayable = PopPlayable();
    PushInt(pPlayable ? pPlayable->RemainTime() : 0);
}

void NSBInterpreter::CreateMovie()
{
    string Handle = PopString();
    int32_t Priority = PopInt();
    /*NSBPosition X = */PopPos();
    /*NSBPosition Y = */PopPos();
    bool Loop = PopBool();
    bool Alpha = PopBool();
    string File = PopString();
    bool Audio = PopBool();

    Movie* pMovie = new Movie(File, pWindow, Priority, Alpha, Audio);
    pMovie->SetLoop(Loop);
    pWindow->AddTexture(pMovie);
    ObjectHolder.Write(Handle, pMovie);
}

void NSBInterpreter::DurationTime()
{
    Playable* pPlayable = PopPlayable();
    PushInt(pPlayable ? pPlayable->DurationTime() : 0);
}

void NSBInterpreter::SetFrequency()
{
    string Handle = PopString();
    int32_t Time = PopInt();
    int32_t Frequency = PopInt();
    /*int32_t Tempo = */PopTempo();

    if (Playable* pPlayable = Get<Playable>(Handle))
        pPlayable->SetFrequency(Time, Frequency);
}

void NSBInterpreter::SetPan()
{
    string Handle = PopString();
    int32_t Time = PopInt();
    int32_t Pan = PopInt();
    /*int32_t Tempo = */PopTempo();

    if (Playable* pPlayable = Get<Playable>(Handle))
        pPlayable->SetPan(Time, Pan);
}

void NSBInterpreter::SetAlias()
{
    string Handle = PopString();
    string Alias = PopString();
    ObjectHolder.WriteAlias(Handle, Alias);
}

void NSBInterpreter::CreateName()
{
    ObjectHolder.Write(PopString(), new Name);
}

void NSBInterpreter::CreateWindow()
{
    string Handle = PopString();

    Window_t* pWindow = new Window_t;
    pWindow->Priority = PopInt();
    pWindow->X = PopInt();
    pWindow->Y = PopInt();
    pWindow->Width = PopInt();
    pWindow->Height = PopInt();
    /*bool unk = */PopBool();

    ObjectHolder.Write(Handle, pWindow);
}

void NSBInterpreter::CreateChoice()
{
    ObjectHolder.Write(PopString(), new Choice);

    for (int i = 1; i < pContext->GetNumParams(); ++i)
        PopInt();
}

/*
 * Check if selected (see: Select) event satisfies a case.
 * In practise, this checks if button was clicked, and if so, jumps to
 * beginning of case code block. Otherwise, jump over the case block.
 * After the code block is executed, no other cases will be checked.
 *
 * NOTE: For unknown reason, MAGIC_CASE's last parameter is label which
 * points to beginning of the case, even though case code block always
 * begins on the next line of code.
 * */
void NSBInterpreter::Case()
{
    bool Choose = false;
    if (Choice* pChoice = Get<Choice>(pContext->GetParam(0)))
    {
        Choose = pChoice->IsSelected(Event);
        pChoice->Reset();
    }

    pContext->Jump(Choose ? pContext->GetParam(2) : pContext->GetParam(1));
}

void NSBInterpreter::CaseEnd()
{
}

void NSBInterpreter::SetNextFocus()
{
    Choice* pFirst = Get<Choice>(PopString());
    Choice* pSecond = Get<Choice>(PopString());
    string Key = PopString();

    if (pFirst && pSecond)
        pFirst->SetNextFocus(pSecond, Key);
}

void NSBInterpreter::PassageTime()
{
    Playable* pPlayable = PopPlayable();
    PushInt(pPlayable ? pPlayable->PassageTime() : 0);
}

void NSBInterpreter::ParseText()
{
    string Handle = pContext->GetParam(0);
    string Box = pContext->GetParam(1);
    string XML = pContext->GetParam(2);

    ObjectHolder.Delete(GetVar("$SYSTEM_present_text")->ToString());

    Text* pText = new Text;
    pText->CreateFromXML(XML);
    Handle = Box + "/" + Handle;
    SetString("$SYSTEM_present_text", Handle);
    ObjectHolder.Write(Handle, pText);
}

void NSBInterpreter::LoadText()
{
    /*string unk = */PopString();
    /*string unk = */PopString();
    string TextHandle = PopString();
    int32_t Width = PopInt();
    /*int32_t Height = */PopInt();
    /*int32_t unk = */PopInt();
    /*int32_t unk = */PopInt();

    if (Text* pText = Get<Text>(TextHandle))
    {
        pText->SetColor(Text::dInColor);
        pText->SetCharacterSize(Text::dSize);
        pText->SetPriority(0xFFFF); // [HACK]
        pText->SetWrap(Width);
        pText->Advance();
        pWindow->AddTexture(pText);
    }
}

void NSBInterpreter::WaitText()
{
    string Handle = PopString();
    int32_t Time = PopInt();

    if (Text* pText = Get<Text>(Handle))
        if (!SkipHack)
            pContext->WaitText(pText, Time);
}

void NSBInterpreter::LockVideo()
{
    /*bool Lock = */PopBool();
}

void NSBInterpreter::Save()
{
    Npa::Buffer SaveData;
    SaveData.Write<uint32_t>(VariableHolder.Cache.size());
    vector<pair<string, Variable*> > Arrays;
    for (auto& var : VariableHolder.Cache)
    {
        if (var.first.front() == '#')
            continue;

        SaveData.WriteStr32(NpaFile::FromUtf8(var.first));
        SaveData.WriteStr32(NpaFile::FromUtf8(var.first));
        SaveData.Write<uint32_t>(var.second->GetTag());
        SaveData.Write<int32_t>(var.second->IsInt() ? var.second->ToInt() : 0);
        SaveData.Write<float>(var.second->IsFloat() ? var.second->ToFloat() : 0);
        SaveData.WriteStr32(NpaFile::FromUtf8(var.second->IsString() ? var.second->ToString() : ""));
        SaveData.Write<bool>(0); // unk - maybe bool? 4?
        SaveData.WriteStr32(NpaFile::FromUtf8("")); // TODO: arrayref?
        if (!var.second->Assoc.empty())
            Arrays.push_back(var);
    }
    SaveData.Write<uint32_t>(Arrays.size());
    for (auto& arr : Arrays)
    {
        SaveData.WriteStr32(NpaFile::FromUtf8(arr.first));
        SaveData.Write<uint32_t>(arr.second->Assoc.size());
        for (auto& i : arr.second->Assoc)
            SaveData.WriteStr32(NpaFile::ToUtf8(i.first));
    }
    fs::WriteFile(PopSave(), NpaFile::Encrypt(SaveData.GetData(), SaveData.GetSize()), SaveData.GetSize());
}

void NSBInterpreter::DeleteSaveFile()
{
    string Filename = PopSave();
    fs::DeleteFile(Filename);
    fs::DeleteDirectory(Filename.substr(0, Filename.size() - 4));
}

void NSBInterpreter::Conquest()
{
    /*string unk = */PopString();
    /*string unk = */PopString();
    /*string unk = */PopBool();

    // [HACK]
    PushInt(0);
}

void NSBInterpreter::ClearScore()
{
    /*string unk = */PopString();
}

void NSBInterpreter::ClearBacklog()
{
}

void NSBInterpreter::SetFont()
{
    Text::dFont = PopString();
    Text::dSize = PopInt();
    Text::dInColor = PopColor();
    Text::dOutColor = PopColor();
    Text::dWeight = PopInt();
    Text::dAlign = PopString();
}

void NSBInterpreter::SetShortcut()
{
    string Key = PopString();
    string Script = PopString();
    Shortcuts.push_back({SDLK_a + Key[0] - 'A', Script});
}

void NSBInterpreter::CreateClipTexture()
{
    string Handle = PopString();
    int32_t Priority = PopInt();
    NSBPosition X1 = PopPos();
    NSBPosition Y1 = PopPos();
    int32_t X2 = PopInt();
    int32_t Y2 = PopInt();
    int32_t Width = PopInt();
    int32_t Height = PopInt();
    string Source = PopString();

    Texture* pTexture = new Texture;
    if (Source.size() < 4 || Source[Source.size() - 4] != '.')
        pTexture->CreateFromImageClip(Get<Image>(Source), X2, Y2, Width, Height);
    else
        pTexture->CreateFromFileClip(Source, X2, Y2, Width, Height);

    pTexture->Move(X1(pTexture->GetWidth()), Y1(pTexture->GetHeight()));
    pTexture->SetPriority(Priority);

    pWindow->AddTexture(pTexture);
    ObjectHolder.Write(Handle, pTexture);
}

void NSBInterpreter::ExistSave()
{
    PushInt(fs::Exists(PopSave()));
}

void NSBInterpreter::WaitAction()
{
    string Handle = PopString();
    int32_t Time = pContext->GetNumParams() == 2 ? PopInt() : -1;

    if (Object* pObject = GetObject(Handle))
        pContext->WaitAction(pObject, Time);
}

void NSBInterpreter::Load()
{
    uint32_t Size;
    char* pData = fs::ReadFile(PopSave(), Size);
    Npa::Buffer SaveData(NpaFile::Decrypt(pData, Size), Size);

    uint32_t NumVars = SaveData.Read<uint32_t>();
    for (uint32_t i = 0; i < NumVars; ++i)
    {
        string Name1 = NpaFile::ToUtf8(SaveData.ReadStr32());
        string Name2 = NpaFile::ToUtf8(SaveData.ReadStr32());
        /*uint32_t Type = */SaveData.Read<uint32_t>();
        /*int32_t IntVal = */SaveData.Read<int32_t>();
        /*float FloatVal = */SaveData.Read<float>();
        string StrVal = NpaFile::ToUtf8(SaveData.ReadStr32());
        /*bool unk = */SaveData.Read<bool>();
        string ArrayRef = NpaFile::ToUtf8(SaveData.ReadStr32());
    }

    uint32_t NumArrs = SaveData.Read<uint32_t>();
    for (uint32_t i = 0; i < NumArrs; ++i)
    {
        string Name1 = NpaFile::ToUtf8(SaveData.ReadStr32());
        uint32_t NumElems = SaveData.Read<uint32_t>();
        for (uint32_t j = 0; j < NumElems; ++j)
            string Key = NpaFile::ToUtf8(SaveData.ReadStr32());
    }
}

void NSBInterpreter::SetBacklog()
{
    /*string Text = */PopString();
    /*string Voice = */PopString();

    // [WORKAROUND] In JAST the third parameter may be an integer
    Variable::Destroy(PopVar());
    ///*string Name = */PopString();
}

void NSBInterpreter::CreateText()
{
    string Handle = PopString();
    int32_t Priority = PopInt();
    NSBPosition X = PopPos();
    NSBPosition Y = PopPos();
    NSBPosition Width = PopPos();
    /*NSBPosition Height = */PopPos();
    string String = PopString();

    Text* pText = new Text;
    pText->SetWrap(Width(0));
    pText->CreateFromString(String);
    pText->SetPriority(Priority);
    pText->SetPosition(X(pText->GetWidth()), Y(pText->GetHeight()));

    pWindow->AddTexture(pText);
    ObjectHolder.Write(Handle, pText);
}

void NSBInterpreter::AtExpression()
{
    Variable* pVar = PopVar();
    pVar->Relative = true;
    PushVar(pVar);
}

void NSBInterpreter::Random()
{
    PushInt(random() % PopInt());
}

void NSBInterpreter::CreateEffect()
{
    string Handle = PopString();
    /*int32_t Priority = */PopInt();
    NSBPosition X = PopPos();
    NSBPosition Y = PopPos();
    /*int32_t Width = */PopInt();
    /*int32_t Height = */PopInt();
    /*int32_t Effect = */PopEffect();
}

void NSBInterpreter::SetTone()
{
    if (Texture* pTexture = PopTexture())
        pTexture->SetTone(PopTone());
}

void NSBInterpreter::DateTime()
{
    time_t t = time(nullptr);
    tm* tms = localtime(&t);
    PopVar()->Set(tms->tm_year + 1900);
    PopVar()->Set(tms->tm_mon + 1);
    PopVar()->Set(tms->tm_mday);
    PopVar()->Set(tms->tm_hour);
    PopVar()->Set(tms->tm_min);
    PopVar()->Set(tms->tm_sec);
}

void NSBInterpreter::Shake()
{
    string Handle = PopString();
    int32_t Time = PopInt();
    int32_t XWidth = PopInt();
    int32_t YWidth = PopInt();
    /*int32_t unk1 = */PopInt();
    /*int32_t unk2 = */PopInt();
    /*int32_t unk3 = */PopInt();
    /*int32_t Tempo = */PopTempo();
    bool Wait = PopBool();

    ObjectHolder.Execute(Handle, [Time, XWidth, YWidth] (Object** ppObject)
    {
        if (Texture* pTexture = dynamic_cast<Texture*>(*ppObject))
            pTexture->Shake(XWidth, YWidth, Time);
    });

    if (Wait)
        pContext->Wait(Time);
}

void NSBInterpreter::MoviePlay()
{
    string File = PopString();
    /*bool unk = */PopBool();
}

void NSBInterpreter::SetStream()
{
    string Handle = PopString();
    /*int32_t unk = */PopInt();
}

void NSBInterpreter::WaitPlay()
{
    Playable* pPlayable = PopPlayable();
    /*string unk = */PopString();

    pContext->Wait(pPlayable->RemainTime());
}

void NSBInterpreter::WaitFade()
{
    Texture* pTexture = PopTexture();
    /*string unk = */PopString();

    pContext->Wait(pTexture->RemainFade());
}

void NSBInterpreter::SoundAmplitude()
{
    string Handle = PopString();
    /*string unk = */PopString();

    // [HACK]
    PushInt(0);
}

void NSBInterpreter::Rotate()
{
    Texture* pTexture = PopTexture();
    int32_t Time = PopInt();
    /*int32_t unk1 = */PopInt();
    /*int32_t unk2 = */PopInt();
    NSBPosition Angle = PopRelative();
    /*int32_t Tempo = */PopTempo();
    bool Wait = PopBool();

    pTexture->Rotate(Angle(0, pTexture->GetAngle()), Time);
    if (Wait)
        pContext->Wait(Time);
}

void NSBInterpreter::Message()
{
    string Title = PopString();
    string Text = PopString();
    string Type = PopString();
    string Icon = PopString();
}

void NSBInterpreter::Integer()
{
    PushVar(PopVar());
}

void NSBInterpreter::CreateScrollbar()
{
    string Handle = PopString();
    int32_t Priority = PopInt();
    int32_t X1 = PopInt();
    int32_t Y1 = PopInt();
    int32_t X2 = PopInt();
    int32_t Y2 = PopInt();
    /*int32_t unk1 = */PopInt();
    int32_t Min = PopInt();
    int32_t Max = PopInt();
    /*int32_t unk2 = */PopInt();
    /*int32_t unk3 = */PopInt();
    string Type = PopString();
    string Filename = PopString();
    string Callback = PopString();

    Texture* pTexture = new Texture;
    Scrollbar* pScrollbar = new Scrollbar(pTexture, X1, Y1, X2, Y2, Min, Max, Type, Callback);
    pTexture->CreateFromFile(Filename);
    pTexture->Move(X1, Y1);
    pTexture->SetPriority(Priority);
    pWindow->AddTexture(pTexture);
    ObjectHolder.Write(Handle, pScrollbar);
}

void NSBInterpreter::SetScrollbarValue()
{
    Scrollbar* pScrollbar = PopScrollbar();
    int32_t Value = PopInt();
    pScrollbar->SetValue(Value);
}

void NSBInterpreter::SetScrollbarWheelArea()
{
    Scrollbar* pScrollbar = PopScrollbar();
    int32_t X = PopInt();
    int32_t Y = PopInt();
    int32_t Width = PopInt();
    int32_t Height = PopInt();
    pScrollbar->SetWheelArea(X, Y, Width, Height);
}

void NSBInterpreter::ScrollbarValue()
{
    Scrollbar* pScrollbar = PopScrollbar();
    PushInt(pScrollbar->GetValue());
}

void NSBInterpreter::CreateStencil()
{
    string Handle = PopString();
    /*int32_t unk = */PopInt();
    NSBPosition X = PopPos();
    NSBPosition Y = PopPos();
    /*int32_t unk = */PopInt();
    string Filename = PopString();
    /*bool unk = */PopBool(); // Maybe Inheritance?

    // Hack
    ObjectHolder.Write(Handle, new Name);
}

void NSBInterpreter::CreateMask()
{
    string Handle = PopString();
    /*int32_t Priority = */PopInt();
    NSBPosition X = PopPos();
    NSBPosition Y = PopPos();
    string Filename = PopString();
    /*bool Inheritance = */PopBool();

    // Hack
    ObjectHolder.Write(Handle, new Name);
}
