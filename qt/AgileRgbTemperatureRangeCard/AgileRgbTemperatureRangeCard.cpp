/*---------------------------------------------------------*\
| AgileRgbTemperatureRangeCard.cpp                           |
|                                                             |
|   A single temperature range card: label, min/max degree    |
|   threshold(s), and a color picker for that range           |
|                                                             |
|   This file is part of the Agile Rgb project                |
|   SPDX-License-Identifier: GPL-2.0-or-later                 |
\*---------------------------------------------------------*/

#include "AgileRgbTemperatureRangeCard.h"
#include "ui_AgileRgbTemperatureRangeCard.h"

AgileRgbTemperatureRangeCard::AgileRgbTemperatureRangeCard(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::AgileRgbTemperatureRangeCard)
{
    ui->setupUi(this);

    kind = TemperatureRangeKind::MEDIUM;

    UpdateSwatch();
}

AgileRgbTemperatureRangeCard::~AgileRgbTemperatureRangeCard()
{
    delete ui;
}

void AgileRgbTemperatureRangeCard::SetRangeKind(TemperatureRangeKind new_kind)
{
    kind = new_kind;

    switch(kind)
    {
        case TemperatureRangeKind::LOW:
            ui->TitleLabel->setText(tr("Low Temperature"));
            break;

        case TemperatureRangeKind::MEDIUM:
            ui->TitleLabel->setText(tr("Medium Temperature"));
            break;

        case TemperatureRangeKind::HIGH:
            ui->TitleLabel->setText(tr("High Temperature (Alert)"));
            break;
    }

    UpdateFieldVisibility();
}

void AgileRgbTemperatureRangeCard::UpdateFieldVisibility()
{
    /*-----------------------------------------------------*\
    | LOW only has an upper bound ("below X")                |
    | HIGH only has a lower bound ("above Y")                 |
    | MEDIUM has both                                         |
    \*-----------------------------------------------------*/
    bool show_min = (kind != TemperatureRangeKind::LOW);
    bool show_max = (kind != TemperatureRangeKind::HIGH);

    ui->MinTempLabel->setVisible(show_min);
    ui->MinTempSpinBox->setVisible(show_min);
    ui->MaxTempLabel->setVisible(show_max);
    ui->MaxTempSpinBox->setVisible(show_max);
}

void AgileRgbTemperatureRangeCard::SetMinTemperature(int celsius)
{
    ui->MinTempSpinBox->blockSignals(true);
    ui->MinTempSpinBox->setValue(celsius);
    ui->MinTempSpinBox->blockSignals(false);
}

void AgileRgbTemperatureRangeCard::SetMaxTemperature(int celsius)
{
    ui->MaxTempSpinBox->blockSignals(true);
    ui->MaxTempSpinBox->setValue(celsius);
    ui->MaxTempSpinBox->blockSignals(false);
}

int AgileRgbTemperatureRangeCard::GetMinTemperature()
{
    return ui->MinTempSpinBox->value();
}

int AgileRgbTemperatureRangeCard::GetMaxTemperature()
{
    return ui->MaxTempSpinBox->value();
}

void AgileRgbTemperatureRangeCard::SetColor(const QColor& color)
{
    ui->ColorWheelBox->blockSignals(true);
    ui->ColorWheelBox->setColor(color);
    ui->ColorWheelBox->blockSignals(false);

    UpdateSwatch();
}

QColor AgileRgbTemperatureRangeCard::GetColor()
{
    return ui->ColorWheelBox->color();
}

void AgileRgbTemperatureRangeCard::UpdateSwatch()
{
    QColor color = ui->ColorWheelBox->color();

    ui->ColorSwatch->setStyleSheet(QString("background-color: %1;").arg(color.name()));
}

void AgileRgbTemperatureRangeCard::on_MinTempSpinBox_valueChanged(int value)
{
    /*-----------------------------------------------------*\
    | Keep min <= max                                        |
    \*-----------------------------------------------------*/
    if(ui->MaxTempSpinBox->isVisible() && value > ui->MaxTempSpinBox->value())
    {
        ui->MaxTempSpinBox->setValue(value);
    }

    emit RangeChanged();
}

void AgileRgbTemperatureRangeCard::on_MaxTempSpinBox_valueChanged(int value)
{
    if(ui->MinTempSpinBox->isVisible() && value < ui->MinTempSpinBox->value())
    {
        ui->MinTempSpinBox->setValue(value);
    }

    emit RangeChanged();
}

void AgileRgbTemperatureRangeCard::on_ColorWheelBox_colorChanged(const QColor /*color*/)
{
    UpdateSwatch();

    emit RangeChanged();
}
