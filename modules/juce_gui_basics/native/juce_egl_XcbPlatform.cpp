
#if JUCE_COMPLETION
#include "../../../juce.h"
#include "juce_egl_Platform.h"
#include <EGL/egl.h>
#include <xcb/xcb.h>
#include <X11/Xlib-xcb.h>
#endif

//==============================================================================
namespace XcbKeys
{
    static ModifierKeys currentModifiers;

    enum MouseButtons
    {
        NoButton = 0,
        LeftButton = 1,
        MiddleButton = 2,
        RightButton = 3,
        WheelUp = 4,
        WheelDown = 5
    };

    static int AltMask = 0;
    static int NumLockMask = 0;
    static bool numLock = false;
    static bool capsLock = false;
    static char keyStates [32];
    static const int extendedKeyModifier = 0x10000000;
}

//==============================================================================
namespace XcbAtoms
{
    enum
    {
        netWmName = 0,
        utf8String,
        wmProtocols,
        wmDeleteWindow,
        netWmState,
        netWmStateFullscreen,
        numAtoms
    };
}

class EglXcbPlatform :  public EglPlatform
{

public:

    EglXcbPlatform()
        : connection (nullptr),
          window (0)
    {
    }

    ~EglXcbPlatform()
    {
    }

    bool getNextEvent() override
    {
        if (! connection)
            return false;

        if (const xcb_generic_event_t* ev = xcb_poll_for_event (connection))
            handleGenericEvent (ev);

        return true;
    }

    xcb_connection_t *getConnection() const { return connection; }
    const xcb_atom_t *getAtoms() const { return atoms; }

    void handleGenericEvent (const xcb_generic_event_t* ev)
    {
        const uint32 responseType = ev->response_type & ~0x80;

        switch (responseType)
        {
            case XCB_BUTTON_PRESS:
            {
                xcb_button_press_event_t *press = (xcb_button_press_event_t*) ev;
                Point<int> point (press->event_x, press->event_y);
                handleButtonPress (point, press->detail, press->state, press->time);
                break;
            }
            case XCB_BUTTON_RELEASE:
            {
                xcb_button_release_event_t *release = (xcb_button_release_event_t *) ev;
                Point<int> point (release->event_x, release->event_y);
                handleButtonRelease (point, release->detail, release->state, release->time);
            }
            case XCB_MOTION_NOTIFY:
            {
                xcb_motion_notify_event_t *motion = (xcb_motion_notify_event_t *) ev;
                Point<int> point (motion->event_x, motion->event_y);
                handleMotionNotify (point, motion->detail, motion->state, motion->time);
                break;
            }
            case XCB_EXPOSE:
            {
                xcb_expose_event_t* exp = (xcb_expose_event_t*) ev;
                const Rectangle<int> bounds (exp->x, exp->y, exp->width, exp->height);
            }
            case XCB_CLIENT_MESSAGE:
            {
                xcb_client_message_event_t *client = (xcb_client_message_event_t *) ev;
                const xcb_atom_t *atoms = getAtoms();
                if (client->format == 32
                     && client->type == atoms [XcbAtoms::wmProtocols]
                     && client->data.data32[0] == atoms [XcbAtoms::wmDeleteWindow])
                {
                    JUCEApplicationBase::quit();
                }
                break;
            }
            default:
            {
                DBG ("EGL: unhandled xcb event: " + String(responseType));
                break;
            }
        }
    }

    void sendAtom (xcb_atom_t atom)
    {
        xcb_client_message_event_t event;
        zerostruct (event);

        event.response_type = XCB_CLIENT_MESSAGE;
        event.format = 32;
        event.sequence = 0;
        event.window = eventWindow;
        event.type = atom;

        xcb_send_event (connection, false, eventWindow,
                        XCB_EVENT_MASK_NO_EVENT, (const char*) &event);
        xcb_flush (connection);
    }

    void initialisePlatform()
    {
        display = XOpenDisplay (0);
        if (! display)
        {
            Logger::writeToLog ("EGL: could not open x display");
            Process::terminate();
        }

        XSetEventQueueOwner (display, XCBOwnsEventQueue);
        connection = XGetXCBConnection (display);

        xcb_screen_iterator_t it = xcb_setup_roots_iterator (xcb_get_setup (connection));
        eventWindow = xcb_generate_id (connection);
        xcb_create_window (connection, XCB_COPY_FROM_PARENT,
                           eventWindow, it.data->root,
                           0, 0, 1, 1, 0, XCB_WINDOW_CLASS_INPUT_ONLY,
                           it.data->root_visual, 0, 0);
    }

    void shutdownPlatform()
    {
        sendAtom (XCB_ATOM_NONE);
        XCloseDisplay (display);
        display = 0;
        connection = nullptr;
    }

    EGLNativeDisplayType getNativeDisplay() const { return display; }

    Point<int> getScreenSize() const
    {
        // TODO check for some environment variables for this
        if (screenSize.isOrigin())
        {
            screenSize = Point<int> (320, 240);
            Logger::writeToLog ("EGL: screen size not set, falling back to 640x480");
        }

        return screenSize;
    }

    EGLNativeWindowType createNativeWindow (int width, int height)
    {
        xcb_screen_iterator_t it = xcb_setup_roots_iterator (xcb_get_setup (connection));
        window = xcb_generate_id (connection);

        const uint32 mask = XCB_CW_EVENT_MASK;
        const uint32 valwin[] = { XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS |
                                  XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION };

        xcb_create_window (connection, XCB_COPY_FROM_PARENT, window, it.data->root,
                           0, 0, (uint16_t) width, (uint16_t) height, 0,
                           XCB_WINDOW_CLASS_INPUT_OUTPUT, it.data->root_visual,
                           mask, valwin);

        xcb_map_window (connection, window);

        static const char *atomNames [XcbAtoms::numAtoms] = {
            "_NET_WM_NAME",
            "UTF8_STRING",
            "WM_PROTOCOLS",
            "WM_DELETE_WINDOW",
            "_NET_WM_STATE",
            "_NET_WM_STATE_FULLSCREEN"
        };

        xcb_intern_atom_cookie_t cookies [XcbAtoms::numAtoms];
        for (int i = 0; i < XcbAtoms::numAtoms; ++i)
        {
            cookies[i] = xcb_intern_atom (connection, false, strlen (atomNames[i]), atomNames[i]);
            xcb_intern_atom_reply_t *reply = xcb_intern_atom_reply (connection, cookies[i], 0);
            atoms[i] = reply->atom;
            std::free (reply);
        }

        xcb_change_property (connection, XCB_PROP_MODE_REPLACE, window,
                             atoms [XcbAtoms::netWmName],
                             atoms [XcbAtoms::utf8String], 8, 8, "Juce EGL");

        xcb_change_property (connection, XCB_PROP_MODE_REPLACE, window,
                             atoms [XcbAtoms::wmProtocols], XCB_ATOM_ATOM, 32, 1,
                             &atoms [XcbAtoms::wmDeleteWindow]);

        static bool doFullscreen = false;
        if (doFullscreen)
        {
            xcb_change_property (connection, XCB_PROP_MODE_REPLACE, window,
                                 atoms [XcbAtoms::netWmState], XCB_ATOM_ATOM, 32, 1,
                                 &atoms [XcbAtoms::netWmStateFullscreen]);
        }

        xcb_flush (connection);

        return window;
    }

    static ModifierKeys::Flags translateMouseButton (uint32 button)
    {
        switch (button)
        {
            case XcbKeys::NoButton:     return ModifierKeys::noModifiers;
            case XcbKeys::RightButton:  return ModifierKeys::rightButtonModifier; break;
            case XcbKeys::LeftButton:   return ModifierKeys::leftButtonModifier;  break;
            case XcbKeys::MiddleButton: return ModifierKeys::middleButtonModifier; break;
        }

        jassertfalse;
        return ModifierKeys::noModifiers;
    }

    void handleButtonRelease (const Point<int>& p, uint32 btn, uint32 st, uint32 time)
    {
        switch (btn)
        {
            case XcbKeys::RightButton:
            case XcbKeys::LeftButton:
            case XcbKeys::MiddleButton:
            {
                const int buttonFlag = translateMouseButton (btn);
                XcbKeys::currentModifiers = XcbKeys::currentModifiers.withoutFlags (buttonFlag);
                getOpenGLContext()->getTargetComponent()->getPeer()->handleMouseEvent (0, p, XcbKeys::currentModifiers, time);
                break;
            }
        }
    }

    void handleButtonPress (const Point<int>& p, uint32 btn, uint32 st, uint32 time)
    {
        switch (btn)
        {
            case XcbKeys::RightButton:
            case XcbKeys::LeftButton:
            case XcbKeys::MiddleButton:
            {
                const int buttonFlag = translateMouseButton (btn);
                XcbKeys::currentModifiers = XcbKeys::currentModifiers.withFlags (buttonFlag);
                getOpenGLContext()->getTargetComponent()->getPeer()->toFront (true);
                getOpenGLContext()->getTargetComponent()->getPeer()->handleMouseEvent (0, p, XcbKeys::currentModifiers, time);
                break;
            }
        }
    }

    void handleMotionNotify (const Point<int>& p, uint32 btn, uint32 st, uint32 time)
    {
        getOpenGLContext()->getTargetComponent()->getPeer()->
                handleMouseEvent (0, p, XcbKeys::currentModifiers, time);
    }

    void destroyNativeWindow (EGLNativeWindowType window)
    {
        xcb_destroy_window (connection, window);
    }

    Display *display;
    xcb_connection_t *connection;
    xcb_atom_t atoms [XcbAtoms::numAtoms];
    xcb_window_t window, eventWindow;

    mutable Point<int> screenSize;
};


EglPlatform* EglPlatform::createPlatform()
{
    return new EglXcbPlatform();
}
