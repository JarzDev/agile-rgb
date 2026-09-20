/*---------------------------------------------------------*\
| AgileRgbSimpleDialog.cpp                                   |
|                                                             |
|   Simplified main window: three tabs (fixed color/         |
|   breathing, temperature, rainbow) plus a "Pro Mode"        |
|   button that opens the full classic OpenRGBDialog for       |
|   advanced per-device/per-zone control                       |
|                                                             |
|   This file is part of the Agile Rgb project                |
|   SPDX-License-Identifier: GPL-2.0-or-later                 |
\*---------------------------------------------------------*/

#include "AgileRgbSimpleDialog.h"
#include "ui_AgileRgbSimpleDialog.h"

#include "AgileRgbFixedColorPage.h"
#include "AgileRgbTemperaturePage.h"
#include "AgileRgbRainbowPage.h"

AgileRgbSimpleDialog::AgileRgbSimpleDialog(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::AgileRgbSimpleDialog)
{
    ui->setupUi(this);

    /*-------------------------------------------------*\
    | The classic dialog is created up front (so devices |
    | are detected once) but stays hidden until the user  |
    | clicks "Pro Mode"                                    |
    \*-------------------------------------------------*/
    pro_dialog = new OpenRGBDialog(nullptr);

    /*-------------------------------------------------*\
    | Only the tab that is actually visible should drive  |
    | the lighting; otherwise all three tabs would apply   |
    | their effect at once and fight over the same devices |
    \*-------------------------------------------------*/
    on_SimpleTabBar_currentChanged(ui->SimpleTabBar->currentIndex());
}

AgileRgbSimpleDialog::~AgileRgbSimpleDialog()
{
    delete ui;
}

OpenRGBDialog* AgileRgbSimpleDialog::GetProDialog()
{
    return pro_dialog;
}

void AgileRgbSimpleDialog::on_ProModeButton_clicked()
{
    pro_dialog->show();
    pro_dialog->raise();
    pro_dialog->activateWindow();
}

void AgileRgbSimpleDialog::on_SimpleTabBar_currentChanged(int index)
{
    ui->FixedColorTab->SetPageActive(index == ui->SimpleTabBar->indexOf(ui->FixedColorTab));
    ui->TemperatureTab->SetPageActive(index == ui->SimpleTabBar->indexOf(ui->TemperatureTab));
    ui->RainbowTab->SetPageActive(index == ui->SimpleTabBar->indexOf(ui->RainbowTab));
}
