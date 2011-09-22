/***************************************************************************
 *  The FreeMedForms project is a set of free, open source medical         *
 *  applications.                                                          *
 *  (C) 2008-2011 by Eric MAEKER, MD (France) <eric.maeker@gmail.com>      *
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
 *   Main Developper : Eric MAEKER, <eric.maeker@gmail.com>                *
 *   Contributors :                                                        *
 *       NAME <MAIL@ADRESS>                                                *
 ***************************************************************************/
#ifndef USERCALENDARVIEWER_H
#define USERCALENDARVIEWER_H

#include <QWidget>
QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/**
 * \file usercalendarviewer.h
 * \author Eric MAEKER <eric.maeker@gmail.com>
 * \version 0.6.0
 * \date 12 Aug 2011
*/

namespace Agenda {
class UserCalendar;

namespace Internal {
class UserCalendarViewerPrivate;
class AgendaMode;

class UserCalendarViewer : public QWidget
{
    Q_OBJECT
    friend class Agenda::Internal::AgendaMode;

public:
    enum AgendaOwner {
        OwnAgendas = 0,
        DelegatedAgendas
    };

    explicit UserCalendarViewer(QWidget *parent = 0);
    ~UserCalendarViewer();

protected:
    void recalculateComboAgendaIndex();

private Q_SLOTS:
    void newEventAtAvailabity(QAction *action);
    void recalculateAvailabilitiesWithDurationIndex(const int index);
    void on_availableAgendasCombo_activated(const int index);
    void userChanged();

protected:
    bool event(QEvent *e);

private:
    UserCalendarViewerPrivate *d;
};

}  // End namespace Internal
}  // End namespace Agenda

#endif // USERCALENDARVIEWER_H
