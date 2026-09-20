/*---------------------------------------------------------*\
| AgileRgbFixedColorPage.h                                   |
|                                                             |
|   Simplified fixed color / breathing page: a color wheel,   |
|   brightness slider, breathing toggle, and quick color       |
|   presets applied to all detected RGB devices                |
|                                                             |
|   This file is part of the Agile Rgb project                |
|   SPDX-License-Identifier: GPL-2.0-or-later                 |
\*---------------------------------------------------------*/

#pragma once

#include <QFrame>
#include <QColor>

#include "AgileRgbLightingHelper.h"

namespace Ui
{
    class AgileRgbFixedColorPage;
}

class AgileRgbFixedColorPage : public QFrame
{
    Q_OBJECT

public:
    explicit AgileRgbFixedColorPage(QWidget *parent = nullptr);
    ~AgileRgbFixedColorPage();

    /*-----------------------------------------------------*\
    | Called by the containing window when this tab becomes  |
    | active/inactive, so only the visible tab drives the     |
    | lighting (prevents tabs from fighting over the same      |
    | devices)                                                 |
    \*-----------------------------------------------------*/
    void        SetPageActive(bool active);

protected:
    void        changeEvent(QEvent *event) override;

private slots:
    void        on_ColorWheelBox_colorChanged(const QColor color);
    void        on_BrightnessSlider_valueChanged(int value);
    void        on_BreathingToggle_toggled(bool checked);
    void        on_SwatchBox_swatchChanged(const QColor color);
    void        on_ApplyButton_clicked();

private:
    Ui::AgileRgbFixedColorPage* ui;
    AgileRgbLightingHelper*     lighting;

    void        Apply();
};
