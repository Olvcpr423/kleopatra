/* -*- mode: c++; c-basic-offset:4 -*-
    utils/expiration.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2023 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "expiration.h"

#include <settings.h>

#include <KDateComboBox>
#include <KLocalizedString>

QDate Kleo::minimumExpirationDate()
{
    return expirationDateRange().minimum;
}

QDate Kleo::maximumExpirationDate()
{
    return expirationDateRange().maximum;
}

Kleo::DateRange Kleo::expirationDateRange()
{
    Kleo::DateRange range;

    const auto settings = Kleo::Settings{};
    const auto today = QDate::currentDate();

    const auto minimumExpiry = std::max(1, settings.validityPeriodInDaysMin());
    range.minimum = today.addDays(minimumExpiry);

    const auto maximumExpiry = settings.validityPeriodInDaysMax();
    if (maximumExpiry >= 0) {
        range.maximum = std::max(today.addDays(maximumExpiry), range.minimum);
    }

    return range;
}

static QString dateToString(const QDate &date, QWidget *widget)
{
    // workaround for QLocale using "yy" way too often for years
    // stolen from KDateComboBox
    auto locale = widget ? widget->locale() : QLocale{};
    const auto dateFormat = (locale.dateFormat(QLocale::ShortFormat) //
                                    .replace(QLatin1String{"yy"}, QLatin1String{"yyyy"})
                                    .replace(QLatin1String{"yyyyyyyy"}, QLatin1String{"yyyy"}));
    return locale.toString(date, dateFormat);
}

static QString validityPeriodHint(const Kleo::DateRange &dateRange, QWidget *widget)
{
    // the minimum date is always valid
    if (dateRange.maximum.isValid()) {
        if (dateRange.maximum == dateRange.minimum) {
            return i18nc("@info", "The validity period cannot be changed.");
        } else {
            return i18nc("@info ... between <a date> and <another date>.", "The validity period must end between %1 and %2.",
                         dateToString(dateRange.minimum, widget), dateToString(dateRange.maximum, widget));
        }
    } else {
        return i18nc("@info ... after <a date>.", "The validity period must end after %1.", dateToString(dateRange.minimum, widget));
    }
}

void Kleo::setUpExpirationDateComboBox(KDateComboBox *dateCB)
{
    const auto dateRange = expirationDateRange();
    dateCB->setMinimumDate(dateRange.minimum);
    dateCB->setMaximumDate(dateRange.maximum);
    if (dateRange.minimum == dateRange.maximum) {
        // validity period is a fixed number of days
        dateCB->setEnabled(false);
    }
    dateCB->setToolTip(validityPeriodHint(dateRange, dateCB));
}