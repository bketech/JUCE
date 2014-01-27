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

#ifndef JUCE_EGLPLATFORM_INCLUDED_H
#define JUCE_EGLPLATFORM_INCLUDED_H

class EglPlatform
{
public:
    virtual ~EglPlatform() { }

    void initialise();
    void shutdown();

    virtual EGLNativeDisplayType getNativeDisplay() const { return (EGLNativeDisplayType) 0; }
    virtual EGLContext getContext() const { return EGL_NO_CONTEXT; }
    virtual Point<int> getScreenSize() const { return Point<int> (1, 1); }

    void addPeer (ComponentPeer* peer);
    void removePeer (ComponentPeer* peer);

    static EglPlatform* getInstance();


protected:

    EglPlatform();

    OpenGLContext* getOpenGLContext() const;
    virtual void initialisePlatform();
    virtual void shutdownPlatform();
    virtual EGLNativeWindowType createNativeWindow (int width, int height);
    virtual void destroyNativeWindow (EGLNativeWindowType window);
    virtual bool getNextEvent() { return true; }

private:

    class Pimpl;
    ScopedPointer<Pimpl> pimpl;
    Array<ComponentPeer*> peers;

    // This method is implemented in backend specific files
    static EglPlatform* createPlatform();

    // This is wired in with juce_events
    static bool queryEventsCallback();
    bool processEvents();

};

#endif  // JUCE_EGLPLATFORM_INCLUDED_H
