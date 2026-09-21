/*---------------------------------------------------------*\
| AgileRgbTemperaturePage.h                                  |
|                                                             |
|   Simplified temperature-reactive lighting page: shows      |
|   current GPU temperature and lets the user configure       |
|   three color-coded temperature ranges (low/medium/high)    |
|   applied to all detected RGB devices                       |
|                                                             |
|   This file is part of the Agile Rgb project                |
|   SPDX-License-Identifier: GPL-2.0-or-later                 |
\*---------------------------------------------------------*/

#pragma once

#include <QFrame>
#include <QTimer>
#include <vector>
#include <nlohmann/json.hpp>

#include "AgileRgbLightingHelper.h"

using json = nlohmann::json;

namespace Ui
{
    class AgileRgbTemperaturePage;
}

class AgileRgbTemperatureRangeCard;

/*-----------------------------------------------------*\
| Minimum/maximum number of temperature range cards.     |
| Minimum is the original Low/Medium/High set; maximum    |
| allows up to three extra cards for a smoother gradient   |
\*-----------------------------------------------------*/
static const int AGILERGB_TEMP_RANGES_MIN = 3;
static const int AGILERGB_TEMP_RANGES_MAX = 6;

class AgileRgbTemperaturePage : public QFrame
{
    Q_OBJECT

public:
    explicit AgileRgbTemperaturePage(QWidget *parent = nullptr);
    ~AgileRgbTemperaturePage();

    /*-----------------------------------------------------*\
    | Called by the containing window when this tab becomes  |
    | active/inactive, so only the visible tab drives the     |
    | lighting (prevents tabs from fighting over the same      |
    | devices)                                                 |
    \*-----------------------------------------------------*/
    void        SetPageActive(bool active);

protected:
    void        changeEvent(QEvent *event) override;

public slots:
    void        UpdateCurrentTemperature();

private slots:
    void        on_SaveButton_clicked();
    void        on_CancelButton_clicked();
    void        on_EnableCheckBox_toggled(bool checked);
    void        on_AddRangeButton_clicked();
    void        on_RemoveRangeButton_clicked();

private:
    Ui::AgileRgbTemperaturePage*                   ui;
    QTimer*                                        poll_timer;
    AgileRgbLightingHelper*                        lighting;
    std::vector<AgileRgbTemperatureRangeCard*>     range_cards;

    void        LoadSettings();
    void        SaveSettings();
    void        ApplyColorForTemperature(int celsius);

    /*-----------------------------------------------------*\
    | Builds range_cards with `count` cards (clamped to       |
    | [AGILERGB_TEMP_RANGES_MIN, AGILERGB_TEMP_RANGES_MAX]),   |
    | applying a default green->yellow->orange->red gradient   |
    | and evenly-spaced default thresholds across them          |
    \*-----------------------------------------------------*/
    void        RebuildCards(int count);

    /*-----------------------------------------------------*\
    | Keeps every range contiguous and non-overlapping:       |
    | each card's min is always the previous card's max + 1    |
    \*-----------------------------------------------------*/
    void        SyncRangeAt(int index);

    void        UpdateAddRemoveButtons();
};
