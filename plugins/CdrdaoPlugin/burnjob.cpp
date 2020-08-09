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
#include "burnjob.h"
#include "burnjobwidget.h"

#include <QTextStream>
#include <QTemporaryDir>
#include <QProcess>

#include <taglib/fileref.h>
#include <taglib/tag.h>

struct BurnJobPrivate {
    QStringList sourceFiles;
    QString blockDevice;
    QString albumTitle;
    int nextItem = 0;

    BurnJob::State state = BurnJob::Processing;

    QString description;
    bool canCancel;

    quint64 progress;
    quint64 maxProgress;

    QByteArray processBuffer;

    QTemporaryDir workDir;
};

BurnJob::BurnJob(QStringList files, QString blockDevice, QString albumTitle, QObject* parent) : tJob(parent) {
    d = new BurnJobPrivate();
    d->sourceFiles = files;
    d->blockDevice = blockDevice;
    d->albumTitle = albumTitle;

    d->description = tr("Preparing to burn");
    performNextAction();
}

BurnJob::~BurnJob() {
    delete d;
}

QString BurnJob::description() {
    return d->description;
}

bool BurnJob::canCancel() {
    return d->canCancel;
}

#include <QDebug>
void BurnJob::performNextAction() {
    if (d->nextItem < d->sourceFiles.count()) {
        d->description = tr("Preparing Track %1").arg(d->nextItem + 1);
        emit descriptionChanged(d->description);

        //Transcode the file with ffmpeg
        QString sourceFile = d->sourceFiles.at(d->nextItem);
        QProcess* ffmpeg = new QProcess();
        ffmpeg->setWorkingDirectory(d->workDir.path());
        connect(ffmpeg, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [ = ](int exitCode, QProcess::ExitStatus status) {
            if (exitCode != 0) {
                d->state = Failed;
                emit stateChanged(Failed);

                d->description = tr("Couldn't transcode track");
                emit descriptionChanged(d->description);
                return;
            }

            d->nextItem++;
            performNextAction();
            ffmpeg->deleteLater();
        });
        ffmpeg->start("ffmpeg", {"-i", sourceFile, d->workDir.filePath(QStringLiteral("Track%1.wav").arg(d->nextItem, 2, 10, QLatin1Char('0')))});
    } else if (d->nextItem == d->sourceFiles.count()) {
        d->description = tr("Preparing to burn").arg(d->nextItem + 1);
        emit descriptionChanged(d->description);

        //Generate the TOC file and call cdrdao
        QFile tocFile(d->workDir.filePath("contents.toc"));
        tocFile.open(QFile::WriteOnly);

        QTextStream tocStream(&tocFile);
        tocStream << "CD_DA\n";
        //Write CD Text
        tocStream << "CD_TEXT {\n";
        tocStream << "LANGUAGE_MAP {\n";
        tocStream << "0:EN\n";
        tocStream << "}\n"; //LANGUAGE_MAP
        tocStream << "LANGUAGE 0 {\n";
        tocStream << QStringLiteral(R"(TITLE "%1")" "\n").arg(d->albumTitle);
        tocStream << "}\n"; //LANGUAGE 0
        tocStream << "}\n"; //CD_TEXT

        for (int i = 0; i < d->sourceFiles.count(); i++) {
            tocStream << "TRACK AUDIO\n";

            TagLib::FileRef file(d->sourceFiles.at(i).toStdString().data());
            if (file.tag()) {
                //Write CD Text
                tocStream << "CD_TEXT {\n";
                tocStream << "LANGUAGE 0 {\n";
                if (!file.tag()->title().isEmpty()) tocStream << QStringLiteral(R"(TITLE "%1")" "\n").arg(file.tag()->title().toCString());
                if (!file.tag()->artist().isEmpty()) tocStream << QStringLiteral(R"(ARRANGER "%1")" "\n").arg(file.tag()->artist().toCString());
                if (!file.tag()->artist().isEmpty()) tocStream << QStringLiteral(R"(COMPOSER "%1")" "\n").arg(file.tag()->artist().toCString());
                tocStream << "}\n"; //LANGUAGE 0
                tocStream << "}\n"; //CD_TEXT
            }

            tocStream << QStringLiteral(R"(FILE "Track%1.wav" 0)" "\n").arg(i, 2, 10, QLatin1Char('0'));
        }
        tocFile.close();

        //Call cdrdao to write to the disc
        QProcess* process = new QProcess();
        process->setProcessChannelMode(QProcess::MergedChannels);
        process->setWorkingDirectory(d->workDir.path());
        connect(process, &QProcess::readyRead, this, [ = ] {
            QByteArray peek = process->peek(1024);
            while (process->canReadLine() || peek.contains('\r')) {
                QString line;
                if (process->canReadLine()) {
                    line = process->readLine();
                } else {
                    line = process->read(peek.indexOf('\r') + 1);
                }
                line = line.trimmed();
                qDebug() << line;

                if (line.startsWith("Writing track")) {
                    QStringList parts = line.split(" ");

                    d->description = tr("Burning Track %1").arg(parts.at(2));
                    emit descriptionChanged(d->description);
                } else if (line.startsWith("Wrote ")) {
                    if (line.contains("blocks.")) {
                        d->description = tr("Finalising CD");
                        emit descriptionChanged(d->description);
                        d->progress = 0;
                        d->maxProgress = 0;

                        emit totalProgressChanged(d->progress);
                        emit progressChanged(d->progress);
                    } else {
                        QStringList parts = line.split(" ");
                        d->progress = parts.at(1).toInt();
                        d->maxProgress = parts.at(3).toInt();

                        emit totalProgressChanged(d->maxProgress);
                        emit progressChanged(d->progress);
                    }
                }

                peek = process->peek(1024);
            }
        });
        connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, [ = ](int exitCode, QProcess::ExitStatus status) {
            if (exitCode != 0) {
                d->state = Failed;
                emit stateChanged(Failed);

                d->description = tr("Couldn't burn tracks");
                emit descriptionChanged(d->description);
                return;
            }

            d->nextItem++;
            performNextAction();
            process->deleteLater();
        });

        QStringList daoArgs = {"write", "-n", "--eject", "--device", "/dev/" + d->blockDevice, "contents.toc"};
        process->start("cdrdao", daoArgs);
    } else {
        d->state = Finished;
        emit stateChanged(Finished);

        d->progress = 100;
        d->maxProgress = 100;
        emit totalProgressChanged(d->maxProgress);
        emit progressChanged(d->progress);

        d->description = tr("Burn Successful");
        emit descriptionChanged(d->description);

        d->workDir.remove();
    }
}

quint64 BurnJob::progress() {
    return d->progress;
}

quint64 BurnJob::totalProgress() {
    return d->maxProgress;
}

BurnJob::State BurnJob::state() {
    return d->state;
}

QWidget* BurnJob::makeProgressWidget() {
    return new BurnJobWidget(this);
}
