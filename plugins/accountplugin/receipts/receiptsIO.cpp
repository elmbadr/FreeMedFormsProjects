/***************************************************************************
 *  The FreeMedForms project is a set of free, open source medical         *
 *  applications.                                                          *
 *  (C) 2008-2011 by Eric MAEKER, MD (France) <eric.maeker@gmail.com>      *
 *  All rights reserved.                                                   *
 *                                                                         *
 *  The FreeAccount plugins are free, open source FreeMedForms' plugins.   *
 *  (C) 2010-2011 by Pierre-Marie Desombre, MD <pm.desombre@medsyn.fr>     *
 *  and Eric Maeker, MD <eric.maeker@gmail.com>                            *
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
 *  Main Developpers : Pierre-Marie DESOMBRE <pm.desombre@medsyn.fr>,      *
 *                     Eric MAEKER, <eric.maeker@gmail.com>                *
 *  Contributors :                                                         *
 *      NAME <MAIL@ADRESS>                                                 *
 ***************************************************************************/
#include "receiptsIO.h"
#include <accountbaseplugin/constants.h>

#include <QSqlDriver>
#include <QMessageBox>
#include <QUuid>
#include <QDate>
#include <QSqlQuery>

#include <QDebug>

using namespace AccountDB;
using namespace Constants;



receiptsEngine::receiptsEngine(){
    m_mpmodel = new AccountModel(this);
    m_db = QSqlDatabase::database(Constants::DB_ACCOUNTANCY);
}

receiptsEngine::~receiptsEngine()
{
}

bool receiptsEngine::insertIntoAccount(const QHash<int,QVariant> &hashValues, const QString &userUuid)
{
    // fetch all the account model
    /*while (m_mpmodel->canFetchMore(QModelIndex())) {
        qDebug() << __FILE__ << QString::number(__LINE__)<< " while ";
        m_mpmodel->QAbstractItemModel::fetchMore(QModelIndex());
    }*/

    QSqlDatabase db = QSqlDatabase::database(Constants::DB_ACCOUNTANCY);
    QSqlDriver *driver = db.driver();
    if (driver->hasFeature(QSqlDriver::QuerySize)) {
        qDebug() << __FILE__ << QString::number(__LINE__) << "driver has feature";
    }
    QString filterUser = QString("%1='%2'").arg("USER_UID",userUuid);
    m_mpmodel->AccountModel::setFilter(filterUser);
    int rowBefore = m_mpmodel->AccountModel::rowCount(QModelIndex());
    qDebug() << __FILE__ << QString::number(__LINE__) << " rowBefore = " << QString::number(rowBefore);
    if (m_mpmodel->insertRows(rowBefore,1,QModelIndex()))
    {
    	  qWarning() << __FILE__ << QString::number(__LINE__) << "Row inserted !" ;
        }
    bool ret = true;
    QVariant data;
    
    for(int i = 1 ; i < ACCOUNT_MaxParam ; i ++){
         data = hashValues.value(i);
         //qDebug() << __FILE__ << QString::number(__LINE__) << " data + i =" << data.toString()+" "+QString::number(i);
         if (!m_mpmodel-> setData(m_mpmodel->index(rowBefore,i), data ,Qt::EditRole))
            {
            	qWarning() << __FILE__ << QString::number(__LINE__) << " model account error = " 
                                                                    << m_mpmodel->lastError().text() ;
                }
        }
        if (!m_mpmodel->submit())
        {
        	  qWarning() << __FILE__ << QString::number(__LINE__) << "submit error = " 
        	                         << m_mpmodel->lastError().text() ;
            }


    if (m_mpmodel->rowCount(QModelIndex()) == rowBefore) {
        QMessageBox::warning(0,trUtf8("Warning ReceiptsEngine : "),trUtf8("Error = ") + m_mpmodel->lastError().text(),
                             QMessageBox::Ok);
        ret = false;
    }
    return ret;
}

QHash<QString,QVariant> receiptsEngine::getNamesAndValuesFromMP(){//obsolete
    QHash<QString,QVariant> hash;
    MedicalProcedureModel model(this);
    int rows = model.rowCount(QModelIndex());
    qDebug() << __FILE__ << QString::number(__LINE__) << " MP row count =" << QString::number(rows) ;
    for (int i = 0; i < rows; i += 1)
    {
    	QString name = model.data(model.index(i,MP_NAME),Qt::DisplayRole).toString();
    	QVariant value = model.data(model.index(i,MP_AMOUNT),Qt::DisplayRole);
    	hash.insert(name,value);
    }
    return hash;
}

bool receiptsEngine::insertInThesaurus(const QString &listOfValuesStr, const QString &userUuid){
    bool ret = true;
    QUuid uuid;
    QString uuidStr = uuid.createUuid();
    ThesaurusModel model(this);
    int rowBefore = model.ThesaurusModel::rowCount(QModelIndex());
    qDebug() << __FILE__ << QString::number(__LINE__) << " rowCount thesaurus =" << QString::number(rowBefore) ;
    if (model.insertRows(rowBefore,1,QModelIndex()))
        {
    	  qWarning() << __FILE__ << QString::number(__LINE__) << "Row inserted !" ;
        }
    qDebug() << __FILE__ << QString::number(__LINE__) << " rowCount thesaurus =" << QString::number(model.ThesaurusModel::rowCount(QModelIndex())) ;
    model.setData(model.index(rowBefore,THESAURUS_UID), uuidStr,Qt::EditRole);
    model.setData(model.index(rowBefore,THESAURUS_USERUID), userUuid,Qt::EditRole);
    model.setData(model.index(rowBefore,THESAURUS_VALUES),listOfValuesStr,Qt::EditRole);
    model.setData(model.index(rowBefore,THESAURUS_PREF),0,Qt::EditRole);
    if (!model.submit())
    {
    	  ret = false;
        }
    return ret;
}

bool receiptsEngine::deleteFromThesaurus(QString & data ){
    bool ret = true;
    ThesaurusModel model(this);
    int row = 0;
    for (int i = 0; i < model.rowCount(); i += 1)
    {
    	QString dataReturned = model.data(model.index(i,THESAURUS_VALUES)).toString();
    	if (dataReturned == data)
    	{
    		  row = i;
    	    }
    }
    if (!model.removeRow(row))
    {
    	  ret = false;
        }
    return ret;
}

bool receiptsEngine::addBoolTrue(QString & data){
    bool ret = true;
    ThesaurusModel model(this);
    int row = 0;
    for (int i = 0; i < model.rowCount(); i += 1)
    {
    	QString dataReturned = model.data(model.index(i,THESAURUS_VALUES)).toString();
    	int b = model.data(model.index(i,THESAURUS_PREF)).toInt();
    	if (dataReturned == data)
    	{
    		  row = i;
    	    }
    	if(b == 1){
    	    if (!model.setData(model.index(i,THESAURUS_PREF),0,Qt::EditRole))
    	    {
    	    	  qWarning() << __FILE__ << QString::number(__LINE__) << "thesaurusModel cannot set bool to false !" ;

    	    	  ret = false;

    	        }
    	}
    }
    if (!model.setData(model.index(row,THESAURUS_PREF),1,Qt::EditRole))
    {
    	  ret = false;
        }
    return ret;
}

double receiptsEngine::getMinDistanceValue(const QString & data){
    double minDistance = 0.00;
    DistanceRulesModel model(this);
    QString filter = QString("%1 = '%2'").arg("TYPE",data);
    model.setFilter(filter);
    qDebug() << __FILE__ << QString::number(__LINE__) << " filter =" << model.filter() ;
    minDistance = model.data(model.index(0,DISTRULES_MIN_KM),Qt::DisplayRole).toDouble();
    return minDistance;
}

QHash<int,QVariant> receiptsEngine::getListOfPreferedValues(QString & userUuid,
                                                           int choice){
    QHash<int,QVariant> hash;
    double cash = 0.00;
    double check = 0.00;
    double visa = 0.00;
    double banking = 0.00;
    double other = 0.00;
    double due = 0.00;
    WorkingPlacesModel modelWP(this);
    modelWP.setFilter("PREFERED = '1'");
    QVariant preferedSiteUid = modelWP.data(modelWP.index(0,SITES_UID),Qt::DisplayRole);
    InsuranceModel modelINS(this);
    modelINS.setFilter("PREFERED = '1'");
    QVariant preferedInsurance = modelINS.data(modelINS.index(0,INSURANCE_NAME),Qt::DisplayRole);
    QVariant preferedInsuranceUid = modelINS.data(modelINS.index(0,INSURANCE_UID),Qt::DisplayRole);
    QVariant dateThisDay = QVariant(QDate::currentDate().toString("yyyy-MM-dd"));
    ThesaurusModel model(this);
    QString filter = QString("%1 = '%2'").arg("PREFERED",QString::number(true));
    model.setFilter(filter);
    model.select();
    QString data = model.data(model.index(0,THESAURUS_VALUES)).toString();
    MedicalProcedureModel MPmodel(this);
    double value = 0.00;
    QString MPfilter ;
    QStringList list;
    if(!data.isEmpty()){
    qDebug() << __FILE__ << QString::number(__LINE__) << " data is not empty " ;
        if (data.contains("+"))
        {
    	    list = data.split("+");
          }
        else{
              list << data;
            }
        QString str;
        foreach(str,list){
            str = str.trimmed();
            QString field = "NAME";
            QHash<QString,double> hashTypeAndValue = getFilteredValueFromMedicalProcedure(str,field); 
            double val = hashTypeAndValue.value(str);
            value += val;
            }
    }
    else
    {
            	  QMessageBox::warning(0,trUtf8("Warning"),trUtf8("You have to insert your prefered "
            	                 	  "value\nin thesaurus\nand choose it as prefered."),QMessageBox::Ok);
            	  value = -1.13;
        }
    QVariant preferedAct = QVariant(data);
    double preferedValue = value;
    switch(choice){
        case CASH :
            cash = preferedValue;
            break;
        case CHECK :
            check = preferedValue;
            break;
        case VISA :
            visa = preferedValue;
            break;
        case BANKING :
            banking = preferedValue;
            break;
        case OTHER :
            other = preferedValue;
            break;
        case DUE :
            due = preferedValue;
            break;
        default :
            break;    
        }
    
         
    QVariant comment = QVariant(trUtf8("prefered act"));
    hash.insert(ACCOUNT_UID,"UID");
    hash.insert(ACCOUNT_USER_UID,userUuid);
    hash.insert(ACCOUNT_PATIENT_UID,"patientUid");
    hash.insert(ACCOUNT_PATIENT_NAME,"patientName");
    hash.insert(ACCOUNT_SITE_ID,preferedSiteUid);//AccountDB::Constants::SITES_UID
    hash.insert(ACCOUNT_INSURANCE_ID,preferedInsuranceUid);
    hash.insert(ACCOUNT_DATE,dateThisDay);
    hash.insert(ACCOUNT_MEDICALPROCEDURE_XML, QVariant());
    hash.insert(ACCOUNT_MEDICALPROCEDURE_TEXT,preferedAct);
    hash.insert(ACCOUNT_COMMENT, comment);
    hash.insert(ACCOUNT_CASHAMOUNT,cash);
    hash.insert(ACCOUNT_CHEQUEAMOUNT,check);
    hash.insert(ACCOUNT_VISAAMOUNT,visa);
    hash.insert(ACCOUNT_INSURANCEAMOUNT,banking);
    hash.insert(ACCOUNT_OTHERAMOUNT,other);
    hash.insert(ACCOUNT_DUEAMOUNT,due);
    hash.insert(ACCOUNT_DUEBY,preferedInsurance);
    hash.insert(ACCOUNT_ISVALID,0);
    hash.insert(ACCOUNT_TRACE, QVariant());
    return hash;
}

QHash<QString,double> receiptsEngine::getFilteredValueFromMedicalProcedure(const QString & act, 
                                                                              const QString & field){
    QHash<QString,double> hash;
    const QString baseName = trUtf8("medical_procedure");
    const QString name = act;
    const QString amount = trUtf8("AMOUNT");
    const QString type = field;
    QString filter = QString("WHERE %1 = '%2'").arg(type,act);
    QString req = QString("SELECT %1 FROM %2 ").arg(amount,baseName )+filter;
    QSqlQuery q(m_db);
    if (!q.exec(req))
    {
    	 qWarning() << __FILE__ << QString::number(__LINE__) 
    	                        << "Error __FILE__"+QString::number(__LINE__)+q.lastError().text() ; 
        }
    while (q.next())
    {
    	double amount = q.value(0).toDouble();
    	//qDebug() << __FILE__ << QString::number(__LINE__) << " n and a	= " << n << a;
        hash.insertMulti(act,amount);
        } 
    if (hash.size()>1)
    {
    	  QMessageBox::warning(0,trUtf8("Warning"),trUtf8("More than one value")+__FILE__+QString::number(__LINE__),
    	  	                                     QMessageBox::Ok);
        }   
    return hash;
}

QString receiptsEngine::getStringFromInsuranceUid(const QVariant & insuranceUid){
    QString debtor;
    const QString baseName = trUtf8("insurance");    
    const QString insuranceUidField = trUtf8("INSURANCE_UID");
    const QString name = trUtf8("NAME");
    QString filter = QString("WHERE %1 = '%2'").arg(insuranceUidField,insuranceUid.toString());
    QString req = QString("SELECT %1 FROM %2 ").arg(name,baseName )+filter;
    QSqlQuery q(m_db);
    if (!q.exec(req))
    {
    	 qWarning() << __FILE__ << QString::number(__LINE__) 
    	                        << "Error __FILE__"+QString::number(__LINE__)+q.lastError().text() ; 
        }
    while (q.next())
    { 
        debtor = q.value(0).toString();
        }
    return debtor;
}

QVariant receiptsEngine::getSiteUidFromSite(const QString & site){
    QVariant uid = QVariant();
    const QString baseName = trUtf8("sites");    
    const QString uidField = trUtf8("SITE_UID");
    const QString name = trUtf8("NAME");
    QString filter = QString("WHERE %1 = '%2'").arg(name,site);
    QString req = QString("SELECT %1 FROM %2 ").arg(uidField,baseName )+filter;
    QSqlQuery q(m_db);
    if (!q.exec(req))
    {
    	 qWarning() << __FILE__ << QString::number(__LINE__) 
    	                        << "Error __FILE__"+QString::number(__LINE__)+q.lastError().text() ; 
        }
    while (q.next())
    { 
        uid = q.value(0);
        }    
    return uid;
}

QVariant receiptsEngine::getInsuranceUidFromInsurance(const QString & insurance){
    QVariant uid = QVariant();
    const QString baseName = trUtf8("insurance");    
    const QString uidField = trUtf8("INSURANCE_UID");
    const QString name = trUtf8("NAME");
    QString filter = QString("WHERE %1 = '%2'").arg(name,insurance);
    QString req = QString("SELECT %1 FROM %2 ").arg(uidField,baseName )+filter;
    QSqlQuery q(m_db);
    if (!q.exec(req))
    {
    	 qWarning() << __FILE__ << QString::number(__LINE__) 
    	                        << "Error __FILE__"+QString::number(__LINE__)+q.lastError().text() ; 
        }
    while (q.next())
    { 
        uid = q.value(0);
        } 
    qDebug() << __FILE__ << QString::number(__LINE__) << " uid insuranceUid =" << uid.toString() ;   
    return uid;
}
