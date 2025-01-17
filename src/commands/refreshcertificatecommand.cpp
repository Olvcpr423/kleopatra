/* -*- mode: c++; c-basic-offset:4 -*-
    commands/refreshcertificatecommand.cpp

    This file is part of Kleopatra, the KDE keymanager
    SPDX-FileCopyrightText: 2022 g10 Code GmbH
    SPDX-FileContributor: Ingo Klöcker <dev@ingo-kloecker.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <config-kleopatra.h>

#include "command_p.h"
#include "refreshcertificatecommand.h"

#include <Libkleo/Formatting>

#include <KLocalizedString>
#include <KMessageBox>

#include <QGpgME/Protocol>
#if QGPGME_SUPPORTS_KEY_REFRESH
#include <QGpgME/ReceiveKeysJob>
#include <QGpgME/RefreshKeysJob>
#endif

#include <gpgme++/importresult.h>

#include "kleopatra_debug.h"

using namespace Kleo;
using namespace GpgME;

class RefreshCertificateCommand::Private : public Command::Private
{
    friend class ::RefreshCertificateCommand;
    RefreshCertificateCommand *q_func() const
    {
        return static_cast<RefreshCertificateCommand *>(q);
    }

public:
    explicit Private(RefreshCertificateCommand *qq);
    ~Private() override;

    void start();
    void cancel();

#if QGPGME_SUPPORTS_KEY_REFRESH
    std::unique_ptr<QGpgME::ReceiveKeysJob> startOpenPGPJob();
    std::unique_ptr<QGpgME::RefreshKeysJob> startSMIMEJob();
#endif
    void onOpenPGPJobResult(const ImportResult &result);
    void onSMIMEJobResult(const Error &err);
    void showError(const Error &err);

private:
    Key key;
#if QGPGME_SUPPORTS_KEY_REFRESH
    QPointer<QGpgME::Job> job;
#endif
};

RefreshCertificateCommand::Private *RefreshCertificateCommand::d_func()
{
    return static_cast<Private *>(d.get());
}
const RefreshCertificateCommand::Private *RefreshCertificateCommand::d_func() const
{
    return static_cast<const Private *>(d.get());
}

#define d d_func()
#define q q_func()

RefreshCertificateCommand::Private::Private(RefreshCertificateCommand *qq)
    : Command::Private{qq}
{
}

RefreshCertificateCommand::Private::~Private() = default;

namespace
{
Key getKey(const std::vector<Key> &keys)
{
    if (keys.size() != 1) {
        qCWarning(KLEOPATRA_LOG) << "Expected exactly one key, but got" << keys.size();
        return {};
    }
    const Key key = keys.front();
    if (key.protocol() == GpgME::UnknownProtocol) {
        qCWarning(KLEOPATRA_LOG) << "Key has unknown protocol";
        return {};
    }
    return key;
}
}

void RefreshCertificateCommand::Private::start()
{
    key = getKey(keys());
    if (key.isNull()) {
        finished();
        return;
    }

#if QGPGME_SUPPORTS_KEY_REFRESH
    std::unique_ptr<QGpgME::Job> refreshJob;
    switch (key.protocol()) {
    case GpgME::OpenPGP:
        refreshJob = startOpenPGPJob();
        break;
    case GpgME::CMS:
        refreshJob = startSMIMEJob();
        break;
    default:; // cannot happen ;-)
    }
    if (!refreshJob) {
        finished();
        return;
    }
    job = refreshJob.release();
#else
    error(i18n("The backend does not support updating individual certificates."));
    finished();
#endif
}

void RefreshCertificateCommand::Private::cancel()
{
#if QGPGME_SUPPORTS_KEY_REFRESH
    if (job) {
        job->slotCancel();
    }
    job.clear();
#endif
}

#if QGPGME_SUPPORTS_KEY_REFRESH
std::unique_ptr<QGpgME::ReceiveKeysJob> RefreshCertificateCommand::Private::startOpenPGPJob()
{
    std::unique_ptr<QGpgME::ReceiveKeysJob> refreshJob{QGpgME::openpgp()->receiveKeysJob()};
    Q_ASSERT(refreshJob);

    connect(refreshJob.get(), &QGpgME::ReceiveKeysJob::result, q, [this](const GpgME::ImportResult &result) {
        onOpenPGPJobResult(result);
    });
#if QGPGME_JOB_HAS_NEW_PROGRESS_SIGNALS
    connect(refreshJob.get(), &QGpgME::Job::jobProgress, q, &Command::progress);
#else
    connect(refreshJob.get(), &QGpgME::Job::progress, q, [this](const QString &, int current, int total) {
        Q_EMIT q->progress(current, total);
    });
#endif

    const GpgME::Error err = refreshJob->start({QString::fromLatin1(key.primaryFingerprint())});
    if (err) {
        showError(err);
        return {};
    }
    Q_EMIT q->info(i18nc("@info:status", "Updating key..."));

    return refreshJob;
}

std::unique_ptr<QGpgME::RefreshKeysJob> RefreshCertificateCommand::Private::startSMIMEJob()
{
    std::unique_ptr<QGpgME::RefreshKeysJob> refreshJob{QGpgME::smime()->refreshKeysJob()};
    Q_ASSERT(refreshJob);

    connect(refreshJob.get(), &QGpgME::RefreshKeysJob::result, q, [this](const GpgME::Error &err) {
        onSMIMEJobResult(err);
    });
#if QGPGME_JOB_HAS_NEW_PROGRESS_SIGNALS
    connect(refreshJob.get(), &QGpgME::Job::jobProgress, q, &Command::progress);
#else
    connect(refreshJob.get(), &QGpgME::Job::progress, q, [this](const QString &, int current, int total) {
        Q_EMIT q->progress(current, total);
    });
#endif

    const GpgME::Error err = refreshJob->start({key});
    if (err) {
        showError(err);
        return {};
    }
    Q_EMIT q->info(i18nc("@info:status", "Updating certificate..."));

    return refreshJob;
}
#endif

namespace
{
static auto informationOnChanges(const ImportResult &result)
{
    QString text;

    // if additional keys have been retrieved via WKD, then most of the below
    // details are just a guess and may concern the additional keys instead of
    // the refresh keys; this could only be clarified by a thorough comparison of
    // unrefreshed and refreshed key

    if (result.numUnchanged() == result.numConsidered()) {
        // if numUnchanged < numConsidered, then it is not clear whether the refreshed key
        // hasn't changed or whether another key retrieved via WKD hasn't changed
        text = i18n("The key hasn't changed.");
    } else if (result.newRevocations() > 0) {
        // it is possible that a revoked key has been newly imported via WKD,
        // but it is much more likely that the refreshed key was revoked
        text = i18n("The key has been revoked.");
    } else {
        // it doesn't make much sense to list below details if the key has been revoked
        text = i18n("The key has been updated.");

        QStringList details;
        if (result.newUserIDs() > 0) {
            details.push_back(i18n("New user IDs: %1", result.newUserIDs()));
        }
        if (result.newSubkeys() > 0) {
            details.push_back(i18n("New subkeys: %1", result.newSubkeys()));
        }
        if (result.newSignatures() > 0) {
            details.push_back(i18n("New signatures: %1", result.newSignatures()));
        }
        if (!details.empty()) {
            text += QLatin1String{"<br><br>"} + details.join(QLatin1String{"<br>"});
        }
    }

    text = QLatin1String{"<p>"} + text + QLatin1String{"</p>"};
    if (result.numImported() > 0) {
        text += QLatin1String{"<p>"}
            + i18np("Additionally, one new key has been retrieved.", "Additionally, %1 new keys have been retrieved.", result.numImported())
            + QLatin1String{"</p>"};
    }

    return text;
}

}

void RefreshCertificateCommand::Private::onOpenPGPJobResult(const ImportResult &result)
{
    if (result.error()) {
        showError(result.error());
        finished();
        return;
    }

    if (!result.error().isCanceled()) {
        information(informationOnChanges(result), i18nc("@title:window", "Key Updated"));
    }
    finished();
}

void RefreshCertificateCommand::Private::onSMIMEJobResult(const Error &err)
{
    if (err) {
        showError(err);
        finished();
        return;
    }

    if (!err.isCanceled()) {
        information(i18nc("@info", "The certificate has been updated."), i18nc("@title:window", "Certificate Updated"));
    }
    finished();
}

void RefreshCertificateCommand::Private::showError(const Error &err)
{
    error(xi18nc("@info",
                 "<para>An error occurred while updating the certificate:</para>"
                 "<para><message>%1</message></para>",
                 Formatting::errorAsString(err)),
          i18nc("@title:window", "Update Failed"));
}

RefreshCertificateCommand::RefreshCertificateCommand(const GpgME::Key &key)
    : Command{key, new Private{this}}
{
}

RefreshCertificateCommand::~RefreshCertificateCommand() = default;

void RefreshCertificateCommand::doStart()
{
    d->start();
}

void RefreshCertificateCommand::doCancel()
{
    d->cancel();
}

#undef d
#undef q

#include "moc_refreshcertificatecommand.cpp"
