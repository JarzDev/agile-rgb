/*---------------------------------------------------------*\
| AgileRgbFixedColorPage.cpp                                 |
|                                                             |
|   Simplified fixed color / breathing page: a color wheel,   |
|   brightness slider, breathing toggle, and quick color       |
|   presets applied to all detected RGB devices                |
|                                                             |
|   This file is part of the Agile Rgb project                |
|   SPDX-License-Identifier: GPL-2.0-or-later                 |
\*---------------------------------------------------------*/

#include "AgileRgbFixedColorPage.h"
#include "ui_AgileRgbFixedColorPage.h"

AgileRgbFixedColorPage::AgileRgbFixedColorPage(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::AgileRgbFixedColorPage)
{
    ui->setupUi(this);

    lighting = new AgileRgbLightingHelper(this);

    ui->ColorWheelBox->setColor(QColor(0, 255, 0));

    Apply();
}

AgileRgbFixedColorPage::~AgileRgbFixedColorPage()
{
    delete ui;
}

void AgileRgbFixedColorPage::Apply()
{
    QColor color = ui->ColorWheelBox->color();
    unsigned int brightness = (unsigned int)ui->BrightnessSlider->value();

    if(ui->BreathingToggle->isChecked())
    {
        lighting->StartBreathing(color, brightness);
    }
    else
    {
        lighting->ApplyStaticColor(color, brightness);
    }
}

void AgileRgbFixedColorPage::on_ColorWheelBox_colorChanged(const QColor /*color*/)
{
    Apply();
}

void AgileRgbFixedColorPage::on_BrightnessSlider_valueChanged(int /*value*/)
{
    Apply();
}

void AgileRgbFixedColorPage::on_BreathingToggle_toggled(bool /*checked*/)
{
    Apply();
}

void AgileRgbFixedColorPage::on_SwatchBox_swatchChanged(const QColor color)
{
    ui->ColorWheelBox->blockSignals(true);
    ui->ColorWheelBox->setColor(color);
    ui->ColorWheelBox->blockSignals(false);

    Apply();
}
