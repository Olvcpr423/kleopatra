/*  commands/authenticatepivcardapplicationcommand.h

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2020 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#ifndef __KLEOPATRA_COMMMANDS_AUTHENTICATEPIVCARDAPPLICATIONCOMMAND_H__
#define __KLEOPATRA_COMMMANDS_AUTHENTICATEPIVCARDAPPLICATIONCOMMAND_H__

#include "cardcommand.h"

namespace GpgME
{
class Error;
}

namespace Kleo
{
namespace Commands
{

class AuthenticatePIVCardApplicationCommand : public CardCommand
{
    Q_OBJECT
public:
    explicit AuthenticatePIVCardApplicationCommand(const std::string &serialNumber, QWidget *parent);
    ~AuthenticatePIVCardApplicationCommand() override;

private:
    void doStart() override;

private:
    class Private;
    inline Private *d_func();
    inline const Private *d_func() const;
    Q_PRIVATE_SLOT(d_func(), void slotDialogAccepted())
    Q_PRIVATE_SLOT(d_func(), void slotDialogRejected())
    Q_PRIVATE_SLOT(d_func(), void slotResult(GpgME::Error))
};

} // namespace Commands
} // namespace Kleo

#endif // __KLEOPATRA_COMMMANDS_AUTHENTICATEPIVCARDAPPLICATIONCOMMAND_H__
