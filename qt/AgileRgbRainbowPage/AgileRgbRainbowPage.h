/*---------------------------------------------------------*\
| AgileRgbRainbowPage.h                                      |
|                                                             |
|   Simplified rainbow/spectrum cycle page: cycle speed and   |
|   direction, applied to all detected RGB devices             |
|                                                             |
|   This file is part of the Agile Rgb project                |
|   SPDX-License-Identifier: GPL-2.0-or-later                 |
\*---------------------------------------------------------*/

#pragma once

#include <QFrame>

#include "AgileRgbLightingHelper.h"

namespace Ui
{
    class AgileRgbRainbowPage;
}

class AgileRgbRainbowPage : public QFrame
{
    Q_OBJECT

public:
    explicit AgileRgbRainbowPage(QWidget *parent = nullptr);
    ~AgileRgbRainbowPage();

private slots:
    void        on_SpeedSlider_valueChanged(int value);
    void        on_LeftToRightButton_clicked();
    void        on_RightToLeftButton_clicked();

private:
    Ui::AgileRgbRainbowPage*    ui;
    AgileRgbLightingHelper*     lighting;
    bool                        left_to_right;

    void        Apply();
};
