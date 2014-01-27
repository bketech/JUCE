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

#if JUCE_COMPLETION
#include "../../juce_gui_extra/juce_gui_extra.h"
#include <EGL/egl.h>
#endif

//==============================================================================
class EglComponentPeer  : public ComponentPeer
{
    enum { KeyPressEventType = 2 };
    Rectangle<int> bounds;

public:

    EglComponentPeer (Component& comp, const int windowStyleFlags)
        : ComponentPeer (comp, windowStyleFlags),
          component (comp),
          fullScreen (false),
          sizeAllocated (0),
          depth (32)
    {
        bounds = comp.getBounds();

        EglPlatform* platform = EglPlatform::getInstance();
        platform->addPeer (this);

        if (isFocused())
            handleFocusGain();
    }

    ~EglComponentPeer()
    {
        EglPlatform* platform = EglPlatform::getInstance();
        platform->removePeer (this);
    }

    void* getNativeHandle() const override { return (void*) 0; }
    void setVisible (bool shouldBeVisible) override { (void) shouldBeVisible; }
    void setTitle (const String& title) override { }

    void setBounds (const Rectangle<int>& r, bool /*isNowFullScreen*/) override
    {
        if (bounds == r)
            return;

        Logger::writeToLog ("EGL: " + component.getName() + " bounds: " + r.toString());
        bounds = r;
    }

    Rectangle<int> getBounds() const override
    {
        return bounds;
    }

    void handleScreenSizeChange() override
    {
        ComponentPeer::handleScreenSizeChange();

        if (isFullScreen())
            setFullScreen (true);
    }

    Point<int> getScreenPosition() const
    {
        return bounds.getTopLeft();
    }

    Point<int> localToGlobal (Point<int> relativePosition) override
    {
        return relativePosition + getScreenPosition();
    }

    Point<int> globalToLocal (Point<int> screenPosition) override
    {
        return screenPosition - getScreenPosition();
    }

    void setMinimised (bool shouldBeMinimised) override
    {
        // n/a
    }

    bool isMinimised() const override
    {
        return false;
    }

    void setFullScreen (bool shouldBeFullScreen) override
    {
        Rectangle<int> r (shouldBeFullScreen ? Desktop::getInstance().getDisplays().getMainDisplay().userArea
                                             : lastNonFullscreenBounds);

        if ((! shouldBeFullScreen) && r.isEmpty())
            r = getBounds();

        // (can't call the component's setBounds method because that'll reset our fullscreen flag)
        if (! r.isEmpty())
            setBounds (r, shouldBeFullScreen);

        component.repaint();
    }

    bool isFullScreen() const override
    {
        return fullScreen;
    }

    void setIcon (const Image& newIcon) override
    {
        // n/a
    }

    bool contains (Point<int> localPos, bool trueIfInAChildWindow) const override
    {
        return isPositiveAndBelow (localPos.x, component.getWidth())
            && isPositiveAndBelow (localPos.y, component.getHeight());
    }

    BorderSize<int> getFrameSize() const override
    {
        // TODO
        return BorderSize<int>();
    }

    bool setAlwaysOnTop (bool alwaysOnTop) override
    {
        // TODO
        return false;
    }

    void toFront (bool makeActive) override
    {
        Logger::writeToLog ("EGL: peer to front " + String((int) makeActive));
    }

    void toBehind (ComponentPeer* other) override
    {
        Logger::writeToLog ("EGL: peer to behind of " + other->getComponent().getName());
    }

    //==============================================================================
    void handleMouseDownCallback (int index, Point<float> pos, int64 time)
    {
        lastMousePos = pos;

        // this forces a mouse-enter/up event, in case for some reason we didn't get a mouse-up before.
        handleMouseEvent (index, pos.toInt(), currentModifiers.withoutMouseButtons(), time);

        if (isValidPeer (this))
            handleMouseDragCallback (index, pos, time);
    }

    void handleMouseDragCallback (int index, Point<float> pos, int64 time)
    {
        lastMousePos = pos;

        jassert (index < 64);
        touchesDown = (touchesDown | (1 << (index & 63)));
        currentModifiers = currentModifiers.withoutMouseButtons().withFlags (ModifierKeys::leftButtonModifier);
        handleMouseEvent (index, pos.toInt(), currentModifiers.withoutMouseButtons()
                                                  .withFlags (ModifierKeys::leftButtonModifier), time);
    }

    void handleMouseUpCallback (int index, Point<float> pos, int64 time)
    {
        lastMousePos = pos;

        jassert (index < 64);
        touchesDown = (touchesDown & ~(1 << (index & 63)));

        if (touchesDown == 0)
            currentModifiers = currentModifiers.withoutMouseButtons();

        handleMouseEvent (index, pos.toInt(), currentModifiers.withoutMouseButtons(), time);
    }

    void handleKeyDownCallback (int k, int kc)
    {
        handleKeyPress (k, kc);
    }

    void handleKeyUpCallback (int k, int kc)
    {
    }

    //==============================================================================
    bool isFocused() const override
    {
        return true;
    }

    void grabFocus() override
    {
    }

    void handleFocusChangeCallback (bool hasFocus)
    {
        if (hasFocus)
            handleFocusGain();
        else
            handleFocusLoss();
    }

    void textInputRequired (const Point<int>&) override
    {
    }

    void dismissPendingTextInput() override
    {
    }

    void repaint (const Rectangle<int>& area) override
    {
        // repainter->repaint (area.getIntersection (component.getLocalBounds()));
    }

    void performAnyPendingRepaintsNow() override
    {
        // repainter->performAnyPendingRepaintsNow();
    }

    void setAlpha (float newAlpha) override
    {
        // TODO
    }

    StringArray getAvailableRenderingEngines() override
    {
        return StringArray ("EGL Renderer");
    }

    //==============================================================================
    static ModifierKeys currentModifiers;
    static Point<float> lastMousePos;
    static int64 touchesDown;

private:
    //==============================================================================
    Component& component;
    bool fullScreen;
    int sizeAllocated, depth;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EglComponentPeer)
};

ModifierKeys EglComponentPeer::currentModifiers = 0;
Point<float> EglComponentPeer::lastMousePos;
int64 EglComponentPeer::touchesDown = 0;

//==============================================================================
ComponentPeer* Component::createNewPeer (int styleFlags, void*)
{
    return new EglComponentPeer (*this, styleFlags);
}

//==============================================================================
bool Desktop::canUseSemiTransparentWindows() noexcept
{
    return true;
}

double Desktop::getDefaultMasterScale()
{
    return 1.0;
}

Desktop::DisplayOrientation Desktop::getCurrentOrientation() const
{
    // TODO
    return upright;
}

bool MouseInputSource::SourceList::addSource()
{
    addSource (sources.size(), false);
    return true;
}

Point<int> MouseInputSource::getCurrentRawMousePosition()
{
    return EglComponentPeer::lastMousePos.toInt();
}

void MouseInputSource::setRawMousePosition (Point<int>)
{
    // not needed
}

void ModifierKeys::updateCurrentModifiers() noexcept
{
    currentModifiers = EglComponentPeer::currentModifiers;
}

ModifierKeys ModifierKeys::getCurrentModifiersRealtime() noexcept
{
    return EglComponentPeer::currentModifiers;
}

//==============================================================================
// TODO
JUCE_API bool JUCE_CALLTYPE Process::isForegroundProcess() { return true; }
JUCE_API void JUCE_CALLTYPE Process::makeForegroundProcess() { }
JUCE_API void JUCE_CALLTYPE Process::hide() { }

void
NativeMessageBox::showMessageBox (AlertWindow::AlertIconType,
                                  String const&,
                                  String const&,
                                  juce::Component*)
{

}

//==============================================================================
void JUCE_CALLTYPE NativeMessageBox::showMessageBoxAsync (AlertWindow::AlertIconType iconType,
                                                          const String& title, const String& message,
                                                          Component* associatedComponent,
                                                          ModalComponentManager::Callback* callback)
{

}

bool JUCE_CALLTYPE NativeMessageBox::showOkCancelBox (AlertWindow::AlertIconType iconType,
                                                      const String& title, const String& message,
                                                      Component* associatedComponent,
                                                      ModalComponentManager::Callback* callback)
{
    return false;
}

int JUCE_CALLTYPE NativeMessageBox::showYesNoCancelBox (AlertWindow::AlertIconType iconType,
                                                        const String& title, const String& message,
                                                        Component* associatedComponent,
                                                        ModalComponentManager::Callback* callback)
{
    return 0;
}

//==============================================================================
void Desktop::setScreenSaverEnabled (const bool isEnabled)
{
    // TODO
}

bool Desktop::isScreenSaverEnabled()
{
    return true;
}

//==============================================================================
void Desktop::setKioskComponent (Component* kioskModeComponent, bool enableOrDisable, bool allowMenusAndBars)
{
    // TODO
}

//==============================================================================
bool juce_areThereAnyAlwaysOnTopWindows()
{
    return false;
}

//==============================================================================
void Desktop::Displays::findDisplays (float masterScale)
{
    EglPlatform* platform = EglPlatform::getInstance();

    Display d;
    d.userArea = d.totalArea = Rectangle<int> (platform->getScreenSize().getX(),
                                               platform->getScreenSize().getY()) / masterScale;
    d.isMain = true;
    d.scale = masterScale;
    d.dpi = 100;

    displays.add (d);
}

//==============================================================================
Image juce_createIconForFile (const File& file)
{
    return Image::null;
}

//==============================================================================
void* CustomMouseCursorInfo::create() const                                                     { return nullptr; }
void* MouseCursor::createStandardMouseCursor (const MouseCursor::StandardCursorType)            { return nullptr; }
void MouseCursor::deleteMouseCursor (void* const /*cursorHandle*/, const bool /*isStandard*/)   {}

//==============================================================================
void MouseCursor::showInWindow (ComponentPeer*) const   {}
void MouseCursor::showInAllWindows() const  {}

//==============================================================================
bool DragAndDropContainer::performExternalDragDropOfFiles (const StringArray& files, const bool canMove)
{
    return false;
}

bool DragAndDropContainer::performExternalDragDropOfText (const String& text)
{
    return false;
}

//==============================================================================
void LookAndFeel::playAlertSound()
{
}

//==============================================================================
void SystemClipboard::copyTextToClipboard (const String& text)
{
}

String SystemClipboard::getTextFromClipboard()
{
    return String::empty;
}


//==============================================================================
const int extendedKeyModifier       = 0x10000;

#undef KeyPress

//==============================================================================
bool KeyPress::isKeyCurrentlyDown (const int keyCode)
{
    // TODO
    return false;
}

const int KeyPress::spaceKey        = ' ';
const int KeyPress::returnKey       = 66;
const int KeyPress::escapeKey       = 4;
const int KeyPress::backspaceKey    = 67;
const int KeyPress::leftKey         = extendedKeyModifier + 1;
const int KeyPress::rightKey        = extendedKeyModifier + 2;
const int KeyPress::upKey           = extendedKeyModifier + 3;
const int KeyPress::downKey         = extendedKeyModifier + 4;
const int KeyPress::pageUpKey       = extendedKeyModifier + 5;
const int KeyPress::pageDownKey     = extendedKeyModifier + 6;
const int KeyPress::endKey          = extendedKeyModifier + 7;
const int KeyPress::homeKey         = extendedKeyModifier + 8;
const int KeyPress::deleteKey       = extendedKeyModifier + 9;
const int KeyPress::insertKey       = -1;
const int KeyPress::tabKey          = 61;
const int KeyPress::F1Key           = extendedKeyModifier + 10;
const int KeyPress::F2Key           = extendedKeyModifier + 11;
const int KeyPress::F3Key           = extendedKeyModifier + 12;
const int KeyPress::F4Key           = extendedKeyModifier + 13;
const int KeyPress::F5Key           = extendedKeyModifier + 14;
const int KeyPress::F6Key           = extendedKeyModifier + 16;
const int KeyPress::F7Key           = extendedKeyModifier + 17;
const int KeyPress::F8Key           = extendedKeyModifier + 18;
const int KeyPress::F9Key           = extendedKeyModifier + 19;
const int KeyPress::F10Key          = extendedKeyModifier + 20;
const int KeyPress::F11Key          = extendedKeyModifier + 21;
const int KeyPress::F12Key          = extendedKeyModifier + 22;
const int KeyPress::F13Key          = extendedKeyModifier + 23;
const int KeyPress::F14Key          = extendedKeyModifier + 24;
const int KeyPress::F15Key          = extendedKeyModifier + 25;
const int KeyPress::F16Key          = extendedKeyModifier + 26;
const int KeyPress::numberPad0      = extendedKeyModifier + 27;
const int KeyPress::numberPad1      = extendedKeyModifier + 28;
const int KeyPress::numberPad2      = extendedKeyModifier + 29;
const int KeyPress::numberPad3      = extendedKeyModifier + 30;
const int KeyPress::numberPad4      = extendedKeyModifier + 31;
const int KeyPress::numberPad5      = extendedKeyModifier + 32;
const int KeyPress::numberPad6      = extendedKeyModifier + 33;
const int KeyPress::numberPad7      = extendedKeyModifier + 34;
const int KeyPress::numberPad8      = extendedKeyModifier + 35;
const int KeyPress::numberPad9      = extendedKeyModifier + 36;
const int KeyPress::numberPadAdd            = extendedKeyModifier + 37;
const int KeyPress::numberPadSubtract       = extendedKeyModifier + 38;
const int KeyPress::numberPadMultiply       = extendedKeyModifier + 39;
const int KeyPress::numberPadDivide         = extendedKeyModifier + 40;
const int KeyPress::numberPadSeparator      = extendedKeyModifier + 41;
const int KeyPress::numberPadDecimalPoint   = extendedKeyModifier + 42;
const int KeyPress::numberPadEquals         = extendedKeyModifier + 43;
const int KeyPress::numberPadDelete         = extendedKeyModifier + 44;
const int KeyPress::playKey         = extendedKeyModifier + 45;
const int KeyPress::stopKey         = extendedKeyModifier + 46;
const int KeyPress::fastForwardKey  = extendedKeyModifier + 47;
const int KeyPress::rewindKey       = extendedKeyModifier + 48;
