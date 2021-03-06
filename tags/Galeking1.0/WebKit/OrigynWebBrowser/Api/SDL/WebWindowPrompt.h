/*
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
#ifndef WebWindowPrompt_H
#define WebWindowPrompt_H

#include "WebKit.h"
#include "WebWindow.h"
#include <string.h>

class WebView;

class WEBKIT_OWB_API WebWindowPrompt : public WebWindow
{
public:
    static WebWindowPrompt* createWebWindowPrompt(const char* text, const char* defaultText, WebView *);
    virtual ~WebWindowPrompt();
    virtual void onExpose(BalEventExpose event);
    virtual void onKeyDown(BalEventKey event);
    virtual void onKeyUp(BalEventKey event);
    virtual void onMouseMotion(BalEventMotion event);
    virtual void onMouseButtonDown(BalEventButton event);
    virtual void onMouseButtonUp(BalEventButton event);
    virtual void onScroll(BalEventScroll event);
    virtual void onResize(BalResizeEvent event);
    virtual void onQuit(BalQuitEvent);
    virtual void onUserEvent(BalUserEvent);

    const char* value();

private:
    WebWindowPrompt(const char* text, const char* defaultText, WebView *);
    void drawTextBox();
    void updateTextBox();
    void drawButton();
    void updateButton();

    std::string m_text;
    std::string m_defaultText;
    std::string m_buttonOk;
    std::string m_buttonCancel;
    bool m_button;
    bool m_stateLeft;
    bool m_stateRigth;
    WebView *m_view;
    bool isPainted;
    std::string m_value;
    bool m_Prompt;
    bool isThemable;
    BalRectangle m_buttonOKRect;
    BalRectangle m_buttonCancelRect;
    BalRectangle m_textRect;
};

#endif

