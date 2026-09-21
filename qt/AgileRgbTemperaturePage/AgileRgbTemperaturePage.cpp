/*---------------------------------------------------------*\
| AgileRgbTemperaturePage.cpp                                |
|                                                             |
|   Simplified temperature-reactive lighting page: shows      |
|   current GPU temperature and lets the user configure       |
|   a chain of color-coded temperature ranges (3 to 6)        |
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
#include <algorithm>

static const char* SETTINGS_KEY = "AgileRgbTemperature";

/*-----------------------------------------------------*\
| Default gradient stops (green -> yellow -> orange ->    |
| red) that default range colors are interpolated across   |
\*-----------------------------------------------------*/
static const QColor DEFAULT_GRADIENT_STOPS[] =
{
    QColor(0,   255, 0),
    QColor(255, 255, 0),
    QColor(255, 140, 0),
    QColor(255, 0,   0)
};

static QColor DefaultColorForIndex(int index, int count)
{
    if(count <= 1)
    {
        return DEFAULT_GRADIENT_STOPS[0];
    }

    int stop_count = (int)(sizeof(DEFAULT_GRADIENT_STOPS) / sizeof(DEFAULT_GRADIENT_STOPS[0]));

    double position   = (double)index / (double)(count - 1);
    double scaled      = position * (stop_count - 1);
    int    stop_index  = (int)scaled;

    if(stop_index >= stop_count - 1)
    {
        return DEFAULT_GRADIENT_STOPS[stop_count - 1];
    }

    double fraction = scaled - stop_index;

    const QColor& a = DEFAULT_GRADIENT_STOPS[stop_index];
    const QColor& b = DEFAULT_GRADIENT_STOPS[stop_index + 1];

    return QColor(
        (int)(a.red()   + (b.red()   - a.red())   * fraction),
        (int)(a.green() + (b.green() - a.green()) * fraction),
        (int)(a.blue()  + (b.blue()  - a.blue())  * fraction)
    );
}

AgileRgbTemperaturePage::AgileRgbTemperaturePage(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::AgileRgbTemperaturePage)
{
    ui->setupUi(this);

    lighting = new AgileRgbLightingHelper(this);

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
        for(std::size_t i = 0; i < range_cards.size(); i++)
        {
            TemperatureRangeKind kind = (i == 0) ? TemperatureRangeKind::LOW :
                                         (i == range_cards.size() - 1) ? TemperatureRangeKind::HIGH :
                                         TemperatureRangeKind::MEDIUM;

            range_cards[i]->SetRangeKind(kind);
        }
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

void AgileRgbTemperaturePage::RebuildCards(int count)
{
    count = std::max(AGILERGB_TEMP_RANGES_MIN, std::min(AGILERGB_TEMP_RANGES_MAX, count));

    /*-------------------------------------------------*\
    | Preserve existing colors/thresholds where possible  |
    | (index-for-index) so add/remove doesn't scramble the |
    | ranges the user already configured                    |
    \*-------------------------------------------------*/
    std::vector<QColor> previous_colors;
    std::vector<int>    previous_maxes;

    for(AgileRgbTemperatureRangeCard* card : range_cards)
    {
        previous_colors.push_back(card->GetColor());
        previous_maxes.push_back(card->GetMaxTemperature());

        disconnect(card, nullptr, this, nullptr);
        ui->CardsLayout->removeWidget(card);
        card->deleteLater();
    }

    range_cards.clear();

    /*-------------------------------------------------*\
    | Spread default upper thresholds evenly across        |
    | 0..90C for any newly-created cards                    |
    \*-------------------------------------------------*/
    for(int i = 0; i < count; i++)
    {
        AgileRgbTemperatureRangeCard* card = new AgileRgbTemperatureRangeCard(this);

        TemperatureRangeKind kind = (i == 0) ? TemperatureRangeKind::LOW :
                                     (i == count - 1) ? TemperatureRangeKind::HIGH :
                                     TemperatureRangeKind::MEDIUM;

        card->SetRangeKind(kind);
        card->SetRemovable((i > 0) && (i < count - 1) && (count > AGILERGB_TEMP_RANGES_MIN));

        if((std::size_t)i < previous_colors.size())
        {
            card->SetColor(previous_colors[i]);
        }
        else
        {
            card->SetColor(DefaultColorForIndex(i, count));
        }

        if((std::size_t)i < previous_maxes.size())
        {
            card->SetMaxTemperature(previous_maxes[i]);
        }
        else
        {
            int default_max = -20 + (int)(((double)(i + 1) / count) * 110.0);
            card->SetMaxTemperature(default_max);
        }

        connect(card, &AgileRgbTemperatureRangeCard::RangeChanged, this, [this, i]() { SyncRangeAt(i); });
        connect(card, &AgileRgbTemperatureRangeCard::RemoveRequested, this, [this, card]()
        {
            for(std::size_t idx = 0; idx < range_cards.size(); idx++)
            {
                if(range_cards[idx] == card)
                {
                    RebuildCards((int)range_cards.size() - 1);
                    break;
                }
            }
        });

        ui->CardsLayout->addWidget(card);
        range_cards.push_back(card);
    }

    for(int i = 1; i < (int)range_cards.size(); i++)
    {
        SyncRangeAt(i);
    }

    UpdateAddRemoveButtons();
}

void AgileRgbTemperaturePage::SyncRangeAt(int index)
{
    if((index <= 0) || (index >= (int)range_cards.size()))
    {
        return;
    }

    range_cards[index]->SetMinTemperature(range_cards[index - 1]->GetMaxTemperature() + 1);

    /*-----------------------------------------------------*\
    | SetMinTemperature() may have just pushed this card's   |
    | own max up (min <= max is enforced internally), so the  |
    | next card downstream needs to stay in sync too           |
    \*-----------------------------------------------------*/
    SyncRangeAt(index + 1);
}

void AgileRgbTemperaturePage::UpdateAddRemoveButtons()
{
    ui->AddRangeButton->setEnabled((int)range_cards.size() < AGILERGB_TEMP_RANGES_MAX);
    ui->RemoveRangeButton->setEnabled((int)range_cards.size() > AGILERGB_TEMP_RANGES_MIN);
}

void AgileRgbTemperaturePage::on_AddRangeButton_clicked()
{
    RebuildCards((int)range_cards.size() + 1);
}

void AgileRgbTemperaturePage::on_RemoveRangeButton_clicked()
{
    RebuildCards((int)range_cards.size() - 1);
}

void AgileRgbTemperaturePage::LoadSettings()
{
    json settings = ResourceManager::get()->GetSettingsManager()->GetSettings(SETTINGS_KEY);

    bool enabled = settings.contains("enabled") ? settings["enabled"].get<bool>() : false;

    int range_count = AGILERGB_TEMP_RANGES_MIN;

    if(settings.contains("ranges") && settings["ranges"].is_array() && !settings["ranges"].empty())
    {
        range_count = (int)settings["ranges"].size();
    }

    RebuildCards(range_count);

    if(settings.contains("ranges") && settings["ranges"].is_array())
    {
        const json& ranges = settings["ranges"];

        for(std::size_t i = 0; (i < ranges.size()) && (i < range_cards.size()); i++)
        {
            const json& entry = ranges[i];

            if(entry.contains("max"))
            {
                range_cards[i]->SetMaxTemperature(entry["max"].get<int>());
            }

            if(entry.contains("color"))
            {
                range_cards[i]->SetColor(QColor(QString::fromStdString(entry["color"].get<std::string>())));
            }
        }

        for(int i = 1; i < (int)range_cards.size(); i++)
        {
            SyncRangeAt(i);
        }
    }

    ui->EnableCheckBox->blockSignals(true);
    ui->EnableCheckBox->setChecked(enabled);
    ui->EnableCheckBox->blockSignals(false);
}

void AgileRgbTemperaturePage::SaveSettings()
{
    json settings;
    json ranges = json::array();

    for(AgileRgbTemperatureRangeCard* card : range_cards)
    {
        json entry;
        entry["max"]   = card->GetMaxTemperature();
        entry["color"] = card->GetColor().name().toStdString();
        ranges.push_back(entry);
    }

    settings["ranges"]  = ranges;
    settings["enabled"] = ui->EnableCheckBox->isChecked();

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
    QColor selected_color = range_cards.back()->GetColor();

    for(AgileRgbTemperatureRangeCard* card : range_cards)
    {
        if(celsius <= card->GetMaxTemperature())
        {
            selected_color = card->GetColor();
            break;
        }
    }

    lighting->ApplyStaticColor(selected_color, 100);
}
