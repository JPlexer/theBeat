/****************************************
 *
 *   INSERT-PROJECT-NAME-HERE - INSERT-GENERIC-NAME-HERE
 *   Copyright (C) 2020 Victor Tran
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * *************************************/
#include "cdchecker.h"

#include <ui_cdchecker.h>
#include <QDBusInterface>
#include <QDBusConnection>
#include <pluginmediasource.h>
#include <statemanager.h>
#include <sourcemanager.h>
#include <playlist.h>
#include <tpromise.h>
#include "phononcdmediaitem.h"
#include "trackinfo.h"

#include <phonon/MediaObject>
#include <phonon/MediaController>
#include <phonon/MediaSource>
#include <phonon/AudioDataOutput>

#ifdef HAVE_MUSICBRAINZ
    #include <musicbrainz5/Query.h>
    #include <musicbrainz5/Release.h>
    #include <musicbrainz5/Medium.h>
    #include <musicbrainz5/Track.h>
    #include <musicbrainz5/Recording.h>
    #include <musicbrainz5/ArtistCredit.h>
    #include <musicbrainz5/NameCredit.h>
    #include <musicbrainz5/Artist.h>
#endif

using namespace Phonon;

struct CdCheckerPrivate {
    QString blockDevice;
    QDBusObjectPath cdDrivePath;

    PluginMediaSource* source;
    QStringList mbDiscIds;
    QList<TrackInfoPtr> trackInfo;

#ifdef HAVE_MUSICBRAINZ
    QString currentDiscId;
    QString currentReleaseId;
    MusicBrainz5::CReleaseList releases;
#endif
};

CdChecker::CdChecker(QString blockDevice, QWidget* parent) :
    QWidget(parent),
    ui(new Ui::CdChecker) {
    ui->setupUi(this);

    d = new CdCheckerPrivate();
    d->blockDevice = blockDevice;
    d->source = new PluginMediaSource(this);
    d->source->setName(tr("CD"));
    d->source->setIcon(QIcon::fromTheme("media-optical-audio"));

    //Get CD drive and attach to CD information
    QDBusInterface blockInterface("org.freedesktop.UDisks2", "/org/freedesktop/UDisks2/block_devices/" + blockDevice, "org.freedesktop.UDisks2.Block", QDBusConnection::systemBus());
    d->cdDrivePath = blockInterface.property("Drive").value<QDBusObjectPath>();
    QDBusConnection::systemBus().connect("org.freedesktop.UDisks2", d->cdDrivePath.path(), "org.freedesktop.DBus.Properties", "PropertiesChanged", this, SLOT(checkCd()));

    checkCd();
}

CdChecker::~CdChecker() {
    delete d;
}

void CdChecker::checkCd() {
    struct CdInformation {
        bool available = false;
        int numberOfTracks = 0;
        QStringList mbDiscId;
    };

    tPromise<CdInformation>::runOnNewThread([ = ](tPromiseFunctions<CdInformation>::SuccessFunction res, tPromiseFunctions<CdInformation>::FailureFunction rej) {
        if (d->cdDrivePath.path() == "") {
            res(CdInformation());
            return;
        }

        CdInformation info;
        QDBusInterface cdDriveInterface("org.freedesktop.UDisks2", d->cdDrivePath.path(), "org.freedesktop.UDisks2.Drive", QDBusConnection::systemBus());
        if (cdDriveInterface.property("MediaAvailable").toBool()) {

            QEventLoop* eventLoop = new QEventLoop();
            MediaObject* cdFinder = new MediaObject();
            MediaController* cdController = new MediaController(cdFinder);
            AudioDataOutput dummyOutput;
            createPath(cdFinder, &dummyOutput);
            connect(cdController, &MediaController::availableTitlesChanged, [ =, &info](int tracks) {
                //New CD inserted
                info.numberOfTracks = tracks;

                if (tracks > 0) {
                    connect(cdFinder, &MediaObject::metaDataChanged, [ =, &info]() {
                        info.mbDiscId = cdFinder->metaData(Phonon::MusicBrainzDiscIdMetaData);
                        eventLoop->quit();
                    });
                    cdController->setCurrentTitle(1);
                } else {
                    eventLoop->quit();
                }
            });

            cdFinder->setCurrentSource(MediaSource(Phonon::Cd, "/dev/" + d->blockDevice));
            cdFinder->play();
            cdFinder->pause();

            eventLoop->exec();

            eventLoop->deleteLater();
            cdFinder->deleteLater();
            cdController->deleteLater();
            info.available = true;
        }
        res(info);
    })->then([ = ](CdInformation info) {
        if (info.numberOfTracks == 0) {
            //No CD
            PhononCdMediaItem::blockDeviceGone(d->blockDevice);
            StateManager::instance()->sources()->removeSource(d->source);
        } else {
            d->mbDiscIds = info.mbDiscId;
            d->trackInfo.clear();

#ifdef HAVE_MUSICBRAINZ
            d->releases = MusicBrainz5::CReleaseList();
            d->currentReleaseId = "";
            d->currentDiscId = "";
#endif

            for (int i = 0; i < info.numberOfTracks; i++) {
                d->trackInfo.append(TrackInfoPtr(new TrackInfo(i)));
            }

            d->source->setName(tr("CD"));
            ui->albumTitleLabel->setText(tr("CD"));
            StateManager::instance()->sources()->addSource(d->source);

            updateTrackListing();

            if (!info.mbDiscId.isEmpty()) {
                loadMusicbrainzData(info.mbDiscId.first());
            }
        }
    });
}

void CdChecker::updateTrackListing() {
    ui->tracksWidget->clear();
    for (TrackInfoPtr info : d->trackInfo) {
        QListWidgetItem* item = new QListWidgetItem();
        item->setText(info->title());
        item->setData(Qt::UserRole, info->track());
        ui->tracksWidget->addItem(item);
    }
}

void CdChecker::loadMusicbrainzData(QString discId) {
    //Load information from MusicBrainz
#ifdef HAVE_MUSICBRAINZ
    d->currentDiscId = discId;

    MusicBrainz5::CQuery query("thebeat-3.0");
    try {
        d->releases = query.LookupDiscID(discId.toStdString());
        if (d->releases.Count() > 0) {
            selectMusicbrainzRelease(QString::fromStdString(d->releases.Item(0)->ID()));
        }
    }  catch (...) {
    }
#endif
}

void CdChecker::selectMusicbrainzRelease(QString release) {
#ifdef HAVE_MUSICBRAINZ
    d->currentReleaseId = release;

    tPromise<MusicBrainz5::CRelease*>::runOnNewThread([ = ](tPromiseFunctions<MusicBrainz5::CRelease*>::SuccessFunction res, tPromiseFunctions<MusicBrainz5::CMetadata>::FailureFunction rej) {
        try {
            MusicBrainz5::CQuery query("thebeat-3.0");
            res(query.LookupRelease(release.toStdString()).Clone());
        } catch (...) {
            rej("Failure");
        }
    })->then([ = ](MusicBrainz5::CRelease * release) {
        ui->albumTitleLabel->setText(QString::fromStdString(release->Title()));
        d->source->setName(QString::fromStdString(release->Title()));

        MusicBrainz5::CMediumList* mediumList = release->MediumList();
        for (int h = 0; h < mediumList->NumItems(); h++) {
            MusicBrainz5::CMedium* medium = mediumList->Item(h);
            if (medium->ContainsDiscID(d->currentDiscId.toStdString())) {
                MusicBrainz5::CTrackList* tracks = medium->TrackList();
                for (int i = 0; i < tracks->Count(); i++) {
                    if (d->trackInfo.count() <= i) continue;

                    MusicBrainz5::CTrack* track = tracks->Item(i);
                    MusicBrainz5::CRecording* recording = track->Recording();
                    if (recording) {
                        QStringList artists;
                        MusicBrainz5::CNameCreditList* nameCreditList = release->ArtistCredit()->NameCreditList();
                        for (int j = 0; j < nameCreditList->Count(); j++) {
                            MusicBrainz5::CNameCredit* credit = nameCreditList->Item(j);
                            artists.append(QString::fromStdString(credit->Artist()->Name()));
                        }
                        artists.removeDuplicates();

                        TrackInfoPtr trackInfo = d->trackInfo.at(i);
                        trackInfo->setData(QString::fromStdString(recording->Title()), artists, QString::fromStdString(release->Title()));
                    }
                }

                break;
            }
        }

        updateTrackListing();
        delete release;
    });
#endif
}


void CdChecker::on_tracksWidget_itemActivated(QListWidgetItem* item) {
    int track = item->data(Qt::UserRole).toInt();
    StateManager::instance()->playlist()->addItem(new PhononCdMediaItem(d->blockDevice, d->trackInfo.at(track)));
}

void CdChecker::on_enqueueAllButton_clicked() {
    for (TrackInfoPtr trackInfo : d->trackInfo) {
        StateManager::instance()->playlist()->addItem(new PhononCdMediaItem(d->blockDevice, trackInfo));
    }
}

void CdChecker::on_ejectButton_clicked() {
    //Remove any tracks in the playlist
    PhononCdMediaItem::blockDeviceGone(d->blockDevice);

    //Eject the CD
    QDBusInterface cdDriveInterface("org.freedesktop.UDisks2", d->cdDrivePath.path(), "org.freedesktop.UDisks2.Drive", QDBusConnection::systemBus());
    cdDriveInterface.call(QDBus::NoBlock, "Eject", QVariantMap());
}
