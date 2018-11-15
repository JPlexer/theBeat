#include "tagcache.h"

#include <QSharedPointer>
#include <taglib/id3v2frame.h>
#include <taglib/id3v2header.h>
#include <taglib/id3v2tag.h>
#include <taglib/mpegfile.h>
#include <taglib/attachedpictureframe.h>

QList<TagLib::FileRef*> TagCache::fileRefs;
QMap<QString, TagLib::Tag*> TagCache::tags;
QMap<QString, TagLib::AudioProperties*> TagCache::audioProperties;

TagCache::TagCache(QObject *parent) : QObject(parent)
{

}

TagLib::Tag* TagCache::getTag(QString filename) {
    if (tags.contains(filename)) {
        return tags.value(filename);
    } else {
        return cache(filename)->tag();
    }
}

TagLib::AudioProperties* TagCache::getAudioProperties(QString filename) {
    if (audioProperties.contains(filename)) {
        return audioProperties.value(filename);
    } else {
        return cache(filename)->audioProperties();
    }
}

TagLib::FileRef* TagCache::cache(QString filename) {
    //FileRef doesn't get deleted so tag doesn't get deleted
    TagLib::FileRef* tag = new TagLib::FileRef(filename.toUtf8());
    tags.insert(filename, tag->tag());
    audioProperties.insert(filename, tag->audioProperties());
    fileRefs.append(tag);
    return tag;
}

tPromise<QImage>* TagCache::getAlbumArt(QString filename) {
    return new tPromise<QImage>([=](QString& error) {
        TagLib::MPEG::File mpegFile(filename.toUtf8());
        if (!mpegFile.hasID3v2Tag()) {
            error = "No ID3v2 Tag";
            return QImage();
        }

        TagLib::ID3v2::Tag* id3v2Tag = mpegFile.ID3v2Tag();
        if (id3v2Tag == nullptr) {
            error = "Invalid ID3v2 Tag";
            return QImage();
        }

        TagLib::ID3v2::FrameList frameList = id3v2Tag->frameListMap()["APIC"];
        if (frameList.isEmpty()) {
            error = "No Album Art available";
            return QImage();
        }

        //QSharedPointer<TagLib::ID3v2::AttachedPictureFrame> frame;
        TagLib::ID3v2::AttachedPictureFrame* frame;
        for (TagLib::ID3v2::FrameList::ConstIterator i = frameList.begin(); i != frameList.end(); i++) {
            //frame = QSharedPointer<TagLib::ID3v2::AttachedPictureFrame>((TagLib::ID3v2::AttachedPictureFrame*) *i);
            frame = (TagLib::ID3v2::AttachedPictureFrame*) *i;
            if (frame->type() == TagLib::ID3v2::AttachedPictureFrame::FrontCover) {
                QByteArray ba;
                ba.append(frame->picture().data(), frame->picture().size());
                QImage image;
                image.loadFromData(ba);
                return image;
            }
        }

        error = "Front Cover art unavailable";
        return QImage();
    });
}
