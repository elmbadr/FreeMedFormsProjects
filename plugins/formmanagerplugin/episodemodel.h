/***************************************************************************
 *  The FreeMedForms project is a set of free, open source medical         *
 *  applications.                                                          *
 *  (C) 2008-2012 by Eric MAEKER, MD (France) <eric.maeker@gmail.com>      *
 *  All rights reserved.                                                   *
 *                                                                         *
 *  This program is free software: you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation, either version 3 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  This program is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with this program (COPYING.FREEMEDFORMS file).                   *
 *  If not, see <http://www.gnu.org/licenses/>.                            *
 ***************************************************************************/
/***************************************************************************
 *  Main developer: Eric MAEKER, <eric.maeker@gmail.com>                   *
 *  Contributors:                                                          *
 *       NAME <MAIL@ADDRESS.COM>                                           *
 *       NAME <MAIL@ADDRESS.COM>                                           *
 ***************************************************************************/
#ifndef FORM_NEW_EPISODEMODEL_H
#define FORM_NEW_EPISODEMODEL_H

#include <formmanagerplugin/formmanager_exporter.h>

#include <QAbstractListModel>
QT_BEGIN_NAMESPACE
class QSqlRecord;
QT_END_NAMESPACE

/**
 * \file episodemodel.h
 * \author Eric MAEKER
 * \version 0.8.0
 * \date 01 Oct 2012
*/

// friend class only
namespace Patients {
namespace Internal {
class IdentityViewerWidget;
}
}

namespace Form {
class FormMain;
class EpisodeManager;

namespace Internal {
class EpisodeModelPrivate;
}  // namespace Internal

class FORM_EXPORT EpisodeModel : public QAbstractListModel
{
    Q_OBJECT
    friend class Form::EpisodeManager;
    friend class Patients::Internal::IdentityViewerWidget;

protected:
    EpisodeModel(Form::FormMain *rootEmptyForm, QObject *parent = 0);
    bool initialize();

public:
    enum DataRepresentation {
        ValidationStateIcon = 0,
        PriorityIcon,
        UserTimeStamp,
        Label,
        IsValid,
        CreationDate,
        Priority,
        UserCreatorName,
        XmlContent,
        Icon,
        Uuid,       // Read-Only
        FormUuid,   // Read-Only
        FormLabel,  // Read-Only - decoration includes the form icon
        EmptyColumn1,
        EmptyColumn2,
        MaxData
    };

    enum Priority {
        High = 0,
        Medium,
        Low
    };

    virtual ~EpisodeModel();
    QString formUid() const;
    void setCurrentPatient(const QString &uuid);
    void setUseFormContentCache(bool useCache);

    Qt::ItemFlags flags(const QModelIndex &index) const;
    void fetchMore(const QModelIndex &parent);
    bool canFetchMore(const QModelIndex &parent) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);

    QVariant headerData(int section, Qt::Orientation orientation,
                                int role = Qt::DisplayRole) const;

    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex());
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex());
    bool removeAllEpisodes();

    void setReadOnly(bool state);
    bool isReadOnly() const;
    bool isDirty() const;

    bool validateEpisode(const QModelIndex &index);
    bool isEpisodeValidated(const QModelIndex &index) const;

    bool removeEpisode(const QModelIndex &index);
    QModelIndex renewEpisode(const QModelIndex &episodeToRenew);

public Q_SLOTS:
    bool populateFormWithEpisodeContent(const QModelIndex &episode, bool feedPatientModel);
    bool submit();

Q_SIGNALS:
    void episodeAboutToChange(const QModelIndex &index);
    void episodeAboutToBeDeleted(const QModelIndex &index);
    void episodeAboutToBeCreated(const QModelIndex &index);

    void episodeChanged(const QModelIndex &index);
    void episodeDeleted(const QModelIndex &index);
    void episodeCreated(const QModelIndex &index);

private Q_SLOTS:
    void onUserChanged();
    void onPatientFormLoaded();
    void populateNewRowWithDefault(int row, QSqlRecord &record);

public Q_SLOTS:
    QString lastEpisodesSynthesis() const;

    void onCoreDatabaseServerChanged();

private:
    Internal::EpisodeModelPrivate *d;
};

}  // End namespace Form


#endif // End FORM_NEW_EPISODEMODEL_H
