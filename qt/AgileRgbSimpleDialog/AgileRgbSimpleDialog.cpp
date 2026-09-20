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

#include "ResourceManager.h"
#include "SettingsManager.h"

#include <QFile>
#include <QTextStream>
#include <QApplication>
#include <QMetaObject>

#include <thread>

using json = nlohmann::json;

AgileRgbSimpleDialog::AgileRgbSimpleDialog(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::AgileRgbSimpleDialog)
{
    /*-------------------------------------------------*\
    | Load the saved UI language (shared with the classic |
    | dialog's "UserInterface" settings) before building   |
    | the UI, so labels are translated on first render      |
    \*-------------------------------------------------*/
    json ui_settings = ResourceManager::get()->GetSettingsManager()->GetSettings("UserInterface");
    std::string saved_language = ui_settings.contains("language") ? ui_settings["language"].get<std::string>() : "en_US";

    SetLanguage(saved_language);

    ui->setupUi(this);

    ui->LanguageBox->blockSignals(true);
    ui->LanguageBox->setCurrentIndex(saved_language == "es_ES" ? 1 : 0);
    ui->LanguageBox->blockSignals(false);

    /*-------------------------------------------------*\
    | Apply the dark theme with RGB accent borders        |
    \*-------------------------------------------------*/
    QFile theme_file(":/AgileRgbSimpleDialog/AgileRgbTheme.qss");

    if(theme_file.open(QFile::ReadOnly | QFile::Text))
    {
        QTextStream theme_stream(&theme_file);
        setStyleSheet(theme_stream.readAll());
    }

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

void AgileRgbSimpleDialog::on_ScanDevicesButton_clicked()
{
    ui->ScanDevicesButton->setEnabled(false);
    ui->ScanDevicesButton->setText(tr("Scanning..."));

    AgileRgbSimpleDialog* this_dialog = this;

    std::thread rescan_thread([this_dialog]()
    {
        ResourceManager::get()->RescanDevices();

        QMetaObject::invokeMethod(this_dialog, [this_dialog]()
        {
            this_dialog->ui->ScanDevicesButton->setEnabled(true);
            this_dialog->ui->ScanDevicesButton->setText(tr("All Devices"));

            /*-----------------------------------------------*\
            | Newly-detected devices won't have the current    |
            | tab's effect applied to them yet; re-trigger the  |
            | active tab so they pick it up immediately          |
            \*-----------------------------------------------*/
            this_dialog->on_SimpleTabBar_currentChanged(this_dialog->ui->SimpleTabBar->currentIndex());
        }, Qt::QueuedConnection);
    });
    rescan_thread.detach();
}

void AgileRgbSimpleDialog::on_SimpleTabBar_currentChanged(int index)
{
    ui->FixedColorTab->SetPageActive(index == ui->SimpleTabBar->indexOf(ui->FixedColorTab));
    ui->TemperatureTab->SetPageActive(index == ui->SimpleTabBar->indexOf(ui->TemperatureTab));
    ui->RainbowTab->SetPageActive(index == ui->SimpleTabBar->indexOf(ui->RainbowTab));
}

void AgileRgbSimpleDialog::SetLanguage(std::string locale)
{
    QApplication* app = static_cast<QApplication *>(QApplication::instance());

    app->removeTranslator(&translator);

    bool loaded = translator.load(":/i18n/" + QString("OpenRGB_%1.qm").arg(QString::fromStdString(locale)));

    if(loaded)
    {
        app->installTranslator(&translator);
    }
}

void AgileRgbSimpleDialog::on_LanguageBox_currentIndexChanged(int index)
{
    std::string locale = (index == 1) ? "es_ES" : "en_US";

    SetLanguage(locale);

    json ui_settings = ResourceManager::get()->GetSettingsManager()->GetSettings("UserInterface");
    ui_settings["language"] = locale;
    ResourceManager::get()->GetSettingsManager()->SetSettings("UserInterface", ui_settings);
    ResourceManager::get()->GetSettingsManager()->SaveSettings();
}

void AgileRgbSimpleDialog::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
    }

    QMainWindow::changeEvent(event);
}
