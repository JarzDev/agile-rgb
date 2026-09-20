/*---------------------------------------------------------*\
| AgileRgbLightingHelper.cpp                                 |
|                                                             |
|   Shared lighting logic for the simplified Agile Rgb        |
|   screens (fixed color/breathing, rainbow). Prefers a       |
|   device's native firmware mode when available (matched     |
|   by name) and falls back to a software-driven effect       |
|   otherwise.                                                |
|                                                             |
|   This file is part of the Agile Rgb project                |
|   SPDX-License-Identifier: GPL-2.0-or-later                 |
\*---------------------------------------------------------*/

#include "AgileRgbLightingHelper.h"
#include "ResourceManager.h"

#include <QVariant>

#include <algorithm>
#include <cctype>
#include <cmath>

static const int    EFFECT_TICK_MS = 33; /* ~30 FPS for software effects */
static const double TWO_PI         = 6.283185307179586;

AgileRgbLightingHelper::AgileRgbLightingHelper(QObject *parent) :
    QObject(parent)
{
    effect_timer = new QTimer(this);
    phase        = 0.0;
    rainbow_left_to_right = true;
}

AgileRgbLightingHelper::~AgileRgbLightingHelper()
{
    StopSoftwareTimer();
}

void AgileRgbLightingHelper::StopSoftwareTimer()
{
    effect_timer->stop();
    disconnect(effect_timer, nullptr, this, nullptr);
}

int AgileRgbLightingHelper::FindModeByName(RGBController* controller, const char* needle)
{
    std::string needle_lower = needle;
    std::transform(needle_lower.begin(), needle_lower.end(), needle_lower.begin(),
                    [](unsigned char c) { return std::tolower(c); });

    unsigned int mode_count = controller->GetModeCount();

    for(unsigned int mode_idx = 0; mode_idx < mode_count; mode_idx++)
    {
        std::string mode_name = controller->GetModeName(mode_idx);
        std::transform(mode_name.begin(), mode_name.end(), mode_name.begin(),
                        [](unsigned char c) { return std::tolower(c); });

        if(mode_name.find(needle_lower) != std::string::npos)
        {
            return (int)mode_idx;
        }
    }

    return -1;
}

void AgileRgbLightingHelper::EnsureDirectMode(RGBController* controller)
{
    int direct_mode = FindModeByName(controller, "direct");

    if(direct_mode < 0)
    {
        direct_mode = FindModeByName(controller, "custom");
    }

    if(direct_mode < 0)
    {
        direct_mode = FindModeByName(controller, "static");
    }

    if(direct_mode < 0)
    {
        return;
    }

    unsigned int flags = controller->GetModeFlags((unsigned int)direct_mode);

    if(flags & MODE_FLAG_HAS_BRIGHTNESS)
    {
        controller->SetModeBrightness((unsigned int)direct_mode, controller->GetModeBrightnessMax((unsigned int)direct_mode));
    }

    controller->SetActiveMode(direct_mode);
}

void AgileRgbLightingHelper::ApplyStaticColor(const QColor& color, unsigned int brightness_percent)
{
    StopSoftwareTimer();

    double factor = std::clamp((double)brightness_percent, 0.0, 100.0) / 100.0;

    QColor scaled(
        (int)(color.red()   * factor),
        (int)(color.green() * factor),
        (int)(color.blue()  * factor)
    );

    RGBColor target_color = ToRGBColor(scaled.red(), scaled.green(), scaled.blue());

    std::vector<RGBController*>& controllers = ResourceManager::get()->GetRGBControllers();

    for(RGBController* controller : controllers)
    {
        EnsureDirectMode(controller);
        controller->SetAllColors(target_color);
        controller->UpdateLEDs();
    }
}

void AgileRgbLightingHelper::StartBreathing(const QColor& color, unsigned int speed_percent)
{
    StopSoftwareTimer();

    breathing_color = color;

    std::vector<RGBController*>& controllers = ResourceManager::get()->GetRGBControllers();
    software_fallback_controllers.clear();

    for(RGBController* controller : controllers)
    {
        int breathing_mode = FindModeByName(controller, "breathing");

        if(breathing_mode >= 0)
        {
            unsigned int flags = controller->GetModeFlags((unsigned int)breathing_mode);

            if(flags & MODE_FLAG_HAS_MODE_SPECIFIC_COLOR)
            {
                controller->SetModeColor((unsigned int)breathing_mode, 0, ToRGBColor(color.red(), color.green(), color.blue()));
            }

            if(flags & MODE_FLAG_HAS_SPEED)
            {
                unsigned int speed_min = controller->GetModeSpeedMin((unsigned int)breathing_mode);
                unsigned int speed_max = controller->GetModeSpeedMax((unsigned int)breathing_mode);
                unsigned int speed_val = speed_min + (unsigned int)(((double)(speed_max - speed_min)) * (speed_percent / 100.0));

                controller->SetModeSpeed((unsigned int)breathing_mode, speed_val);
            }

            controller->SetActiveMode(breathing_mode);
        }
        else
        {
            software_fallback_controllers.push_back(controller);
        }
    }

    if(!software_fallback_controllers.empty())
    {
        /*-------------------------------------------------*\
        | Software fallback: pulse brightness via a sine wave |
        | Speed 0-100% maps roughly to a 6s..1s breath cycle  |
        | Devices need to be switched to their Direct/Custom   |
        | mode first, or their firmware ignores the colors we  |
        | send it (e.g. a device left in "Off" mode)            |
        \*-------------------------------------------------*/
        for(RGBController* controller : software_fallback_controllers)
        {
            EnsureDirectMode(controller);
        }

        phase = 0.0;

        double cycle_seconds  = 6.0 - (5.0 * (speed_percent / 100.0));
        double phase_step     = TWO_PI / (cycle_seconds * (1000.0 / EFFECT_TICK_MS));

        connect(effect_timer, &QTimer::timeout, this, &AgileRgbLightingHelper::OnBreathingTick);
        effect_timer->setProperty("phase_step", phase_step);
        effect_timer->start(EFFECT_TICK_MS);
    }
}

void AgileRgbLightingHelper::OnBreathingTick()
{
    double phase_step = effect_timer->property("phase_step").toDouble();
    phase += phase_step;

    double brightness = (std::sin(phase) + 1.0) / 2.0; /* 0.0 .. 1.0 */

    QColor scaled(
        (int)(breathing_color.red()   * brightness),
        (int)(breathing_color.green() * brightness),
        (int)(breathing_color.blue()  * brightness)
    );

    RGBColor target_color = ToRGBColor(scaled.red(), scaled.green(), scaled.blue());

    for(RGBController* controller : software_fallback_controllers)
    {
        controller->SetAllColors(target_color);
        controller->UpdateLEDs();
    }
}

void AgileRgbLightingHelper::StartRainbow(unsigned int speed_percent, bool left_to_right)
{
    StopSoftwareTimer();

    rainbow_left_to_right = left_to_right;

    std::vector<RGBController*>& controllers = ResourceManager::get()->GetRGBControllers();
    software_fallback_controllers.clear();

    for(RGBController* controller : controllers)
    {
        int rainbow_mode = FindModeByName(controller, "spectrum cycle");

        if(rainbow_mode < 0)
        {
            rainbow_mode = FindModeByName(controller, "rainbow");
        }

        if(rainbow_mode >= 0)
        {
            unsigned int flags = controller->GetModeFlags((unsigned int)rainbow_mode);

            if(flags & MODE_FLAG_HAS_SPEED)
            {
                unsigned int speed_min = controller->GetModeSpeedMin((unsigned int)rainbow_mode);
                unsigned int speed_max = controller->GetModeSpeedMax((unsigned int)rainbow_mode);
                unsigned int speed_val = speed_min + (unsigned int)(((double)(speed_max - speed_min)) * (speed_percent / 100.0));

                controller->SetModeSpeed((unsigned int)rainbow_mode, speed_val);
            }

            if(flags & MODE_FLAG_HAS_DIRECTION_LR)
            {
                controller->SetModeDirection((unsigned int)rainbow_mode, left_to_right ? MODE_DIRECTION_LEFT : MODE_DIRECTION_RIGHT);
            }

            controller->SetActiveMode(rainbow_mode);
        }
        else
        {
            software_fallback_controllers.push_back(controller);
        }
    }

    if(!software_fallback_controllers.empty())
    {
        /*-------------------------------------------------*\
        | Devices need to be switched to their Direct/Custom   |
        | mode first, or their firmware ignores the colors we  |
        | send it (e.g. a device left in "Off" mode)            |
        \*-------------------------------------------------*/
        for(RGBController* controller : software_fallback_controllers)
        {
            EnsureDirectMode(controller);
        }

        phase = 0.0;

        /*-------------------------------------------------*\
        | Speed 0-100% maps to a 20s..2s full hue cycle       |
        \*-------------------------------------------------*/
        double cycle_seconds = 20.0 - (18.0 * (speed_percent / 100.0));
        double phase_step    = (360.0 / (cycle_seconds * (1000.0 / EFFECT_TICK_MS)));

        connect(effect_timer, &QTimer::timeout, this, &AgileRgbLightingHelper::OnRainbowTick);
        effect_timer->setProperty("phase_step", phase_step);
        effect_timer->start(EFFECT_TICK_MS);
    }
}

void AgileRgbLightingHelper::OnRainbowTick()
{
    double phase_step = effect_timer->property("phase_step").toDouble();
    phase += rainbow_left_to_right ? phase_step : -phase_step;

    if(phase >= 360.0) phase -= 360.0;
    if(phase < 0.0)    phase += 360.0;

    QColor color = QColor::fromHsv((int)phase, 255, 255);
    RGBColor target_color = ToRGBColor(color.red(), color.green(), color.blue());

    for(RGBController* controller : software_fallback_controllers)
    {
        controller->SetAllColors(target_color);
        controller->UpdateLEDs();
    }
}

void AgileRgbLightingHelper::Stop()
{
    StopSoftwareTimer();
}
