/* 
 * libnpengine: Nitroplus script interpreter
 * Copyright (C) 2013 Mislav Blažević <krofnica996@gmail.com>
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
#include "text.hpp"
#include "resourcemgr.hpp"

#include <cassert>
#include <sstream>
#include <string>
#include <memory>
#include <SFML/Audio/Music.hpp>

sf::Font Text::Font;

Text::Text(const std::string& XML) :
::Drawable(new sf::Text, 0xFFFFFF, DRAWABLE_TEXT),
LineIter(0),
pCurrentMusic(nullptr)
{
    std::istringstream ss(XML);
    std::string TextLine;
    sf::Music* pMusic = nullptr;

    while (std::getline(ss, TextLine))
    {
        if (TextLine.empty() || TextLine.compare(0, 4, "<PRE") == 0 || TextLine == "</PRE>")
            continue;

        // TODO: Line may start with <RUBY text or <FONT incolor
        if (TextLine.front() == '<')
        {
            std::string Attr;
            size_t i = TextLine.find('"'),  j, k = 0;
            while (i != std::string::npos)
            {
                j = TextLine.find('"', i + 1);
                Attr = TextLine.substr(i + 1, j - i - 1);
                i = TextLine.find('"', j + 1);
                switch (k)
                {
                    case 0: // ？？？
                        static_cast<sf::Text*>(pDrawable)->setString(sf::String::fromUtf8(Attr.begin(), Attr.end()));
                        break;
                    case 1: // VID_MAY
                        break;
                    case 2: // voice/MAY_0001
                    {
                        // TODO: Move to resourcemgr and reuse in interpreter
                        pMusic = new sf::Music;
                        uint32_t Size;
                        char* pMusicData = sResourceMgr->Read(Attr + ".ogg", &Size);
                        assert(pMusicData);
                        pMusic->openFromMemory(pMusicData, Size);
                        break;
                    }
                    case 3: // on
                        break;
                }
                ++k;
            }
        }
        else
        {
            Voices.push_back({pMusic, sf::String::fromUtf8(TextLine.begin(), TextLine.end())});
            sf::String* Str = &Voices[Voices.size() - 1].String;
            size_t i = 28;
            while (i < Str->getSize())
            {
                const char* lf = "\n";
                Str->insert(i, sf::String::fromUtf8(lf, lf + 1));
                i += 29;
            }
            pMusic = nullptr;
        }
    }

    setFont(Font);
    setString(Voices[0].String);
}

Text::~Text()
{
    if (pCurrentMusic)
        pCurrentMusic->stop();
    std::for_each(Voices.begin(), Voices.end(), [](const Voice& V) { delete V.pMusic; });
}

bool Text::NextLine()
{
    if (++LineIter >= Voices.size())
        return false;
    setString(Voices[LineIter].String);
    if (sf::Music* pMusic = Voices[LineIter].pMusic)
    {
        if (pCurrentMusic)
            pCurrentMusic->stop();
        pMusic->play();
        pCurrentMusic = pMusic;
    }
    return true;
}

void Text::Initialize(const std::string& FontFile)
{
    assert(Font.loadFromFile(FontFile));
}
