/*
 * Copyright (C) 2009 Joerg Strohmayer.
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
#include "WebViewPrivate.h"
#include "IntRect.h"
#include "FrameView.h"
#include "Frame.h"
#include "cairo.h"
#include "GraphicsContext.h"
#include "Page.h"
#include "ProgressTracker.h"
#include "EventHandler.h"
#include "FocusController.h"
#include <MainThread.h>
#include <PlatformKeyboardEvent.h>
#include <PlatformMouseEvent.h>
#include <PlatformWheelEvent.h>
#include "SelectionController.h"
#include <SharedTimer.h>
#include "Editor.h"
#include "owb-config.h"
#include "PopupMenu.h"
#include "CString.h"
#include "FileIO.h"
#include "WebFrame.h"
#include "WebView.h"

#include <proto/exec.h>
#include <proto/clicktab.h>
#include <proto/fuelgauge.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/layers.h>
#include <proto/layout.h>
#include <proto/popupmenu.h>
#include <proto/requester.h>
#include <classes/requester.h>
#include <gadgets/clicktab.h>
#include <gadgets/fuelgauge.h>
#include <reaction/reaction_macros.h>
#include <graphics/blitattr.h>
#include <libraries/keymap.h>

using namespace WebCore;

AmigaWebNotificationDelegate::AmigaWebNotificationDelegate()
{
    m_OS41 = IExec->Data.LibBase->lib_Version >= 53;
}

AmigaWebNotificationDelegate::~AmigaWebNotificationDelegate()
{
}

void AmigaWebNotificationDelegate::startLoadNotification(WebFrame* webFrame)
{
    WebView* webView = webFrame->webView();
    if (webView->mainFrame() != webFrame)
        return;

    BalWidget* owbwindow = webView->viewWindow();
    if (m_OS41 && owbwindow && owbwindow->window && owbwindow->gad_fuelgauge) {
        if (amigaConfig.tabs) {
            if (ILayout->SetPageGadgetAttrs(owbwindow->gad_fuelgauge, owbwindow->page,
                                            owbwindow->window, NULL,
                                            FUELGAUGE_Level, 10,
                                            GA_Text, "",
                                            TAG_DONE))
                ILayout->RefreshPageGadget(owbwindow->gad_fuelgauge, owbwindow->page, owbwindow->window, NULL);
        }
        else
            if (IIntuition->SetAttrs(owbwindow->gad_fuelgauge,
                                     FUELGAUGE_Level, 10,
                                     GA_Text, "",
                                     TAG_DONE))
                ILayout->RefreshPageGadget(owbwindow->gad_hlayout, owbwindow->page, owbwindow->window, NULL);
    }
}

void AmigaWebNotificationDelegate::progressNotification(WebFrame* webFrame)
{
    WebView* webView = webFrame->webView();
    if (webView->mainFrame() != webFrame)
        return;

    BalWidget* owbwindow = webView->viewWindow();
    if (m_OS41 && owbwindow && owbwindow->window && owbwindow->gad_fuelgauge) {
        uint32 oldprogress = 0;
        IIntuition->GetAttr(FUELGAUGE_Level, owbwindow->gad_fuelgauge, &oldprogress);
        uint32 newprogress = (uint32)(webView->estimatedProgress() * 1000);

        if (oldprogress != newprogress) {
            owbwindow->fuelGaugeArgs[0] = (uint32)(webView->page()->progress()->totalBytesReceived() / 1024);
            owbwindow->fuelGaugeArgs[1] = (uint32)(webView->page()->progress()->totalPageAndResourceBytesToLoad() / 1024);
            if (amigaConfig.tabs) {
                if (ILayout->SetPageGadgetAttrs(owbwindow->gad_fuelgauge, owbwindow->page,
                                                owbwindow->window, NULL,
                                                FUELGAUGE_Level, newprogress,
                                                GA_Text, "%lU / %lU KB",
                                                FUELGAUGE_VarArgs, owbwindow->fuelGaugeArgs,
                                                TAG_DONE))
                    ILayout->RefreshPageGadget(owbwindow->gad_fuelgauge, owbwindow->page, owbwindow->window, NULL);
            }
            else
                if (IIntuition->SetAttrs(owbwindow->gad_fuelgauge,
                                         FUELGAUGE_Level, newprogress,
                                         GA_Text, "%lU / %lU KB",
                                         FUELGAUGE_VarArgs, owbwindow->fuelGaugeArgs,
                                         TAG_DONE))
                    ILayout->RefreshPageGadget(owbwindow->gad_hlayout, owbwindow->page, owbwindow->window, NULL);
        }
    }
}

void AmigaWebNotificationDelegate::finishedLoadNotification(WebFrame* webFrame)
{
    WebView* webView = webFrame->webView();
    BalWidget* owbwindow = webView->viewWindow();
    WebFrame* mainFrame = webView->mainFrame();
    Frame* frame = core(mainFrame);
    if (frame->view()) {
        webView->addToDirtyRegion(IntRect(0, 0, frame->view()->visibleWidth(), frame->view()->visibleHeight()));
        owbwindow->expose = true;
    }

    if (mainFrame != webFrame)
        return;

    if (m_OS41 && owbwindow && owbwindow->window && owbwindow->gad_fuelgauge) {
        if (amigaConfig.tabs) {
            if (ILayout->SetPageGadgetAttrs(owbwindow->gad_fuelgauge, owbwindow->page,
                                            owbwindow->window, NULL,
                                            FUELGAUGE_Level, 0,
                                            GA_Text, "",
                                            TAG_DONE))
                ILayout->RefreshPageGadget(owbwindow->gad_fuelgauge, owbwindow->page, owbwindow->window, NULL);
        }
        else
            if (IIntuition->SetAttrs(owbwindow->gad_fuelgauge,
                                     FUELGAUGE_Level, 0,
                                     GA_Text, "",
                                     TAG_DONE))
                ILayout->RefreshPageGadget(owbwindow->gad_hlayout, owbwindow->page, owbwindow->window, NULL);
    }
}



WebViewPrivate::WebViewPrivate(WebView* webView)
    : m_webView(webView)
    , isInitialized(false)
    , m_amigaWebNotificationDelegate(new AmigaWebNotificationDelegate())
{
    webView->setWebNotificationDelegate(m_amigaWebNotificationDelegate);
}

WebViewPrivate::~WebViewPrivate()
{
    delete m_amigaWebNotificationDelegate;
}

void WebViewPrivate::fireWebKitEvents()
{
    WTF::dispatchFunctionsFromMainThread();
    fireTimerIfNeeded();
}

void WebViewPrivate::onExpose(BalEventExpose event)
{
    Frame* frame = core(m_webView->mainFrame());
    if (!frame)
        return;

    if(!isInitialized) {
        isInitialized = true;
        frame->view()->resize(m_rect.width(), m_rect.height());
        frame->forceLayout();
        frame->view()->adjustViewSize();
    }

    BalWidget* widget = m_webView->viewWindow();
    if (!widget->window)
        return;

    GraphicsContext ctx(widget->cr);
    IntRect rect(dirtyRegion());
    if (frame->contentRenderer() && frame->view() && !rect.isEmpty()) {
        frame->view()->layoutIfNeededRecursive();
        frame->view()->paint(&ctx, rect);
        updateView(widget, rect);
        clearDirtyRegion();
    }
}

void WebViewPrivate::onKeyDown(BalEventKey event)
{
    Frame* frame = m_webView->page()->focusController()->focusedOrMainFrame();
    if (!frame)
        return;
    PlatformKeyboardEvent keyboardEvent(&event);

    if (frame->eventHandler()->keyEvent(keyboardEvent))
        return;

    if (IDCMP_RAWKEY == event.Class) {
        FrameView* view = frame->view();
        SelectionController::EAlteration alteration;
        if (event.Qualifier & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT))
            alteration = SelectionController::EXTEND;
        else
            alteration = SelectionController::MOVE;

        switch (event.Code) {
        case RAWKEY_CRSRDOWN:
            view->scrollBy(IntSize(0, cMouseWheelPixelsPerLineStep));
            return;
        case RAWKEY_CRSRUP:
            view->scrollBy(IntSize(0, -cMouseWheelPixelsPerLineStep));
            return;
        case RAWKEY_CRSRRIGHT:
            view->scrollBy(IntSize(cMouseWheelPixelsPerLineStep, 0));
            return;
        case RAWKEY_CRSRLEFT:
            view->scrollBy(IntSize(-cMouseWheelPixelsPerLineStep, 0));
            return;
        case RAWKEY_PAGEDOWN:
            view->scrollBy(IntSize(0, view->visibleHeight() * 0.8));
            return;
        case RAWKEY_PAGEUP:
            view->scrollBy(IntSize(0, view->visibleHeight() * -0.8));
            return;
        case RAWKEY_HOME:
            frame->selection()->modify(alteration, SelectionController::BACKWARD, DocumentBoundary, true);
            return;
        case RAWKEY_END:
            frame->selection()->modify(alteration, SelectionController::FORWARD, DocumentBoundary, true);
            return;
#if 0
        case SDLK_ESCAPE:
            SDL_Event ev;
            ev.type = SDL_QUIT;
            SDL_PushEvent(&ev);
            return;
#endif
        case RAWKEY_F1:
        {
            m_webView->goBack();
            GraphicsContext ctx(m_webView->viewWindow()->cr);
            if (frame->contentRenderer() && frame->view()) {
                frame->view()->layoutIfNeededRecursive();
                IntRect dirty(0, 0, m_rect.width(), m_rect.height());
                frame->view()->paint(&ctx, dirty);
                clearDirtyRegion();
            }
            return;
        }
        case RAWKEY_F2:
        {
            m_webView->goForward();
            GraphicsContext ctx(m_webView->viewWindow()->cr);
            if (frame->contentRenderer() && frame->view()) {
                frame->view()->layoutIfNeededRecursive();
                IntRect dirty(0, 0, m_rect.width(), m_rect.height());
                frame->view()->paint(&ctx, dirty);
                clearDirtyRegion();
            }
            return;
        }
        case RAWKEY_F3:
            // We ignore the return value as we are not interested
            // in whether the zoom occurred.
            m_webView->zoomPageIn();
            return;
        case RAWKEY_F4:
            // Same comment as above.
            m_webView->zoomPageOut();
            return;
        }
    }
    else if (event.Qualifier & (IEQUALIFIER_CONTROL | AMIGAKEYS))
        switch (event.Code) {
        case 'c':
        case 'x':
            frame->editor()->command("Copy").execute();
            break;
        }
}

void WebViewPrivate::onKeyUp(BalEventKey event)
{
    Frame* frame = m_webView->page()->focusController()->focusedOrMainFrame();
    if (!frame)
        return;
    PlatformKeyboardEvent keyboardEvent(&event);

    if (frame->eventHandler()->keyEvent(keyboardEvent))
        return ;
}

void WebViewPrivate::onMouseMotion(BalEventMotion event)
{
    Frame* frame = core(m_webView->mainFrame());
    if (!frame)
        return;
    frame->eventHandler()->mouseMoved(PlatformMouseEvent(&event));
}

void WebViewPrivate::onMouseButtonDown(BalEventButton event)
{
    Frame* frame = core(m_webView->mainFrame());

    if (!frame)
        return;

    if (amigaConfig.contextMenuRMB) {
        if (IECODE_RBUTTON == event.Code) {
            frame->eventHandler()->sendContextMenuEvent(PlatformMouseEvent(&event));
            return;
        }
    }
    else {
        if (IECODE_MBUTTON == event.Code) {
            frame->eventHandler()->sendContextMenuEvent(PlatformMouseEvent(&event));
            return;
        }
    }

    frame->eventHandler()->handleMousePressEvent(PlatformMouseEvent(&event));
}

void WebViewPrivate::onMouseButtonUp(BalEventButton event)
{
    Frame* focusedFrame = m_webView->page()->focusController()->focusedFrame();
    if (!focusedFrame)
        return;
//     if (focusedFrame && focusedFrame->editor()->canEdit()) {
//         ;
//     }

    focusedFrame->eventHandler()->handleMouseReleaseEvent(PlatformMouseEvent(&event));
}

void WebViewPrivate::onScroll(BalEventScroll event)
{
    Frame* frame = core(m_webView->mainFrame());
    if (!frame)
        return;
    PlatformWheelEvent wheelEvent(&event);
    frame->eventHandler()->handleWheelEvent(wheelEvent);
}

void WebViewPrivate::onResize(BalResizeEvent event)
{
    Frame* frame = core(m_webView->mainFrame());
    if (!frame)
        return;
    m_rect.setWidth(event.w);
    m_rect.setHeight(event.h);
    frame->view()->resize(event.w, event.h);
    frame->forceLayout();
    frame->view()->adjustViewSize();
}

void WebViewPrivate::onQuit(BalQuitEvent)
{
}

void WebViewPrivate::onUserEvent(BalUserEvent)
{
}

void WebViewPrivate::popupMenuHide()
{
    //BOOKMARKLET_INSTALL_PATH
}


void WebViewPrivate::popupMenuShow(void* popupInfo)
{
    extern char* utf8ToAmiga(const char* utf8);

    PopupMenu* pop = static_cast<PopupMenu*>(popupInfo);
    if (!pop)
        return;

    BalWidget* widget = pop->client()->hostWindow()->platformWindow();
    if (!widget)
        return;

    if (widget != m_webView->viewWindow())
        return;

    int itemCount = pop->client()->listSize();
    if (itemCount > 25 * 25)
        return;

    Object *menu, *items[itemCount], *subMenu[25], *subMenuItems[25];
    String subMenuTitles[25];
    memset(subMenu, 0, sizeof(subMenu));

    menu = (Object*)IIntuition->NewObject(IPopupMenu->POPUPMENU_GetClass(), NULL,
                                          PMA_Left, pop->windowRect().x() + widget->offsetx,
                                          PMA_Top, pop->windowRect().y() + widget->offsety,
                                          TAG_DONE);
    if (!menu)
        return;

    bool gotAllItems = true;

    if (itemCount <= 25) {
        for (int i = 0; i < itemCount; ++i) {
            char* text = utf8ToAmiga(pop->client()->itemText(i).utf8().data());
            items[i] = (Object*)IIntuition->NewObject(IPopupMenu->POPUPMENU_GetItemClass(), NULL,
                                                      PMIA_Title, pop->client()->itemIsSeparator(i) ? (const char *)~0 : text ?: "",
                                                      PMIA_Hidden, !pop->client()->itemIsEnabled(i),
                                                      PMIA_FillPen, pop->client()->itemIsSelected(i),
                                                      TAG_DONE);
            free(text);
            if (!items[i])
                gotAllItems = false;
            else
                IIntuition->IDoMethod(menu, OM_ADDMEMBER, items[i]);
        }
    }
    else {
        for (int i = 0; i < (itemCount + 24) / 25; i++) {
            subMenu[i] = (Object*)IIntuition->NewObject(IPopupMenu->POPUPMENU_GetClass(), NULL, TAG_DONE);
            if (!subMenu[i])
                gotAllItems = false;
            else {
                int j;
                for (j = 0; i * 25 + j < itemCount && j < 25 ; j++) {
                    char* text = utf8ToAmiga(pop->client()->itemText(i * 25 + j).utf8().data());
                    items[i * 25 + j] = (Object*)IIntuition->NewObject(IPopupMenu->POPUPMENU_GetItemClass(), NULL,
                                                                       PMIA_Title, pop->client()->itemIsSeparator(i * 25 + j) ? (const char *)~0 : text ?: "",
                                                                       PMIA_Hidden, !pop->client()->itemIsEnabled(i * 25 + j),
                                                                       PMIA_FillPen, pop->client()->itemIsSelected(i * 25 + j),
                                                                       TAG_DONE);
                    free(text);
                    if (!items[i * 25 + j])
                        gotAllItems = false;
                    else
                        IIntuition->IDoMethod(subMenu[i], OM_ADDMEMBER, items[i * 25 + j]);
                }

                subMenuTitles[i] = pop->client()->itemText(i * 25);
                if (j > 0) {
                    subMenuTitles[i] += " ... ";
                    subMenuTitles[i] += pop->client()->itemText(i * 25 + j - 1);
                }
                char* text = utf8ToAmiga(subMenuTitles[i].utf8().data());
                subMenuItems[i] = (Object*)IIntuition->NewObject(IPopupMenu->POPUPMENU_GetItemClass(), NULL,
                                                                 PMIA_Title, text ?: "...",
                                                                 PMIA_SubMenu, subMenu[i],
                                                                 TAG_DONE);
                free(text);
                if (!subMenuItems[i])
                    gotAllItems = false;
                else
                    IIntuition->IDoMethod(menu, OM_ADDMEMBER, subMenuItems[i]);
            }
        }
    }

    if (gotAllItems) {
        Object* selected = (Object*)IIntuition->IDoMethod(menu, PM_OPEN, widget->window);
        for (int i = 0; i < itemCount; ++i)
            if (selected == items[i]) {
                pop->client()->setTextFromItem(i);
                pop->client()->valueChanged(i);
                break;
            }

        pop->client()->hidePopup();
    }

    IIntuition->DisposeObject(menu);
}

void WebViewPrivate::updateView(BalWidget* widget, IntRect rect)
{
    if (!widget || !widget->window || rect.isEmpty())
        return;

    if (amigaConfig.tabs) {
        ::Node* node;
        BalWidget* activeWidget = widget;
        IIntuition->GetAttr(CLICKTAB_CurrentNode, widget->gad_clicktab, (uint32*)(void*)&node);
        IClickTab->GetClickTabNodeAttrs(node, TNA_UserData, &activeWidget, TAG_DONE);
        if (widget != activeWidget)
            return;
    }

    int x = rect.x();
    int y = rect.y();
    int width = rect.width();
    int height = rect.height();

    if (width > widget->webViewWidth - x)
        width = widget->webViewWidth - x;
    if (height > widget->webViewHeight - y)
        height = widget->webViewHeight - y;

    if (width <= 0 || height <= 0)
        return;

    IGraphics->BltBitMapTags(BLITA_Source,         cairo_image_surface_get_data(widget->surface),
                             BLITA_SrcType,        BLITT_ARGB32,
                             BLITA_SrcBytesPerRow, cairo_image_surface_get_stride(widget->surface),
                             BLITA_SrcX,           x,
                             BLITA_SrcY,           y,
                             BLITA_Width,          width,
                             BLITA_Height,         height,
                             BLITA_Dest,           widget->window->RPort,
                             BLITA_DestType,       BLITT_RASTPORT,
                             BLITA_DestX,          x + widget->offsetx,
                             BLITA_DestY,          y + widget->offsety,
                             TAG_DONE);
}

void WebViewPrivate::sendExposeEvent(IntRect)
{
    if (m_webView->viewWindow())
        m_webView->viewWindow()->expose = true;
}

void WebViewPrivate::repaint(const WebCore::IntRect& windowRect, bool contentChanged, bool immediate, bool repaintContentOnly)
{
    if (contentChanged)
        addToDirtyRegion(windowRect);
    sendExposeEvent(windowRect);
}

void WebViewPrivate::scrollBackingStore(WebCore::FrameView* view, int dx, int dy, const WebCore::IntRect& scrollViewRect, const WebCore::IntRect& clipRect)
{
    m_backingStoreDirtyRegion.move(dx, dy);

    BalWidget* widget = m_webView->viewWindow();
    if (!widget || !widget->window)
        return;

    IntRect updateRect = clipRect;
    updateRect.intersect(scrollViewRect);

    int x = updateRect.x();
    int y = updateRect.y();
    int width = updateRect.width();
    int height = updateRect.height();
    int dirtyX = 0, dirtyY = 0, dirtyW = 0, dirtyH = 0;

    if (width > widget->webViewWidth - x)
        width = widget->webViewWidth - x;
    if (height > widget->webViewHeight - y)
        height = widget->webViewHeight - y;

    dx = -dx;
    dy = -dy;

    if (dy == 0 && dx < 0 && -dx < width) {
        dirtyX = x;
        dirtyY = y;
        dirtyW = -dx;
        dirtyH = height;
    }
    else if (dy == 0 && dx > 0 && dx < width) {
        dirtyX = x + width - dx;
        dirtyY = y;
        dirtyW = dx;
        dirtyH = height;
    }
    else if (dx == 0 && dy < 0 && -dy < height) {
        dirtyX = x;
        dirtyY = y;
        dirtyW = width;
        dirtyH = -dy;
    }
    else if (dx == 0 && dy > 0 && dy < height) {
        dirtyX = x;
        dirtyY = y + height - dy;
        dirtyW = width;
        dirtyH = dy;
    }

    if (dirtyX || dirtyY || dirtyW || dirtyH) {
        RastPort* RPort = widget->window->RPort;
        Layer* Layer = RPort->Layer;
        Hook* oldhook = ILayers->InstallLayerHook(Layer, LAYERS_NOBACKFILL);
        x += widget->offsetx;
        y += widget->offsety;
        IGraphics->ScrollRasterBF(RPort, dx, dy, x, y, x + width - 1, y + height - 1);
        ILayers->InstallLayerHook(Layer, oldhook);

        addToDirtyRegion(IntRect(dirtyX, dirtyY, dirtyW, dirtyH));
    }
    else
        addToDirtyRegion(updateRect);

    sendExposeEvent(IntRect());
}

void WebViewPrivate::runJavaScriptAlert(WebFrame* frame, const char* message)
{
    extern char* utf8ToAmiga(const char* utf8);

    char* messageAmiga = utf8ToAmiga(message);
    Object* requester = (Object*)RequesterObject,
                                     REQ_TitleText, "OWB Javascript Alert",
                                     REQ_BodyText, messageAmiga,
                                     REQ_GadgetText, "Ok",
                                 End;
    if (requester) {
        Window* window = m_webView->viewWindow()->window;
        Requester dummyRequester;

        if (window) {
            IIntuition->InitRequester(&dummyRequester);
            IIntuition->Request(&dummyRequester, window);
            IIntuition->SetWindowPointer(window, WA_BusyPointer, TRUE, WA_PointerDelay, TRUE, TAG_DONE);
        }

        OpenRequester(requester, window);

        if (window) {
            IIntuition->SetWindowPointer(window, WA_BusyPointer, FALSE, TAG_DONE);
            IIntuition->EndRequest(&dummyRequester, window);
        }

        IIntuition->DisposeObject(requester);
    }
    free(messageAmiga);
}
