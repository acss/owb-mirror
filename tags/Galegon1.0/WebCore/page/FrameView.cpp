/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *           (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "FrameView.h"

#include "AXObjectCache.h"
#include "CSSStyleSelector.h"
#include "ChromeClient.h"
#include "EventHandler.h"
#include "FloatRect.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameTree.h"
#include "GraphicsContext.h"
#include "HTMLDocument.h"
#include "HTMLFrameElement.h"
#include "HTMLFrameSetElement.h"
#include "HTMLNames.h"
#include "OverflowEvent.h"
#include "Page.h"
#include "RenderPart.h"
#include "RenderPartObject.h"
#include "RenderScrollbar.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "Settings.h"
#include <wtf/CurrentTime.h>

#if USE(ACCELERATED_COMPOSITING)
#include "RenderLayerCompositor.h"
#endif

namespace WebCore {

using namespace HTMLNames;

double FrameView::sCurrentPaintTimeStamp = 0.0;

struct ScheduledEvent {
    RefPtr<Event> m_event;
    RefPtr<Node> m_eventTarget;
};

FrameView::FrameView(Frame* frame)
    : m_refCount(1)
    , m_frame(frame)
    , m_vmode(ScrollbarAuto)
    , m_hmode(ScrollbarAuto)
    , m_slowRepaintObjectCount(0)
    , m_layoutTimer(this, &FrameView::layoutTimerFired)
    , m_layoutRoot(0)
    , m_postLayoutTasksTimer(this, &FrameView::postLayoutTimerFired)
    , m_needToInitScrollbars(true)
    , m_isTransparent(false)
    , m_baseBackgroundColor(Color::white)
    , m_mediaType("screen")
    , m_enqueueEvents(0)
    , m_overflowStatusDirty(true)
    , m_viewportRenderer(0)
    , m_wasScrolledByUser(false)
    , m_inProgrammaticScroll(false)
    , m_shouldUpdateWhileOffscreen(true)
{
    init();
    show();
}

FrameView::FrameView(Frame* frame, const IntSize& initialSize)
    : m_refCount(1)
    , m_frame(frame)
    , m_vmode(ScrollbarAuto)
    , m_hmode(ScrollbarAuto)
    , m_slowRepaintObjectCount(0)
    , m_layoutTimer(this, &FrameView::layoutTimerFired)
    , m_layoutRoot(0)
    , m_postLayoutTasksTimer(this, &FrameView::postLayoutTimerFired)
    , m_needToInitScrollbars(true)
    , m_isTransparent(false)
    , m_baseBackgroundColor(Color::white)
    , m_mediaType("screen")
    , m_enqueueEvents(0)
    , m_overflowStatusDirty(true)
    , m_viewportRenderer(0)
    , m_wasScrolledByUser(false)
    , m_inProgrammaticScroll(false)
    , m_shouldUpdateWhileOffscreen(true)
{
    init();
    Widget::setFrameRect(IntRect(x(), y(), initialSize.width(), initialSize.height()));
    show();
}

FrameView::~FrameView()
{
    if (m_postLayoutTasksTimer.isActive()) {
        m_postLayoutTasksTimer.stop();
        m_scheduledEvents.clear();
        m_enqueueEvents = 0;
    }

    resetScrollbars();
    setHasHorizontalScrollbar(false); // Remove native scrollbars now before we lose the connection to the HostWindow.
    setHasVerticalScrollbar(false);
    
    ASSERT(m_refCount == 0);
    ASSERT(m_scheduledEvents.isEmpty());
    ASSERT(!m_enqueueEvents);

    if (m_frame) {
        ASSERT(m_frame->view() != this || !m_frame->document() || !m_frame->contentRenderer());
        RenderPart* renderer = m_frame->ownerRenderer();
        if (renderer && renderer->widget() == this)
            renderer->setWidget(0);
    }
}

void FrameView::reset()
{
    m_useSlowRepaints = false;
    m_borderX = 30;
    m_borderY = 30;
    m_layoutTimer.stop();
    m_layoutRoot = 0;
    m_delayedLayout = false;
    m_doFullRepaint = true;
    m_layoutSchedulingEnabled = true;
    m_midLayout = false;
    m_layoutCount = 0;
    m_nestedLayoutCount = 0;
    m_postLayoutTasksTimer.stop();
    m_firstLayout = true;
    m_firstLayoutCallbackPending = false;
    m_wasScrolledByUser = false;
    m_lastLayoutSize = IntSize();
    m_lastZoomFactor = 1.0f;
    m_deferringRepaints = 0;
    m_repaintCount = 0;
    m_repaintRect = IntRect();
    m_repaintRects.clear();
    m_paintRestriction = PaintRestrictionNone;
    m_isPainting = false;
    m_isVisuallyNonEmpty = false;
    m_firstVisuallyNonEmptyLayoutCallbackPending = true;
}

bool FrameView::isFrameView() const 
{ 
    return true; 
}

void FrameView::clearFrame()
{
    m_frame = 0;
}

void FrameView::resetScrollbars()
{
    // Reset the document's scrollbars back to our defaults before we yield the floor.
    m_firstLayout = true;
    setScrollbarsSuppressed(true);
    setScrollbarModes(m_hmode, m_vmode);
    setScrollbarsSuppressed(false);
}

void FrameView::init()
{
    reset();

    m_margins = IntSize(-1, -1); // undefined
    m_size = IntSize();

    // Propagate the marginwidth/height and scrolling modes to the view.
    Element* ownerElement = m_frame && m_frame->document() ? m_frame->document()->ownerElement() : 0;
    if (ownerElement && (ownerElement->hasTagName(frameTag) || ownerElement->hasTagName(iframeTag))) {
        HTMLFrameElement* frameElt = static_cast<HTMLFrameElement*>(ownerElement);
        if (frameElt->scrollingMode() == ScrollbarAlwaysOff)
            setCanHaveScrollbars(false);
        int marginWidth = frameElt->getMarginWidth();
        int marginHeight = frameElt->getMarginHeight();
        if (marginWidth != -1)
            setMarginWidth(marginWidth);
        if (marginHeight != -1)
            setMarginHeight(marginHeight);
    }
}

void FrameView::clear()
{
    setCanBlitOnScroll(true);
    
    reset();

    if (m_frame) {
        if (RenderPart* renderer = m_frame->ownerRenderer())
            renderer->viewCleared();
    }

    setScrollbarsSuppressed(true);
}

bool FrameView::didFirstLayout() const
{
    return !m_firstLayout;
}

void FrameView::initScrollbars()
{
    if (!m_needToInitScrollbars)
        return;
    m_needToInitScrollbars = false;
    updateDefaultScrollbarState();
}

void FrameView::updateDefaultScrollbarState()
{
    m_hmode = horizontalScrollbarMode();
    m_vmode = verticalScrollbarMode();
    setScrollbarModes(m_hmode, m_vmode);
}

void FrameView::invalidateRect(const IntRect& rect)
{
    if (!parent()) {
        if (hostWindow())
            hostWindow()->repaint(rect, true);
        return;
    }

    if (!m_frame)
        return;

    RenderPart* renderer = m_frame->ownerRenderer();
    if (!renderer)
        return;

    IntRect repaintRect = rect;
    repaintRect.move(renderer->borderLeft() + renderer->paddingLeft(),
                     renderer->borderTop() + renderer->paddingTop());
    renderer->repaintRectangle(repaintRect);
}

void FrameView::setMarginWidth(int w)
{
    // make it update the rendering area when set
    m_margins.setWidth(w);
}

void FrameView::setMarginHeight(int h)
{
    // make it update the rendering area when set
    m_margins.setHeight(h);
}

void FrameView::setCanHaveScrollbars(bool canScroll)
{
    ScrollView::setCanHaveScrollbars(canScroll);
    scrollbarModes(m_hmode, m_vmode);
}

PassRefPtr<Scrollbar> FrameView::createScrollbar(ScrollbarOrientation orientation)
{
    // FIXME: We need to update the scrollbar dynamically as documents change (or as doc elements and bodies get discovered that have custom styles).
    Document* doc = m_frame->document();
    if (!doc)
        return ScrollView::createScrollbar(orientation);

    // Try the <body> element first as a scrollbar source.
    Element* body = doc->body();
    if (body && body->renderer() && body->renderer()->style()->hasPseudoStyle(SCROLLBAR))
        return RenderScrollbar::createCustomScrollbar(this, orientation, body->renderBox());
    
    // If the <body> didn't have a custom style, then the root element might.
    Element* docElement = doc->documentElement();
    if (docElement && docElement->renderer() && docElement->renderer()->style()->hasPseudoStyle(SCROLLBAR))
        return RenderScrollbar::createCustomScrollbar(this, orientation, docElement->renderBox());
        
    // If we have an owning iframe/frame element, then it can set the custom scrollbar also.
    RenderPart* frameRenderer = m_frame->ownerRenderer();
    if (frameRenderer && frameRenderer->style()->hasPseudoStyle(SCROLLBAR))
        return RenderScrollbar::createCustomScrollbar(this, orientation, frameRenderer);
    
    // Nobody set a custom style, so we just use a native scrollbar.
    return ScrollView::createScrollbar(orientation);
}

void FrameView::setContentsSize(const IntSize& size)
{
    ScrollView::setContentsSize(size);

    Page* page = frame() ? frame()->page() : 0;
    if (!page)
        return;

    page->chrome()->contentsSizeChanged(frame(), size); //notify only
}

void FrameView::adjustViewSize()
{
    ASSERT(m_frame->view() == this);
    RenderView* root = m_frame->contentRenderer();
    if (!root)
        return;
    setContentsSize(IntSize(root->overflowWidth(), root->overflowHeight()));
}

void FrameView::applyOverflowToViewport(RenderObject* o, ScrollbarMode& hMode, ScrollbarMode& vMode)
{
    // Handle the overflow:hidden/scroll case for the body/html elements.  WinIE treats
    // overflow:hidden and overflow:scroll on <body> as applying to the document's
    // scrollbars.  The CSS2.1 draft states that HTML UAs should use the <html> or <body> element and XML/XHTML UAs should
    // use the root element.
    switch (o->style()->overflowX()) {
        case OHIDDEN:
            hMode = ScrollbarAlwaysOff;
            break;
        case OSCROLL:
            hMode = ScrollbarAlwaysOn;
            break;
        case OAUTO:
            hMode = ScrollbarAuto;
            break;
        default:
            // Don't set it at all.
            ;
    }
    
     switch (o->style()->overflowY()) {
        case OHIDDEN:
            vMode = ScrollbarAlwaysOff;
            break;
        case OSCROLL:
            vMode = ScrollbarAlwaysOn;
            break;
        case OAUTO:
            vMode = ScrollbarAuto;
            break;
        default:
            // Don't set it at all.
            ;
    }

    m_viewportRenderer = o;
}

#if USE(ACCELERATED_COMPOSITING)
void FrameView::updateCompositingLayers(CompositingUpdate updateType)
{
    RenderView* view = m_frame->contentRenderer();
    if (!view || !view->usesCompositing())
        return;

    if (updateType == ForcedCompositingUpdate)
        view->compositor()->setCompositingLayersNeedUpdate();
    
    view->compositor()->updateCompositingLayers();
}

void FrameView::setNeedsOneShotDrawingSynchronization()
{
    Page* page = frame() ? frame()->page() : 0;
    if (page)
        page->chrome()->client()->setNeedsOneShotDrawingSynchronization();
}
#endif // USE(ACCELERATED_COMPOSITING)

void FrameView::didMoveOnscreen()
{
    RenderView* view = m_frame->contentRenderer();
    if (view)
        view->didMoveOnscreen();
}

void FrameView::willMoveOffscreen()
{
    RenderView* view = m_frame->contentRenderer();
    if (view)
        view->willMoveOffscreen();
}

RenderObject* FrameView::layoutRoot(bool onlyDuringLayout) const
{
    return onlyDuringLayout && layoutPending() ? 0 : m_layoutRoot;
}

void FrameView::layout(bool allowSubtree)
{
    if (m_midLayout)
        return;

    m_layoutTimer.stop();
    m_delayedLayout = false;

    // Protect the view from being deleted during layout (in recalcStyle)
    RefPtr<FrameView> protector(this);

    if (!m_frame) {
        // FIXME: Do we need to set m_size.width here?
        // FIXME: Should we set m_size.height here too?
        m_size.setWidth(layoutWidth());
        return;
    }
    
    // we shouldn't enter layout() while painting
    ASSERT(!isPainting());
    if (isPainting())
        return;

    if (!allowSubtree && m_layoutRoot) {
        m_layoutRoot->markContainingBlocksForLayout(false);
        m_layoutRoot = 0;
    }

    ASSERT(m_frame->view() == this);
    // This early return should be removed when rdar://5598072 is resolved. In the meantime, there is a
    // gigantic CrashTracer because of this issue, and the early return will hopefully cause graceful 
    // failure instead.  
    if (m_frame->view() != this)
        return;

    Document* document = m_frame->document();
    if (!document) {
        // FIXME: Should we set m_size.height here too?
        m_size.setWidth(layoutWidth());
        return;
    }

    m_layoutSchedulingEnabled = false;

    if (!m_nestedLayoutCount && m_postLayoutTasksTimer.isActive()) {
        // This is a new top-level layout. If there are any remaining tasks from the previous
        // layout, finish them now.
        m_postLayoutTasksTimer.stop();
        performPostLayoutTasks();
    }

    // Viewport-dependent media queries may cause us to need completely different style information.
    // Check that here.
    if (document->styleSelector()->affectedByViewportChange())
        document->updateStyleSelector();

    // Always ensure our style info is up-to-date.  This can happen in situations where
    // the layout beats any sort of style recalc update that needs to occur.
    if (m_frame->needsReapplyStyles())
        m_frame->reapplyStyles();
    else if (document->hasChangedChild())
        document->recalcStyle();
    
    bool subtree = m_layoutRoot;

    // If there is only one ref to this view left, then its going to be destroyed as soon as we exit, 
    // so there's no point to continuing to layout
    if (protector->hasOneRef())
        return;

    RenderObject* root = subtree ? m_layoutRoot : document->renderer();
    if (!root) {
        // FIXME: Do we need to set m_size here?
        m_layoutSchedulingEnabled = true;
        return;
    }

    m_nestedLayoutCount++;

    ScrollbarMode hMode = m_hmode;
    ScrollbarMode vMode = m_vmode;

    if (!subtree) {
        RenderObject* rootRenderer = document->documentElement() ? document->documentElement()->renderer() : 0;
        Node* body = document->body();
        if (body && body->renderer()) {
            if (body->hasTagName(framesetTag)) {
                body->renderer()->setChildNeedsLayout(true);
                vMode = ScrollbarAlwaysOff;
                hMode = ScrollbarAlwaysOff;
            } else if (body->hasTagName(bodyTag)) {
                if (!m_firstLayout && m_size.height() != layoutHeight()
                        && toRenderBox(body->renderer())->stretchesToViewHeight())
                    body->renderer()->setChildNeedsLayout(true);
                // It's sufficient to just check the X overflow,
                // since it's illegal to have visible in only one direction.
                RenderObject* o = rootRenderer->style()->overflowX() == OVISIBLE && document->documentElement()->hasTagName(htmlTag) ? body->renderer() : rootRenderer;
                applyOverflowToViewport(o, hMode, vMode);
            }
        } else if (rootRenderer)
            applyOverflowToViewport(rootRenderer, hMode, vMode);
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
        if (m_firstLayout && !document->ownerElement())
            printf("Elapsed time before first layout: %d\n", document->elapsedTime());
#endif
    }

    m_doFullRepaint = !subtree && (m_firstLayout || toRenderView(root)->printing());

    if (!subtree) {
        // Now set our scrollbar state for the layout.
        ScrollbarMode currentHMode = horizontalScrollbarMode();
        ScrollbarMode currentVMode = verticalScrollbarMode();

        if (m_firstLayout || (hMode != currentHMode || vMode != currentVMode)) {
            setScrollbarsSuppressed(true);
            if (m_firstLayout) {
                m_firstLayout = false;
                m_firstLayoutCallbackPending = true;
                m_lastLayoutSize = IntSize(width(), height());
                m_lastZoomFactor = root->style()->zoom();

                // Set the initial vMode to AlwaysOn if we're auto.
                if (vMode == ScrollbarAuto)
                    setVerticalScrollbarMode(ScrollbarAlwaysOn); // This causes a vertical scrollbar to appear.
                // Set the initial hMode to AlwaysOff if we're auto.
                if (hMode == ScrollbarAuto)
                    setHorizontalScrollbarMode(ScrollbarAlwaysOff); // This causes a horizontal scrollbar to disappear.
            }
            setScrollbarModes(hMode, vMode);
            setScrollbarsSuppressed(false, true);
        }

        IntSize oldSize = m_size;

        m_size = IntSize(layoutWidth(), layoutHeight());

        if (oldSize != m_size)
            m_doFullRepaint = true;
    }

    RenderLayer* layer = root->enclosingLayer();

    pauseScheduledEvents();

    if (subtree)
        root->view()->pushLayoutState(root);
        
    m_midLayout = true;
    beginDeferredRepaints();
    root->layout();
    endDeferredRepaints();
    m_midLayout = false;

    if (subtree)
        root->view()->popLayoutState();
    m_layoutRoot = 0;

    m_frame->invalidateSelection();
   
    m_layoutSchedulingEnabled = true;

    if (!subtree && !toRenderView(root)->printing())
        adjustViewSize();

    // Now update the positions of all layers.
    beginDeferredRepaints();
    layer->updateLayerPositions(m_doFullRepaint);
    endDeferredRepaints();

#if USE(ACCELERATED_COMPOSITING)
    updateCompositingLayers();
#endif
    
    m_layoutCount++;

#if PLATFORM(MAC)
    if (AXObjectCache::accessibilityEnabled())
        root->document()->axObjectCache()->postNotificationToElement(root, "AXLayoutComplete");
#endif
#if ENABLE(DASHBOARD_SUPPORT)
    updateDashboardRegions();
#endif

    ASSERT(!root->needsLayout());

    setCanBlitOnScroll(!useSlowRepaints());

    if (document->hasListenerType(Document::OVERFLOWCHANGED_LISTENER))
        updateOverflowStatus(layoutWidth() < contentsWidth(),
                             layoutHeight() < contentsHeight());

    if (!m_postLayoutTasksTimer.isActive()) {
        // Calls resumeScheduledEvents()
        performPostLayoutTasks();

        if (needsLayout()) {
            // Post-layout widget updates or an event handler made us need layout again.
            // Lay out again, but this time defer widget updates and event dispatch until after
            // we return.
            m_postLayoutTasksTimer.startOneShot(0);
            pauseScheduledEvents();
            layout();
        }
    } else {
        resumeScheduledEvents();
        ASSERT(m_enqueueEvents);
    }

    m_nestedLayoutCount--;
}

void FrameView::addWidgetToUpdate(RenderPartObject* object)
{
    if (!m_widgetUpdateSet)
        m_widgetUpdateSet.set(new HashSet<RenderPartObject*>);

    m_widgetUpdateSet->add(object);
}

void FrameView::removeWidgetToUpdate(RenderPartObject* object)
{
    if (!m_widgetUpdateSet)
        return;

    m_widgetUpdateSet->remove(object);
}

void FrameView::setMediaType(const String& mediaType)
{
    m_mediaType = mediaType;
}

String FrameView::mediaType() const
{
    // See if we have an override type.
    String overrideType = m_frame->loader()->client()->overrideMediaType();
    if (!overrideType.isNull())
        return overrideType;
    return m_mediaType;
}

bool FrameView::useSlowRepaints() const
{
    return m_useSlowRepaints || m_slowRepaintObjectCount > 0;
}

void FrameView::setUseSlowRepaints()
{
    m_useSlowRepaints = true;
    setCanBlitOnScroll(false);
}

void FrameView::addSlowRepaintObject()
{
    if (!m_slowRepaintObjectCount)
        setCanBlitOnScroll(false);
    m_slowRepaintObjectCount++;
}

void FrameView::removeSlowRepaintObject()
{
    ASSERT(m_slowRepaintObjectCount > 0);
    m_slowRepaintObjectCount--;
    if (!m_slowRepaintObjectCount)
        setCanBlitOnScroll(!m_useSlowRepaints);
}

void FrameView::restoreScrollbar()
{
    setScrollbarsSuppressed(false);
}

void FrameView::scrollRectIntoViewRecursively(const IntRect& r)
{
    bool wasInProgrammaticScroll = m_inProgrammaticScroll;
    m_inProgrammaticScroll = true;
    ScrollView::scrollRectIntoViewRecursively(r);
    m_inProgrammaticScroll = wasInProgrammaticScroll;
}

void FrameView::setScrollPosition(const IntPoint& scrollPoint)
{
    bool wasInProgrammaticScroll = m_inProgrammaticScroll;
    m_inProgrammaticScroll = true;
    ScrollView::setScrollPosition(scrollPoint);
    m_inProgrammaticScroll = wasInProgrammaticScroll;
}

HostWindow* FrameView::hostWindow() const
{
    Page* page = frame() ? frame()->page() : 0;
    if (!page)
        return 0;
    return page->chrome();
}

const unsigned cRepaintRectUnionThreshold = 25;

void FrameView::repaintContentRectangle(const IntRect& r, bool immediate)
{
    ASSERT(!m_frame->document()->ownerElement());

    if (m_deferringRepaints && !immediate) {
        IntRect visibleContent = visibleContentRect();
        visibleContent.intersect(r);
        if (!visibleContent.isEmpty()) {
            m_repaintCount++;
            m_repaintRect.unite(r);
            if (m_repaintCount == cRepaintRectUnionThreshold)
                m_repaintRects.clear();
            else if (m_repaintCount < cRepaintRectUnionThreshold)
                m_repaintRects.append(r);
        }
        return;
    }
    
    if (!immediate && isOffscreen() && !shouldUpdateWhileOffscreen())
        return;

    ScrollView::repaintContentRectangle(r, immediate);
}

void FrameView::beginDeferredRepaints()
{
    Page* page = m_frame->page();
    if (page->mainFrame() != m_frame)
        return page->mainFrame()->view()->beginDeferredRepaints();

    m_deferringRepaints++;
    m_repaintCount = 0;
    m_repaintRect = IntRect();
    m_repaintRects.clear();
}


void FrameView::endDeferredRepaints()
{
    Page* page = m_frame->page();
    if (page->mainFrame() != m_frame)
        return page->mainFrame()->view()->endDeferredRepaints();

    ASSERT(m_deferringRepaints > 0);
    if (--m_deferringRepaints == 0) {
        if (m_repaintCount >= cRepaintRectUnionThreshold)
            repaintContentRectangle(m_repaintRect, false);
        else {
            unsigned size = m_repaintRects.size();
            for (unsigned i = 0; i < size; i++)
                repaintContentRectangle(m_repaintRects[i], false);
            m_repaintRects.clear();
        }
    }
}

void FrameView::layoutTimerFired(Timer<FrameView>*)
{
#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (m_frame->document() && !m_frame->document()->ownerElement())
        printf("Layout timer fired at %d\n", m_frame->document()->elapsedTime());
#endif
    layout();
}

void FrameView::scheduleRelayout()
{
    ASSERT(!m_frame->document() || !m_frame->document()->inPageCache());
    ASSERT(m_frame->view() == this);

    if (m_layoutRoot) {
        m_layoutRoot->markContainingBlocksForLayout(false);
        m_layoutRoot = 0;
    }
    if (!m_layoutSchedulingEnabled)
        return;
    if (!needsLayout())
        return;
    if (!m_frame->document() || !m_frame->document()->shouldScheduleLayout())
        return;

    int delay = m_frame->document()->minimumLayoutDelay();
    if (m_layoutTimer.isActive() && m_delayedLayout && !delay)
        unscheduleRelayout();
    if (m_layoutTimer.isActive())
        return;

    m_delayedLayout = delay != 0;

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (!m_frame->document()->ownerElement())
        printf("Scheduling layout for %d\n", delay);
#endif

    m_layoutTimer.startOneShot(delay * 0.001);
}

static bool isObjectAncestorContainerOf(RenderObject* ancestor, RenderObject* descendant)
{
    for (RenderObject* r = descendant; r; r = r->container()) {
        if (r == ancestor)
            return true;
    }
    return false;
}

void FrameView::scheduleRelayoutOfSubtree(RenderObject* relayoutRoot)
{
    ASSERT(m_frame->view() == this);

    if (!m_layoutSchedulingEnabled || (m_frame->contentRenderer()
            && m_frame->contentRenderer()->needsLayout())) {
        if (relayoutRoot)
            relayoutRoot->markContainingBlocksForLayout(false);
        return;
    }

    if (layoutPending()) {
        if (m_layoutRoot != relayoutRoot) {
            if (isObjectAncestorContainerOf(m_layoutRoot, relayoutRoot)) {
                // Keep the current root
                relayoutRoot->markContainingBlocksForLayout(false, m_layoutRoot);
            } else if (m_layoutRoot && isObjectAncestorContainerOf(relayoutRoot, m_layoutRoot)) {
                // Re-root at relayoutRoot
                m_layoutRoot->markContainingBlocksForLayout(false, relayoutRoot);
                m_layoutRoot = relayoutRoot;
            } else {
                // Just do a full relayout
                if (m_layoutRoot)
                    m_layoutRoot->markContainingBlocksForLayout(false);
                m_layoutRoot = 0;
                relayoutRoot->markContainingBlocksForLayout(false);
            }
        }
    } else {
        int delay = m_frame->document()->minimumLayoutDelay();
        m_layoutRoot = relayoutRoot;
        m_delayedLayout = delay != 0;
        m_layoutTimer.startOneShot(delay * 0.001);
    }
}

bool FrameView::layoutPending() const
{
    return m_layoutTimer.isActive();
}

bool FrameView::needsLayout() const
{
    // This can return true in cases where the document does not have a body yet.
    // Document::shouldScheduleLayout takes care of preventing us from scheduling
    // layout in that case.
    if (!m_frame)
        return false;
    RenderView* root = m_frame->contentRenderer();
    Document* document = m_frame->document();
    return layoutPending()
        || (root && root->needsLayout())
        || m_layoutRoot
        || (document && document->hasChangedChild()) // can occur when using WebKit ObjC interface
        || m_frame->needsReapplyStyles();
}

void FrameView::setNeedsLayout()
{
    RenderView* root = m_frame->contentRenderer();
    if (root)
        root->setNeedsLayout(true);
}

void FrameView::unscheduleRelayout()
{
    if (!m_layoutTimer.isActive())
        return;

#ifdef INSTRUMENT_LAYOUT_SCHEDULING
    if (m_frame->document() && !m_frame->document()->ownerElement())
        printf("Layout timer unscheduled at %d\n", m_frame->document()->elapsedTime());
#endif
    
    m_layoutTimer.stop();
    m_delayedLayout = false;
}

bool FrameView::isTransparent() const
{
    return m_isTransparent;
}

void FrameView::setTransparent(bool isTransparent)
{
    m_isTransparent = isTransparent;
}

Color FrameView::baseBackgroundColor() const
{
    return m_baseBackgroundColor;
}

void FrameView::setBaseBackgroundColor(Color bc)
{
    if (!bc.isValid())
        bc = Color::white;
    m_baseBackgroundColor = bc;
}

void FrameView::updateBackgroundRecursively(const Color& backgroundColor, bool transparent)
{
    for (Frame* frame = m_frame.get(); frame; frame = frame->tree()->traverseNext(m_frame.get())) {
        FrameView* view = frame->view();
        if (!view)
            continue;

        view->setTransparent(transparent);
        view->setBaseBackgroundColor(backgroundColor);
    }
}

bool FrameView::shouldUpdateWhileOffscreen() const
{
    return m_shouldUpdateWhileOffscreen;
}

void FrameView::setShouldUpdateWhileOffscreen(bool shouldUpdateWhileOffscreen)
{
    m_shouldUpdateWhileOffscreen = shouldUpdateWhileOffscreen;
}

void FrameView::scheduleEvent(PassRefPtr<Event> event, PassRefPtr<Node> eventTarget)
{
    if (!m_enqueueEvents) {
        ExceptionCode ec = 0;
        eventTarget->dispatchEvent(event, ec);
        return;
    }

    ScheduledEvent* scheduledEvent = new ScheduledEvent;
    scheduledEvent->m_event = event;
    scheduledEvent->m_eventTarget = eventTarget;
    m_scheduledEvents.append(scheduledEvent);
}

void FrameView::pauseScheduledEvents()
{
    ASSERT(m_scheduledEvents.isEmpty() || m_enqueueEvents);
    m_enqueueEvents++;
}

void FrameView::resumeScheduledEvents()
{
    m_enqueueEvents--;
    if (!m_enqueueEvents)
        dispatchScheduledEvents();
    ASSERT(m_scheduledEvents.isEmpty() || m_enqueueEvents);
}

void FrameView::performPostLayoutTasks()
{
    if (m_firstLayoutCallbackPending) {
        m_firstLayoutCallbackPending = false;
        m_frame->loader()->didFirstLayout();
    }

    if (m_isVisuallyNonEmpty && m_firstVisuallyNonEmptyLayoutCallbackPending) {
        m_firstVisuallyNonEmptyLayoutCallbackPending = false;
        m_frame->loader()->didFirstVisuallyNonEmptyLayout();
    }

    RenderView* root = m_frame->contentRenderer();

    root->updateWidgetPositions();
    if (m_widgetUpdateSet && m_nestedLayoutCount <= 1) {
        Vector<RenderPartObject*> objectVector;
        copyToVector(*m_widgetUpdateSet, objectVector);
        size_t size = objectVector.size();
        for (size_t i = 0; i < size; ++i) {
            RenderPartObject* object = objectVector[i];
            object->updateWidget(false);

            // updateWidget() can destroy the RenderPartObject, so we need to make sure it's
            // alive by checking if it's still in m_widgetUpdateSet.
            if (m_widgetUpdateSet->contains(object))
                object->updateWidgetPosition();
        }
        m_widgetUpdateSet->clear();
    }

    resumeScheduledEvents();

    if (!root->printing()) {
        IntSize currentSize = IntSize(width(), height());
        float currentZoomFactor = root->style()->zoom();
        bool resized = !m_firstLayout && (currentSize != m_lastLayoutSize || currentZoomFactor != m_lastZoomFactor);
        m_lastLayoutSize = currentSize;
        m_lastZoomFactor = currentZoomFactor;
        if (resized)
            m_frame->eventHandler()->sendResizeEvent();
    }
}

void FrameView::postLayoutTimerFired(Timer<FrameView>*)
{
    performPostLayoutTasks();
}

void FrameView::updateOverflowStatus(bool horizontalOverflow, bool verticalOverflow)
{
    if (!m_viewportRenderer)
        return;
    
    if (m_overflowStatusDirty) {
        m_horizontalOverflow = horizontalOverflow;
        m_verticalOverflow = verticalOverflow;
        m_overflowStatusDirty = false;
        return;
    }
    
    bool horizontalOverflowChanged = (m_horizontalOverflow != horizontalOverflow);
    bool verticalOverflowChanged = (m_verticalOverflow != verticalOverflow);
    
    if (horizontalOverflowChanged || verticalOverflowChanged) {
        m_horizontalOverflow = horizontalOverflow;
        m_verticalOverflow = verticalOverflow;
        
        scheduleEvent(OverflowEvent::create(horizontalOverflowChanged, horizontalOverflow,
            verticalOverflowChanged, verticalOverflow),
            m_viewportRenderer->element());
    }
    
}

void FrameView::dispatchScheduledEvents()
{
    if (m_scheduledEvents.isEmpty())
        return;

    Vector<ScheduledEvent*> scheduledEventsCopy = m_scheduledEvents;
    m_scheduledEvents.clear();
    
    Vector<ScheduledEvent*>::iterator end = scheduledEventsCopy.end();
    for (Vector<ScheduledEvent*>::iterator it = scheduledEventsCopy.begin(); it != end; ++it) {
        ScheduledEvent* scheduledEvent = *it;
        
        ExceptionCode ec = 0;
        
        // Only dispatch events to nodes that are in the document
        if (scheduledEvent->m_eventTarget->inDocument())
            scheduledEvent->m_eventTarget->dispatchEvent(scheduledEvent->m_event, ec);
        
        delete scheduledEvent;
    }
}

IntRect FrameView::windowClipRect(bool clipToContents) const
{
    ASSERT(m_frame->view() == this);

    // Set our clip rect to be our contents.
    IntRect clipRect = contentsToWindow(visibleContentRect(!clipToContents));
    if (!m_frame || !m_frame->document() || !m_frame->document()->ownerElement())
        return clipRect;

    // Take our owner element and get the clip rect from the enclosing layer.
    Element* elt = m_frame->document()->ownerElement();
    RenderLayer* layer = elt->renderer()->enclosingLayer();
    // FIXME: layer should never be null, but sometimes seems to be anyway.
    if (!layer)
        return clipRect;
    FrameView* parentView = elt->document()->view();
    clipRect.intersect(parentView->windowClipRectForLayer(layer, true));
    return clipRect;
}

IntRect FrameView::windowClipRectForLayer(const RenderLayer* layer, bool clipToLayerContents) const
{
    // If we have no layer, just return our window clip rect.
    if (!layer)
        return windowClipRect();

    // Apply the clip from the layer.
    IntRect clipRect;
    if (clipToLayerContents)
        clipRect = layer->childrenClipRect();
    else
        clipRect = layer->selfClipRect();
    clipRect = contentsToWindow(clipRect); 
    return intersection(clipRect, windowClipRect());
}

bool FrameView::isActive() const
{
    Page* page = frame()->page();
    return page && page->focusController()->isActive();
}

void FrameView::valueChanged(Scrollbar* bar)
{
    // Figure out if we really moved.
    IntSize offset = scrollOffset();
    ScrollView::valueChanged(bar);
    if (offset != scrollOffset())
        frame()->eventHandler()->sendScrollEvent();
}

void FrameView::invalidateScrollbarRect(Scrollbar* scrollbar, const IntRect& rect)
{
    // Add in our offset within the FrameView.
    IntRect dirtyRect = rect;
    dirtyRect.move(scrollbar->x(), scrollbar->y());
    invalidateRect(dirtyRect);
}

void FrameView::getTickmarks(Vector<IntRect>& tickmarks) const
{
    tickmarks = frame()->document()->renderedRectsForMarkers(DocumentMarker::TextMatch);
}

IntRect FrameView::windowResizerRect() const
{
    Page* page = frame() ? frame()->page() : 0;
    if (!page)
        return IntRect();
    return page->chrome()->windowResizerRect();
}

#if ENABLE(DASHBOARD_SUPPORT)
void FrameView::updateDashboardRegions()
{
    Document* document = m_frame->document();
    if (!document->hasDashboardRegions())
        return;
    Vector<DashboardRegionValue> newRegions;
    document->renderBox()->collectDashboardRegions(newRegions);
    if (newRegions == document->dashboardRegions())
        return;
    document->setDashboardRegions(newRegions);
    Page* page = m_frame->page();
    if (!page)
        return;
    page->chrome()->client()->dashboardRegionsChanged();
}
#endif

void FrameView::updateControlTints()
{
    // This is called when control tints are changed from aqua/graphite to clear and vice versa.
    // We do a "fake" paint, and when the theme gets a paint call, it can then do an invalidate.
    // This is only done if the theme supports control tinting. It's up to the theme and platform
    // to define when controls get the tint and to call this function when that changes.
    
    // Optimize the common case where we bring a window to the front while it's still empty.
    if (!m_frame || m_frame->loader()->url().isEmpty()) 
        return;
    
    if (theme()->supportsControlTints() && m_frame->contentRenderer()) {
        if (needsLayout())
            layout();
        PlatformGraphicsContext* const noContext = 0;
        GraphicsContext context(noContext);
        context.setUpdatingControlTints(true);
        if (platformWidget())
            paintContents(&context, visibleContentRect());
        else
            paint(&context, frameRect());
    }
}

bool FrameView::wasScrolledByUser() const
{
    return m_wasScrolledByUser;
}

void FrameView::setWasScrolledByUser(bool wasScrolledByUser)
{
    if (m_inProgrammaticScroll)
        return;
    m_wasScrolledByUser = wasScrolledByUser;
}

void FrameView::paintContents(GraphicsContext* p, const IntRect& rect)
{
    if (!frame())
        return;
    
    Document* document = frame()->document();
    if (!document)
        return;

#ifndef NDEBUG
    bool fillWithRed;
    if (document || document->printing())
        fillWithRed = false; // Printing, don't fill with red (can't remember why).
    else if (document->ownerElement())
        fillWithRed = false; // Subframe, don't fill with red.
    else if (isTransparent())
        fillWithRed = false; // Transparent, don't fill with red.
    else if (m_paintRestriction == PaintRestrictionSelectionOnly || m_paintRestriction == PaintRestrictionSelectionOnlyBlackText)
        fillWithRed = false; // Selections are transparent, don't fill with red.
    else if (m_nodeToDraw)
        fillWithRed = false; // Element images are transparent, don't fill with red.
    else
        fillWithRed = true;
    
    if (fillWithRed)
        p->fillRect(rect, Color(0xFF, 0, 0));
#endif

    bool isTopLevelPainter = !sCurrentPaintTimeStamp;
    if (isTopLevelPainter)
        sCurrentPaintTimeStamp = currentTime();
    
    RenderView* contentRenderer = frame()->contentRenderer();
    if (!contentRenderer) {
        LOG_ERROR("called Frame::paint with nil renderer");
        return;
    }

    ASSERT(!needsLayout());
    ASSERT(!m_isPainting);
        
    m_isPainting = true;
        
    // m_nodeToDraw is used to draw only one element (and its descendants)
    RenderObject* eltRenderer = m_nodeToDraw ? m_nodeToDraw->renderer() : 0;
    if (m_paintRestriction == PaintRestrictionNone)
        document->invalidateRenderedRectsForMarkersInRect(rect);
    contentRenderer->layer()->paint(p, rect, m_paintRestriction, eltRenderer);
        
    m_isPainting = false;

#if ENABLE(DASHBOARD_SUPPORT)
    // Regions may have changed as a result of the visibility/z-index of element changing.
    if (document->dashboardRegionsDirty())
        updateDashboardRegions();
#endif

    if (isTopLevelPainter)
        sCurrentPaintTimeStamp = 0;
}

void FrameView::setPaintRestriction(PaintRestriction pr)
{
    m_paintRestriction = pr;
}
    
bool FrameView::isPainting() const
{
    return m_isPainting;
}

void FrameView::setNodeToDraw(Node* node)
{
    m_nodeToDraw = node;
}

void FrameView::layoutIfNeededRecursive()
{
    // We have to crawl our entire tree looking for any FrameViews that need
    // layout and make sure they are up to date.
    // Mac actually tests for intersection with the dirty region and tries not to
    // update layout for frames that are outside the dirty region.  Not only does this seem
    // pointless (since those frames will have set a zero timer to layout anyway), but
    // it is also incorrect, since if two frames overlap, the first could be excluded from the dirty
    // region but then become included later by the second frame adding rects to the dirty region
    // when it lays out.

    if (needsLayout())
        layout();

    const HashSet<Widget*>* viewChildren = children();
    HashSet<Widget*>::const_iterator end = viewChildren->end();
    for (HashSet<Widget*>::const_iterator current = viewChildren->begin(); current != end; ++current)
        if ((*current)->isFrameView())
            static_cast<FrameView*>(*current)->layoutIfNeededRecursive();
}

void FrameView::forceLayout(bool allowSubtree)
{
    layout(allowSubtree);
    // We cannot unschedule a pending relayout, since the force can be called with
    // a tiny rectangle from a drawRect update.  By unscheduling we in effect
    // "validate" and stop the necessary full repaint from occurring.  Basically any basic
    // append/remove DHTML is broken by this call.  For now, I have removed the optimization
    // until we have a better invalidation stategy. -dwh
    //unscheduleRelayout();
}

void FrameView::forceLayoutWithPageWidthRange(float minPageWidth, float maxPageWidth, bool _adjustViewSize)
{
    // Dumping externalRepresentation(m_frame->renderer()).ascii() is a good trick to see
    // the state of things before and after the layout
    RenderView *root = toRenderView(m_frame->document()->renderer());
    if (root) {
        // This magic is basically copied from khtmlview::print
        int pageW = (int)ceilf(minPageWidth);
        root->setWidth(pageW);
        root->setNeedsLayoutAndPrefWidthsRecalc();
        forceLayout();

        // If we don't fit in the minimum page width, we'll lay out again. If we don't fit in the
        // maximum page width, we will lay out to the maximum page width and clip extra content.
        // FIXME: We are assuming a shrink-to-fit printing implementation.  A cropping
        // implementation should not do this!
        int rightmostPos = root->rightmostPosition();
        if (rightmostPos > minPageWidth) {
            pageW = std::min(rightmostPos, (int)ceilf(maxPageWidth));
            root->setWidth(pageW);
            root->setNeedsLayoutAndPrefWidthsRecalc();
            forceLayout();
        }
    }

    if (_adjustViewSize)
        adjustViewSize();
}

void FrameView::adjustPageHeight(float *newBottom, float oldTop, float oldBottom, float /*bottomLimit*/)
{
    RenderView* root = m_frame->contentRenderer();
    if (root) {
        // Use a context with painting disabled.
        GraphicsContext context((PlatformGraphicsContext*)0);
        root->setTruncatedAt((int)floorf(oldBottom));
        IntRect dirtyRect(0, (int)floorf(oldTop), root->docWidth(), (int)ceilf(oldBottom - oldTop));
        root->layer()->paint(&context, dirtyRect);
        *newBottom = root->bestTruncatedAt();
        if (*newBottom == 0)
            *newBottom = oldBottom;
    } else
        *newBottom = oldBottom;
}

} // namespace WebCore
