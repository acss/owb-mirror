/*
 * Copyright (C) 2008 Joerg Strohmayer.
 * Copyright (C) 2008 Pleyo.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Pleyo nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PLEYO AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL PLEYO OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Pasteboard.h"

#include "CString.h"
#include "DocumentFragment.h"
#include "Frame.h"
#include "PlatformString.h"
#include "Image.h"
#include "Range.h"
#include "RenderImage.h"
#include "KURL.h"
#include "markup.h"
#include "proto/iffparse.h"
#include "datatypes/textclass.h"

#include <cstdio>

namespace WKAL {

/* FIXME: we must get rid of this and use the enum in webkitwebview.h someway */
typedef enum
{
    WEBKIT_WEB_VIEW_TARGET_INFO_HTML = - 1,
    WEBKIT_WEB_VIEW_TARGET_INFO_TEXT = - 2
} WebKitWebViewTargetInfo;

class PasteboardSelectionData {
public:
    PasteboardSelectionData(char* text, char* markup)
        : m_text(text)
        , m_markup(markup) { }

    ~PasteboardSelectionData() {
        free(m_text);
        free(m_markup);
    }

    const char* text() const { return m_text; }
    const char* markup() const { return m_markup; }

private:
    char* m_text;
    char* m_markup;
};

Pasteboard* Pasteboard::generalPasteboard()
{
//    printf("Pasteboard::generalPasteboard\n");
    static Pasteboard* pasteboard = new Pasteboard();
    return pasteboard;
}

Pasteboard::Pasteboard()
{
//    printf("Pasteboard::Pasteboard\n");
    NotImplemented();
}

Pasteboard::~Pasteboard()
{
    printf("Pasteboard::~Pasteboard\n");
    delete m_helper;
}

void Pasteboard::setHelper(PasteboardHelper* helper)
{
    printf("Pasteboard::setHelper\n");
    m_helper = helper;
}

void Pasteboard::writeSelection(Range* selectedRange, bool canSmartCopyOrDelete, Frame* frame)
{
    if (IFFHandle *ih = IIFFParse->AllocIFF()) {
        if (ClipboardHandle *ch = IIFFParse->OpenClipboard(PRIMARY_CLIP)) {
            ih->iff_Stream = (uint32)ch;
            IIFFParse->InitIFFasClip(ih);
            if (0 == IIFFParse->OpenIFF(ih, IFFF_WRITE)) {
                if (0 == IIFFParse->PushChunk(ih, ID_FTXT, ID_FORM, IFFSIZE_UNKNOWN)) {
                    if (0 == IIFFParse->PushChunk(ih, 0, ID_CHRS, IFFSIZE_UNKNOWN)) {
                        CString utf8 = selectedRange->text().utf8();
                        const char *data = utf8.data();
                        size_t len = utf8.length();

                        if (data && len)
                            IIFFParse->WriteChunkBytes(ih, data, len);

                        IIFFParse->PopChunk(ih);
                    }
                    IIFFParse->PopChunk(ih);
                }
                IIFFParse->CloseIFF(ih);
            }
            IIFFParse->CloseClipboard(ch);
        }
        IIFFParse->FreeIFF(ih);
    }
}

void Pasteboard::writeURL(const KURL& url, const String&, Frame* frame)
{
    printf("Pasteboard::writeURL\n");

}

void Pasteboard::writeImage(Node* node, const KURL&, const String&)
{
    printf("Pasteboard::writeImage\n");
    NotImplemented();
}

void Pasteboard::clear()
{
    printf("Pasteboard::clear\n");
}

bool Pasteboard::canSmartReplace()
{
//    printf("Pasteboard::canSmartReplace\n");
    NotImplemented();
    return false;
}

PassRefPtr<DocumentFragment> Pasteboard::documentFragment(Frame* frame, PassRefPtr<Range> context,
                                                          bool allowPlainText, bool& chosePlainText)
{
    printf("Pasteboard::documentFragment\n");
    return 0;
}

static void copycollection(String &str, CollectionItem* ci)
{
    if (ci->ci_Next)
        copycollection(str, ci->ci_Next);

    str.append(String().fromUTF8((const char *)ci->ci_Data, ci->ci_Size));
}

String Pasteboard::plainText(Frame* frame)
{
    String result;

    if (IFFHandle *ih = IIFFParse->AllocIFF()) {
        if (ClipboardHandle *ch = IIFFParse->OpenClipboard(PRIMARY_CLIP)) {
            ih->iff_Stream = (uint32)ch;
            IIFFParse->InitIFFasClip(ih);

            if (0 == IIFFParse->OpenIFF(ih, IFFF_READ)) {
                if (0 == IIFFParse->CollectionChunk(ih, ID_FTXT, ID_CHRS)
                 && 0 == IIFFParse->StopOnExit(ih, ID_FTXT, ID_FORM)) {
                    while (true) {
                        int32 err = IIFFParse->ParseIFF(ih, IFFPARSE_SCAN);

                        if (IFFERR_EOF == err)
                            break;
                        else if (IFFERR_EOC == err) {
                            if (CollectionItem *ci = IIFFParse->FindCollection(ih, ID_FTXT, ID_CHRS))
                                copycollection(result, ci);
                        }
                        else
                            break;
                    }
                }
                IIFFParse->CloseIFF(ih);
            }
            IIFFParse->CloseClipboard(ch);
        }
        IIFFParse->FreeIFF(ih);
    }
    return result;
}

}
