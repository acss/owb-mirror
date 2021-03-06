/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Collabora Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "PluginView.h"

#include "Document.h"
#include "DocumentLoader.h"
#include "Element.h"
#include "EventNames.h"
#include "FrameLoader.h"
#include "FrameLoadRequest.h"
#include "FrameTree.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "Image.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "PluginDebug.h"
#include "PluginPackage.h"
#include "RenderLayer.h"
#include "Settings.h"
#include "JSDOMBinding.h"
#include "ScriptController.h"
#include "npruntime_impl.h"
#include "runtime.h"
#include "runtime_root.h"
#include <kjs/JSLock.h>
#include <kjs/JSValue.h>

#include <gdkconfig.h>
#include <gtk/gtk.h>

#if defined(GDK_WINDOWING_X11)
#include "gtk2xtbin.h"
#include <gdk/gdkx.h>
#endif
#ifdef GDK_WINDOWING_WIN32
#include <gdk/gdkwin32.h>
#endif

using JSC::ExecState;
using JSC::Interpreter;
using JSC::JSLock;
using JSC::JSObject;
using JSC::JSValue;
using JSC::UString;

using std::min;

using namespace WTF;

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

void PluginView::updateWindow() const
{
    if (!parent() || !m_isWindowed)
        return;

    ASSERT(parent()->isFrameView());
    FrameView* frameView = static_cast<FrameView*>(parent());

    IntRect oldWindowRect = m_windowRect;
    IntRect oldClipRect = m_clipRect;

    m_windowRect = IntRect(frameView->contentsToWindow(frameGeometry().location()), frameGeometry().size());
    m_clipRect = windowClipRect();
    m_clipRect.move(-m_windowRect.x(), -m_windowRect.y());

    GtkAllocation allocation = { m_windowRect.x(), m_windowRect.y(), m_windowRect.width(), m_windowRect.height() };
    if (m_window) {
        gtk_widget_size_allocate(m_window, &allocation);
#if defined(GDK_WINDOWING_X11)
        if (!m_needsXEmbed) {
            gtk_xtbin_set_position(GTK_XTBIN(m_window), m_windowRect.x(), m_windowRect.y());
            gtk_xtbin_resize(m_window, m_windowRect.width(), m_windowRect.height());
        }
#endif
    }
}

void PluginView::setFocus()
{
    if (m_window)
        gtk_widget_grab_focus(m_window);

    Widget::setFocus();
}

void PluginView::show()
{
    m_isVisible = true;

    if (m_attachedToWindow && m_window)
        gtk_widget_show(m_window);

    Widget::show();
}

void PluginView::hide()
{
    m_isVisible = false;

    if (m_attachedToWindow && m_window)
        gtk_widget_hide(m_window);

    Widget::hide();
}

void PluginView::paint(GraphicsContext* context, const IntRect& rect)
{
    if (!m_isStarted) {
        // Draw the "missing plugin" image
        //paintMissingPluginIcon(context, rect);
        return;
    }

    if (m_isWindowed || context->paintingDisabled())
        return;

    NPEvent npEvent;
    /* Need to synthesize Xevents here */

    m_npWindow.type = NPWindowTypeDrawable;

    ASSERT(parent()->isFrameView());

    if (m_plugin->pluginFuncs()->event) {
        JSC::JSLock::DropAllLocks dropAllLocks(false);
        m_plugin->pluginFuncs()->event(m_instance, &npEvent);
    }

    setNPWindowRect(frameGeometry());
}

void PluginView::handleKeyboardEvent(KeyboardEvent* event)
{
    NPEvent npEvent;
    
    /* FIXME: Synthesize an XEvent to pass through */

    JSC::JSLock::DropAllLocks dropAllLocks(false);
    if (!m_plugin->pluginFuncs()->event(m_instance, &npEvent))
        event->setDefaultHandled();
}

void PluginView::handleMouseEvent(MouseEvent* event)
{
    NPEvent npEvent;

    if (!m_isWindowed)
      return;

    /* FIXME: Synthesize an XEvent to pass through */
    IntPoint p = static_cast<FrameView*>(parent())->contentsToWindow(IntPoint(event->pageX(), event->pageY()));

    JSC::JSLock::DropAllLocks dropAllLocks(false);
    if (!m_plugin->pluginFuncs()->event(m_instance, &npEvent))
        event->setDefaultHandled();
}

void PluginView::setParent(ScrollView* parent)
{
    Widget::setParent(parent);

    if (parent)
        init();
    else {
        if (!m_window)
            return;
    }
}

void PluginView::setNPWindowRect(const IntRect& rect)
{
    if (!m_isStarted || !parent())
        return;

    IntPoint p = static_cast<FrameView*>(parent())->contentsToWindow(rect.location());
    m_npWindow.x = p.x();
    m_npWindow.y = p.y();

    m_npWindow.width = rect.width();
    m_npWindow.height = rect.height();

    m_npWindow.clipRect.left = 0;
    m_npWindow.clipRect.top = 0;
    m_npWindow.clipRect.right = rect.width();
    m_npWindow.clipRect.bottom = rect.height();

    if (m_npWindow.x < 0 || m_npWindow.y < 0 ||
        m_npWindow.width <= 0 || m_npWindow.height <= 0)
        return;

    if (m_plugin->pluginFuncs()->setwindow) {
        PluginView::setCurrentPluginView(this);
        JSC::JSLock::DropAllLocks dropAllLocks(false);
        setCallingPlugin(true);
        m_plugin->pluginFuncs()->setwindow(m_instance, &m_npWindow);
        setCallingPlugin(false);
        PluginView::setCurrentPluginView(0);

        if (!m_isWindowed)
            return;

        ASSERT(m_window);
    }
}

void PluginView::attachToWindow()
{
    if (m_attachedToWindow)
        return;

    m_attachedToWindow = true;
    if (m_isVisible && m_window)
        gtk_widget_show(m_window);
}

void PluginView::detachFromWindow()
{
    if (!m_attachedToWindow)
        return;

    if (m_isVisible && m_window)
        gtk_widget_hide(m_window);
    m_attachedToWindow = false;
}

void PluginView::stop()
{
    if (!m_isStarted)
        return;

    HashSet<RefPtr<PluginStream> > streams = m_streams;
    HashSet<RefPtr<PluginStream> >::iterator end = streams.end();
    for (HashSet<RefPtr<PluginStream> >::iterator it = streams.begin(); it != end; ++it) {
        (*it)->stop();
        disconnectStream((*it).get());
    }

    ASSERT(m_streams.isEmpty());

    m_isStarted = false;
    JSC::JSLock::DropAllLocks dropAllLocks(false);

    // Clear the window
    m_npWindow.window = 0;
#ifdef XP_UNIX
    if (m_isWindowed && m_npWindow.ws_info) 
           delete (NPSetWindowCallbackStruct *)m_npWindow.ws_info;
    m_npWindow.ws_info = 0;
#endif
    if (m_plugin->pluginFuncs()->setwindow && !m_plugin->quirks().contains(PluginQuirkDontSetNullWindowHandleOnDestroy)) {
        PluginView::setCurrentPluginView(this);
        setCallingPlugin(true);
        m_plugin->pluginFuncs()->setwindow(m_instance, &m_npWindow);
        setCallingPlugin(false);
        PluginView::setCurrentPluginView(0);
    }

    // Destroy the plugin
    {
        PluginView::setCurrentPluginView(this);
        setCallingPlugin(true);
        m_plugin->pluginFuncs()->destroy(m_instance, 0);
        setCallingPlugin(false);
        PluginView::setCurrentPluginView(0);
    }

    m_instance->pdata = 0;
}

static const char* MozillaUserAgent = "Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.8.1) Gecko/20061010 Firefox/2.0";

const char* PluginView::userAgent()
{
    if (m_plugin->quirks().contains(PluginQuirkWantsMozillaUserAgent))
        return MozillaUserAgent;

    if (m_userAgent.isNull())
        m_userAgent = m_parentFrame->loader()->userAgent(m_url).utf8();

    return m_userAgent.data();
}

const char* PluginView::userAgentStatic()
{
    //FIXME - Lie and say we are Mozilla
    return MozillaUserAgent;
}

NPError PluginView::handlePostReadFile(Vector<char>& buffer, uint32 len, const char* buf)
{
    String filename(buf, len);

    if (filename.startsWith("file:///"))
        filename = filename.substring(8);

    // Get file info
    if (!g_file_test ((filename.utf8()).data(), (GFileTest)(G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)))
        return NPERR_FILE_NOT_FOUND;

    //FIXME - read the file data into buffer
    FILE* fileHandle = fopen((filename.utf8()).data(), "r");
    
    if (fileHandle == 0)
        return NPERR_FILE_NOT_FOUND;

    //buffer.resize();

    int bytesRead = fread(buffer.data(), 1, 0, fileHandle);

    fclose(fileHandle);

    if (bytesRead <= 0)
        return NPERR_FILE_NOT_FOUND;

    return NPERR_NO_ERROR;
}

NPError PluginView::getValueStatic(NPNVariable variable, void* value)
{
    switch (variable) {
    case NPNVToolkit:
#if PLATFORM(GTK)
        *((uint32 *)value) = 2;
#else
        *((uint32 *)value) = 0;
#endif
        return NPERR_NO_ERROR;

    case NPNVSupportsXEmbedBool:
#if defined(GDK_WINDOWING_X11)
        *((uint32 *)value) = true;
#else
        *((uint32 *)value) = false;
#endif
        return NPERR_NO_ERROR;

    case NPNVjavascriptEnabledBool:
        *((uint32 *)value) = true;
        return NPERR_NO_ERROR;

    default:
        return NPERR_GENERIC_ERROR;
    }
}

NPError PluginView::getValue(NPNVariable variable, void* value)
{
    switch (variable) {
    case NPNVxDisplay:
#if defined(GDK_WINDOWING_X11)
        if (m_needsXEmbed)
            *(void **)value = (void *)GDK_DISPLAY();
        else
            *(void **)value = (void *)GTK_XTBIN(m_window)->xtclient.xtdisplay;
        return NPERR_NO_ERROR;
#else
        return NPERR_GENERIC_ERROR;
#endif

#if defined(GDK_WINDOWING_X11)
    case NPNVxtAppContext:
        if (!m_needsXEmbed) {
            *(void **)value = XtDisplayToApplicationContext (GTK_XTBIN(m_window)->xtclient.xtdisplay);

            return NPERR_NO_ERROR;
        } else
            return NPERR_GENERIC_ERROR;
#endif

#if ENABLE(NETSCAPE_PLUGIN_API)
        case NPNVWindowNPObject: {
            if (m_isJavaScriptPaused)
                return NPERR_GENERIC_ERROR;

            NPObject* windowScriptObject = m_parentFrame->script()->windowScriptNPObject();

            // Return value is expected to be retained, as described here: <http://www.mozilla.org/projects/plugin/npruntime.html>
            if (windowScriptObject)
                _NPN_RetainObject(windowScriptObject);

            void** v = (void**)value;
            *v = windowScriptObject;
            
            return NPERR_NO_ERROR;
        }

        case NPNVPluginElementNPObject: {
            if (m_isJavaScriptPaused)
                return NPERR_GENERIC_ERROR;

            NPObject* pluginScriptObject = 0;

            if (m_element->hasTagName(appletTag) || m_element->hasTagName(embedTag) || m_element->hasTagName(objectTag))
                pluginScriptObject = static_cast<HTMLPlugInElement*>(m_element)->getNPObject();

            // Return value is expected to be retained, as described here: <http://www.mozilla.org/projects/plugin/npruntime.html>
            if (pluginScriptObject)
                _NPN_RetainObject(pluginScriptObject);

            void** v = (void**)value;
            *v = pluginScriptObject;

            return NPERR_NO_ERROR;
        }
#endif

        case NPNVnetscapeWindow: {
            void* w = reinterpret_cast<void*>(value);

#ifdef GDK_WINDOWING_X11
            *((XID *)w) = GDK_WINDOW_XWINDOW(containingWindow()->window);
#endif
#ifdef GDK_WINDOWING_WIN32
            *((HWND *)w) = GDK_WINDOWING_HWND(containingWindow()->window);
#endif
            return NPERR_NO_ERROR;
        }

        default:
            return getValueStatic(variable, value);
    }
}

void PluginView::invalidateRect(NPRect* rect)
{
    if (!rect) {
        invalidate();
        return;
    }

    IntRect r(rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top);
    Widget::invalidateRect(r);
}

void PluginView::forceRedraw()
{
    if (m_isWindowed)
        gtk_widget_queue_draw(m_window);
    else
        gtk_widget_queue_draw(containingWindow());
}

PluginView::~PluginView()
{
    stop();

    deleteAllValues(m_requests);

    freeStringArray(m_paramNames, m_paramCount);
    freeStringArray(m_paramValues, m_paramCount);

    m_parentFrame->script()->cleanupScriptObjectsForPlugin(this);

    if (m_plugin && !(m_plugin->quirks().contains(PluginQuirkDontUnloadPlugin)))
        m_plugin->unload();
}

static gboolean
plug_removed_cb(GtkSocket *socket, gpointer)
{
    return TRUE;
}

void PluginView::init()
{
    if (m_haveInitialized)
        return;
    m_haveInitialized = true;

    if (!m_plugin) {
        ASSERT(m_status == PluginStatusCanNotFindPlugin);
        return;
    }

    if (!m_plugin->load()) {
        m_plugin = 0;
        m_status = PluginStatusCanNotLoadPlugin;
        return;
    }

    if (!start()) {
        m_status = PluginStatusCanNotLoadPlugin;
        return;
    }

    if (m_plugin->pluginFuncs()->getvalue) {
        PluginView::setCurrentPluginView(this);
        JSC::JSLock::DropAllLocks dropAllLocks(false);
        setCallingPlugin(true);
        m_plugin->pluginFuncs()->getvalue(m_instance, NPPVpluginNeedsXEmbed, &m_needsXEmbed);
        setCallingPlugin(false);
        PluginView::setCurrentPluginView(0);
    }

#if defined(GDK_WINDOWING_X11)
    if (m_needsXEmbed) {
        m_window = gtk_socket_new();
        gtk_container_add(GTK_CONTAINER(containingWindow()), m_window);
        g_signal_connect(m_window, "plug_removed", G_CALLBACK(plug_removed_cb), NULL);
        setGtkWidget(m_window);
    } else if (m_isWindowed) {
        m_window = gtk_xtbin_new(m_parentFrame->view()->containingWindow()->window, 0);
        setGtkWidget(m_window);
    }
#else
    m_window = gtk_socket_new();
    gtk_container_add(GTK_CONTAINER(m_parentFrame->view()->containingWindow()), m_window);
    setGtkWidget(m_window);
#endif
    show ();

    if (m_isWindowed) {
        m_npWindow.type = NPWindowTypeWindow;
#if defined(GDK_WINDOWING_X11)
        NPSetWindowCallbackStruct *ws = new NPSetWindowCallbackStruct();

        ws->type = 0;

        if (m_needsXEmbed) {
            gtk_widget_realize(m_window);
            m_npWindow.window = (void*)gtk_socket_get_id(GTK_SOCKET(m_window));
            ws->display = GDK_WINDOW_XDISPLAY(m_window->window);
            ws->visual = GDK_VISUAL_XVISUAL(gdk_drawable_get_visual(GDK_DRAWABLE(m_window->window)));
            ws->depth = gdk_drawable_get_visual(GDK_DRAWABLE(m_window->window))->depth;
            ws->colormap = GDK_COLORMAP_XCOLORMAP(gdk_drawable_get_colormap(GDK_DRAWABLE(m_window->window)));
        } else {
            m_npWindow.window = (void*)GTK_XTBIN(m_window)->xtwindow;
            ws->display = GTK_XTBIN(m_window)->xtdisplay;
            ws->visual = GTK_XTBIN(m_window)->xtclient.xtvisual;
            ws->depth = GTK_XTBIN(m_window)->xtclient.xtdepth;
            ws->colormap = GTK_XTBIN(m_window)->xtclient.xtcolormap;
        }
        XFlush (ws->display);

        m_npWindow.ws_info = ws;
#elif defined(GDK_WINDOWING_WIN32)
        m_npWindow.window = (void*)GDK_WINDOW_HWND(m_window->window);
#endif
    } else {
        m_npWindow.type = NPWindowTypeDrawable;
        m_npWindow.window = 0;
    }

    if (!(m_plugin->quirks().contains(PluginQuirkDeferFirstSetWindowCall)))
        setNPWindowRect(frameGeometry());

    m_status = PluginStatusLoadedSuccessfully;
}

} // namespace WebCore
