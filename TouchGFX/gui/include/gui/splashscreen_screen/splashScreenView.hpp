#ifndef SPLASHSCREENVIEW_HPP
#define SPLASHSCREENVIEW_HPP

#include <gui_generated/splashscreen_screen/splashScreenViewBase.hpp>
#include <gui/splashscreen_screen/splashScreenPresenter.hpp>

class splashScreenView : public splashScreenViewBase
{
public:
    splashScreenView();
    virtual ~splashScreenView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();


    virtual void handleTickEvent();

protected:
};

#endif // SPLASHSCREENVIEW_HPP
