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
