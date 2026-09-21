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
#include "OpenRGBZoneInitializationDialog.h"

#include "ResourceManager.h"
#include "SettingsManager.h"
#include "ProfileManager.h"
#include "AutoStart.h"
#include "LogManager.h"

#include <QFile>
#include <QTextStream>
#include <QApplication>
#include <QMetaObject>
#include <QCloseEvent>

#include <thread>

using json = nlohmann::json;

AgileRgbSimpleDialog::AgileRgbSimpleDialog(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::AgileRgbSimpleDialog)
{
    /*-------------------------------------------------*\
    | SettingsManager::SetSettings() silently drops any     |
    | key not declared in the target's registered schema.    |
    | "UserInterface" is registered by OpenRGBDialog (created |
    | further below), but RegisterSettingsSchemaComplete()      |
    | merges properties rather than replacing them, so this      |
    | extra key can be declared here independently, as long as    |
    | it happens before anything ever calls SetSettings on this    |
    | key -- otherwise it gets stripped out on save                  |
    \*-------------------------------------------------*/
    json agile_rgb_ui_schema;
    agile_rgb_ui_schema["agile_rgb_last_tab"]["title"] = "Last Active Tab";
    agile_rgb_ui_schema["agile_rgb_last_tab"]["type"]  = "integer";
    ResourceManager::get()->GetSettingsManager()->RegisterSettingsSchemaComplete("UserInterface", "User Interface", agile_rgb_ui_schema, -1, true, false);

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
    | The classic dialog always shows its own tray icon   |
    | on construction; suppress it since this window's own  |
    | tray icon already covers show/hide/exit for both        |
    \*-------------------------------------------------*/
    pro_dialog->SetTrayIconVisible(false);

    pro_mode_ever_shown = false;

    /*-------------------------------------------------*\
    | Restore the last tab the user had open, so relaunching |
    | (especially via "Start with Windows") lands back where   |
    | they left off instead of always resetting to Fixed Color  |
    \*-------------------------------------------------*/
    if(ui_settings.contains("agile_rgb_last_tab"))
    {
        int last_tab = ui_settings["agile_rgb_last_tab"].get<int>();

        if((last_tab >= 0) && (last_tab < ui->SimpleTabBar->count()))
        {
            ui->SimpleTabBar->setCurrentIndex(last_tab);
        }
    }

    /*-------------------------------------------------*\
    | Only the tab that is actually visible should drive  |
    | the lighting; otherwise all three tabs would apply   |
    | their effect at once and fight over the same devices |
    \*-------------------------------------------------*/
    on_SimpleTabBar_currentChanged(ui->SimpleTabBar->currentIndex());

    SetupTrayIcon();

    bool autostart_enabled = LoadAutoStartSetting();

    ui->StartWithWindowsCheckBox->blockSignals(true);
    ui->StartWithWindowsCheckBox->setChecked(autostart_enabled);
    ui->StartWithWindowsCheckBox->blockSignals(false);

    /*-------------------------------------------------*\
    | Re-apply on every launch (not just when the checkbox  |
    | is toggled), so an existing shortcut always matches     |
    | this build's expected arguments (e.g. --startminimized)  |
    | instead of getting stuck with whatever an older build     |
    | wrote to it                                                |
    \*-------------------------------------------------*/
    if(autostart_enabled)
    {
        json autostart_settings = ResourceManager::get()->GetSettingsManager()->GetSettings("AutoStart");

        if(!autostart_settings.contains("start_minimized") || !autostart_settings["start_minimized"].get<bool>())
        {
            autostart_settings["start_minimized"] = true;
            ResourceManager::get()->GetSettingsManager()->SetSettings("AutoStart", autostart_settings);
            ResourceManager::get()->GetSettingsManager()->SaveSettings();
        }

        ApplyAutoStartSetting(true);
    }

    ui->MinimizeToTrayCheckBox->blockSignals(true);
    ui->MinimizeToTrayCheckBox->setChecked(LoadMinimizeToTraySetting());
    ui->MinimizeToTrayCheckBox->blockSignals(false);
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
    pro_mode_ever_shown = true;

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
            | Some devices (typically ARGB fan/strip headers    |
            | wired through a motherboard or hub) can't report  |
            | how many LEDs are actually connected. Show the    |
            | same "how many LEDs are on this header?" prompt    |
            | the classic dialog uses, instead of guessing a     |
            | size -- guessing (e.g. maxing out at the protocol's |
            | 1024-LED ceiling) sends a malformed-length update  |
            | that visibly corrupts the strip's colors.           |
            \*-----------------------------------------------*/
            OpenRGBZoneInitializationDialog::RunChecks(this_dialog);

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

    json ui_settings = ResourceManager::get()->GetSettingsManager()->GetSettings("UserInterface");
    ui_settings["agile_rgb_last_tab"] = index;
    ResourceManager::get()->GetSettingsManager()->SetSettings("UserInterface", ui_settings);
    ResourceManager::get()->GetSettingsManager()->SaveSettings();
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

        if(trayActionShowHide != nullptr)
        {
            trayActionShowHide->setText(tr("Show/Hide"));
            trayActionExit->setText(tr("Exit"));
        }
    }

    QMainWindow::changeEvent(event);
}

void AgileRgbSimpleDialog::SetupTrayIcon()
{
    trayMenu = new QMenu(this);

    trayActionShowHide = new QAction(tr("Show/Hide"), this);
    connect(trayActionShowHide, &QAction::triggered, this, &AgileRgbSimpleDialog::on_ShowHide);
    trayMenu->addAction(trayActionShowHide);

    trayActionExit = new QAction(tr("Exit"), this);
    connect(trayActionExit, &QAction::triggered, this, &AgileRgbSimpleDialog::on_Exit);
    trayMenu->addAction(trayActionExit);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(QIcon(":org.openrgb.OpenRGB.png"));
    trayIcon->setToolTip("Agile Rgb");
    trayIcon->setContextMenu(trayMenu);

    connect(trayIcon, &QSystemTrayIcon::activated, this, &AgileRgbSimpleDialog::on_TrayActivated);

    trayIcon->show();
}

void AgileRgbSimpleDialog::closeEvent(QCloseEvent *event)
{
    /*-------------------------------------------------*\
    | When enabled, closing (or minimizing) the window     |
    | keeps Agile Rgb running in the system tray instead of  |
    | exiting, so lighting effects keep applying in the       |
    | background                                                |
    \*-------------------------------------------------*/
    if(ui->MinimizeToTrayCheckBox->isChecked() && !this->isHidden() && event->spontaneous())
    {
        hide();
        event->ignore();
    }
    else
    {
        QMainWindow::closeEvent(event);
    }
}

void AgileRgbSimpleDialog::on_ShowHide()
{
    if(isHidden())
    {
        show();

        if(isMinimized())
        {
            showNormal();
        }

        /*-------------------------------------------------*\
        | The classic dialog's own tray icon is suppressed,   |
        | so if the user opened Pro Mode and then closed it     |
        | directly (its minimize-on-close setting hid it        |
        | rather than exiting), this is the only remaining way  |
        | to bring it back                                       |
        \*-------------------------------------------------*/
        if(pro_mode_ever_shown && pro_dialog->isHidden())
        {
            pro_dialog->show();
        }
    }
    else
    {
        hide();
    }
}

void AgileRgbSimpleDialog::on_TrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if(reason == QSystemTrayIcon::DoubleClick)
    {
        on_ShowHide();
    }
}

void AgileRgbSimpleDialog::on_Exit()
{
    /*-------------------------------------------------*\
    | This is the exit from the tray icon, not the main   |
    | window's close button. hide() first so closeEvent()  |
    | below sees isHidden() == true and does not just re-    |
    | minimize back to the tray                              |
    \*-------------------------------------------------*/
    this->hide();
    trayIcon->hide();
    close();

    /*-------------------------------------------------*\
    | The classic dialog owns its own cleanup (unregisters |
    | resource manager callbacks, unloads plugins, saves     |
    | exit profile) in its closeEvent(); route through the    |
    | same close() -> closeEvent() path instead of a bare      |
    | QApplication::quit() so that cleanup still runs          |
    \*-------------------------------------------------*/
    pro_dialog->hide();
    pro_dialog->close();
}

bool AgileRgbSimpleDialog::LoadAutoStartSetting()
{
    json autostart_settings = ResourceManager::get()->GetSettingsManager()->GetSettings("AutoStart");

    return autostart_settings.contains("enabled") ? autostart_settings["enabled"].get<bool>() : false;
}

void AgileRgbSimpleDialog::ApplyAutoStartSetting(bool enabled)
{
    AutoStart auto_start("Agile Rgb");

    if(enabled)
    {
        AutoStartInfo auto_start_info;

        auto_start_info.args     = "--startminimized";
        auto_start_info.category = "Utility;";
        auto_start_info.desc     = "Agile Rgb";
        auto_start_info.icon     = "Agile Rgb";
        auto_start_info.path     = auto_start.GetExePath();

        auto_start.EnableAutoStart(auto_start_info);
    }
    else
    {
        auto_start.DisableAutoStart();
    }
}

void AgileRgbSimpleDialog::on_StartWithWindowsCheckBox_toggled(bool checked)
{
    ApplyAutoStartSetting(checked);

    /*-------------------------------------------------*\
    | The classic dialog's own on_SettingsUpdated() also      |
    | writes this same shortcut whenever it runs (e.g. right    |
    | after its own construction), independently rebuilding      |
    | the argument list from "start_minimized"/"custom_arguments"  |
    | instead of ApplyAutoStartSetting()'s hardcoded                |
    | "--startminimized". Leaving start_minimized unset meant that   |
    | the classic dialog's pass overwrote the shortcut with an empty  |
    | argument list right after this one wrote the correct one. Set    |
    | it here too so both code paths agree on the same shortcut         |
    \*-------------------------------------------------*/
    json autostart_settings = ResourceManager::get()->GetSettingsManager()->GetSettings("AutoStart");
    autostart_settings["enabled"]         = checked;
    autostart_settings["start_minimized"] = true;
    ResourceManager::get()->GetSettingsManager()->SetSettings("AutoStart", autostart_settings);
    ResourceManager::get()->GetSettingsManager()->SaveSettings();
}

bool AgileRgbSimpleDialog::LoadMinimizeToTraySetting()
{
    json ui_settings = ResourceManager::get()->GetSettingsManager()->GetSettings("UserInterface");

    /*-------------------------------------------------*\
    | Shares the "minimize_on_close" key with the classic  |
    | dialog's Settings tab, so both windows agree on the   |
    | same behavior. Defaults to on here (unlike Pro Mode's  |
    | off-by-default), since staying in the tray is the       |
    | expected behavior for a simplified always-on app         |
    \*-------------------------------------------------*/
    return ui_settings.contains("minimize_on_close") ? ui_settings["minimize_on_close"].get<bool>() : true;
}

void AgileRgbSimpleDialog::on_MinimizeToTrayCheckBox_toggled(bool checked)
{
    json ui_settings = ResourceManager::get()->GetSettingsManager()->GetSettings("UserInterface");
    ui_settings["minimize_on_close"] = checked;
    ResourceManager::get()->GetSettingsManager()->SetSettings("UserInterface", ui_settings);
    ResourceManager::get()->GetSettingsManager()->SaveSettings();
}
