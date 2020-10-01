/*  smartcard/netkeycard.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2017 Intevation GmbH

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "netkeycard.h"

#include "kleopatra_debug.h"

#include <gpgme++/error.h>
#include <gpgme++/context.h>
#include <gpgme++/keylistresult.h>

#include <memory>
#include <string>

using namespace Kleo;
using namespace Kleo::SmartCard;

// static
const std::string NetKeyCard::AppName = "nks";

namespace
{
static std::string parse_keypairinfo(const std::string &kpi)
{
    static const char hexchars[] = "0123456789abcdefABCDEF";
    return '&' + kpi.substr(0, kpi.find_first_not_of(hexchars));
}

static GpgME::Key parse_keypairinfo_and_lookup_key(GpgME::Context *ctx, const std::string &kpi)
{
    if (!ctx) {
        return GpgME::Key();
    }
    const std::string pattern = parse_keypairinfo(kpi);
    qCDebug(KLEOPATRA_LOG) << "parse_keypairinfo_and_lookup_key: pattern=" << pattern.c_str();
    if (const auto err = ctx->startKeyListing(pattern.c_str())) {
        qCDebug(KLEOPATRA_LOG) << "parse_keypairinfo_and_lookup_key: startKeyListing failed:" << err.asString();
        return GpgME::Key();
    }
    GpgME::Error e;
    const auto key = ctx->nextKey(e);
    ctx->endKeyListing();
    qCDebug(KLEOPATRA_LOG) << "parse_keypairinfo_and_lookup_key: e=" << e.code() << "; key.isNull()" << key.isNull();
    return key;
}

} // namespace

NetKeyCard::NetKeyCard(const Card &card)
    : Card(card)
{
    setAppName(AppName);
}

void NetKeyCard::setKeyPairInfo(const std::vector<std::string> &infos)
{
    // check that any of the keys are new
    const std::unique_ptr<GpgME::Context> klc(GpgME::Context::createForProtocol(GpgME::CMS));
    if (!klc.get()) {
        return;
    }
    klc->setKeyListMode(GpgME::Ephemeral);
    klc->addKeyListMode(GpgME::Validate);

    setCanLearnKeys(false);
    mKeys.clear();
    for (const auto &info: infos) {
        const auto key = parse_keypairinfo_and_lookup_key(klc.get(), info);
        if (key.isNull()) {
            setCanLearnKeys(true);
        }
        mKeys.push_back(key);
    }
}


// State 0 -> NKS PIN Retry counter
// State 1 -> NKS PUK Retry counter
// State 2 -> SigG PIN Retry counter
// State 3 -> SigG PUK Retry counter

bool NetKeyCard::hasNKSNullPin() const
{
    const auto states = pinStates();
    if (states.size() < 2) {
        qCWarning(KLEOPATRA_LOG) << "Invalid size of pin states:" << states.size();
        return false;
    }
    return states[0] == Card::NullPin;
}

bool NetKeyCard::hasSigGNullPin() const
{
    const auto states = pinStates();
    if (states.size() < 4) {
        qCWarning(KLEOPATRA_LOG) << "Invalid size of pin states:" << states.size();
        return false;
    }
    return states[2] == Card::NullPin;
}

std::vector<GpgME::Key> NetKeyCard::keys() const
{
    return mKeys;
}
