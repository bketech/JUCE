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


typedef bool (*EventQueryCallback)();
extern EventQueryCallback eventQueryCallback;

EGLDisplay eglDisplay = nullptr;
EGLNativeDisplayType nativeDisplay = nullptr;
EGLNativeWindowType nativeWindow = 0;
typedef pointer_sized_uint WindowID;



#include <GLES2/gl2.h>
class EglRenderer :  public OpenGLRenderer
{
public:

    EglRenderer() { }
    ~EglRenderer() { }

    void newOpenGLContextCreated() override { }
    void openGLContextClosing() override { }

    void renderOpenGL() override
    {
        glClear (0);
    }
};

class EglWindow
{
public:

    EglWindow()
        : context (nullptr),
          surface (EGL_NO_SURFACE),
          window (0)
    {
        id = nextId();
    }

    ~EglWindow()
    {

    }

    OpenGLContext* getContext() const { return context; }
    WindowID getId() const { return id; }
    EGLSurface getSurface() const { return surface; }
    EGLNativeWindowType getNativeWindow() const { return window; }

protected:

    EGLSurface surface;
    EGLNativeWindowType window;

private:

    ScopedPointer<OpenGLContext> context;
    EGLConfig config;
    WindowID id;

    static WindowID nextId()
    {
        static WindowID lastId = 0;
        return ++lastId;
    }
};

class EglPlatform::Pimpl
{
public:

    Pimpl()
        : nativeWindow (0)
    {

    }

    ~Pimpl() { }

    ScopedPointer<OpenGLContext> context;
    ScopedPointer<EglRenderer> renderer;
    ScopedPointer<Component> dummy;
    EGLNativeWindowType nativeWindow;

private:

};

EglPlatform::EglPlatform()
{
    pimpl = new Pimpl();
}

OpenGLContext* EglPlatform::getOpenGLContext() const
{
    return pimpl->context;
}

void EglPlatform::addPeer (ComponentPeer* peer)
{
    Component& comp = peer->getComponent();
    Rectangle<int> b (comp.getLocalBounds());

    if (! pimpl->context)
    {
        pimpl->nativeWindow = nativeWindow = createNativeWindow (getScreenSize().getX(),
                                                                 getScreenSize().getY());
        pimpl->context = new OpenGLContext();
        pimpl->context->attachTo (comp);
        pimpl->context->triggerRepaint();
        peer->handleMovedOrResized();
    }

    comp.setBounds (0, 0, getScreenSize().getX(), getScreenSize().getY());
    peers.add (peer);
}

void EglPlatform::removePeer (ComponentPeer* peer)
{
    peers.removeFirstMatchingValue (peer);
    if (peers.size() == 0)
    {
        pimpl->context->detach();
        pimpl->context = nullptr;
    }
}

void EglPlatform::initialise()
{
    initialisePlatform();

    if (! eglBindAPI (EGL_OPENGL_ES_API))
        Logger::writeToLog ("EGL: could not bind GL_ES API");

    eglDisplay = eglGetDisplay (getNativeDisplay());

    if (eglDisplay == EGL_NO_DISPLAY)
        Logger::writeToLog ("EGL: could not open egl display");

    EGLint major, minor;

    if (! eglInitialize (eglDisplay, &major, &minor))
    {
        Logger::writeToLog ("EGL: could not initialize egl display");
        Process::terminate();
    }

}

void EglPlatform::initialisePlatform() { }

void EglPlatform::shutdown()
{
    shutdownPlatform();
}

void EglPlatform::shutdownPlatform() { }

EGLNativeWindowType EglPlatform::createNativeWindow (int width, int height) { return (EGLNativeWindowType)0; }
void EglPlatform::destroyNativeWindow (EGLNativeWindowType /*window*/) { }


static ScopedPointer<EglPlatform> platform = nullptr;

bool EglPlatform::queryEventsCallback()     { return platform->processEvents(); }
bool EglPlatform::processEvents()
{
    getNextEvent();
    return true;
}


EglPlatform* EglPlatform::getInstance()
{
    if (platform == nullptr)
    {
        jassert (eventQueryCallback == nullptr);
        platform = EglPlatform::createPlatform();
        platform->initialise();
        eventQueryCallback = &EglPlatform::queryEventsCallback;
    }

    return platform;
}
