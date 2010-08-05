/*****************************************************************************
 * voutgl.m: MacOS X OpenGL provider
 *****************************************************************************
 * Copyright (C) 2001-2009 the VideoLAN team
 * $Id: ac1bd18561eae68266efd5a1af30c733ea724718 $
 *
 * Authors: Colin Delacroix <colin@zoy.org>
 *          Florian G. Pflug <fgp@phlo.org>
 *          Jon Lech Johansen <jon-vl@nanocrew.net>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Eric Petit <titer@m0k.org>
 *          Benjamin Pracht <bigben at videolan dot org>
 *          Damien Fouilleul <damienf at videolan dot org>
 *          Pierre d'Herbemont <pdherbemont at videolan dot org>
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

#import <Cocoa/Cocoa.h>
#import <OpenGL/OpenGL.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_vout_opengl.h>
#include "opengl.h"

/**
 * Forward declarations
 */
static int Open(vlc_object_t *);
static void Close(vlc_object_t *);

static picture_pool_t *Pool(vout_display_t *vd, unsigned requested_count);
static void PictureRender(vout_display_t *vd, picture_t *pic);
static void PictureDisplay(vout_display_t *vd, picture_t *pic);
static int Control (vout_display_t *vd, int query, va_list ap);

static int OpenglLock(vout_opengl_t *gl);
static void OpenglUnlock(vout_opengl_t *gl);
static void OpenglSwap(vout_opengl_t *gl);

/**
 * Module declaration
 */
vlc_module_begin ()
    /* Will be loaded even without interface module. see voutgl.m */
    set_shortname("Mac OS X")
    set_description( N_("Mac OS X OpenGL video output (requires drawable-nsobject)"))
    set_category(CAT_VIDEO)
    set_subcategory(SUBCAT_VIDEO_VOUT )
    set_capability("vout display", 300)
    set_callbacks(Open, Close)

    add_shortcut("macosx")
    add_shortcut("vout_macosx")
vlc_module_end ()

/**
 * Obj-C protocol declaration that drawable-nsobject should follow
 */
@protocol VLCOpenGLVideoViewEmbedding <NSObject>
- (void)addVoutSubview:(NSView *)view;
- (void)removeVoutSubview:(NSView *)view;
@end

@interface VLCOpenGLVideoView : NSOpenGLView
{
    vout_display_t *vd;
    BOOL _hasPendingReshape;
}
- (void)setVoutDisplay:(vout_display_t *)vd;
- (void)setVoutFlushing:(BOOL)flushing;
@end


struct vout_display_sys_t
{
    VLCOpenGLVideoView *glView;
    id<VLCOpenGLVideoViewEmbedding> container;

    vout_opengl_t gl;
    vout_display_opengl_t vgl;

    picture_pool_t *pool;
    picture_t *current;
    bool has_first_frame;
};

static int Open(vlc_object_t *this)
{
    vout_display_t *vd = (vout_display_t *)this;
    vout_display_sys_t *sys = calloc(1, sizeof(*sys));
    NSAutoreleasePool *nsPool = nil;

    if (!sys)
        return VLC_ENOMEM;

    vd->sys = sys;
    sys->pool = NULL;
    sys->gl.sys = NULL;

    /* Get the drawable object */
    id container = var_CreateGetAddress(vd, "drawable-nsobject");
    if (!container)
    {
        msg_Dbg(vd, "No drawable-nsobject, passing over.");
        goto error;
    }

    /* This will be released in Close(), on
     * main thread, after we are done using it. */
    sys->container = [container retain];

    /* Get our main view*/
    nsPool = [[NSAutoreleasePool alloc] init];

    [VLCOpenGLVideoView performSelectorOnMainThread:@selector(getNewView:) withObject:[NSValue valueWithPointer:&sys->glView] waitUntilDone:YES];
    if (!sys->glView)
        goto error;

    [sys->glView setVoutDisplay:vd];

    /* We don't wait, that means that we'll have to be careful about releasing
     * container.
     * That's why we'll release on main thread in Close(). */
    if ([(id)container respondsToSelector:@selector(addVoutSubview:)])
        [(id)container performSelectorOnMainThread:@selector(addVoutSubview:) withObject:sys->glView waitUntilDone:NO];
    else if ([container isKindOfClass:[NSView class]])
    {
        NSView *parentView = container;
        [parentView performSelectorOnMainThread:@selector(addSubview:) withObject:sys->glView waitUntilDone:NO];
        [sys->glView performSelectorOnMainThread:@selector(setFrameWithValue:) withObject:[NSValue valueWithRect:[parentView bounds]] waitUntilDone:NO];
    }
    else
    {
        msg_Err(vd, "Invalid drawable-nsobject object. drawable-nsobject must either be an NSView or comply to the @protocol VLCOpenGLVideoViewEmbedding.");
        goto error;
    }


    [nsPool release];
    nsPool = nil;

    /* Initialize common OpenGL video display */
    sys->gl.lock = OpenglLock;
    sys->gl.unlock = OpenglUnlock;
    sys->gl.swap = OpenglSwap;
    sys->gl.sys = sys;

    if (vout_display_opengl_Init(&sys->vgl, &vd->fmt, &sys->gl))
    {
        sys->gl.sys = NULL;
        goto error;
    }

    /* */
    vout_display_info_t info = vd->info;
    info.has_pictures_invalid = false;

    /* Setup vout_display_t once everything is fine */
    vd->info = info;

    vd->pool = Pool;
    vd->prepare = PictureRender;
    vd->display = PictureDisplay;
    vd->control = Control;

    /* */
    vout_display_SendEventFullscreen (vd, false);
    vout_display_SendEventDisplaySize (vd, vd->source.i_visible_width, vd->source.i_visible_height, false);

    return VLC_SUCCESS;

error:
    [nsPool release];
    Close(this);
    return VLC_EGENERIC;
}

void Close(vlc_object_t *this)
{
    vout_display_t *vd = (vout_display_t *)this;
    vout_display_sys_t *sys = vd->sys;

    [sys->glView setVoutDisplay:nil];

    var_Destroy(vd, "drawable-nsobject");
    if ([(id)sys->container respondsToSelector:@selector(removeVoutSubview:)])
    {
        /* This will retain sys->glView */
        [(id)sys->container performSelectorOnMainThread:@selector(removeVoutSubview:) withObject:sys->glView waitUntilDone:NO];
    }
    /* release on main thread as explained in Open() */
    [(id)sys->container performSelectorOnMainThread:@selector(release) withObject:nil waitUntilDone:NO];
    [sys->glView performSelectorOnMainThread:@selector(removeFromSuperview) withObject:nil waitUntilDone:NO];

    [sys->glView release];

    if (sys->gl.sys != NULL)
        vout_display_opengl_Clean(&sys->vgl);

    free (sys);
}

/*****************************************************************************
 * vout display callbacks
 *****************************************************************************/

static picture_pool_t *Pool(vout_display_t *vd, unsigned requested_count)
{
    vout_display_sys_t *sys = vd->sys;
    VLC_UNUSED(requested_count);

    if (!sys->pool)
        sys->pool = vout_display_opengl_GetPool (&sys->vgl);
    assert(sys->pool);
    return sys->pool;
}

static void PictureRender(vout_display_t *vd, picture_t *pic)
{

    vout_display_sys_t *sys = vd->sys;

    vout_display_opengl_Prepare( &sys->vgl, pic );
}

static void PictureDisplay(vout_display_t *vd, picture_t *pic)
{
    vout_display_sys_t *sys = vd->sys;
    [sys->glView setVoutFlushing:YES];
    vout_display_opengl_Display(&sys->vgl, &vd->fmt );
    [sys->glView setVoutFlushing:NO];
    picture_Release (pic);
    sys->has_first_frame = true;
}

static int Control (vout_display_t *vd, int query, va_list ap)
{
    vout_display_sys_t *sys = vd->sys;

    switch (query)
    {
        case VOUT_DISPLAY_CHANGE_FULLSCREEN:
        case VOUT_DISPLAY_CHANGE_WINDOW_STATE:
        case VOUT_DISPLAY_CHANGE_DISPLAY_SIZE:
        case VOUT_DISPLAY_CHANGE_DISPLAY_FILLED:
        case VOUT_DISPLAY_CHANGE_ZOOM:
        case VOUT_DISPLAY_CHANGE_SOURCE_ASPECT:
        case VOUT_DISPLAY_CHANGE_SOURCE_CROP:
        {
            /* todo */
            return VLC_EGENERIC;
        }
        case VOUT_DISPLAY_HIDE_MOUSE:
            return VLC_SUCCESS;

        case VOUT_DISPLAY_GET_OPENGL:
        {
            vout_opengl_t **gl = va_arg (ap, vout_opengl_t **);
            *gl = &sys->gl;
            return VLC_SUCCESS;
        }

        case VOUT_DISPLAY_RESET_PICTURES:
            assert (0);
        default:
            msg_Err (vd, "Unknown request in Mac OS X vout display");
            return VLC_EGENERIC;
    }
}

/*****************************************************************************
 * vout opengl callbacks
 *****************************************************************************/
static int OpenglLock(vout_opengl_t *gl)
{
    vout_display_sys_t *sys = (vout_display_sys_t *)gl->sys;
    NSOpenGLContext *context = [sys->glView openGLContext];
    CGLError err = CGLLockContext([context CGLContextObj]);
    if (kCGLNoError == err)
    {
        [context makeCurrentContext];
        return 0;
    }
    return 1;
}

static void OpenglUnlock(vout_opengl_t *gl)
{
    vout_display_sys_t *sys = (vout_display_sys_t *)gl->sys;
    CGLUnlockContext([[sys->glView openGLContext] CGLContextObj]);
}

static void OpenglSwap(vout_opengl_t *gl)
{
    vout_display_sys_t *sys = (vout_display_sys_t *)gl->sys;
    [[sys->glView openGLContext] flushBuffer];
}

/*****************************************************************************
 * Our NSView object
 *****************************************************************************/
@implementation VLCOpenGLVideoView

#define VLCAssertMainThread() assert([[NSThread currentThread] isMainThread])

+ (void)getNewView:(NSValue *)value
{
    id *ret = [value pointerValue];
    *ret = [[self alloc] init];
}

/**
 * Gets called by the Open() method.
 */
- (id)init
{
    VLCAssertMainThread();

    /* Warning - this may be called on non main thread */

    NSOpenGLPixelFormatAttribute attribs[] =
    {
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAAccelerated,
        NSOpenGLPFANoRecovery,
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFAAlphaSize, 8,
        NSOpenGLPFADepthSize, 24,
        NSOpenGLPFAWindow,
        0
    };

    NSOpenGLPixelFormat *fmt = [[NSOpenGLPixelFormat alloc] initWithAttributes:attribs];

    if (!fmt)
        return nil;

    self = [super initWithFrame:NSMakeRect(0,0,10,10) pixelFormat:fmt];
    [fmt release];

    if (!self)
        return nil;

    /* Swap buffers only during the vertical retrace of the monitor.
     http://developer.apple.com/documentation/GraphicsImaging/
     Conceptual/OpenGL/chap5/chapter_5_section_44.html */
    GLint params[] = { 1 };
    CGLSetParameter([[self openGLContext] CGLContextObj], kCGLCPSwapInterval, params);

    [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    return self;
}

/**
 * Gets called by the Open() method.
 */
- setFrameWithValue:(NSValue *)value
{
    [self setFrame:[value rectValue]];
}

/**
 * Gets called by the Close and Open methods.
 * (Non main thread).
 */
- (void)setVoutDisplay:(vout_display_t *)aVd
{
    @synchronized(self) {
        vd = aVd;
    }
}


/**
 * Gets called when the vout will aquire the lock and flush.
 * (Non main thread).
 */
- (void)setVoutFlushing:(BOOL)flushing
{
    if (!flushing)
        return;
    @synchronized(self) {
        _hasPendingReshape = NO;
    }
}

/**
 * Can -drawRect skip rendering?.
 */
- (BOOL)canSkipRendering
{
    VLCAssertMainThread();

    @synchronized(self) {
        BOOL hasFirstFrame = vd && vd->sys->has_first_frame;
        return !_hasPendingReshape && hasFirstFrame;
    }
}


/**
 * Local method that locks the gl context.
 */
- (BOOL)lockgl
{
    VLCAssertMainThread();
    NSOpenGLContext *context = [self openGLContext];
    CGLError err = CGLLockContext([context CGLContextObj]);
    if (err == kCGLNoError)
        [context makeCurrentContext];
    return err == kCGLNoError;
}

/**
 * Local method that unlocks the gl context.
 */
- (void)unlockgl
{
    VLCAssertMainThread();
    CGLUnlockContext([[self openGLContext] CGLContextObj]);
}

/**
 * Local method that force a rendering of a frame.
 * This will get called if Cocoa forces us to redraw (via -drawRect).
 */
- (void)render
{
    VLCAssertMainThread();

    // We may have taken some times to take the opengl Lock.
    // Check here to see if we can just skip the frame as well.
    if ([self canSkipRendering])
        return;

    BOOL hasFirstFrame;
    @synchronized(self) { // vd can be accessed from multiple threads
        hasFirstFrame = vd && vd->sys->has_first_frame;
    }

    if (hasFirstFrame) {
        // This will lock gl.
        vout_display_opengl_Display( &vd->sys->vgl, &vd->source );
    }
    else
        glClear(GL_COLOR_BUFFER_BIT);
}

/**
 * Method called by Cocoa when the view is resized.
 */
- (void)reshape
{
    VLCAssertMainThread();

    NSRect bounds = [self bounds];

    CGFloat height = bounds.size.height;
    CGFloat width = bounds.size.width;

    GLint x = width, y = height;

    @synchronized(self) {
        if (vd) {
            CGFloat videoHeight = vd->source.i_visible_height;
            CGFloat videoWidth = vd->source.i_visible_width;

            GLint sarNum = vd->source.i_sar_num;
            GLint sarDen = vd->source.i_sar_den;

            if (height * videoWidth * sarNum < width * videoHeight * sarDen)
            {
                x = (height * videoWidth * sarNum) / (videoHeight * sarDen);
                y = height;
            }
            else
            {
                x = width;
                y = (width * videoHeight * sarDen) / (videoWidth * sarNum);
            }
        }
    }

    if ([self lockgl]) {
        glViewport((width - x) / 2, (height - y) / 2, x, y);

        @synchronized(self) {
            // This may be cleared before -drawRect is being called,
            // in this case we'll skip the rendering.
            // This will save us for rendering two frames (or more) for nothing
            // (one by the vout, one (or more) by drawRect)
            _hasPendingReshape = YES;
        }

        [self unlockgl];

        [super reshape];
    }
}

/**
 * Method called by Cocoa when the view is resized or the location has changed.
 * We just need to make sure we are locking here.
 */
- (void)update
{
    VLCAssertMainThread();
    BOOL success = [self lockgl];
    if (!success)
        return;

    [super update];

    [self unlockgl];
}

/**
 * Method called by Cocoa to force redraw.
 */
- (void)drawRect:(NSRect) rect
{
    VLCAssertMainThread();

    if ([self canSkipRendering])
        return;

    BOOL success = [self lockgl];
    if (!success)
        return;

    [self render];

    [self unlockgl];
}

- (void)renewGState
{
    NSWindow *window = [self window];

    // Remove flashes with splitter view.
	if ([window respondsToSelector:@selector(disableScreenUpdatesUntilFlush)])
		[window disableScreenUpdatesUntilFlush];

    [super renewGState];
}

- (BOOL)mouseDownCanMoveWindow
{
    return YES;
}

- (BOOL)isOpaque
{
    return YES;
}
@end
