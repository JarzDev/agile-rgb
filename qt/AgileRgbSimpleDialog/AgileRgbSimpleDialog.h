/*---------------------------------------------------------*\
| AgileRgbSimpleDialog.h                                     |
|                                                             |
|   Simplified main window: three tabs (fixed color/         |
|   breathing, temperature, rainbow) plus a "Pro Mode"        |
|   button that opens the full classic OpenRGBDialog for       |
|   advanced per-device/per-zone control                       |
|                                                             |
|   This file is part of the Agile Rgb project                |
|   SPDX-License-Identifier: GPL-2.0-or-later                 |
\*---------------------------------------------------------*/

#pragma once

#include <QMainWindow>
#include <QTranslator>

#include "OpenRGBDialog.h"

namespace Ui
{
    class AgileRgbSimpleDialog;
}

class AgileRgbSimpleDialog : public QMainWindow
{
    Q_OBJECT

public:
    explicit AgileRgbSimpleDialog(QWidget *parent = nullptr);
    ~AgileRgbSimpleDialog();

    /*-----------------------------------------------------*\
    | Access to the embedded classic dialog, so startup.cpp  |
    | can still call AddClientTab()/AddI2CToolsPage()/etc.    |
    | on it exactly as it did before this window existed      |
    \*-----------------------------------------------------*/
    OpenRGBDialog*  GetProDialog();

protected:
    void        changeEvent(QEvent *event) override;

private slots:
    void        on_ProModeButton_clicked();
    void        on_SimpleTabBar_currentChanged(int index);
    void        on_LanguageBox_currentIndexChanged(int index);

private:
    Ui::AgileRgbSimpleDialog*  ui;
    OpenRGBDialog*             pro_dialog;
    QTranslator                translator;

    void        SetLanguage(std::string locale);
};
