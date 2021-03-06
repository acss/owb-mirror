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

#include <WebKit.h>
#include "SDL.h"
#include "signal.h"
#include <unistd.h>

using namespace WebCore;
static WebView* webView;
static SDL_Surface* s_screen = NULL;
static bool quit = false;

void signalCatcher(int signum)
{
    // BEFORE YOU ABORT QUIT SDL.
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    SDL_Quit();
    abort();
}

// Used for quitting OWB when receiving a signal.
void signalQuitCatcher(int /*signum*/)
{
    quit = true;
}

SDL_Surface* createSDLWindow(int w, int h)
{
    const SDL_VideoInfo* vi;
    int flags = SDL_RESIZABLE;

    vi = SDL_GetVideoInfo();
    if (vi && vi->wm_available) /* Change title */
        SDL_WM_SetCaption("Origyn Web Browser", "Origyn Web Browser");
    if (vi && vi->hw_available) /* Double buffering will not be set by SDL if not supported */
        flags |= SDL_HWSURFACE | SDL_DOUBLEBUF;

    SDL_Surface *screen;
    screen = SDL_SetVideoMode(w, h, 32, flags);
    if ( screen == NULL ) {
        fprintf(stderr, "Unable to create screen : %s", SDL_GetError());
        exit(1);
    }
    if (SDL_InitSubSystem(SDL_INIT_EVENTTHREAD) < 0) {
        fprintf(stderr, "Unable to init SDL: %s", SDL_GetError());
        exit(1);
    } else {
        SDL_EnableUNICODE(1);
        if (0 != SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL))
            fprintf(stderr, "SDL can't enable key repeat\n");
    }
    if (SDL_InitSubSystem(SDL_INIT_TIMER) < 0) {
        fprintf(stderr, "Unable to init SDL: %s", SDL_GetError());
        exit(1);
    }

    if (getenv("SDL_NOMOUSE"))
        SDL_ShowCursor(SDL_DISABLE);

    return screen;
}

int eventFilter(const SDL_Event* event)
{
    //printf("eventFilter\n");
    switch (event->type) {
        case SDL_VIDEOEXPOSE:
            //if(!isExposed) {
                //printf("--------------------video expose\n");
              //  webView->onExpose(event->expose);
              //  isExposed = true;
            //}
            break;
        default:
            ;
    }
    return 1;
}

void handleKeyDownEvent(const SDL_KeyboardEvent& event)
{
    switch (event.keysym.sym) {
    case SDLK_DOWN:
        webView->scrollWithDirection(Down);
        break;
    case SDLK_UP:
        webView->scrollWithDirection(Up);
        break;
    case SDLK_RIGHT:
        webView->scrollWithDirection(Right);
        break;
    case SDLK_LEFT:
        webView->scrollWithDirection(Left);
        break;
    case SDLK_PAGEDOWN:
    {
        SDL_Rect frameRect = webView->frameRect();
        BalPoint offset = { 0, frameRect.h };
        webView->scrollBy(offset);
        break;
    }
    case SDLK_PAGEUP:
    {
        SDL_Rect frameRect = webView->frameRect();
        BalPoint offset = { 0, -frameRect.h };
        webView->scrollBy(offset);
        break;
    }
    case SDLK_ESCAPE:
        quit = true;
        break;
    case SDLK_F1:
        webView->goBack();
        break;
    case SDLK_F2:
        webView->goForward();
        break;
    case SDLK_F3:
        // We ignore the return value as we are not interested
        // in whether the zoom occurred.
        webView->zoomPageIn();
        break;
    case SDLK_F4:
        // Same comment as above.
        webView->zoomPageOut();
        break;
    default:
        webView->onKeyUp(event);
        break;
    }
}

void waitEvent()
{
    SDL_Event event;

    SDL_GetEventFilter();
    SDL_Surface* scr;
    while (!quit) {
        //printf("waitEvent \n");
        if (SDL_WaitEvent(&event) != 0)
        {
            switch (event.type) {
                case SDL_ACTIVEEVENT:
                    //printf("gain =%d state = %d\n", event.active.gain, event.active.state);
                    if(event.active.gain == 1 && event.active.state == 2) {
                        SDL_Rect rect = {0, 0, 1280, 720};
                        webView->addToDirtyRegion(rect);
                    }
                    SDL_ExposeEvent ev;
                    webView->onExpose(ev);
                    break;
                case SDL_KEYDOWN:
                    //printf("the key is %s \n", SDL_GetKeyName(event.key.keysym.sym));
                    if (event.key.keysym.sym == SDLK_ESCAPE)
                        quit = true;
                    else if (!webView->onKeyDown(event.key))
                        handleKeyDownEvent(event.key);
                    break;
                case SDL_KEYUP:
                    //printf("La touche %s a été relachée!\n", SDL_GetKeyName(event.key.keysym.sym));
                    webView->onKeyUp(event.key);
                    break;
                case SDL_MOUSEMOTION:
                    //printf("mouse motion\n");
                    webView->onMouseMotion(event.motion);
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    //printf("bouton mouse down\n");
                    if(event.button.button == SDL_BUTTON_WHEELUP || event.button.button == SDL_BUTTON_WHEELDOWN)
                        webView->onScroll(event.button);
                    else
                        webView->onMouseButtonDown(event.button);
                    break;
                case SDL_MOUSEBUTTONUP:
                    //printf("button mouse up\n");
                    if(event.button.button == SDL_BUTTON_WHEELUP || event.button.button == SDL_BUTTON_WHEELDOWN) {
                        webView->onScroll(event.button);
                    }
                    else
                        webView->onMouseButtonUp(event.button);
                    break;
                case SDL_VIDEORESIZE:
                    //printf("video resize\n");
                    scr = createSDLWindow(event.resize.w, event.resize.h);
                    webView->setViewWindow(scr);
                    webView->onResize(event.resize);
                    SDL_FreeSurface(s_screen);
                    s_screen = scr;
                    break;
                case SDL_VIDEOEXPOSE:
                    //printf("#######################video expose\n");
                    webView->onExpose(event.expose);
                    break;
                case SDL_USEREVENT:
                    //printf("user event\n");
                    if (event.user.code == SDLUserEventCode_Timer)
                        webView->fireWebKitTimerEvents();
                    else if (event.user.code == SDLUserEventCode_Thread)
                        webView->fireWebKitThreadEvents();
                    else
                        webView->onUserEvent(event.user);
                    break;
                case SDL_SYSWMEVENT:
                    //printf("sys wm event\n");
                    break;
                case SDL_QUIT:
                    //printf("SDL_QUIT\n");
                    quit = true;
                    break;
                default:
                    ;//printf("other event\n");
            }
        }
    }
}

void usage()
{
    printf("owb [-c tokenizerChunkSize -d tokenizerDelay -f configFile] [url_to_load]\n");
}

// Used to print the notifications.
class MainWebNotificationDelegate : public WebNotificationDelegate {

public:
    static TransferSharedPtr<MainWebNotificationDelegate> createInstance()
    {
        return new MainWebNotificationDelegate();
    }

    ~MainWebNotificationDelegate()
    {
    }

    virtual void startLoadNotification(WebFrame*)
    {
    }

    virtual void progressNotification(WebFrame* webFrame)
    {
        printf("progress : %d \n", int(webFrame->webView()->estimatedProgress() * 100));
    }

    virtual void finishedLoadNotification(WebFrame*)
    {
    }

private:
    MainWebNotificationDelegate()
    {
    }
};

class MainJSActionDelegate : public JSActionDelegate {

public:
    static TransferSharedPtr<MainJSActionDelegate> createInstance()
    {
        return new MainJSActionDelegate();
    }
    ~MainJSActionDelegate() { }

    virtual void windowObjectClearNotification(WebFrame*, void*, void*)
    {
    }

    virtual void consoleMessage(WebFrame*, int line, const char *message)
    {
        printf("CONSOLE MESSAGE: line %d: %s\n", line, message);
    }

    virtual bool jsAlert(WebFrame *frame, const char *message)
    {
#if PLATFORM(SDL)
        WebWindowAlert* alert = WebWindowAlert::createWebWindowAlert(true, frame->webView(), message);
        alert->show();
        delete alert; 
#endif
        return true;
    }

    virtual bool jsConfirm(WebFrame *frame, const char *message)
    {
#if PLATFORM(SDL)
        WebWindowConfirm* confirm = WebWindowConfirm::createWebWindowConfirm(true, message, frame->webView());
        confirm->show();
        bool val = confirm->value();;
        delete confirm;
        return val;
#else
        return false;
#endif
    }

    virtual bool jsPrompt(WebFrame *frame, const char *message, const char *defaultValue, char **value)
    {
#if PLATFORM(SDL)
        WebWindowPrompt* prompt = WebWindowPrompt::createWebWindowPrompt(true, message, defaultValue, frame->webView());
        prompt->show();
        const char *val = prompt->value();
        delete prompt;
        if (val) {
            *value = strdup(val);
            return true;
        } else
            return false;
#else
        return false;
#endif
    }

    virtual void exceededDatabaseQuota(WebFrame *frame, WebSecurityOrigin *origin, const char* databaseIdentifier)
    {
        static const unsigned long long defaultQuota = 5 * 1024 * 1024;
        origin->setQuota(defaultQuota);
    }

private:
    MainJSActionDelegate() { }
};

class MainDownloadDelegate : public WebDownloadDelegate {
public:
    static TransferSharedPtr<MainDownloadDelegate> createInstance()
    {
        return new MainDownloadDelegate();
    }

    ~MainDownloadDelegate()
    {
    }

    void decideDestinationWithSuggestedFilename(WebDownload *download, const char* filename)
    {
        printf("MainDownloadDelegate::decideDestinationWithSuggestedFilename(%p, %s)\n", download, filename);
        char destination[1024];
        snprintf(destination, sizeof(destination), "/tmp/%s", filename);
        download->setDestination(destination, false);
    }

    void didCancelAuthenticationChallenge(WebDownload* download, WebURLAuthenticationChallenge* challenge)
    {
        printf("MainDownloadDelegate::didCancelAuthenticationChallenge(%p, %p)\n", download, challenge);
    }

    void didCreateDestination(WebDownload* download, const char* destination)
    {
        printf("MainDownloadDelegate::didCreateDestination(%p, %s)\n", download, destination);
    }

    void didReceiveAuthenticationChallenge(WebDownload* download, WebURLAuthenticationChallenge* challenge)
    {
        printf("MainDownloadDelegate::didReceiveAuthenticationChallenge(%p, %p)\n", download, challenge);
    }

    void didReceiveDataOfLength(WebDownload* download, unsigned length)
    {
        printf("MainDownloadDelegate::didReceiveDataOfLength(%p, %d)\n", download, length);
    }

    void didReceiveResponse(WebDownload* download, WebURLResponse* response)
    {
        printf("MainDownloadDelegate::didReceiveResponse(%p, %p)\n", download, response);
    }

    void willResumeWithResponse(WebDownload* download, WebURLResponse* response, long long fromByte)
    {
        printf("MainDownloadDelegate::willResumeWithResponse(%p, %p, %lld)\n", download, response, fromByte);
    }

    WebMutableURLRequest* willSendRequest(WebDownload* download, WebMutableURLRequest* request, WebURLResponse* redirectResponse)
    {
        printf("MainDownloadDelegate::willSendRequest(%p, %p, %p)\n", download, request, redirectResponse);
        return request;
    }

    bool shouldDecodeSourceDataOfMIMEType(WebDownload* download, const char* type)
    {
        printf("MainDownloadDelegate::shouldDecodeSourceDataOfMIMEType(%p, %s)\n", download, type);
        return false;
    }

    void didBegin(WebDownload* download)
    {
        printf("MainDownloadDelegate::didBegin(%p)\n", download);
        registerDownload(download);
    }

    void didFinish(WebDownload* download)
    {
        printf("MainDownloadDelegate::didFinish(%p)\n", download);
        unregisterDownload(download);
        delete download;
    }

    void didFailWithError(WebDownload* download, WebError* error)
    {
        printf("MainDownloadDelegate::didFailWithError(%p, %p)\n", download, error);
        unregisterDownload(download);
        delete download;
    }
private:
    MainDownloadDelegate()
    {
    }
};

int main (int argc, char* argv[])
{
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Unable to init SDL: %s", SDL_GetError());
        exit(1);
    }

    signal(SIGSEGV, &signalCatcher);

    // Signals that should shutdown OWB properly.
    signal(SIGKILL, &signalQuitCatcher);
    signal(SIGINT, &signalQuitCatcher);

//    SDL_EventState(SDL_VIDEOEXPOSE, SDL_ENABLE);

    //SDL_SetEventFilter(eventFilter);

    webView = WebView::createInstance();

    int optionFirstCharacter;
    bool hasParsedConfigFile = false;
    while ((optionFirstCharacter = getopt(argc, argv, "c:f:d:")) != -1) {
        switch(optionFirstCharacter) {
        case 'c': {
            int chunkSize = (int)strtol(optarg, (char **)NULL, 10);
            if (chunkSize)
                webView->setCustomHTMLTokenizerChunkSize(chunkSize);
            break;
        }
        case 'd': {
            int delay = (int)strtol(optarg, (char **)NULL, 10);
            if (delay)
                webView->setCustomHTMLTokenizerTimeDelay(delay);
            break;
        }
        case 'f':
            hasParsedConfigFile = true;
            webView->parseConfigFile(optarg);
            break;
        default:
            usage();
        }
    }
    argc -= optind;
    argv += optind;

    if (!hasParsedConfigFile)
        webView->parseConfigFile();

    SDL_Rect rect = webView->frameRect();
    if (rect.w == 0 || rect.h == 0) {
        // The rectangle was not initialized so
        // default to a 800 x 600 rect.
        rect.w = 1280;
        rect.h = 720;
    }

    s_screen = createSDLWindow(rect.w, rect.h);
    webView->setViewWindow(s_screen);
    webView->initWithFrame(rect, NULL, NULL);

    webView->setWebNotificationDelegate(MainWebNotificationDelegate::createInstance());
    webView->setJSActionDelegate(MainJSActionDelegate::createInstance());
    webView->setDownloadDelegate(MainDownloadDelegate::createInstance());

    char* uri = (char*) (argc > 0 ? argv[0] : "http://www.google.com/");
    webView->mainFrame()->loadURL(uri);

    /*if(webView->canZoomPageIn()) {
        printf("canZoomPageIn\n");
        webView->zoomPageIn();
    }*/
    /*if(webView->canMakeTextLarger()) {
        printf("canMakeTextLarger\n");
        webView->makeTextLarger();
    }*/

    waitEvent();

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    SDL_Quit();

    delete webView;
    //delete the delegate after webView or set the delegate to 0
    return 0;
}
