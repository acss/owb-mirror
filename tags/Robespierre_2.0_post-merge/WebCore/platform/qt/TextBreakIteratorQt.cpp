/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2006 Lars Knoll <lars@trolltech.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "TextBreakIterator.h"
#include <qtextlayout.h>

namespace WebCore {

    class TextBreakIterator
    {
    public:
        virtual int first() = 0;
        virtual int next() = 0;
        virtual int previous() = 0;
        inline int following(int pos) {
            currentPos = pos;
            return next();
        }
        inline int preceding(int pos) {
            currentPos = pos;
            return previous();
        }
        int currentPos;
        const UChar *string;
        int length;
    };

    class WordBreakIteratorQt : public TextBreakIterator
    {
    public:
        virtual int first();
        virtual int next();
        virtual int previous();
    };

    class CharBreakIteratorQt : public TextBreakIterator
    {
    public:
        virtual int first();
        virtual int next();
        virtual int previous();
        QTextLayout layout;
    };

    int WordBreakIteratorQt::first() {
        currentPos = 0;
        return currentPos;
    }

    int WordBreakIteratorQt::next() {
        if (currentPos == length) {
            currentPos = -1;
            return currentPos;
        }
        bool haveSpace = false;
        while (currentPos < length) {
            if (haveSpace && !QChar(string[currentPos]).isSpace())
                break;
            if (QChar(string[currentPos]).isSpace())
                haveSpace = true;
            ++currentPos;
        }
        return currentPos;
    }
    int WordBreakIteratorQt::previous() {
        if (currentPos == 0) {
            currentPos = -1;
            return currentPos;
        }
        bool haveSpace = false;
        while (currentPos > 0) {
            if (haveSpace && !QChar(string[currentPos]).isSpace())
                break;
            if (QChar(string[currentPos]).isSpace())
                haveSpace = true;
            --currentPos;
        }
        return currentPos;
    }

    int CharBreakIteratorQt::first() {
        currentPos = 0;
        return currentPos;
    }

    int CharBreakIteratorQt::next() {
        if (currentPos == length)
            return -1;
        currentPos = layout.nextCursorPosition(currentPos);
        return currentPos;
    }
    int CharBreakIteratorQt::previous() {
        if (currentPos == 0)
            return -1;
        currentPos = layout.previousCursorPosition(currentPos);
        return currentPos;
    }


TextBreakIterator* wordBreakIterator(const UChar* string, int length)
{
    static WordBreakIteratorQt *iterator = 0;
    if (!iterator)
        iterator = new WordBreakIteratorQt;

    iterator->string = string;
    iterator->length = length;
    iterator->currentPos = 0;

    return iterator;
}

TextBreakIterator* characterBreakIterator(const UChar* string, int length)
{
    static CharBreakIteratorQt *iterator = 0;
    if (!iterator)
        iterator = new CharBreakIteratorQt;

    iterator->string = string;
    iterator->length = length;
    iterator->currentPos = 0;
    iterator->layout.setText(QString(reinterpret_cast<const QChar*>(string), length));

    return iterator;
}

TextBreakIterator* lineBreakIterator(const UChar*, int)
{
    // not yet implemented
    return 0;
}

TextBreakIterator* sentenceBreakIterator(const UChar*, int)
{
    // not yet implemented
    return 0;
}

int textBreakFirst(TextBreakIterator* bi)
{
    return bi->first();
}

int textBreakNext(TextBreakIterator* bi)
{
    return bi->next();
}

int textBreakPreceding(TextBreakIterator* bi, int pos)
{
    return bi->preceding(pos);
}

int textBreakFollowing(TextBreakIterator* bi, int pos)
{
    return bi->following(pos);
}

int textBreakCurrent(TextBreakIterator* bi)
{
    return bi->currentPos;
}

bool isTextBreak(TextBreakIterator*, int)
{
    return true;
}

}

