/*****************************************************************************
 * voutgl.m: MacOS X OpenGL provider
 *****************************************************************************
 * Copyright (C) 2001-2004 the VideoLAN team
 * $Id: vout.m 8351 2004-08-02 13:06:38Z hartman $
 *
 * Authors: Colin Delacroix <colin@zoy.org>
 *          Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Eric Petit <titer@m0k.org>
 *          Benjamin Pracht <bigben at videolan dot org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                                /* free() */
#include <string.h>                                            /* strerror() */

#include <vlc_keys.h>

#include "intf.h"
#include "vout.h"

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>

#include <AGL/agl.h>

/*****************************************************************************
 * VLCView interface
 *****************************************************************************/
@interface VLCGLView : NSOpenGLView <VLCVoutViewResetting>
{
    vout_thread_t * p_vout;
}

+ (void)resetVout: (vout_thread_t *)p_vout;
- (id) initWithVout: (vout_thread_t *) p_vout;
@end

struct vout_sys_t
{
    NSAutoreleasePool * o_pool;
    VLCGLView         * o_glview;
    VLCVoutView       * o_vout_view;
    vlc_bool_t          b_saved_frame;
    NSRect              s_frame;
    vlc_bool_t          b_got_frame;
    vlc_mutex_t         lock;
    /* Mozilla plugin-related variables */
    vlc_bool_t          b_embedded;
    AGLContext          agl_ctx;
    AGLDrawable         agl_drawable;
    int                 i_offx, i_offy;
    int                 i_width, i_height;
    WindowRef           theWindow;
    WindowGroupRef      winGroup;
};

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/

static int  Init   ( vout_thread_t * p_vout );
static void End    ( vout_thread_t * p_vout );
static int  Manage ( vout_thread_t * p_vout );
static int  Control( vout_thread_t *, int, va_list );
static void Swap   ( vout_thread_t * p_vout );
static int  Lock   ( vout_thread_t * p_vout );
static void Unlock ( vout_thread_t * p_vout );

static int  aglInit   ( vout_thread_t * p_vout );
static void aglEnd    ( vout_thread_t * p_vout );
static int  aglManage ( vout_thread_t * p_vout );
static int  aglControl( vout_thread_t *, int, va_list );
static void aglSwap   ( vout_thread_t * p_vout );

int E_(OpenVideoGL)  ( vlc_object_t * p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *) p_this;
    vlc_value_t value_drawable;

    if( !CGDisplayUsesOpenGLAcceleration( kCGDirectMainDisplay ) )
    {
        msg_Warn( p_vout, "no OpenGL hardware acceleration found. "
                          "Video display will be slow" );
        return( 1 );
    }
    msg_Dbg( p_vout, "display is Quartz Extreme accelerated" );

    p_vout->p_sys = malloc( sizeof( vout_sys_t ) );
    if( p_vout->p_sys == NULL )
    {
        msg_Err( p_vout, "out of memory" );
        return( 1 );
    }

    memset( p_vout->p_sys, 0, sizeof( vout_sys_t ) );

    vlc_mutex_init( p_vout, &p_vout->p_sys->lock );

    var_Get( p_vout->p_vlc, "drawable", &value_drawable );
    if( value_drawable.i_int != 0 )
    {
        static const GLint ATTRIBUTES[] = { 
            AGL_WINDOW,
            AGL_RGBA,
            AGL_NO_RECOVERY,
            AGL_ACCELERATED,
            AGL_DOUBLEBUFFER,
            AGL_RED_SIZE,   8,
            AGL_GREEN_SIZE, 8,
            AGL_BLUE_SIZE,  8,
            AGL_ALPHA_SIZE, 8,
            AGL_DEPTH_SIZE, 24,
            AGL_NONE };

        AGLPixelFormat pixFormat;

        p_vout->p_sys->b_embedded = VLC_TRUE;

        pixFormat = aglChoosePixelFormat(NULL, 0, ATTRIBUTES);
        if( NULL == pixFormat )
        {
            msg_Err( p_vout, "no screen renderer available for required attributes." );
            return VLC_EGENERIC;
        }
        
        p_vout->p_sys->agl_ctx = aglCreateContext(pixFormat, NULL);
        aglDestroyPixelFormat(pixFormat);
        if( NULL == p_vout->p_sys->agl_ctx )
        {
            msg_Err( p_vout, "cannot create AGL context." );
            return VLC_EGENERIC;
        }
        else {
            // tell opengl not to sync buffer swap with vertical retrace (too inefficient)
            GLint param = 0;
            aglSetInteger(p_vout->p_sys->agl_ctx, AGL_SWAP_INTERVAL, &param);
            aglEnable(p_vout->p_sys->agl_ctx, AGL_SWAP_INTERVAL);
        }

        p_vout->pf_init             = aglInit;
        p_vout->pf_end              = aglEnd;
        p_vout->pf_manage           = aglManage;
        p_vout->pf_control          = aglControl;
        p_vout->pf_swap             = aglSwap;
        p_vout->pf_lock             = Lock;
        p_vout->pf_unlock           = Unlock;
    }
    else
    {
        NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];

        p_vout->p_sys->b_embedded = VLC_FALSE;
        p_vout->p_sys->b_saved_frame = NO;

        [VLCGLView performSelectorOnMainThread:@selector(initVout:) withObject:[NSValue valueWithPointer:p_vout] waitUntilDone:YES];

        [o_pool release];

        /* Check to see if initVout: was successfull */

        if( !p_vout->p_sys->o_vout_view )
        {
            return VLC_EGENERIC;
        }
        p_vout->pf_init   = Init;
        p_vout->pf_end    = End;
        p_vout->pf_manage = Manage;
        p_vout->pf_control= Control;
        p_vout->pf_swap   = Swap;
        p_vout->pf_lock   = Lock;
        p_vout->pf_unlock = Unlock;
    }
    p_vout->p_sys->b_got_frame = VLC_FALSE;

    return VLC_SUCCESS;
}

void E_(CloseVideoGL) ( vlc_object_t * p_this )
{
    vout_thread_t * p_vout = (vout_thread_t *) p_this;
    if( p_vout->p_sys->b_embedded )
    {
        aglDestroyContext(p_vout->p_sys->agl_ctx);
    }
    else if(!VLCIntf->b_die) 
    {
        NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];

        /* Close the window */
        [p_vout->p_sys->o_vout_view performSelectorOnMainThread:@selector(closeVout) withObject:NULL waitUntilDone:YES];

        [o_pool release];
    }
    /* Clean up */
    vlc_mutex_destroy( &p_vout->p_sys->lock );
    free( p_vout->p_sys );
}

static int Init( vout_thread_t * p_vout )
{
    [[p_vout->p_sys->o_glview openGLContext] makeCurrentContext];
    return VLC_SUCCESS;
}

static void End( vout_thread_t * p_vout )
{
    [[p_vout->p_sys->o_glview openGLContext] makeCurrentContext];
}

static int Manage( vout_thread_t * p_vout )
{
    if( p_vout->i_changes & VOUT_ASPECT_CHANGE )
    {
        [p_vout->p_sys->o_glview reshape];
        p_vout->i_changes &= ~VOUT_ASPECT_CHANGE;
    }
    if( p_vout->i_changes & VOUT_CROP_CHANGE )
    {
        [p_vout->p_sys->o_glview reshape];
        p_vout->i_changes &= ~VOUT_CROP_CHANGE;
    }

    if( p_vout->i_changes & VOUT_FULLSCREEN_CHANGE )
    {
        NSAutoreleasePool *o_pool = [[NSAutoreleasePool alloc] init];

        p_vout->b_fullscreen = !p_vout->b_fullscreen;

        if( p_vout->b_fullscreen ) 
            [p_vout->p_sys->o_vout_view enterFullscreen];
        else
            [p_vout->p_sys->o_vout_view leaveFullscreen];

        [o_pool release];

        p_vout->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
    }

    [p_vout->p_sys->o_vout_view manage];
    return VLC_SUCCESS;
}

/*****************************************************************************
 * Control: control facility for the vout
 *****************************************************************************/
static int Control( vout_thread_t *p_vout, int i_query, va_list args )
{
    vlc_bool_t b_arg;

    switch( i_query )
    {
        case VOUT_SET_STAY_ON_TOP:
            b_arg = va_arg( args, vlc_bool_t );
            [p_vout->p_sys->o_vout_view setOnTop: b_arg];
            return VLC_SUCCESS;

        case VOUT_CLOSE:
        case VOUT_REPARENT:
        default:
            return vout_vaControlDefault( p_vout, i_query, args );
    }
}

static void Swap( vout_thread_t * p_vout )
{
    p_vout->p_sys->b_got_frame = VLC_TRUE;
    [[p_vout->p_sys->o_glview openGLContext] makeCurrentContext];
    glFlush();
}

static int Lock( vout_thread_t * p_vout )
{
    vlc_mutex_lock( &p_vout->p_sys->lock );

#if __INTEL__
    CGLLockContext( (CGLContextObj)p_vout->p_sys->agl_ctx ); 
#endif

    return 0;
}

static void Unlock( vout_thread_t * p_vout )
{
    vlc_mutex_unlock( &p_vout->p_sys->lock );

#if __INTEL__
    CGLUnlockContext( (CGLContextObj)p_vout->p_sys->agl_ctx ); 
#endif
}

/*****************************************************************************
 * VLCGLView implementation
 *****************************************************************************/
@implementation VLCGLView
+ (void)initVout:(NSValue *)arg
{
    vout_thread_t * p_vout = [arg pointerValue];
    NSRect * frame;

    /* Create the GL view */
    p_vout->p_sys->o_glview = [[VLCGLView alloc] initWithVout: p_vout];
    [p_vout->p_sys->o_glview autorelease];

    /* Spawn the window */    
    frame = p_vout->p_sys->b_saved_frame ? &p_vout->p_sys->s_frame : nil;

    p_vout->p_sys->o_vout_view = [VLCVoutView getVoutView: p_vout
                                  subView: p_vout->p_sys->o_glview
                                  frame: frame];
}

/* This function will reset the o_vout_view. It's useful to go fullscreen. */
+ (void)resetVout:(NSData *)arg
{
    vout_thread_t * p_vout = [arg pointerValue];

    if( p_vout->b_fullscreen )
    {
        /* Save window size and position */
        p_vout->p_sys->s_frame.size =
            [p_vout->p_sys->o_vout_view frame].size;
        p_vout->p_sys->s_frame.origin =
            [[p_vout->p_sys->o_vout_view getWindow ]frame].origin;
        p_vout->p_sys->b_saved_frame = VLC_TRUE;
    }

    [p_vout->p_sys->o_vout_view closeVout];

#define o_glview p_vout->p_sys->o_glview
    o_glview = [[VLCGLView alloc] initWithVout: p_vout];
    [o_glview autorelease];
 
    if( p_vout->p_sys->b_saved_frame )
    {
        p_vout->p_sys->o_vout_view = [VLCVoutView getVoutView: p_vout
                                                      subView: o_glview
                                                        frame: &p_vout->p_sys->s_frame];
    }
    else
    {
        p_vout->p_sys->o_vout_view = [VLCVoutView getVoutView: p_vout
                                                      subView: o_glview frame: nil];
 
    }
#undef o_glview
}

- (id) initWithVout: (vout_thread_t *) vout
{
    p_vout = vout;

    NSOpenGLPixelFormatAttribute attribs[] =
    {
        NSOpenGLPFAAccelerated,
        NSOpenGLPFANoRecovery,
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFAAlphaSize, 8,
        NSOpenGLPFADepthSize, 24,
        NSOpenGLPFAWindow,
        0
    };

    NSOpenGLPixelFormat * fmt = [[NSOpenGLPixelFormat alloc]
        initWithAttributes: attribs];

    if( !fmt )
    {
        msg_Warn( p_vout, "could not create OpenGL video output" );
        return nil;
    }

    self = [super initWithFrame: NSMakeRect(0,0,10,10) pixelFormat: fmt];
    [fmt release];

    [[self openGLContext] makeCurrentContext];
    [[self openGLContext] update];

    /* Swap buffers only during the vertical retrace of the monitor.
       http://developer.apple.com/documentation/GraphicsImaging/
       Conceptual/OpenGL/chap5/chapter_5_section_44.html */
    long params[] = { 1 };
    CGLSetParameter( CGLGetCurrentContext(), kCGLCPSwapInterval,
                     params );
    return self;
}

- (void) reshape
{
    int x, y;
    vlc_value_t val;

    Lock( p_vout );
    NSRect bounds = [self bounds];

    [[self openGLContext] makeCurrentContext];

    var_Get( p_vout, "macosx-stretch", &val );
    if( val.b_bool )
    {
        x = bounds.size.width;
        y = bounds.size.height;
    }
    else if( bounds.size.height * p_vout->fmt_in.i_visible_width *
             p_vout->fmt_in.i_sar_num <
             bounds.size.width * p_vout->fmt_in.i_visible_height *
             p_vout->fmt_in.i_sar_den )
    {
        x = ( bounds.size.height * p_vout->fmt_in.i_visible_width *
              p_vout->fmt_in.i_sar_num ) /
            ( p_vout->fmt_in.i_visible_height * p_vout->fmt_in.i_sar_den);

        y = bounds.size.height;
    }
    else
    {
        x = bounds.size.width;
        y = ( bounds.size.width * p_vout->fmt_in.i_visible_height *
              p_vout->fmt_in.i_sar_den) /
            ( p_vout->fmt_in.i_visible_width * p_vout->fmt_in.i_sar_num  );
    }

    glViewport( ( bounds.size.width - x ) / 2,
                ( bounds.size.height - y ) / 2, x, y );

    if( p_vout->p_sys->b_got_frame )
    {
        /* Ask the opengl module to redraw */
        vout_thread_t * p_parent;
        p_parent = (vout_thread_t *) p_vout->p_parent;
        Unlock( p_vout );
        if( p_parent && p_parent->pf_display )
        {
            p_parent->pf_display( p_parent, NULL );
        }
    }
    else
    {
        glClear( GL_COLOR_BUFFER_BIT );
        Unlock( p_vout );
    }
    [super reshape];
}

- (void) update
{
    Lock( p_vout );
    [super update];
    Unlock( p_vout );
}

- (void) drawRect: (NSRect) rect
{
    Lock( p_vout );
    [[self openGLContext] makeCurrentContext];
    glFlush();
    [super drawRect:rect];
    Unlock( p_vout );
}

@end

/*****************************************************************************
 * embedded AGL context implementation
 *****************************************************************************/

static void aglSetViewport( vout_thread_t *p_vout, Rect viewBounds, Rect clipBounds );
static void aglReshape( vout_thread_t * p_vout );
static OSStatus WindowEventHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData);

static int aglInit( vout_thread_t * p_vout )
{
    vlc_value_t val;

    Rect viewBounds;    
    Rect clipBounds;
    
    var_Get( p_vout->p_vlc, "drawable", &val );
    p_vout->p_sys->agl_drawable = (AGLDrawable)val.i_int;
    aglSetDrawable(p_vout->p_sys->agl_ctx, p_vout->p_sys->agl_drawable);

    var_Get( p_vout->p_vlc, "drawable-view-top", &val );
    viewBounds.top = val.i_int;
    var_Get( p_vout->p_vlc, "drawable-view-left", &val );
    viewBounds.left = val.i_int;
    var_Get( p_vout->p_vlc, "drawable-view-bottom", &val );
    viewBounds.bottom = val.i_int;
    var_Get( p_vout->p_vlc, "drawable-view-right", &val );
    viewBounds.right = val.i_int;
    var_Get( p_vout->p_vlc, "drawable-clip-top", &val );
    clipBounds.top = val.i_int;
    var_Get( p_vout->p_vlc, "drawable-clip-left", &val );
    clipBounds.left = val.i_int;
    var_Get( p_vout->p_vlc, "drawable-clip-bottom", &val );
    clipBounds.bottom = val.i_int;
    var_Get( p_vout->p_vlc, "drawable-clip-right", &val );
    clipBounds.right = val.i_int;

    aglSetViewport(p_vout, viewBounds, clipBounds);
    aglSetCurrentContext(p_vout->p_sys->agl_ctx);
    aglReshape( p_vout );
    
    return VLC_SUCCESS;
}

static void aglEnd( vout_thread_t * p_vout )
{
    aglSetCurrentContext(NULL);
    if( p_vout->p_sys->theWindow ) DisposeWindow( p_vout->p_sys->theWindow );
}

static void aglReshape( vout_thread_t * p_vout )
{
    unsigned int x, y;
    unsigned int i_height = p_vout->p_sys->i_height;
    unsigned int i_width  = p_vout->p_sys->i_width;

    Lock( p_vout );

    vout_PlacePicture(p_vout, i_width, i_height, &x, &y, &i_width, &i_height); 

    aglSetCurrentContext(p_vout->p_sys->agl_ctx);

    glViewport( p_vout->p_sys->i_offx + x, p_vout->p_sys->i_offy + y, i_width, i_height );

    if( p_vout->p_sys->b_got_frame )
    {
        /* Ask the opengl module to redraw */
        vout_thread_t * p_parent;
        p_parent = (vout_thread_t *) p_vout->p_parent;
        Unlock( p_vout );
        if( p_parent && p_parent->pf_display )
        {
            p_parent->pf_display( p_parent, NULL );
        }
    }
    else
    {
        glClear( GL_COLOR_BUFFER_BIT );
        Unlock( p_vout );
    }
}

/* private event class */
enum 
{
    kEventClassVLCPlugin = 'vlcp',
};
/* private event kinds */
enum
{
    kEventVLCPluginShowFullscreen = 32768,
    kEventVLCPluginHideFullscreen,
};

static void sendEventToMainThread(EventTargetRef target, UInt32 class, UInt32 kind)
{
    EventRef myEvent;
    if( noErr == CreateEvent(NULL, class, kind, 0, kEventAttributeNone, &myEvent) )
    {
        if( noErr == SetEventParameter(myEvent, kEventParamPostTarget, typeEventTargetRef, sizeof(EventTargetRef), &target) )
        {
            PostEventToQueue(GetMainEventQueue(), myEvent, kEventPriorityStandard);
        }
        ReleaseEvent(myEvent);
    }
}

static int aglManage( vout_thread_t * p_vout )
{
    if( p_vout->i_changes & VOUT_ASPECT_CHANGE )
    {
        aglReshape(p_vout);
        p_vout->i_changes &= ~VOUT_ASPECT_CHANGE;
    }
    if( p_vout->i_changes & VOUT_CROP_CHANGE )
    {
        aglReshape(p_vout);
        p_vout->i_changes &= ~VOUT_CROP_CHANGE;
    }
    if( p_vout->i_changes & VOUT_FULLSCREEN_CHANGE )
    {
        aglSetDrawable(p_vout->p_sys->agl_ctx, NULL);
        Lock( p_vout );
        if( p_vout->b_fullscreen )
        {
            /* Close the window resume normal drawing */
            vlc_value_t val;
            Rect viewBounds;    
            Rect clipBounds;

            var_Get( p_vout->p_vlc, "drawable", &val );
            p_vout->p_sys->agl_drawable = (AGLDrawable)val.i_int;
            aglSetDrawable(p_vout->p_sys->agl_ctx, p_vout->p_sys->agl_drawable);

            var_Get( p_vout->p_vlc, "drawable-view-top", &val );
            viewBounds.top = val.i_int;
            var_Get( p_vout->p_vlc, "drawable-view-left", &val );
            viewBounds.left = val.i_int;
            var_Get( p_vout->p_vlc, "drawable-view-bottom", &val );
            viewBounds.bottom = val.i_int;
            var_Get( p_vout->p_vlc, "drawable-view-right", &val );
            viewBounds.right = val.i_int;
            var_Get( p_vout->p_vlc, "drawable-clip-top", &val );
            clipBounds.top = val.i_int;
            var_Get( p_vout->p_vlc, "drawable-clip-left", &val );
            clipBounds.left = val.i_int;
            var_Get( p_vout->p_vlc, "drawable-clip-bottom", &val );
            clipBounds.bottom = val.i_int;
            var_Get( p_vout->p_vlc, "drawable-clip-right", &val );
            clipBounds.right = val.i_int;

            aglSetCurrentContext(p_vout->p_sys->agl_ctx);
            aglSetViewport(p_vout, viewBounds, clipBounds);

            /* Most Carbon APIs are not thread-safe, therefore delagate some GUI visibilty update to the main thread */
            sendEventToMainThread(GetWindowEventTarget(p_vout->p_sys->theWindow), kEventClassVLCPlugin, kEventVLCPluginHideFullscreen);
        }
        else
        {
            Rect deviceRect;
            
            GDHandle deviceHdl = GetMainDevice();
            deviceRect = (*deviceHdl)->gdRect;
            
            if( !p_vout->p_sys->theWindow )
            {
                /* Create a window */
                WindowAttributes    windowAttrs;

                windowAttrs = kWindowStandardDocumentAttributes
                            | kWindowStandardHandlerAttribute
                            | kWindowLiveResizeAttribute
                            | kWindowNoShadowAttribute;
                                            
                windowAttrs &= (~kWindowResizableAttribute);

                CreateNewWindow(kDocumentWindowClass, windowAttrs, &deviceRect, &p_vout->p_sys->theWindow);
                if( !p_vout->p_sys->winGroup )
                {
                    CreateWindowGroup(0, &p_vout->p_sys->winGroup);
                    SetWindowGroup(p_vout->p_sys->theWindow, p_vout->p_sys->winGroup);
                    SetWindowGroupParent( p_vout->p_sys->winGroup, GetWindowGroupOfClass(kDocumentWindowClass) ) ;
                }
                
                // Window title
                CFStringRef titleKey    = CFSTR("Fullscreen VLC media plugin");
                CFStringRef windowTitle = CFCopyLocalizedString(titleKey, NULL);
                SetWindowTitleWithCFString(p_vout->p_sys->theWindow, windowTitle);
                CFRelease(titleKey);
                CFRelease(windowTitle);
                
                //Install event handler
                static const EventTypeSpec win_events[] = {
                    { kEventClassMouse, kEventMouseUp },
                    { kEventClassWindow, kEventWindowClosed },
                    { kEventClassWindow, kEventWindowBoundsChanged },
                    { kEventClassCommand, kEventCommandProcess },
                    { kEventClassVLCPlugin, kEventVLCPluginShowFullscreen },
                    { kEventClassVLCPlugin, kEventVLCPluginHideFullscreen },
                };
                InstallWindowEventHandler (p_vout->p_sys->theWindow, NewEventHandlerUPP (WindowEventHandler), GetEventTypeCount(win_events), win_events, p_vout, NULL);
            }
            else
            {
                /* just in case device resolution changed */
                SetWindowBounds(p_vout->p_sys->theWindow, kWindowContentRgn, &deviceRect);
            }
            glClear( GL_COLOR_BUFFER_BIT );
            p_vout->p_sys->agl_drawable = (AGLDrawable)GetWindowPort(p_vout->p_sys->theWindow);
            aglSetDrawable(p_vout->p_sys->agl_ctx, p_vout->p_sys->agl_drawable);
            aglSetCurrentContext(p_vout->p_sys->agl_ctx);
            aglSetViewport(p_vout, deviceRect, deviceRect);
            //aglSetFullScreen(p_vout->p_sys->agl_ctx, device_width, device_height, 0, 0);

            /* Most Carbon APIs are not thread-safe, therefore delagate some GUI visibilty update to the main thread */
            sendEventToMainThread(GetWindowEventTarget(p_vout->p_sys->theWindow), kEventClassVLCPlugin, kEventVLCPluginShowFullscreen);
        }
        p_vout->b_fullscreen = !p_vout->b_fullscreen;
        p_vout->i_changes &= ~VOUT_FULLSCREEN_CHANGE;
        Unlock( p_vout );
        aglReshape(p_vout);
    }
    return VLC_SUCCESS;
}

static int aglControl( vout_thread_t *p_vout, int i_query, va_list args )
{
    switch( i_query )
    {
        case VOUT_SET_VIEWPORT:
        {
            Rect viewBounds, clipBounds;
            viewBounds.top = va_arg( args, int);
            viewBounds.left = va_arg( args, int);
            viewBounds.bottom = va_arg( args, int);
            viewBounds.right = va_arg( args, int);
            clipBounds.top = va_arg( args, int);
            clipBounds.left = va_arg( args, int);
            clipBounds.bottom = va_arg( args, int);
            clipBounds.right = va_arg( args, int);
            
            if( !p_vout->b_fullscreen ) 
            {
                Lock( p_vout );
                aglSetViewport(p_vout, viewBounds, clipBounds);
                Unlock( p_vout );
                aglReshape( p_vout );
            }
            return VLC_SUCCESS;
        }

        case VOUT_REPARENT:
        {
            AGLDrawable drawable = (AGLDrawable)va_arg( args, int);
            if( !p_vout->b_fullscreen && drawable != p_vout->p_sys->agl_drawable )
            {
                p_vout->p_sys->agl_drawable = drawable;
                aglSetDrawable(p_vout->p_sys->agl_ctx, drawable);
            }
            return VLC_SUCCESS;
        }

        default:
            return vout_vaControlDefault( p_vout, i_query, args );
    }
}

static void aglSwap( vout_thread_t * p_vout )
{
    p_vout->p_sys->b_got_frame = VLC_TRUE;
    aglSwapBuffers(p_vout->p_sys->agl_ctx);
}

/* Enter this function with the p_vout locked */
static void aglSetViewport( vout_thread_t *p_vout, Rect viewBounds, Rect clipBounds )
{
    // mozilla plugin provides coordinates based on port bounds
    // however AGL coordinates are based on window structure region
    // and are vertically flipped
    GLint rect[4];
    CGrafPtr port = (CGrafPtr)p_vout->p_sys->agl_drawable;
    Rect winBounds, clientBounds;

    GetWindowBounds(GetWindowFromPort(port),
        kWindowStructureRgn, &winBounds);
    GetWindowBounds(GetWindowFromPort(port),
        kWindowContentRgn, &clientBounds);

    /* update video clipping bounds in drawable */
    rect[0] = (clientBounds.left-winBounds.left)
            + clipBounds.left;                  // from window left edge
    rect[1] = (winBounds.bottom-winBounds.top)
            - (clientBounds.top-winBounds.top)
            - clipBounds.bottom;                // from window bottom edge
    rect[2] = clipBounds.right-clipBounds.left; // width
    rect[3] = clipBounds.bottom-clipBounds.top; // height
    aglSetInteger(p_vout->p_sys->agl_ctx, AGL_BUFFER_RECT, rect);
    aglEnable(p_vout->p_sys->agl_ctx, AGL_BUFFER_RECT);

    /* update video internal bounds in drawable */
    p_vout->p_sys->i_width  = viewBounds.right-viewBounds.left;
    p_vout->p_sys->i_height = viewBounds.bottom-viewBounds.top;
    p_vout->p_sys->i_offx   = -clipBounds.left - viewBounds.left;
    p_vout->p_sys->i_offy   = clipBounds.bottom + viewBounds.top
                            - p_vout->p_sys->i_height; 

    aglUpdateContext(p_vout->p_sys->agl_ctx);
}

//default window event handler
static pascal OSStatus WindowEventHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData)
{
    OSStatus result = noErr;
    UInt32 class = GetEventClass (event);
    UInt32 kind = GetEventKind (event); 
    vout_thread_t *p_vout = (vout_thread_t *)userData;

    result = CallNextEventHandler(nextHandler, event);
    if(class == kEventClassCommand)
    {
        HICommand theHICommand;
        GetEventParameter( event, kEventParamDirectObject, typeHICommand, NULL, sizeof( HICommand ), NULL, &theHICommand );
        
        switch ( theHICommand.commandID )
        {
            default:
                result = eventNotHandledErr;
                break;
        }
    }
    else if(class == kEventClassWindow)
    {
        WindowRef     window;
        Rect          rectPort = {0,0,0,0};
        
        GetEventParameter(event, kEventParamDirectObject, typeWindowRef, NULL, sizeof(WindowRef), NULL, &window);

        if(window)
        {
            GetPortBounds(GetWindowPort(window), &rectPort);
        }   

        switch (kind)
        {
            case kEventWindowClosed:
            case kEventWindowZoomed:
            case kEventWindowBoundsChanged:
                break;
            
            default:
                result = eventNotHandledErr;
                break;
        }
    }
    else if(class == kEventClassMouse)
    {
        UInt16     button;
        
        switch (kind)
        {
            case kEventMouseDown:
                GetEventParameter(event, kEventParamMouseButton, typeMouseButton, NULL, sizeof(button), NULL, &button);
                switch (button)
                {
                    case kEventMouseButtonPrimary:
                    {
                        vlc_value_t val;

                        var_Get( p_vout, "mouse-button-down", &val );
                        val.i_int |= 1;
                        var_Set( p_vout, "mouse-button-down", val );
                    }
                    default:
                        result = eventNotHandledErr;
                        break;
                }
                break;

            case kEventMouseUp:
                GetEventParameter(event, kEventParamMouseButton, typeMouseButton, NULL, sizeof(button), NULL, &button);
                switch (button)
                {
                    case kEventMouseButtonPrimary:
                    {
                        UInt32 clickCount = 0;
                        GetEventParameter(event, kEventParamClickCount, typeUInt32, NULL, sizeof(clickCount), NULL, &clickCount);
                        if( clickCount > 1 )
                        {
                            vlc_value_t val;

                            val.b_bool = VLC_FALSE;
                            var_Set((vout_thread_t *) p_vout->p_parent, "fullscreen", val);
                        }
                        else
                        {
                            vlc_value_t val;

                            var_Get( p_vout, "mouse-button-down", &val );
                            val.i_int &= ~1;
                            var_Set( p_vout, "mouse-button-down", val );
                        }
                        break;
                    }
                    default:
                        result = eventNotHandledErr;
                        break;
                }
                break;
            
            default:
                result = eventNotHandledErr;
                break;
        }
    }
    else if(class == kEventClassVLCPlugin)
    {
        switch (kind)
        {
            case kEventVLCPluginShowFullscreen:
                ShowWindow (p_vout->p_sys->theWindow);
                SetSystemUIMode( kUIModeAllHidden, kUIOptionAutoShowMenuBar);
                //CGDisplayHideCursor(kCGDirectMainDisplay);
                break;
            case kEventVLCPluginHideFullscreen:
                HideWindow (p_vout->p_sys->theWindow);
                SetSystemUIMode( kUIModeNormal, 0);
                CGDisplayShowCursor(kCGDirectMainDisplay);
                //DisposeWindow( p_vout->p_sys->theWindow );
                break;
            default:
                result = eventNotHandledErr;
                break;
        }
    }
    return result;
}

