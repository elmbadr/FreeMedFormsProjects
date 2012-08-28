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
 *   Main developers : Eric MAEKER, <eric.maeker@gmail.com>                *
 *   Contributors :                                                        *
 *       NAME <MAIL@ADDRESS.COM>                                           *
 *       NAME <MAIL@ADDRESS.COM>                                           *
 ***************************************************************************/

/**
  \class UserPlugin::Internal::UserBase
  This class owns the user database mechanism. It should never be directly accessed. Use tkUserModel to access to the database.

  1. Initialization\n
  This class owns a singleton. To instanciate it, use instance(). When instanciate for the first time,
  the initialize() member is called.

  2. Users retreiver\n
  You can retreive users using getUserByUuid() or using getUserByLoginPassword().

  3. Users saver\n
  You can regardless save or update users to database using the unique member : saveUser().

  4. Users data checkers\n
  You can check the identifiers of users with the checkLogin() member.

  5. Users trace keeper\n
  The recaordLastLogin() update user's database with the actual login's date and time. It also update the login
  history data of the user. This login history need to be saved by hand (saveUser()).
*/

#include "userbase.h"
#include <usermanagerplugin/constants.h>

#include <coreplugin/icore.h>
#include <coreplugin/isettings.h>
#include <coreplugin/constants_tokensandsettings.h>
#include <printerplugin/textdocumentextra.h>
#include <coreplugin/iuser.h>
#include <coreplugin/icommandline.h>

#include <utils/log.h>
#include <utils/global.h>
#include <utils/databaseconnector.h>

#include <translationutils/constants.h>
#include <translationutils/trans_database.h>
#include <translationutils/trans_msgerror.h>

#include <usermanagerplugin/userdata.h>

#include <QCoreApplication>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QFile>
#include <QDir>
#include <QLocale>
#include <QDateTime>

enum { WarnUserPreferences = false };

using namespace UserPlugin::Internal;
using namespace UserPlugin::Constants;
using namespace Trans::ConstantTranslations;

static inline Core::ISettings *settings()  { return Core::ICore::instance()->settings(); }
static inline Core::ICommandLine *commandLine()  { return Core::ICore::instance()->commandLine(); }
static inline QString bundlePath()  { return settings()->path(Core::ISettings::BundleResourcesPath); }

static inline bool connectDatabase(QSqlDatabase &DB, const int line)
{
    if (!DB.isOpen()) {
        if (!DB.open()) {
            Utils::Log::addError("UserBase", tkTr(Trans::Constants::UNABLE_TO_OPEN_DATABASE_1_ERROR_2)
                                 .arg(DB.connectionName()).arg(DB.lastError().text()),
                                 __FILE__, line);
            return false;
        }
    }
    return true;
}

// Initializing static data
UserBase *UserBase::m_Instance = 0;

/**
  Returns the unique instance of UserBase. If the instance does not exist it is created.
  You should never construct a instance of this object using the constructor.
*/
UserBase *UserBase::instance()
{
    Q_ASSERT(m_Instance);
    return m_Instance;
}

/** Constructor, inform Utils::Database of the database scheme. */
UserBase::UserBase(QObject *parent) :
    QObject(parent), Utils::Database(),
    m_initialized(false), m_IsNewlyCreated(false)
{
    m_Instance = this;
    setObjectName("UserBase");

    // populate tables and fields of database
    addTable(Table_USERS,  "USERS");
    addTable(Table_DATAS,  "DATAS");
    addTable(Table_RIGHTS, "RIGHTS");

    addField(Table_USERS, USER_ID,           "USER_ID",         FieldIsUniquePrimaryKey);
    addField(Table_USERS, USER_UUID,         "USER_UUID",       FieldIsUUID);
    addField(Table_USERS, USER_VALIDITY,     "VALIDITY",        FieldIsBoolean);
    addField(Table_USERS, USER_ISVIRTUAL,    "ISVIRTUAL",       FieldIsBoolean);
    addField(Table_USERS, USER_LOGIN,        "LOGIN",           FieldIsShortText);
    addField(Table_USERS, USER_PASSWORD,     "PASSWORD",        FieldIsShortText);
    addField(Table_USERS, USER_LASTLOG,      "LASTLOGIN",       FieldIsDate);
    addField(Table_USERS, USER_NAME,         "NAME",            FieldIsShortText);
    addField(Table_USERS, USER_SECONDNAME,   "SECONDNAME",      FieldIsShortText);
    addField(Table_USERS, USER_FIRSTNAME,    "SURNAME",         FieldIsShortText);
    addField(Table_USERS, USER_TITLE,        "TITLE",           FieldIsInteger);
    addField(Table_USERS, USER_GENDER,       "GENDER",          FieldIsInteger);
    addField(Table_USERS, USER_MAIL,         "MAIL",            FieldIsShortText);
    addField(Table_USERS, USER_LANGUAGE,     "LANGUAGE",        FieldIsLanguageText);
    addField(Table_USERS, USER_LOCKER,       "LOCKER",          FieldIsBoolean);
    addIndex(Table_USERS, USER_UUID);
    addIndex(Table_USERS, USER_LOGIN);
    addIndex(Table_USERS, USER_NAME);
    addIndex(Table_USERS, USER_SECONDNAME);
    addIndex(Table_USERS, USER_FIRSTNAME);

    addField(Table_DATAS, DATAS_ID,          "DATAS_ID",        FieldIsUniquePrimaryKey);
    addField(Table_DATAS, DATAS_USER_UUID,   "USER_UUID",       FieldIsUUID);
    addField(Table_DATAS, DATAS_DATANAME,    "DATANAME",        FieldIsShortText);
    addField(Table_DATAS, DATAS_STRING,      "DATA_STRING",     FieldIsShortText);
    addField(Table_DATAS, DATAS_LONGSTRING,  "DATA_LONGSTRING", FieldIsLongText);
    addField(Table_DATAS, DATAS_FILE,        "DATA_FILE",       FieldIsBlob);
    addField(Table_DATAS, DATAS_NUMERIC,     "DATA_NUMERIC",    FieldIsInteger);
    addField(Table_DATAS, DATAS_DATE,        "DATA_DATE",       FieldIsDate);
    addField(Table_DATAS, DATAS_LANGUAGE,    "DATA_LANGUAGE",   FieldIsShortText);
    addField(Table_DATAS, DATAS_LASTCHANGE,  "LASTCHANGE",      FieldIsDate);
    addField(Table_DATAS, DATAS_TRACE_ID,    "TRACE_ID",        FieldIsInteger);
    addIndex(Table_DATAS, DATAS_USER_UUID);

    addField(Table_RIGHTS, RIGHTS_ID,        "RIGHTS_ID",       FieldIsUniquePrimaryKey);
    addField(Table_RIGHTS, RIGHTS_USER_UUID, "USER_UUID",       FieldIsUUID);
    addField(Table_RIGHTS, RIGHTS_ROLE,      "RIGHTS_ROLE",     FieldIsShortText);
    addField(Table_RIGHTS, RIGHTS_RIGHTS,    "RIGHTS_RIGHTS",   FieldIsInteger);
    addIndex(Table_RIGHTS, RIGHTS_USER_UUID);

    addField(Table_GROUPS, GROUPS_ID,               "GROUP_ID",   FieldIsUniquePrimaryKey);
    addField(Table_GROUPS, GROUPS_UID,              "GROUP_UID",  FieldIsUUID);
    addField(Table_GROUPS, GROUPS_USER_UID,         "USER_UID",   FieldIsUUID);
    addField(Table_GROUPS, GROUPS_PARENT_GROUP_UID, "PARENT_GROUP_UID", FieldIsUUID);
//    addIndex(Table_RIGHTS, RIGHTS_USER_UUID);

    // links
    addTable(Table_USER_LK_ID, "LK_USER");
    addField(Table_USER_LK_ID, LK_ID,         "ID", FieldIsUniquePrimaryKey);
    addField(Table_USER_LK_ID, LK_LKID,       "LK_ID", FieldIsInteger);
    addField(Table_USER_LK_ID, LK_USER_UUID,  "USER_UID", FieldIsUUID);
    addField(Table_USER_LK_ID, LK_GROUP_UUID, "GROUP_UID", FieldIsUUID);
    addIndex(Table_USER_LK_ID, LK_USER_UUID);
    addIndex(Table_USER_LK_ID, LK_GROUP_UUID);
    addIndex(Table_USER_LK_ID, LK_LKID);

    // information
    addTable(Table_INFORMATION, "INFORMATIONS");
    addField(Table_INFORMATION, INFO_VERSION,  "VERSION", FieldIsShortText);
    addField(Table_INFORMATION, INFO_MAX_LKID, "MAX_LK_ID", FieldIsInteger);

    // Connect first run database creation requested
    connect(Core::ICore::instance(), SIGNAL(firstRunDatabaseCreation()), this, SLOT(onCoreFirstRunCreationRequested()));
}

/**
  Initialize users base using the \e settings values.
  \sa Core::ISettings, Core::ISettings::ReadWriteDatabasesPath
*/
bool UserBase::initialize(Core::ISettings *s)
{
    Core::ISettings *set = s;
    if (!set)
        set = settings();

    if (m_initialized)
        return true;

    // connect
    if (commandLine()->value(Core::ICommandLine::ClearUserDatabases).toBool()) {
        createConnection(USER_DB_CONNECTION, USER_DB_CONNECTION,
                         settings()->databaseConnector(),
                         Utils::Database::DeleteAndRecreateDatabase);
    } else {
        createConnection(USER_DB_CONNECTION, USER_DB_CONNECTION,
                         settings()->databaseConnector(),
                         Utils::Database::CreateDatabase);
    }

    if (!database().isOpen()) {
        if (!database().open()) {
            LOG_ERROR(tkTr(Trans::Constants::UNABLE_TO_OPEN_DATABASE_1_ERROR_2).arg(USER_DB_CONNECTION).arg(database().lastError().text()));
        } else {
            LOG(tkTr(Trans::Constants::CONNECTED_TO_DATABASE_1_DRIVER_2).arg(database().connectionName()).arg(database().driverName() + "@" + database().databaseName()));
        }
    } else {
        LOG(tkTr(Trans::Constants::CONNECTED_TO_DATABASE_1_DRIVER_2).arg(database().connectionName()).arg(database().driverName() + "@" + database().databaseName()));
    }

    if (!checkDatabaseScheme()) {
        LOG_ERROR(tkTr(Trans::Constants::DATABASE_1_SCHEMA_ERROR).arg(USER_DB_CONNECTION));
        return false;
    }

    if (!checkDatabaseVersion())
        return false;

//    connect(Core::ICore::instance(), SIGNAL(databaseServerChanged()), this, SLOT(onCoreDatabaseServerChanged()));
    m_initialized = true;
    return true;
}

/** Return true if the userbase is initialized and usable */
bool UserBase::isInitialized() const
{
    return m_initialized;
}

/** Return true if the userbase has just been created */
bool UserBase::isNewlyCreated() const
{
    return m_IsNewlyCreated;
}

/** Return true if the userbase is the last version (database is updated by this member if needed) */
bool UserBase::checkDatabaseVersion()
{
    // TODO: Code : UserBase::checkDatabaseVersion()
    return true;
}

void UserBase::onCoreFirstRunCreationRequested()
{
    disconnect(Core::ICore::instance(), SIGNAL(firstRunDatabaseCreation()), this, SLOT(onCoreFirstRunCreationRequested()));
    initialize();
}

//--------------------------------------------------------------------------------------------------------
//------------------------------------------- Data retreivers --------------------------------------------
//--------------------------------------------------------------------------------------------------------
/** Retreive all users data from the users' database. If an error occurs, it returns 0. */
UserData *UserBase::getUser(const QHash<int, QString> &conditions) const
{
    QSqlDatabase DB = QSqlDatabase::database(Constants::USER_DB_CONNECTION);
    if (!connectDatabase(DB, __LINE__)) {
        return 0;
    }
    DB.transaction();

    // get USERS table
    QString req = select(Table_USERS, conditions);
    UserData *toReturn = 0;
    QString uuid = "";
    QSqlQuery query(DB);
    if (query.exec(req)) {
        if (query.next()) {
            int i = 0;
            uuid = query.value(USER_UUID).toString();
            toReturn = new UserData(uuid);
            for (i = 0; i < USER_MaxParam; ++i)
                toReturn->setValue(Table_USERS, i , query.value(i));
        }
    } else {
        LOG_QUERY_ERROR(query);
    }
    query.finish();

    // get RIGHTS table  ***************************************** -1
    QHash<int, QString> where;
    where.insert(RIGHTS_USER_UUID, QString("='%1'").arg(uuid));
    req = select(Table_RIGHTS, where);
    if (query.exec(req)) {
        while (query.next()) {
            int i = 0;
            QByteArray id = query.value(RIGHTS_ROLE).toByteArray();
            for (i = 0; i < RIGHTS_MaxParam; ++i)
                toReturn->addRightsFromDatabase(id , i , query.value(i));
        }
    } else {
        LOG_QUERY_ERROR(query);
    }
    query.finish();

    // get DATAS table  ***************************************** -1
    where.clear();
    where.insert(DATAS_USER_UUID, QString("='%1'").arg(uuid));
    req = select(Table_DATAS, where);
    QList<UserDynamicData*> list;
    if (query.exec(req)) {
        while (query.next()) {
            int i = 0;
            UserDynamicData *data = new UserDynamicData();
            for (i = 0; i < DATAS_MaxParam; ++i) {
                data->feedFromSql(i, query.value(i));
            }
            list << data;
        }
    } else {
        LOG_QUERY_ERROR(query);
    }
    query.finish();

    if (list.count())
        toReturn->addDynamicDatasFromDatabase(list);

    // get personal LINK_ID
    // TODO: this must be updated: manage groups too
    where.clear();
    where.insert(Constants::LK_USER_UUID, QString("='%1'").arg(uuid));
    req = select(Constants::Table_USER_LK_ID, Constants::LK_LKID, where);
    int lkid = -1;
    if (query.exec(req)) {
        if (query.next()) {
            lkid = query.value(0).toInt();
        }
    } else {
        LOG_QUERY_ERROR(query);
    }
    query.finish();

    if (lkid == -1) {
        // TODO: WARNING this causes segfault
        LOG_ERROR(QString("No linker for user %1").arg(toReturn->uuid()));
        DB.rollback();
        return 0;
    }
    toReturn->setPersonalLkId(lkid);

    if (toReturn)
        toReturn->setModified(false);

    DB.commit();

    return toReturn;
}

/** Retreive all users data from the users' database. If an error occurs, it returns 0. \sa getUser() */
UserData *UserBase::getUserById(const QVariant & _id) const
{
    // retreive corresponding user
    // construct the where clause
    QString req = QString("=%1").arg(_id.toInt()) ;
    QHash<int, QString> where;
    where.insert(USER_ID, req);
    // get the asked user
    return getUser(where);
}

/** Retreive all users data from the users' database. If an error occurs, it returns 0. \sa getUser() */
UserData* UserBase::getUserByUuid(const QString & uuid) const
{
    // retreive corresponding user
    // construct the where clause
    QString req = QString("='%1'").arg(uuid) ;
    QHash<int, QString> where;
    where.insert(USER_UUID, req);
    // get the asked user
    return getUser(where);
}

/** Retreive all users data from the users' database. If an error occurs, it returns 0. \sa getUser() */
UserData *UserBase::getUserByLoginPassword(const QVariant &login, const QVariant &cryptedPassword) const
{
    // retreive corresponding user
    // construct the where clause
    QHash<int, QString> where;
    where.insert(USER_LOGIN, QString("='%1'").arg(login.toString()));
    where.insert(USER_PASSWORD, QString("='%1'").arg(cryptedPassword.toString()));
    // get the asked user
    return getUser(where);
}

/** Check the couple login/password passed as params. Return true if a user can connect with these identifiants (to the server + to freemedforms). */
bool UserBase::checkLogin(const QString &clearLogin, const QString &clearPassword) const
{
//    qWarning() << Q_FUNC_INFO << clearLogin << clearPassword;
    m_LastUuid.clear();
    m_LastLogin.clear();
    m_LastPass.clear();
    // Connect to drivers
    if (QSqlDatabase::connectionNames().contains("__ConnectionTest__")) {
        QSqlDatabase::removeDatabase("__ConnectionTest__");
    }
    switch (driver()) {
    case Utils::Database::MySQL:
        {
            // Try to connect with the new identifiers to MySQL server
            QSqlDatabase connectionTest = database().cloneDatabase(database(), "__ConnectionTest__");
            connectionTest.setUserName(clearLogin);
            connectionTest.setPassword(clearPassword);
            if (!connectionTest.open()) {
                LOG_ERROR(QString("Unable to connect to the MySQL server, with user %1: %2").arg(clearLogin).arg(clearPassword.length()));
                LOG_ERROR(database().lastError().text());
                return false;
            }
            LOG(QString("Database server identifiers are correct for login %1: %2").arg(clearLogin).arg(clearPassword.length()));
            // Reconnect with these identifiers all databases
            break;
        }
    case Utils::Database::SQLite: break;
    case Utils::Database::PostSQL:
        {
            return false;
            break;
        }
    }

    QSqlDatabase DB = QSqlDatabase::database(Constants::USER_DB_CONNECTION);
    if (!connectDatabase(DB, __LINE__)) {
        return 0;
    }
    DB.transaction();

    // create query
    QList<int> list;
    list << USER_UUID << USER_LOGIN << USER_PASSWORD;
    QHash<int, QString> where;
    where.insert(USER_LOGIN, QString("='%1'").arg(Utils::loginForSQL(clearLogin)));
    where.insert(USER_PASSWORD, QString("='%1'").arg(Utils::cryptPassword(clearPassword)));
    QString req = select(Table_USERS, list, where);
    QSqlQuery query(DB);
    if (query.exec(req)) {
        if (query.next()) {
            m_LastUuid = query.value(0).toString();
            m_LastLogin = query.value(1).toString();
            m_LastPass = query.value(2).toString();
        } else {
            LOG_ERROR("No FreeMedForms user. Unable to connect user.");
            DB.rollback();
            return false;
        }
    } else {
        LOG_ERROR("Unable to connect user.");
        LOG_QUERY_ERROR(query);
        LOG_DATABASE(QSqlDatabase::database(USER_DB_CONNECTION));
        DB.rollback();
        return false;
    }

    if (QSqlDatabase::connectionNames().contains("__ConnectionTest__")) {
        QSqlDatabase::removeDatabase("__ConnectionTest__");
    }

    DB.commit();

    return (!m_LastUuid.isEmpty());
}

/** Return true if the \e login is already used on the server or in freemedforms configuration. */
bool UserBase::isLoginAlreadyExists(const QString &clearLogin) const
{
    QSqlDatabase DB = QSqlDatabase::database(Constants::USER_DB_CONNECTION);
    if (!connectDatabase(DB, __LINE__)) {
        return 0;
    }
    // TODO: test is the login is alreday used in mysql / postgre

    // Check is user login is used
    QHash<int, QString> where;
    where.insert(USER_LOGIN, QString("='%1'").arg(Utils::loginForSQL(clearLogin)));
    return count(Table_USERS, USER_LOGIN, getWhereClause(Table_USERS, where));
}

/** Return the Uuid of the user identified with couple login/password. Returns a null QString if an error occurs. */
QString UserBase::getUuid(const QString &log64, const QString &cryptpass64)
{
    if ((log64 == m_LastLogin) && (cryptpass64 == m_LastPass))
        return m_LastUuid;
    m_LastUuid.clear();

    QSqlDatabase DB = QSqlDatabase::database(Constants::USER_DB_CONNECTION);
    if (!connectDatabase(DB, __LINE__)) {
        return QString::null;
    }
    DB.transaction();

    // create query
    QHash<int, QString> where;
    where.insert(USER_LOGIN, QString("='%1'").arg(log64));
    where.insert(USER_PASSWORD, QString("='%1'").arg(cryptpass64));
    QString req = select(Table_USERS, USER_UUID, where);
    QSqlQuery query(DB);
    if (query.exec(req)) {
        if (query.next()) {
            m_LastUuid = query.value(0).toString();
        }
    } else {
        LOG_ERROR(QApplication::translate("UserBase", "Can not create a new user's UUID, database access error"));
        LOG_QUERY_ERROR(query);
    }
    query.finish();
    DB.commit();
    return m_LastUuid;
}

/** Return a new Uuid assuming that it is not already used in base. */
QString UserBase::createNewUuid()
{
    QSqlDatabase DB = QSqlDatabase::database(Constants::USER_DB_CONNECTION);
    if (!connectDatabase(DB, __LINE__)) {
        return QString::null;
    }
    DB.transaction();
    QString tmp;
    QSqlQuery query(DB);
    while (tmp.isEmpty()) {
        tmp = Utils::Database::createUid();
        // create query
        QHash<int, QString> where;
        where.insert(USER_UUID, QString("='%1'").arg(tmp));
        QString req = select(Table_USERS, USER_UUID, where);
        if (query.exec(req)) {
            if (query.next()) {
                tmp.clear();
            }
        } else {
            LOG_ERROR(QApplication::translate("UserBase", "Can not create a new user's UUID, database access error"));
            LOG_QUERY_ERROR(query);
            break;
        }
        query.finish();
    }
    query.finish();
    DB.commit();
    return tmp;
}

/** Return the associated encrypted login for the user identified by the specified \e uid. */
QString UserBase::getLogin64(const QString &uuid)
{
    if (uuid == m_LastUuid)
        return m_LastLogin;
    QSqlDatabase DB = QSqlDatabase::database(Constants::USER_DB_CONNECTION);
    if (!connectDatabase(DB, __LINE__)) {
        return QString::null;
    }
    DB.transaction();
    // create query
    QHash<int, QString> where;
    where.insert(USER_UUID, QString("='%1'").arg(uuid));
    QString req = select(Table_USERS, USER_LOGIN, where);
    QSqlQuery query(DB);
    if (query.exec(req)) {
        if (query.next()) {
            QString var = query.value(0).toString();
            query.finish();
            DB.commit();
            return var;
        }
    } else {
        LOG_ERROR(QApplication::translate("UserBase", "Can not retreive login from the uuid"));
        LOG_QUERY_ERROR(query);
    }
    DB.commit();
    return QString();
}

/** Returns a specific dynamic data of a user. */
QString UserBase::getUserDynamicData(const QString &userUid, const QString &dynDataUuid)
{
    QSqlDatabase DB = QSqlDatabase::database(Constants::USER_DB_CONNECTION);
    if (!connectDatabase(DB, __LINE__)) {
        return QString::null;
    }
    DB.transaction();
    QSqlQuery query(DB);
    QHash<int, QString> where;
    where.insert(DATAS_USER_UUID, QString("='%1'").arg(userUid));
    where.insert(DATAS_DATANAME, QString("='%1'").arg(dynDataUuid));
    QString req = select(Table_DATAS, where);
    if (query.exec(req)) {
        if (query.next()) {
            int i = 0;
            UserDynamicData *data = new UserDynamicData();
            for (i = 0; i < DATAS_MaxParam; ++i) {
                data->feedFromSql(i, query.value(i));
            }
            query.finish();
            DB.commit();
            return data->value().toString();
        }
    } else {
        LOG_QUERY_ERROR(query);
    }
    query.finish();
    DB.commit();
    return QString::null;
}
//--------------------------------------------------------------------------------------------------------
//--------------------------------------------- Data savers ----------------------------------------------
//--------------------------------------------------------------------------------------------------------
static inline QString defaultPaper(const QString &profession, const QString &paper, const QString &paperType = QString::null)
{
    QString lang = QLocale().name().left(2).toLower();
    QString fileName;
    if (paperType.isEmpty()) {
        fileName = QString(bundlePath() + "/profiles/%1/default/user_%2_%3.xml").arg(profession).arg(paper).arg(lang);
    } else {
        fileName = QString(bundlePath() + "/profiles/%1/default/user_%2_%3_%4.xml").arg(profession).arg(paper).arg(paperType).arg(lang);
    }
    if (QFileInfo(fileName).exists()) {
        return Utils::readTextFile(fileName);
    }
    lang = Trans::Constants::ALL_LANGUAGE;
    if (paperType.isEmpty()) {
        fileName = QString(bundlePath() + "/profiles/%1/default/user_%2_%3.xml").arg(profession).arg(paper).arg(lang);
    } else {
        fileName = QString(bundlePath() + "/profiles/%1/default/user_%2_%3_%4.xml").arg(profession).arg(paper).arg(paperType).arg(lang);
    }
    if (QFileInfo(fileName).exists()) {
        return Utils::readTextFile(fileName);
    }
    if (!paperType.isEmpty()) {
        fileName = QString(bundlePath() + "/profiles/%1/default/user_%2_%3.xml").arg(profession).arg(paper).arg(lang);
        if (QFileInfo(fileName).exists()) {
            return Utils::readTextFile(fileName);
        }
    }
    return QString();
}

static inline QString defaultHeader(const QString &profession)
{
    return defaultPaper(profession, "header");
}

static inline QString defaultFooter(const QString &profession)
{
    return defaultPaper(profession, "footer");
}

// paperType == "prescriptions" "administrative" "generic"
static inline QString defaultWatermark(const QString &profession, const QString &paperType = QString::null)
{
    return defaultPaper(profession, "watermark", paperType);
}

/** Create the default users database if it does not exist. */
bool UserBase::createDatabase(const QString &connectionName , const QString &dbName,
                    const QString &pathOrHostName,
                    TypeOfAccess access, AvailableDrivers driver,
                    const QString &login, const QString &pass,
                    const int port,
                    CreationOption createOption
                   )
{
    Q_UNUSED(access);
    Q_UNUSED(createOption);

    if (connectionName != Constants::USER_DB_CONNECTION)
        return false;

    LOG(QCoreApplication::translate("UserBase",
                                    "Trying to create empty user database.\n"
                                    "       Location: %1\n"
                                    "       FileName: %2\n"
                                    "       Driver: %3")
        .arg(pathOrHostName, dbName).arg(driver));

    setConnectionName(connectionName);
    setDriver(driver);

    // create an empty database and connect
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    QSqlDatabase DB;
    if (driver == SQLite) {
        DB = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        if (!QDir(pathOrHostName).exists())
            if (!QDir().mkpath(pathOrHostName))
                LOG(tkTr(Trans::Constants::_1_ISNOT_AVAILABLE_CANNOTBE_CREATED).arg(pathOrHostName));
        DB.setDatabaseName(QDir::cleanPath(pathOrHostName + QDir::separator() + dbName));
        if (!DB.open())
            LOG_ERROR(tkTr(Trans::Constants::DATABASE_1_CANNOT_BE_CREATED_ERROR_2).arg(dbName).arg(DB.lastError().text()));
        setDriver(Utils::Database::SQLite);
    } else if (driver == MySQL) {
        // TODO: test grants here or before ?
        if (QSqlDatabase::connectionNames().contains(connectionName)) {
            DB = QSqlDatabase::database(connectionName);
        } else {
            DB = QSqlDatabase::addDatabase("QMYSQL" , connectionName);
            DB.setHostName(pathOrHostName);
            DB.setUserName(login);
            DB.setPassword(pass);
            DB.setPort(port);
        }
        DB.setDatabaseName("mysql");
        if (!DB.open()) {
            LOG_ERROR(tkTr(Trans::Constants::UNABLE_TO_OPEN_DATABASE_1_ERROR_2).arg(DB.connectionName()).arg(DB.lastError().text()));
            return false;
        }

        qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
        //        qWarning() << "createMySQLDatabase(dbName);";
        createMySQLDatabase(dbName);
        // change database connection
        DB.setDatabaseName(dbName);
        if (!DB.open()) {
            LOG_ERROR(tkTr(Trans::Constants::UNABLE_TO_OPEN_DATABASE_1_ERROR_2).arg(DB.connectionName()).arg(DB.lastError().text()));
            return false;
        }
    }

    // create db structure
    if (createTables()) {
        LOG(tkTr(Trans::Constants::DATABASE_1_CORRECTLY_CREATED).arg(dbName));
    } else {
        LOG_ERROR(tkTr(Trans::Constants::DATABASE_1_CANNOT_BE_CREATED_ERROR_2)
                  .arg(dbName, DB.lastError().text()));
        return false;
    }

    // add general administrator
    createDefaultUser();

    // Table INFORMATION
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    DB.transaction();
    QSqlQuery query(DB);
    query.prepare(prepareInsertQuery(Constants::Table_INFORMATION));
    query.bindValue(Constants::INFO_VERSION, Constants::USER_DB_VERSION);
    query.bindValue(Constants::INFO_MAX_LKID, 1);
    if (!query.exec()) {
        LOG_QUERY_ERROR(query);
        DB.rollback();
    }
    DB.commit();
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);

    // database is readable/writable
    LOG(tkTr(Trans::Constants::DATABASE_1_CORRECTLY_CREATED).arg(pathOrHostName + QDir::separator() + dbName));

    m_IsNewlyCreated = true;
    return true;
}

/** Create a default user when recreating the database. */
bool UserBase::createDefaultUser()
{
    // MySQL user is created during the Core::FirstRunWizard -> Core::ServerConfigPage::validatePage() using an external SQL script
    UserData* user = new UserData;
    user->setLogin64(Utils::loginForSQL(DEFAULT_USER_CLEARLOGIN));
    user->setClearPassword(DEFAULT_USER_CLEARPASSWORD);
    user->setValidity(true);
    user->setVirtual(false);
    user->setName(DEFAULT_USER_NAME);
    user->setFirstname(DEFAULT_USER_FIRSTNAME);
    user->setLocaleLanguage(QLocale().language());
    user->setSpecialty(QStringList() << DEFAULT_USER_SPECIALTY);
    user->setAddress(DEFAULT_USER_ADDRESS);
    user->setRights(Constants::USER_ROLE_USERMANAGER, Core::IUser::ReadOwn | Core::IUser::ReadDelegates | Core::IUser::ReadAll | Core::IUser::WriteAll | Core::IUser::Create | Core::IUser::Delete | Core::IUser::Print);
    user->setRights(Constants::USER_ROLE_MEDICAL, Core::IUser::ReadAll | Core::IUser::WriteAll | Core::IUser::Create | Core::IUser::Delete | Core::IUser::Print);
    user->setRights(Constants::USER_ROLE_ADMINISTRATIVE, Core::IUser::ReadAll | Core::IUser::WriteAll | Core::IUser::Create | Core::IUser::Delete | Core::IUser::Print);
    user->setRights(Constants::USER_ROLE_PARAMEDICAL, Core::IUser::ReadAll | Core::IUser::WriteAll | Core::IUser::Create | Core::IUser::Delete | Core::IUser::Print);
    user->setPersonalLkId(1);

    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    user->setExtraDocument(Print::TextDocumentExtra::fromXml(defaultHeader("admin")), Core::IUser::GenericHeader);
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    user->setExtraDocument(Print::TextDocumentExtra::fromXml(defaultFooter("admin")), Core::IUser::GenericFooter);
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    user->setExtraDocument(Print::TextDocumentExtra::fromXml(defaultWatermark("admin", "generic")), Core::IUser::GenericWatermark);

    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    user->setExtraDocument(Print::TextDocumentExtra::fromXml(defaultHeader("admin")), Core::IUser::AdministrativeHeader);
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    user->setExtraDocument(Print::TextDocumentExtra::fromXml(defaultFooter("admin")), Core::IUser::AdministrativeFooter);
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    user->setExtraDocument(Print::TextDocumentExtra::fromXml(defaultWatermark("admin", "administrative")), Core::IUser::AdministrativeWatermark);

    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    saveUser(user);

    // create the linker
    QSqlDatabase DB = QSqlDatabase::database(Constants::USER_DB_CONNECTION);
    if (!connectDatabase(DB, __LINE__)) {
        return false;
    }
    DB.transaction();
    QSqlQuery query(DB);
    query.prepare(prepareInsertQuery(Constants::Table_USER_LK_ID));
    query.bindValue(Constants::LK_ID, QVariant());
    query.bindValue(Constants::LK_GROUP_UUID, QVariant());
    query.bindValue(Constants::LK_USER_UUID, user->uuid());
    query.bindValue(Constants::LK_LKID, user->personalLinkId());
    if (!query.exec()) {
        LOG_QUERY_ERROR(query);
        delete user;
        query.finish();
        DB.rollback();
        return false;
    }
    DB.commit();
    delete user;
    return true;
}

/** Create a virtual user. */
bool UserBase::createVirtualUser(const QString &uid, const QString &name, const QString &firstName, int title, int gender,
                                 const QStringList &specialties, const QStringList &qualifications,
                                 int medicalRights, int adminRights, int userRights, int agendaRights, int paramedicRights,
                                 QLocale::Language lang)  // static
{
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    QSqlDatabase DB = QSqlDatabase::database(Constants::USER_DB_CONNECTION);
    if (!connectDatabase(DB, __LINE__)) {
        return false;
    }
    QHash<int, QString> where;
    where.insert(USER_UUID, QString("='%1'").arg(uid));
    int uidAlreadyInBase = count(Table_USERS, USER_UUID, getWhereClause(Table_USERS, where));
    if (uidAlreadyInBase!=0) {
        LOG("User uuid is alreday used, virtual user not created");
        return false;
    }
    UserData* user = 0;
    if (uid.isEmpty())
        user = new UserData;
    else
        user = new UserData(uid);
    QString pass;
    if (firstName.isEmpty())
        pass = name.toLower();
    else
        pass = name.toLower() + '.' + firstName.toLower();
    pass = pass.toLower();
    pass = pass.simplified();
    pass = pass.replace(" ", "_");
    pass = Utils::removeAccents(pass);
    user->setLogin64(Utils::loginForSQL(pass));
    user->setClearPassword(pass);
    user->setValidity(true);
    user->setVirtual(true);
    user->setName(name.toUpper());
    user->setFirstname(firstName);
    user->setTitleIndex(title);
    user->setGenderIndex(gender);
    user->setLocaleLanguage(lang);
    user->setSpecialty(specialties);
    user->setQualification(qualifications);
    user->setRights(Constants::USER_ROLE_USERMANAGER, Core::IUser::UserRights(userRights));
    user->setRights(Constants::USER_ROLE_MEDICAL, Core::IUser::UserRights(medicalRights));
    user->setRights(Constants::USER_ROLE_ADMINISTRATIVE, Core::IUser::UserRights(adminRights));
    user->setRights(Constants::USER_ROLE_PARAMEDICAL, Core::IUser::UserRights(paramedicRights));
    user->setRights(Constants::USER_ROLE_AGENDA, Core::IUser::UserRights(agendaRights));
//    user->setPersonalLkId(1);

    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    user->setExtraDocument(Print::TextDocumentExtra::fromXml(defaultHeader("medicals")), Core::IUser::GenericHeader);
    user->setExtraDocument(Print::TextDocumentExtra::fromXml(defaultFooter("medicals")), Core::IUser::GenericFooter);
    user->setExtraDocument(Print::TextDocumentExtra::fromXml(defaultWatermark("medicals", "generic")), Core::IUser::GenericWatermark);

    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    user->setExtraDocument(Print::TextDocumentExtra::fromXml(defaultHeader("medicals")), Core::IUser::AdministrativeHeader);
    user->setExtraDocument(Print::TextDocumentExtra::fromXml(defaultFooter("medicals")), Core::IUser::AdministrativeFooter);
    user->setExtraDocument(Print::TextDocumentExtra::fromXml(defaultWatermark("medicals", "administrative")), Core::IUser::AdministrativeWatermark);

    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    user->setExtraDocument(Print::TextDocumentExtra::fromXml(defaultHeader("medicals")), Core::IUser::PrescriptionHeader);
    user->setExtraDocument(Print::TextDocumentExtra::fromXml(defaultFooter("medicals")), Core::IUser::PrescriptionFooter);
    user->setExtraDocument(Print::TextDocumentExtra::fromXml(defaultWatermark("medicals", "prescriptions")), Core::IUser::PrescriptionWatermark);

    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    saveUser(user);

    // Create the user on the server
    if (driver()==MySQL) {
        // TODO: this can be a serious security problem
        createMySQLUser(pass, pass, Grant_Select|Grant_Update|Grant_Insert|Grant_Delete|Grant_Create|Grant_Index);
    }

    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    // create the linker
    DB.transaction();
    QSqlQuery query(DB);
    query.prepare(prepareInsertQuery(Constants::Table_USER_LK_ID));
    query.bindValue(Constants::LK_ID, QVariant());
    query.bindValue(Constants::LK_GROUP_UUID, QVariant());
    query.bindValue(Constants::LK_USER_UUID, user->uuid());
    query.bindValue(Constants::LK_LKID, user->personalLinkId());
    if (!query.exec()) {
        LOG_QUERY_ERROR(query);
        delete user;
        query.finish();
        DB.rollback();
        return false;
    }
    DB.commit();
    delete user;
    return true;
}

/**
  Record the last login date for the user identified by couple login/password. The date is returned.
  \todo add a locker for users
*/
QDateTime UserBase::recordLastLogin(const QString &log, const QString &pass)
{
    // create the linker
    QSqlDatabase DB = QSqlDatabase::database(Constants::USER_DB_CONNECTION);
    if (!connectDatabase(DB, __LINE__)) {
        return QDateTime();
    }
    DB.transaction();
    // change last login value
    QDateTime now = QDateTime::currentDateTime();
    QHash< int, QString >  where;
    where.insert(USER_LOGIN, QString("='%1'").arg(log));
    where.insert(USER_PASSWORD, QString("='%1'").arg(pass));
    QSqlQuery query(DB);
    query.prepare(prepareUpdateQuery(Table_USERS, USER_LASTLOG, where));
    query.bindValue(0 , now);
    if (!query.exec()) {
        LOG_QUERY_ERROR(query);
        query.finish();
        DB.rollback();
        return QDateTime();
    }
    query.finish();
    DB.commit();
    // TODO add locker
    LOG(tr("Last recorded user login: %1 ").arg(now.toString()));
    return now;
}

/** Creates a new user in the database according to the actual specified driver. */
bool UserBase::createUser(UserData *user)
{
    // TODO: check current user freemedforms' rights
    switch (driver()) {
    case Utils::Database::MySQL:
        {
            // create user grants
            Utils::Database::Grants grants = Grant_Select | Grant_Update | Grant_Insert | Grant_Delete | Grant_Create | Grant_Drop | Grant_Alter | Grant_Index;
            if (user->hasRight(Constants::USER_ROLE_USERMANAGER, Core::IUser::Create)) {
                grants |= Grant_CreateUser;
            }

            // create a MySQL user
            if (!createMySQLUser(user->clearLogin(), user->clearPassword(), grants))
                return false;
            break;
        }
    case Utils::Database::SQLite:
        {
            break;
        }
    case Utils::Database::PostSQL:
        {
            return false;
            break;
        }
    }

    // Create the FreeMedForms user
    return saveUser(user);
}

/**
  Save users data to the database. \n
  You can use this function to save a newly created user or to update an already existing user. This function
  manages both cases.
*/
bool UserBase::saveUser(UserData *user)
{
//    qWarning() << Q_FUNC_INFO;

    if (!user->isModified())
        return true;

    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    QSqlDatabase DB = QSqlDatabase::database(Constants::USER_DB_CONNECTION);
    if (!connectDatabase(DB, __LINE__)) {
        return false;
    }
    DB.transaction();
    QSqlQuery query(DB);

    // if user already exists ==> update   else ==> insert
    bool toUpdate = false;
    QHash<int, QString> where;
    // Try to find existing user uuid
    if (user->id() != -1) {
        where.insert(USER_UUID, QString("='%1'").arg(user->uuid()));
        toUpdate = (count(Table_USERS, USER_UUID, getWhereClause(Table_USERS, where)) == 1);
    }
    // construct query
    bool error = false;
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    if (toUpdate) {
        // update Table_USERS
        // TODO: update identifiers according to the driver.
        query.prepare(prepareUpdateQuery(Table_USERS, where));
        query.bindValue(USER_ID, user->id());
        query.bindValue(USER_UUID, user->uuid());
        query.bindValue(USER_VALIDITY, (int)user->validity());
        query.bindValue(USER_ISVIRTUAL, (int)user->isVirtual());
        query.bindValue(USER_LOGIN, user->login64());
        query.bindValue(USER_PASSWORD, user->cryptedPassword());
        query.bindValue(USER_LASTLOG, user->lastLogin());
        query.bindValue(USER_NAME, user->name());
        query.bindValue(USER_SECONDNAME, user->secondName());
        query.bindValue(USER_FIRSTNAME, user->firstname());
        query.bindValue(USER_TITLE, user->titleIndex());
        query.bindValue(USER_GENDER, user->genderIndex());
        query.bindValue(USER_MAIL, user->mail());
        query.bindValue(USER_LANGUAGE, user->languageIso());
        query.bindValue(USER_LOCKER, user->locker());
        if (!query.exec()) {
            LOG_QUERY_ERROR(query);
            query.finish();
            DB.rollback();
            return false;
        }
        query.finish();

        // update dynamic data
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
        if (user->hasModifiedDynamicDatasToStore()) {
            const QList<UserDynamicData*> &datasToUpdate = user->modifiedDynamicDatas();
            foreach(UserDynamicData *dyn, datasToUpdate) {
                //                qWarning() << "SAVE UDD TO BASE" ;
                //                dyn->warn();
                if (dyn->id() == -1) {
                    // create the dynamic data
                    query.prepare(prepareInsertQuery(Table_DATAS));
                    query.bindValue(DATAS_ID, QVariant()); // auto-id
                } else {
                    // update the dynamic data
                    QHash<int , QString> w;
                    w.insert(DATAS_USER_UUID, QString("='%1'").arg(user->uuid()));
                    w.insert(DATAS_ID, QString ("=%1").arg(dyn->id()));
                    w.insert(DATAS_DATANAME, QString("='%1'").arg(dyn->name()));
                    query.prepare(prepareUpdateQuery(Table_DATAS, w));
                    query.bindValue(DATAS_ID, dyn->id());
                }
                dyn->prepareQuery(query);
                if (!query.exec()) {
                    LOG_QUERY_ERROR(query);
                    query.finish();
                    DB.rollback();
                    return false;
                } else {
                    dyn->setDirty(false);
                }
                if (dyn->id() == -1) {
                    dyn->setId(query.lastInsertId().toInt());
                }
                query.finish();
            }
        }

        // add Table RIGHTS
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
        if (user->hasModifiedRightsToStore()) {
            const QStringList &rolesToUpdate = user->modifiedRoles();
            foreach(const QString &s, rolesToUpdate) {
                QHash<int , QString> w;
                w.insert(RIGHTS_ROLE, QString("='%1'").arg(s));
                if (count(Table_RIGHTS, RIGHTS_ID, getWhereClause(Table_RIGHTS, w)) == 0) {
                    query.prepare(prepareInsertQuery(Table_RIGHTS));
                    query.bindValue(RIGHTS_ID,         QVariant());
                    query.bindValue(RIGHTS_USER_UUID,  user->uuid());
                    query.bindValue(RIGHTS_ROLE ,      s);
                    query.bindValue(RIGHTS_RIGHTS ,    user->rightsValue(s, RIGHTS_RIGHTS));
                    if (!query.exec()) {
                        LOG_QUERY_ERROR(query);
                        query.finish();
                        DB.rollback();
                        return false;
                    }
                    query.finish();
                } else {
                    query.prepare(prepareUpdateQuery(Table_RIGHTS,
                                                 QList<int>()
                                                 << RIGHTS_RIGHTS, where));
                    query.bindValue(0,  user->rightsValue(s, RIGHTS_RIGHTS));
                    if (!query.exec()) {
                        LOG_QUERY_ERROR(query);
                        query.finish();
                        DB.rollback();
                        return false;
                    }
                    query.finish();
                }
            }
        }

        // TODO: code here : --> update UserLkId

        if (!error) {
            LOG(QCoreApplication::translate("UserBase", "User %1 successfully updated.").arg(user->uuid()));
            user->setModified(false);
        }
    } else {
        // INSERT USER
        // add Table USERS
        query.prepare(prepareInsertQuery(Table_USERS));
        query.bindValue(USER_ID,           QVariant());
        query.bindValue(USER_UUID,         user->uuid());
        query.bindValue(USER_VALIDITY ,    (int)user->validity());
        query.bindValue(USER_ISVIRTUAL ,   (int)user->isVirtual());
        query.bindValue(USER_LOGIN ,       user->login64());
        query.bindValue(USER_PASSWORD ,    user->cryptedPassword());
        query.bindValue(USER_NAME ,        user->name());
        query.bindValue(USER_FIRSTNAME ,   user->firstname());
        query.bindValue(USER_SECONDNAME ,  user->secondName());
        query.bindValue(USER_TITLE,        user->titleIndex());
        query.bindValue(USER_GENDER,       user->genderIndex());
        query.bindValue(USER_MAIL ,        user->mail());
        query.bindValue(USER_LASTLOG ,     user->lastLogin());
        query.bindValue(USER_LANGUAGE,     user->languageIso());
        query.bindValue(USER_LOCKER,       user->locker());
        if (!query.exec()) {
            LOG_QUERY_ERROR(query);
            query.finish();
            DB.rollback();
            return false;
        }
        user->setId(query.lastInsertId());
        query.finish();

        // add dynamic data
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
        if (user->hasModifiedDynamicDatasToStore()) {
            const QList<UserDynamicData *> &datasToUpdate = user->modifiedDynamicDatas();
            foreach(UserDynamicData *dyn, datasToUpdate) {
                query.prepare(prepareInsertQuery(Table_DATAS));
                query.bindValue(DATAS_ID, QVariant());
                dyn->prepareQuery(query);
                if (!query.exec()) {
                    LOG_QUERY_ERROR(query);
                    query.finish();
                    DB.rollback();
                    return false;
                } else {
                    dyn->setId(query.lastInsertId().toInt());
                    dyn->setDirty(false);
                }
                query.finish();
            }
        }
        query.finish();

        // add Table RIGHTS
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
        if (user->hasModifiedRightsToStore()) {
            const QStringList & rolesToUpdate = user->modifiedRoles();
            foreach(const QString & s, rolesToUpdate) {
                query.prepare(prepareInsertQuery(Table_RIGHTS));
                query.bindValue(RIGHTS_ID,         QVariant());
                query.bindValue(RIGHTS_USER_UUID,  user->uuid());
                query.bindValue(RIGHTS_ROLE ,      s);
                query.bindValue(RIGHTS_RIGHTS ,    user->rightsValue(s, RIGHTS_RIGHTS));
                if (!query.exec()) {
                    LOG_QUERY_ERROR(query);
                    query.finish();
                    DB.rollback();
                    return false;
                }
                query.finish();
            }
        }

        // create the USER_LK
        qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
        if (user->personalLinkId() == -1) {
            user->setPersonalLkId(getMaxLinkId() + 1);
            query.prepare(prepareInsertQuery(Table_USER_LK_ID));
            query.bindValue(LK_ID,         QVariant());
            query.bindValue(LK_LKID,       user->personalLinkId());
            query.bindValue(LK_USER_UUID,  user->uuid());
            query.bindValue(LK_GROUP_UUID, QVariant());
            if (!query.exec()) {
                LOG_QUERY_ERROR(query);
                query.finish();
                DB.rollback();
                return false;
            }
            query.finish();
        }
    }

    user->setModified(false);
    query.finish();
    DB.commit();
    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
    return true;
}

/**
  Delete an user identified by its \e uuid from the database.
  \todo delete the user's LKID
*/
bool UserBase::deleteUser(const QString &uuid)
{
    QSqlDatabase DB = QSqlDatabase::database(Constants::USER_DB_CONNECTION);
    if (!connectDatabase(DB, __LINE__)) {
        return false;
    }
    switch (driver()) {
    case Utils::Database::MySQL:
        {
            // drop a MySQL user
            QString clearLog = QString(QByteArray::fromBase64(getLogin64(uuid).toAscii()));
            if (!dropMySQLUser(clearLog))
                return false;
            break;
        }
    case Utils::Database::SQLite:
        {
            break;
        }
    case Utils::Database::PostSQL:
        {
            return false;
            break;
        }
    }

    // delete table USERS
    DB.transaction();
    QSqlQuery query(DB);
    QHash<int,QString> where;
    where.insert(Constants::USER_UUID, QString("='%1'").arg(uuid));
    if (!query.exec(prepareDeleteQuery(Table_USERS, where))) {
        LOG_QUERY_ERROR(query);
        query.finish();
        DB.rollback();
        return false;
    }
    query.finish();
    where.clear();
    where.insert(Constants::DATAS_USER_UUID, QString("='%1'").arg(uuid));
    if (!query.exec(prepareDeleteQuery(Table_RIGHTS, where))) {
        LOG_QUERY_ERROR(query);
        query.finish();
        DB.rollback();
        return false;
    }
    query.finish();
    where.clear();
    where.insert(Constants::RIGHTS_USER_UUID, QString("='%1'").arg(uuid));
    if (!query.exec(prepareDeleteQuery(Table_DATAS, where))) {
        LOG_QUERY_ERROR(query);
        query.finish();
        DB.rollback();
        return false;
    }
    query.finish();
    where.clear();
    where.insert(Constants::LK_USER_UUID, QString("='%1'").arg(uuid));
    if (!query.exec(prepareDeleteQuery(Table_USER_LK_ID, where))) {
        LOG_QUERY_ERROR(query);
        query.finish();
        DB.rollback();
        return false;
    }
    query.finish();
    DB.commit();
    return true;
}

/** Save the current preferences of the user. User is identified by its \e uid and the settings are stored into \e content. */
bool UserBase::saveUserPreferences(const QString &uid, const QString &content)
{
    if (WarnUserPreferences) {
        qWarning() << Q_FUNC_INFO << uid;
        qWarning() << content << "\n\n\n";
    }
    if (uid.isEmpty()) {
        if (WarnUserPreferences)
            qWarning() << "    ****** Not saved";
        return false;
    }
    if (content.isEmpty()) {
        if (WarnUserPreferences)
            qWarning() << "    ****** Not saved";
        return false;
    }

    if (saveUserDynamicData(uid, Constants::USER_DATAS_PREFERENCES, content)) {
        if (WarnUserPreferences)
            qWarning() << "    successfully saved";
        return true;
    }

    if (WarnUserPreferences)
        qWarning() << "    ****** Not saved";
    return false;
}

bool UserBase::savePapers(UserData *user)
{
    if (!user->isModified())
        return true;

    if (!user->hasModifiedDynamicDatasToStore())
        return true;

    QSqlDatabase DB = QSqlDatabase::database(Constants::USER_DB_CONNECTION);
    if (!connectDatabase(DB, __LINE__)) {
        return false;
    }

    QStringList papersId;
    papersId.append(USER_DATAS_GENERICHEADER);
    papersId.append(USER_DATAS_GENERICFOOTER);
    papersId.append(USER_DATAS_GENERICWATERMARK);
    papersId.append(USER_DATAS_ADMINISTRATIVEHEADER);
    papersId.append(USER_DATAS_ADMINISTRATIVEFOOTER);
    papersId.append(USER_DATAS_ADMINISTRATIVEWATERMARK);
    papersId.append(USER_DATAS_PRESCRIPTIONHEADER);
    papersId.append(USER_DATAS_PRESCRIPTIONFOOTER);
    papersId.append(USER_DATAS_PRESCRIPTIONWATERMARK);

    const QList<UserDynamicData*> &datasToUpdate = user->modifiedDynamicDatas();
    DB.transaction();
    QSqlQuery query(DB);
    foreach(UserDynamicData *dyn, datasToUpdate) {
        if (!papersId.contains(dyn->name()))
            continue;
        //            qWarning() << "SAVE PAPER TO BASE" << dyn->name();
        //                dyn->warn();
        if (dyn->id() == -1) {
            // create the dynamic data
            query.prepare(prepareInsertQuery(Table_DATAS));
            query.bindValue(DATAS_ID, QVariant()); // auto-id
        } else {
            // update the dynamic data
            QHash<int , QString> w;
            w.insert(DATAS_USER_UUID, QString("='%1'").arg(user->uuid()));
            w.insert(DATAS_ID, QString ("=%1").arg(dyn->id()));
            w.insert(DATAS_DATANAME, QString("='%1'").arg(dyn->name()));
            query.prepare(prepareUpdateQuery(Table_DATAS, w));
            query.bindValue(DATAS_ID, dyn->id());
        }
        dyn->prepareQuery(query);
        if (!query.exec()) {
            LOG_QUERY_ERROR(query);
            query.finish();
            DB.rollback();
            return false;
        } else {
            dyn->setDirty(false);
        }
        if (dyn->id() == -1) {
            dyn->setId(query.lastInsertId().toInt());
        }
        query.finish();
    }
    query.finish();
    DB.commit();
    return true;
}

/** Update the user's password (taking care of the current server settings). */
bool UserBase::changeUserPassword(UserData *user, const QString &newClearPassword)
{
    if (!user)
        return false;
    if (newClearPassword.isEmpty())
        return false;

    QSqlDatabase DB = QSqlDatabase::database(Constants::USER_DB_CONNECTION);
    if (!connectDatabase(DB, __LINE__)) {
        return false;
    }
    DB.transaction();

    // update FreeMedForms password
    QHash<int, QString> where;
    where.insert(USER_UUID, QString("='%1'").arg(user->uuid()));
    QSqlQuery query(DB);
    query.prepare(prepareUpdateQuery(Table_USERS, USER_PASSWORD, where));
    query.bindValue(0, Utils::cryptPassword(newClearPassword));
    if (!query.exec()) {
        LOG_QUERY_ERROR(query);
        query.finish();
        DB.rollback();
        return false;
    }
    query.finish();

    // update server password
    if (driver()==MySQL) {
        if (!changeMySQLUserPassword(user->clearLogin(), newClearPassword)) {
            DB.rollback();
            return false;
        }
    }
    DB.commit();
    return true;
}

/** Save a dynamic data for \e user using \e value. The data will be saved as a file field. */
bool UserBase::saveUserDynamicData(const QString &userUid, const QString &dynDataUuid, const QVariant &value)
{
    QSqlDatabase DB = QSqlDatabase::database(Constants::USER_DB_CONNECTION);
    if (!connectDatabase(DB, __LINE__)) {
        return false;
    }
    DB.transaction();
    QSqlQuery query(DB);

    QHash<int, QString> where;
    where.insert(DATAS_USER_UUID, QString("='%1'").arg(userUid));
    where.insert(DATAS_DATANAME, QString("='%1'").arg(dynDataUuid));
    // save the dynamic data
    if (count(Constants::Table_DATAS, Constants::DATAS_ID, getWhereClause(Constants::Table_DATAS, where))==0) {
        // save
        query.prepare(prepareInsertQuery(Table_DATAS));
        query.bindValue(DATAS_USER_UUID,  userUid);
        query.bindValue(DATAS_DATANAME ,  dynDataUuid);
        query.bindValue(DATAS_STRING ,    QVariant());
        query.bindValue(DATAS_LONGSTRING, QVariant());
        query.bindValue(DATAS_FILE,       value.toString());
        query.bindValue(DATAS_NUMERIC,    QVariant());
        query.bindValue(DATAS_DATE,       QVariant());
        query.bindValue(DATAS_LANGUAGE,   QLocale().name().left(2));
        query.bindValue(DATAS_LASTCHANGE, QDateTime::currentDateTime());
        query.bindValue(DATAS_TRACE_ID,   QVariant());
        if (!query.exec()) {
            LOG_QUERY_ERROR(query);
            query.finish();
            DB.rollback();
            return false;
        }
    } else {
        // update
        query.prepare(prepareUpdateQuery(Table_DATAS, DATAS_FILE, where));
        query.bindValue(0, value.toString());
        if (!query.exec()) {
            LOG_QUERY_ERROR(query);
            query.finish();
            DB.rollback();
            return false;
        }
    }
    query.finish();
    DB.commit();
    return true;
}

int UserBase::getMaxLinkId()
{
    // TODO: change to Utils::Database::max()
    QSqlDatabase DB = QSqlDatabase::database(Constants::USER_DB_CONNECTION);
    if (!connectDatabase(DB, __LINE__)) {
        return false;
    }
    DB.transaction();
    QSqlQuery query(DB);
    if (!query.exec(select(Constants::Table_INFORMATION, Constants::INFO_MAX_LKID))) {
        LOG_QUERY_ERROR(query);
        query.finish();
        DB.rollback();
        return -1;
    } else {
        if (query.next()) {
            int id = query.value(0).toInt();
            query.finish();
            DB.commit();
            return id;
        }
    }
    DB.rollback();
    return -1;
}

bool UserBase::updateMaxLinkId(const int max)
{
    QSqlDatabase DB = QSqlDatabase::database(Constants::USER_DB_CONNECTION);
    if (!connectDatabase(DB, __LINE__)) {
        return false;
    }
    DB.transaction();
    QSqlQuery query(DB);
    query.prepare(prepareUpdateQuery(Constants::Table_INFORMATION, Constants::INFO_MAX_LKID));
    query.bindValue(0, max);
    if (!query.exec()) {
        LOG_QUERY_ERROR(query);
        query.finish();
        DB.rollback();
        return false;
    }
    DB.commit();
    return true;
}

void UserBase::onCoreDatabaseServerChanged()
{
    m_initialized = false;
    if (QSqlDatabase::connectionNames().contains(Constants::USER_DB_CONNECTION)) {
        QSqlDatabase::removeDatabase(Constants::USER_DB_CONNECTION);
    }
    initialize();
}

