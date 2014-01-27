/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2013 - Raw Material Software Ltd.

   Permission is granted to use this software under the terms of either:
   a) the GPL v2 (or any later version)
   b) the Affero GPL v3

   Details of these licenses can be found at: www.gnu.org/licenses

   JUCE is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
   A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

   ------------------------------------------------------------------------------

   To release a closed-source product which uses JUCE, commercial licenses are
   available: visit www.juce.com for more information.

  ==============================================================================
*/

extern EGLDisplay eglDisplay;
extern EGLNativeWindowType nativeWindow;

//==============================================================================
class OpenGLContext::NativeContext
{
public:
    NativeContext (Component& component,
                   const OpenGLPixelFormat& pixelFormat,
                   void* shareContext,
                   bool /*useMultisampling*/)
        : config (nullptr),
          context (EGL_NO_CONTEXT),
          surface (EGL_NO_SURFACE),
          swapFrames (0),
          sharedContext (shareContext)
    {
        bounds = component.getBounds();

        EGLint attr[] = {
            EGL_BUFFER_SIZE,        pixelFormat.depthBufferBits,
            EGL_RED_SIZE,           pixelFormat.redBits,
            EGL_GREEN_SIZE,         pixelFormat.greenBits,
            EGL_BLUE_SIZE,          pixelFormat.blueBits,
            EGL_RENDERABLE_TYPE,    EGL_OPENGL_ES2_BIT,
            EGL_NONE
        };

        EGLint numConfigs;
        if (! eglChooseConfig (eglDisplay, attr, &config, 1, &numConfigs))
        {
            Logger::writeToLog ("Failed to choose config (eglError: " + String (eglGetError()) + String(")"));
            return;
        }

        if (numConfigs != 1)
        {
            Logger::writeToLog ("Didn't get exactly one config, but " + String (numConfigs));
            return;
        }

        EGLint satts[] = {
            EGL_NONE
        };

        surface = eglCreateWindowSurface (eglDisplay, config, nativeWindow, satts);
        if (surface == EGL_NO_SURFACE) {

            Logger::writeToLog ("Unable to create EGL surface (eglError: " + String(eglGetError()) + String(")"));
        } else {
            Logger::writeToLog ("EGL: Surface was created");
        }
    }

    ~NativeContext()
    {
        if (! eglDestroySurface (eglDisplay, surface))
            Logger::writeToLog ("EGL: surface not destroyed");
    }

    void initialiseOnRenderThread (OpenGLContext& ctx)
    {
        EGLint atts[] = {
            EGL_CONTEXT_CLIENT_VERSION, 2,
            EGL_NONE
        };
        context = eglCreateContext (eglDisplay, config, (EGLContext) sharedContext, atts);
        ctx.makeActive();
    }

    void shutdownOnRenderThread()
    {
        deactivateCurrentContext();
        if (! eglDestroyContext (eglDisplay, context))
            Logger::writeToLog ("EGL: Context not destroyed");

        context = EGL_NO_CONTEXT;
    }

    bool makeActive() const noexcept
    {
        return context != EGL_NO_CONTEXT
             && surface != EGL_NO_SURFACE
             && EGL_TRUE == eglMakeCurrent (eglDisplay, surface, surface, context);
    }

    bool isActive() const noexcept
    {
        return eglGetCurrentContext() == context && context != EGL_NO_CONTEXT;
    }

    static void deactivateCurrentContext()
    {
        if (EGL_TRUE != eglMakeCurrent (eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT))
            Logger::writeToLog("EGL: could not deactivate context");
    }

    void swapBuffers()
    {
        eglSwapBuffers (eglDisplay, surface);
    }

    void updateWindowPosition (const Rectangle<int>& newBounds)
    {
        bounds = newBounds;
    }

    bool setSwapInterval (int numFramesPerSwap)
    {
        if (numFramesPerSwap == swapFrames)
            return true;

        if (! eglSwapInterval (eglDisplay, (EGLint) numFramesPerSwap))
            return false;

        swapFrames = numFramesPerSwap;
        return true;
    }

    int getSwapInterval() const                 { return swapFrames; }
    bool createdOk() const noexcept             { return true; }
    void* getRawContext() const noexcept        { return (void*) context; }
    GLuint getFrameBufferID() const noexcept    { return 0; }

    struct Locker { Locker (NativeContext&) {} };

private:

    EGLConfig  config;
    EGLContext context;
    EGLSurface surface;

    int swapFrames;
    Rectangle<int> bounds;
    void* sharedContext;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NativeContext)
};

//==============================================================================
bool OpenGLHelpers::isContextActive()
{
    return eglGetCurrentContext() != EGL_NO_CONTEXT;
}
