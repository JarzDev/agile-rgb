/*---------------------------------------------------------*\
| AgileRgbLightingHelper.h                                   |
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

#pragma once

#include <QObject>
#include <QTimer>
#include <QColor>
#include <vector>

#include "RGBController.h"

class AgileRgbLightingHelper : public QObject
{
    Q_OBJECT

public:
    explicit AgileRgbLightingHelper(QObject *parent = nullptr);
    ~AgileRgbLightingHelper();

    /*-----------------------------------------------------*\
    | Stops any running software effect and applies a flat   |
    | static color to every detected device                  |
    \*-----------------------------------------------------*/
    void        ApplyStaticColor(const QColor& color, unsigned int brightness_percent);

    /*-----------------------------------------------------*\
    | Starts breathing (native mode if a device has one       |
    | named "Breathing", software pulse otherwise)            |
    \*-----------------------------------------------------*/
    void        StartBreathing(const QColor& color, unsigned int speed_percent);

    /*-----------------------------------------------------*\
    | Starts a rainbow/spectrum cycle (native mode if a       |
    | device has one, software cycle otherwise)               |
    \*-----------------------------------------------------*/
    void        StartRainbow(unsigned int speed_percent, bool left_to_right);

    /*-----------------------------------------------------*\
    | Stops any software timer-driven effect. Does not touch  |
    | devices that are running a native hardware mode.        |
    \*-----------------------------------------------------*/
    void        Stop();

private slots:
    void        OnBreathingTick();
    void        OnRainbowTick();

private:
    QTimer*                         effect_timer;
    double                          phase;

    QColor                          breathing_color;
    bool                            rainbow_left_to_right;

    /*-----------------------------------------------------*\
    | Controllers currently driven by the software timer      |
    | (i.e. that have no matching native mode)                 |
    \*-----------------------------------------------------*/
    std::vector<RGBController*>    software_fallback_controllers;

    /*-----------------------------------------------------*\
    | Returns the index of a mode whose name contains         |
    | needle (case-insensitive), or -1 if none match          |
    \*-----------------------------------------------------*/
    int         FindModeByName(RGBController* controller, const char* needle);

    /*-----------------------------------------------------*\
    | Switches a controller to its "Direct"/"Custom" mode     |
    | (and turns its brightness up if the mode supports it),  |
    | so that a subsequent SetAllColors() call actually shows  |
    | up instead of being ignored by whatever mode (often      |
    | "Off") the device was previously in                      |
    \*-----------------------------------------------------*/
    void        EnsureDirectMode(RGBController* controller);

    void        StopSoftwareTimer();
};
