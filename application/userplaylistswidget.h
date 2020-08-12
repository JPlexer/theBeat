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
#ifndef USERPLAYLISTSWIDGET_H
#define USERPLAYLISTSWIDGET_H

#include <QWidget>
#include <QListWidgetItem>

namespace Ui {
    class UserPlaylistsWidget;
}

struct UserPlaylistsWidgetPrivate;
class UserPlaylistsWidget : public QWidget {
        Q_OBJECT

    public:
        explicit UserPlaylistsWidget(QWidget* parent = nullptr);
        ~UserPlaylistsWidget();

        void setTopPadding(int padding);

    private slots:
        void on_createButton_clicked();

        void on_playlistsList_itemActivated(QListWidgetItem* item);

        void on_backButton_clicked();

        void on_enqueueAllButton_clicked();

        void on_burnButton_clicked();

    private:
        Ui::UserPlaylistsWidget* ui;
        UserPlaylistsWidgetPrivate* d;

        void updatePlaylists();
        void loadPlaylist(int id);
        void updateBurn();
};

#endif // USERPLAYLISTSWIDGET_H
