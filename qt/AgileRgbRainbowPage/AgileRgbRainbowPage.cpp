/*---------------------------------------------------------*\
| AgileRgbRainbowPage.cpp                                    |
|                                                             |
|   Simplified rainbow/spectrum cycle page: cycle speed and   |
|   direction, applied to all detected RGB devices             |
|                                                             |
|   This file is part of the Agile Rgb project                |
|   SPDX-License-Identifier: GPL-2.0-or-later                 |
\*---------------------------------------------------------*/

#include "AgileRgbRainbowPage.h"
#include "ui_AgileRgbRainbowPage.h"

AgileRgbRainbowPage::AgileRgbRainbowPage(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::AgileRgbRainbowPage)
{
    ui->setupUi(this);

    lighting      = new AgileRgbLightingHelper(this);
    left_to_right = true;
}

AgileRgbRainbowPage::~AgileRgbRainbowPage()
{
    delete ui;
}

void AgileRgbRainbowPage::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
    }

    QFrame::changeEvent(event);
}

void AgileRgbRainbowPage::SetPageActive(bool active)
{
    /*-------------------------------------------------*\
    | Switching tabs must never apply a color/effect on   |
    | its own -- only the "Apply" button does that. This    |
    | only stops this page's effect when it stops being      |
    | the visible tab, so it doesn't keep fighting the        |
    | other tabs over the same devices                         |
    \*-------------------------------------------------*/
    if(!active)
    {
        lighting->Stop();
    }
}

void AgileRgbRainbowPage::Apply()
{
    unsigned int speed = (unsigned int)ui->SpeedSlider->value();

    lighting->StartRainbow(speed, left_to_right);
}

void AgileRgbRainbowPage::on_SpeedSlider_valueChanged(int /*value*/)
{
}

void AgileRgbRainbowPage::on_LeftToRightButton_clicked()
{
    left_to_right = true;

    ui->LeftToRightButton->setChecked(true);
    ui->RightToLeftButton->setChecked(false);
}

void AgileRgbRainbowPage::on_RightToLeftButton_clicked()
{
    left_to_right = false;

    ui->LeftToRightButton->setChecked(false);
    ui->RightToLeftButton->setChecked(true);
}

void AgileRgbRainbowPage::on_ApplyButton_clicked()
{
    Apply();
}
