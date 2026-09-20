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
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Ui
{
    class AgileRgbTemperaturePage;
}

class AgileRgbTemperaturePage : public QFrame
{
    Q_OBJECT

public:
    explicit AgileRgbTemperaturePage(QWidget *parent = nullptr);
    ~AgileRgbTemperaturePage();

public slots:
    void        UpdateCurrentTemperature();

private slots:
    void        on_SaveButton_clicked();
    void        on_CancelButton_clicked();
    void        on_EnableCheckBox_toggled(bool checked);

private:
    Ui::AgileRgbTemperaturePage*   ui;
    QTimer*                        poll_timer;

    void        LoadSettings();
    void        SaveSettings();
    void        ApplyColorForTemperature(int celsius);
};
