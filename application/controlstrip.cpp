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
#include "controlstrip.h"
#include "ui_controlstrip.h"

#include <statemanager.h>
#include <playlist.h>
#include <tvariantanimation.h>
#include "common.h"

struct ControlStripPrivate {
    MediaItem* currentItem = nullptr;
    bool isCollapsed = true;

    tVariantAnimation* volumeBarAnim;
};

ControlStrip::ControlStrip(QWidget* parent) :
    QWidget(parent),
    ui(new Ui::ControlStrip) {
    ui->setupUi(this);

    d = new ControlStripPrivate();

    ui->playPauseButton->setIconSize(SC_DPI_T(QSize(32, 32), QSize));
    connect(StateManager::instance()->playlist(), &Playlist::stateChanged, this, &ControlStrip::updateState);
    connect(StateManager::instance()->playlist(), &Playlist::currentItemChanged, this, &ControlStrip::updateCurrentItem);
    connect(StateManager::instance()->playlist(), &Playlist::repeatOneChanged, ui->repeatOneButton, &QToolButton::setChecked);
    connect(StateManager::instance()->playlist(), &Playlist::shuffleChanged, ui->shuffleButton, &QToolButton::setChecked);
    connect(StateManager::instance()->playlist(), &Playlist::volumeChanged, this, [ = ] {
        ui->volumeSlider->setValue(StateManager::instance()->playlist()->volume() * 100);
    });
    ui->volumeSlider->setValue(StateManager::instance()->playlist()->volume() * 100);

    this->setFixedHeight(0);
    ui->volumeWidget->installEventFilter(this);
    ui->volumeSlider->setFixedWidth(0);


   d->volumeBarAnim = new tVariantAnimation(this);
   d->volumeBarAnim->setDuration(500);
   d->volumeBarAnim->setEasingCurve(QEasingCurve::OutCubic);
   connect(d->volumeBarAnim, &tVariantAnimation::valueChanged, this, [ = ](QVariant value) {
       this->setFixedHeight(value.toInt());
   });
   connect(d->volumeBarAnim, &tVariantAnimation::finished, this, [ = ] {
       this->setFixedHeight(QWIDGETSIZE_MAX);
       d->volumeBarAnim->deleteLater();
   });

    updateCurrentItem();
}

ControlStrip::~ControlStrip() {
    delete d;
    delete ui;
}

void ControlStrip::on_playPauseButton_clicked() {
    StateManager::instance()->playlist()->playPause();
}

void ControlStrip::updateState() {
    switch (StateManager::instance()->playlist()->state()) {
        case Playlist::Playing:
            expand();
            ui->playPauseButton->setIcon(QIcon::fromTheme("media-playback-pause"));
            break;
        case Playlist::Paused:
            expand();
            ui->playPauseButton->setIcon(QIcon::fromTheme("media-playback-start"));
            break;
        case Playlist::Stopped:
            collapse();
            break;
    }
}

void ControlStrip::updateCurrentItem() {
    if (d->currentItem) {
        d->currentItem->disconnect(this);
    }
    d->currentItem = StateManager::instance()->playlist()->currentItem();
    if (d->currentItem) {
        connect(d->currentItem, &MediaItem::metadataChanged, this, &ControlStrip::updateMetadata);
        connect(d->currentItem, &MediaItem::elapsedChanged, this, &ControlStrip::updateBar);
        connect(d->currentItem, &MediaItem::durationChanged, this, &ControlStrip::updateBar);
        updateMetadata();
    }
}

void ControlStrip::updateMetadata() {
    ui->titleLabel->setText(d->currentItem->title());

    QStringList parts;
    parts.append(QLocale().createSeparatedList(d->currentItem->authors()));
    parts.append(d->currentItem->album());

    parts.removeAll("");
    ui->metadataLabel->setText(parts.join(" · "));

    QImage image = d->currentItem->albumArt();
    if (image.isNull()) {
        ui->artLabel->setPixmap(QIcon::fromTheme("audio").pixmap(SC_DPI_T(QSize(48, 48), QSize)));
    } else {
        ui->artLabel->setPixmap(QPixmap::fromImage(image).scaled(SC_DPI_T(QSize(48, 48), QSize), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void ControlStrip::updateBar() {
    QSignalBlocker blocker(ui->progressSlider);
    ui->progressSlider->setMaximum(d->currentItem->duration());
    ui->progressSlider->setValue(d->currentItem->elapsed());

    ui->durationLabel->setText(Common::durationToString(d->currentItem->duration()));
    ui->elapsedLabel->setText(Common::durationToString(d->currentItem->elapsed()));
}

void ControlStrip::expand() {
    if (!d->isCollapsed) return;

    d->volumeBarAnim->stop();
    d->volumeBarAnim->setStartValue(this->height());
    d->volumeBarAnim->setEndValue(this->sizeHint().height());
    d->volumeBarAnim->start();
    d->isCollapsed = false;
}

void ControlStrip::collapse() {
    if (d->isCollapsed) return;

    d->volumeBarAnim->stop();
    d->volumeBarAnim->setStartValue(this->height());
    d->volumeBarAnim->setEndValue(0);
    d->volumeBarAnim->start();
    d->isCollapsed = true;
}

void ControlStrip::on_skipBackButton_clicked() {
    StateManager::instance()->playlist()->previous();
}

void ControlStrip::on_skipNextButton_clicked() {
    StateManager::instance()->playlist()->next();
}

void ControlStrip::on_progressSlider_valueChanged(int value) {
    d->currentItem->seek(value);
}

void ControlStrip::on_repeatOneButton_toggled(bool checked) {
    StateManager::instance()->playlist()->setRepeatOne(checked);
}

void ControlStrip::on_shuffleButton_toggled(bool checked) {
    StateManager::instance()->playlist()->setShuffle(checked);
}

void ControlStrip::on_volumeSlider_valueChanged(int value) {
    StateManager::instance()->playlist()->setVolume(static_cast<double>(value) / 100);
}

bool ControlStrip::eventFilter(QObject* watched, QEvent* event) {
    if (watched == ui->volumeWidget) {
        if (event->type() == QEvent::Enter) {
            tVariantAnimation* anim = new tVariantAnimation(this);
            anim->setStartValue(ui->volumeSlider->width());
            anim->setEndValue(SC_DPI(150));
            anim->setDuration(500);
            anim->setEasingCurve(QEasingCurve::OutCubic);
            connect(anim, &tVariantAnimation::valueChanged, this, [ = ](QVariant value) {
                ui->volumeSlider->setFixedWidth(value.toInt());
                ui->volumeWidget->updateGeometry();
            });
            connect(anim, &tVariantAnimation::finished, anim, &tVariantAnimation::deleteLater);
            anim->start();
        } else if (event->type() == QEvent::Leave) {
            tVariantAnimation* anim = new tVariantAnimation(this);
            anim->setStartValue(ui->volumeSlider->width());
            anim->setEndValue(0);
            anim->setDuration(500);
            anim->setEasingCurve(QEasingCurve::OutCubic);
            connect(anim, &tVariantAnimation::valueChanged, this, [ = ](QVariant value) {
                ui->volumeSlider->setFixedWidth(value.toInt());
                ui->volumeWidget->updateGeometry();
            });
            connect(anim, &tVariantAnimation::finished, anim, &tVariantAnimation::deleteLater);
            anim->start();
        }
    }
    return false;
}
