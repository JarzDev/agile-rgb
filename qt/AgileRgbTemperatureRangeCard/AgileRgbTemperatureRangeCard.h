/*---------------------------------------------------------*\
| AgileRgbTemperatureRangeCard.h                             |
|                                                             |
|   A single temperature range card: label, min/max degree    |
|   threshold(s), and a color picker for that range           |
|                                                             |
|   This file is part of the Agile Rgb project                |
|   SPDX-License-Identifier: GPL-2.0-or-later                 |
\*---------------------------------------------------------*/

#pragma once

#include <QFrame>
#include <QColor>

namespace Ui
{
    class AgileRgbTemperatureRangeCard;
}

enum class TemperatureRangeKind
{
    LOW,     /* temperature < max                */
    MEDIUM,  /* min <= temperature <= max         */
    HIGH     /* temperature > min                 */
};

class AgileRgbTemperatureRangeCard : public QFrame
{
    Q_OBJECT

public:
    explicit AgileRgbTemperatureRangeCard(QWidget *parent = nullptr);
    ~AgileRgbTemperatureRangeCard();

    void        SetRangeKind(TemperatureRangeKind kind);

    void        SetMinTemperature(int celsius);
    void        SetMaxTemperature(int celsius);
    int         GetMinTemperature();
    int         GetMaxTemperature();

    void        SetColor(const QColor& color);
    QColor      GetColor();

signals:
    void        RangeChanged();

private slots:
    void        on_MinTempSpinBox_valueChanged(int value);
    void        on_MaxTempSpinBox_valueChanged(int value);
    void        on_ColorWheelBox_colorChanged(const QColor color);

private:
    Ui::AgileRgbTemperatureRangeCard*  ui;
    TemperatureRangeKind               kind;

    void        UpdateSwatch();
    void        UpdateFieldVisibility();
};
