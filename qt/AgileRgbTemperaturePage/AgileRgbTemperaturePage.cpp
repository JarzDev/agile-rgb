/*---------------------------------------------------------*\
| AgileRgbTemperaturePage.cpp                                |
|                                                             |
|   Simplified temperature-reactive lighting page: shows      |
|   current GPU temperature and lets the user configure       |
|   three color-coded temperature ranges (low/medium/high)    |
|   applied to all detected RGB devices                       |
|                                                             |
|   This file is part of the Agile Rgb project                |
|   SPDX-License-Identifier: GPL-2.0-or-later                 |
\*---------------------------------------------------------*/

#include "AgileRgbTemperaturePage.h"
#include "ui_AgileRgbTemperaturePage.h"
#include "AgileRgbTemperatureRangeCard.h"

#include "ResourceManager.h"
#include "SettingsManager.h"
#include "TemperatureMonitor.h"

#include <QColor>

static const char* SETTINGS_KEY = "AgileRgbTemperature";

AgileRgbTemperaturePage::AgileRgbTemperaturePage(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::AgileRgbTemperaturePage)
{
    ui->setupUi(this);

    lighting = new AgileRgbLightingHelper(this);

    ui->LowRangeCard->SetRangeKind(TemperatureRangeKind::LOW);
    ui->MediumRangeCard->SetRangeKind(TemperatureRangeKind::MEDIUM);
    ui->HighRangeCard->SetRangeKind(TemperatureRangeKind::HIGH);

    /*-------------------------------------------------*\
    | Keep the three ranges contiguous: changing one      |
    | range's boundary pushes the neighbor's boundary to   |
    | stay exactly adjacent (no gaps, no overlaps)          |
    \*-------------------------------------------------*/
    connect(ui->LowRangeCard,    &AgileRgbTemperatureRangeCard::RangeChanged, this, &AgileRgbTemperaturePage::on_LowRangeChanged);
    connect(ui->MediumRangeCard, &AgileRgbTemperatureRangeCard::RangeChanged, this, &AgileRgbTemperaturePage::on_MediumRangeChanged);

    LoadSettings();

    poll_timer = new QTimer(this);
    connect(poll_timer, &QTimer::timeout, this, &AgileRgbTemperaturePage::UpdateCurrentTemperature);

    UpdateCurrentTemperature();
}

AgileRgbTemperaturePage::~AgileRgbTemperaturePage()
{
    delete ui;
}

void AgileRgbTemperaturePage::changeEvent(QEvent *event)
{
    if(event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);

        /*---------------------------------------------------*\
        | The range card titles ("Low Temperature", etc.) are  |
        | set dynamically in code, not via the .ui file, so    |
        | they need to be re-applied after retranslateUi()      |
        \*---------------------------------------------------*/
        ui->LowRangeCard->SetRangeKind(TemperatureRangeKind::LOW);
        ui->MediumRangeCard->SetRangeKind(TemperatureRangeKind::MEDIUM);
        ui->HighRangeCard->SetRangeKind(TemperatureRangeKind::HIGH);
    }

    QFrame::changeEvent(event);
}

void AgileRgbTemperaturePage::SetPageActive(bool active)
{
    if(active)
    {
        poll_timer->start(2000);
        UpdateCurrentTemperature();
    }
    else
    {
        poll_timer->stop();
    }
}

void AgileRgbTemperaturePage::LoadSettings()
{
    json settings = ResourceManager::get()->GetSettingsManager()->GetSettings(SETTINGS_KEY);

    int low_max     = settings.contains("low_max")     ? settings["low_max"].get<int>()     : 50;
    int medium_min   = settings.contains("medium_min")   ? settings["medium_min"].get<int>()   : 51;
    int medium_max   = settings.contains("medium_max")   ? settings["medium_max"].get<int>()   : 75;
    int high_min     = settings.contains("high_min")     ? settings["high_min"].get<int>()     : 76;

    QColor low_color    = settings.contains("low_color")    ? QColor(QString::fromStdString(settings["low_color"].get<std::string>()))    : QColor(0, 255, 0);
    QColor medium_color  = settings.contains("medium_color")  ? QColor(QString::fromStdString(settings["medium_color"].get<std::string>()))  : QColor(255, 255, 0);
    QColor high_color    = settings.contains("high_color")    ? QColor(QString::fromStdString(settings["high_color"].get<std::string>()))    : QColor(255, 0, 0);

    bool enabled = settings.contains("enabled") ? settings["enabled"].get<bool>() : false;

    ui->LowRangeCard->SetMaxTemperature(low_max);
    ui->LowRangeCard->SetColor(low_color);

    ui->MediumRangeCard->SetMinTemperature(medium_min);
    ui->MediumRangeCard->SetMaxTemperature(medium_max);
    ui->MediumRangeCard->SetColor(medium_color);

    ui->HighRangeCard->SetMinTemperature(high_min);
    ui->HighRangeCard->SetColor(high_color);

    ui->EnableCheckBox->blockSignals(true);
    ui->EnableCheckBox->setChecked(enabled);
    ui->EnableCheckBox->blockSignals(false);
}

void AgileRgbTemperaturePage::SaveSettings()
{
    json settings;

    settings["low_max"]      = ui->LowRangeCard->GetMaxTemperature();
    settings["medium_min"]   = ui->MediumRangeCard->GetMinTemperature();
    settings["medium_max"]   = ui->MediumRangeCard->GetMaxTemperature();
    settings["high_min"]     = ui->HighRangeCard->GetMinTemperature();

    settings["low_color"]    = ui->LowRangeCard->GetColor().name().toStdString();
    settings["medium_color"] = ui->MediumRangeCard->GetColor().name().toStdString();
    settings["high_color"]   = ui->HighRangeCard->GetColor().name().toStdString();

    settings["enabled"]      = ui->EnableCheckBox->isChecked();

    ResourceManager::get()->GetSettingsManager()->SetSettings(SETTINGS_KEY, settings);
    ResourceManager::get()->GetSettingsManager()->SaveSettings();
}

void AgileRgbTemperaturePage::on_SaveButton_clicked()
{
    SaveSettings();
    UpdateCurrentTemperature();
}

void AgileRgbTemperaturePage::on_CancelButton_clicked()
{
    LoadSettings();
}

void AgileRgbTemperaturePage::on_EnableCheckBox_toggled(bool /*checked*/)
{
    SaveSettings();
}

void AgileRgbTemperaturePage::SyncMediumMinToLowMax()
{
    ui->MediumRangeCard->SetMinTemperature(ui->LowRangeCard->GetMaxTemperature() + 1);

    /*---------------------------------------------------*\
    | Medium's min may have just pushed its own max up      |
    | (SetMinTemperature keeps min <= max internally), so    |
    | High's min needs to stay in sync with Medium's max too |
    \*---------------------------------------------------*/
    SyncHighMinToMediumMax();
}

void AgileRgbTemperaturePage::SyncHighMinToMediumMax()
{
    ui->HighRangeCard->SetMinTemperature(ui->MediumRangeCard->GetMaxTemperature() + 1);
}

void AgileRgbTemperaturePage::on_LowRangeChanged()
{
    SyncMediumMinToLowMax();
}

void AgileRgbTemperaturePage::on_MediumRangeChanged()
{
    SyncHighMinToMediumMax();
}

void AgileRgbTemperaturePage::UpdateCurrentTemperature()
{
    int celsius = TemperatureMonitor::get()->GetHighestGPUTemperature();

    if(celsius < 0)
    {
        ui->CurrentTempValueLabel->setText(tr("N/A"));
        return;
    }

    ui->CurrentTempValueLabel->setText(QString("%1°C").arg(celsius));

    if(ui->EnableCheckBox->isChecked())
    {
        ApplyColorForTemperature(celsius);
    }
}

void AgileRgbTemperaturePage::ApplyColorForTemperature(int celsius)
{
    QColor selected_color;

    if(celsius <= ui->LowRangeCard->GetMaxTemperature())
    {
        selected_color = ui->LowRangeCard->GetColor();
    }
    else if(celsius <= ui->MediumRangeCard->GetMaxTemperature())
    {
        selected_color = ui->MediumRangeCard->GetColor();
    }
    else
    {
        selected_color = ui->HighRangeCard->GetColor();
    }

    lighting->ApplyStaticColor(selected_color, 100);
}
