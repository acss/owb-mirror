/*
 * Copyright (C) 2007 Pleyo.  All rights reserved.
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
 
/**
 * @file  main.cpp
 *
 * @brief bal launcher
 *
 * Repository informations :
 * - $URL$
 * - $Rev$
 * - $Date: 2006/05/11 13:44:56 $

 */


#include "config.h"
#include "BALConfiguration.h"
#include "BIEventLoop.h"
#include "BIKeyboardEvent.h"
#include "BIWindow.h"
#include "BIWindowEvent.h"
#include "BIWindowManager.h"
#include "BIGraphicsDevice.h"
#include "BTLogHelper.h"
#include "BTDeviceChannel.h"
#include "BTTextLogFormatter.h"
#include "CookieJar.h"
#include "KURL.h"
#include "wtf/RefPtr.h"
#ifdef __OWBAL_PLATFORM_MACPORT__
// SDL on Mac has some tricks with main()
#include <SDL/SDL.h>
#endif
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef __OWB_NPAPI__
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#endif


/**
 * DeepSee lib init
 */
DS_INIT_DEEPSEE_FRAMEWORK();


/**
 * Signal 11 catcher to prevent process from freezing on segfaults.
 */
void signalCatcher(int signum)
{
    printf("signal %d catched: abort.\n", signum);
    abort();
}

using namespace BAL;

static void handle_event( BIEventLoop& aEventLoop ) {
    BIEvent* aEvent = NULL;
    bool isQuitCalled = false;
    bool isEventValid = false;

#ifdef __OWB_NPAPI__
    gtk_init(NULL, NULL);
#endif
    while (!isQuitCalled)
    {
        if (aEvent != NULL) {
            delete aEvent;
            aEvent = NULL; // otherwise may be freed twice if WaitEvent returns an invalid event
        }
        
        isEventValid = aEventLoop.WaitEvent(aEvent);
        if (isEventValid) {
            BIKeyboardEvent* aKeyboardEvent = aEvent->queryIsKeyboardEvent();
            if(aKeyboardEvent)
            {
                if(aKeyboardEvent->virtualKeyCode() == BAL::BIKeyboardEvent::VK_ESCAPE ) {
                    break; // stop loop
                }
            }

            BIWindowEvent* aWindowEvent = aEvent->queryIsWindowEvent();
            if( aWindowEvent && aWindowEvent->type() == BAL::BIWindowEvent::QUIT )
            {
                break; // stop loop
            }
            // In other cases, event is handled by frame
            getBIWindowManager()->handleEvent(aEvent);
        }
#ifdef __OWB_NPAPI__	
        gtk_main_iteration_do(false);
#endif
    }
    if (aEvent) {
        delete aEvent;
        aEvent = NULL;
    }
}

int main(int argc, char *argv[])
{
    signal(SIGSEGV, &signalCatcher);
    DS_SIGNAL_CATCH(SIGSEGV);

#ifdef BAL_LOG // logger is not defined in NDEBUG
    // Setup a nice formatter for log
    RefPtr<BALFacilities::BTDeviceChannel> channel = new BALFacilities::BTDeviceChannel();
    RefPtr<BALFacilities::BTTextLogFormatter> textLogFormatter
            = new BALFacilities::BTTextLogFormatter();
    channel->setFormatter(textLogFormatter.get());
    BALFacilities::logger.addChannel(channel.get());
#endif
    // default values for arguments
    int width = 800, height = 600, depth = 32;
    char* url = "http://google.com";

    // simple arg parsing
    int argCount = 1;
    char* option;
    while (argCount < argc) {
        bool optionIsUnknown=true;
        option = argv[argCount];
        if ((strcmp(option, "--help")==0) || (strcmp(option,"-h")==0))
        {
            printf("usage:\n%s [--help] [-h] [--width w] [--height h] [--depth d] [[--url] url]\n",argv[0]);
            return 0;
        }
        else if (argv[argCount][0]!='-')
        {
            optionIsUnknown=false;
            url=argv[argCount];
        }else if (argCount+1<argc){
            if (strcmp(option, "--width") == 0){
                optionIsUnknown=false;
                width = (int)strtol(argv[++argCount], NULL, 10);
                }
            else if (strcmp(option, "--height") == 0){
                optionIsUnknown=false;
                height = (int)strtol(argv[++argCount], NULL, 10);
            }
            else if (strcmp(option, "--depth") == 0){
                optionIsUnknown=false;
                depth = (int)strtol(argv[++argCount], NULL, 10);
            }
            else if (strcmp(option, "--url") == 0){
                optionIsUnknown=false;
                url = argv[++argCount];
            }
        }

        
        
        if (optionIsUnknown)
            fprintf(stderr, "error: unknown option %s\n", argv[argCount]);

        // next arg
        argCount += 1;
    }
    printf("Using --width %d --height %d --depth %d --url %s\n", width, height, depth, url);

    // FIXME first thing to do, because event loop may rely on GraphicsDevice (eg SDL)
    getBIGraphicsDevice()->initialize(width, height, depth);
 
    BIEventLoop* aEventLoop = getBIEventLoop();
    if( aEventLoop == NULL ) {
      printf("No event loop\n");
      return 1;
    }

#if 1
    BIWindow *window = getBIWindowManager()->openWindow(0, 0, width, height);
#else // code below left as example
    BIWindow *window = getBIWindowManager()->openWindow(10, 10, 385, 
285);
    BIWindow *window2 = getBIWindowManager()->openWindow(10, 305, 385, 285);
    window2->setURL("http://www.pleyo.com");
    BIWindow *window3 = getBIWindowManager()->openWindow(405, 10, 385, 285);
    window3->setURL("http://webkit.org/");
    BIWindow *window4 = getBIWindowManager()->openWindow(405, 305, 385, 285);
    window4->setURL("http://www.ubuntu-fr.org");
#endif
    
    if(::WebCore::cookiesEnabled())
        getBICookieJar();
        
    window->setURL(url);
    
    handle_event( *aEventLoop );
    getBIWindowManager()->closeWindow(window);
    deleteBIWindowManager();
    deleteBIEventLoop();
    aEventLoop = NULL;

    getBIGraphicsDevice()->finalize();
    deleteBIGraphicsDevice();
    
    if(::WebCore::cookiesEnabled())
        deleteBICookieJar(getBICookieJar());


    DS_CLEAN_DEEPSEE_FRAMEWORK(); 

    return 0;
}
