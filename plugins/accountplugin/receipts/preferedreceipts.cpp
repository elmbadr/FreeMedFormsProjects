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
#include "preferedreceipts.h"
#include "ui_preferedreceipts.h"
#include "choiceDialog.h"
#include "receiptsIO.h"

#include <coreplugin/icore.h>
#include <coreplugin/iuser.h>
#include <coreplugin/ipatient.h>
#include <accountbaseplugin/constants.h>

#include <QMessageBox>

static inline Core::IUser *user() { return Core::ICore::instance()->user(); }
static inline Core::IPatient *patient() { return Core::ICore::instance()->patient(); }

PreferedReceipts::PreferedReceipts(QWidget * parent):QWidget(parent),ui(new Ui::PreferedReceiptsWidget){
    ui->setupUi(this);
    ui->resultLabel->setText("");
    m_typeOfChoice = 0;
    QTimer::singleShot(100,this,SLOT(showChoiceDialog())) ;
    
}

PreferedReceipts::~PreferedReceipts(){}

void PreferedReceipts::insertPreferedValuesIntoAccount(){
    QString currency = trUtf8("euros");
    receiptsEngine receiptsIO;
    QString userUuid = user()->value(Core::IUser::Uuid).toString();
    QString patientUid = patient()->data(Core::IPatient::Uid).toString();
    if (patientUid.isEmpty())
    {
    	  patientUid = "no-patient-uid";
        }
    QString patientName = patient()->data(Core::IPatient::FullName).toString();
    if (patientName.isEmpty())
    {
    	  patientName = "Patient Name";
        }
    QList<double> listOfPercentages;
    listOfPercentages = m_choiceAndPercentagesHash.values();
    QString preferedAct;
    QStringList listOfValues;
    QString listOfValuesStr;
    for (int i = 0; i < listOfPercentages.size(); i += 1)
    {
    	int typeOfChoice = m_choiceAndPercentagesHash.key(listOfPercentages[i]);
    	QHash <int,QVariant> hashOfPrefValues = receiptsIO.getListOfPreferedValues(userUuid,typeOfChoice); 
    	preferedAct = hashOfPrefValues.value(Constants::ACCOUNT_MEDICALPROCEDURE_TEXT).toString();
        double preferedValue = hashOfPrefValues.value(Constants::ACCOUNT_CHEQUEAMOUNT+typeOfChoice-1).toDouble();
        QVariant debtor = m_choiceAndDebtorHash.value(typeOfChoice);

        if (m_percent!=100.00)
        {
    	    hashOfPrefValues.insert(Constants::ACCOUNT_CHEQUEAMOUNT+typeOfChoice-1,preferedValue*listOfPercentages[i]/100.00);
            hashOfPrefValues.insert(Constants::ACCOUNT_INSURANCE_ID,debtor);
            hashOfPrefValues.insert(Constants::ACCOUNT_DUEBY,debtor);
            }
                                   
       if (!receiptsIO.insertIntoAccount(hashOfPrefValues,userUuid))
       {
    	    QMessageBox::warning(0,trUtf8("Warning"),trUtf8("Unable to insert datas into account")
    	                                     +__FILE__+QString::number(__LINE__),QMessageBox::Ok);
           }
       else{
           listOfValues << QString::number(preferedValue*listOfPercentages[i]/100.00);
           }
        }
    listOfValuesStr = listOfValues.join("+") ;
        
    const QString resultText = trUtf8("The value ")
                               +preferedAct
                               +" : "
                               +listOfValuesStr
                               +" "+currency
                               +trUtf8(" has been inserted.");
    const QString resultTextHtml = "<html><body><font size = 4 color = green><bold>"+resultText+"<bold></font>"
                                   "</body></html>";
    ui->resultLabel->setText(resultTextHtml);
}

void PreferedReceipts::showChoiceDialog(){
    choiceDialog * choice = new choiceDialog(this);
    if (choice->exec() == QDialog::Accepted)
    {
    	  m_typeOfChoice = choice->returnChoiceDialog();
    	  //m_percentagesList = choice->listOfPercentValues();
    	  QStandardItemModel * model = choice->getChoicePercentageDebtorSiteDistruleModel();
    	  for (int i = 0; i < model->rowCount(); i += 1)
    	  {
    	  	int TYPE = choice->TYPE_OF_CHOICE;
    	  	int PERCENTAGE = choice->PERCENTAGE;
    	  	int DEBTOR = choice->DEBTOR;
    	  	
    	  	int typeOfChoice = model->data(model->index(0,TYPE),Qt::DisplayRole).toInt();
    	  	double percentage = model->data(model->index(0,PERCENTAGE),Qt::DisplayRole).toDouble();
    	  	QVariant debtor = model->data(model->index(0,DEBTOR),Qt::DisplayRole);
    	        m_choiceAndPercentagesHash.insertMulti(typeOfChoice,percentage);
    	        m_choiceAndDebtorHash.insertMulti(typeOfChoice,debtor);
    	      }
    	  insertPreferedValuesIntoAccount();
    	  delete model;
    	  delete choice;
        }    
}

/*    TYPE_OF_CHOICE = 0,
      PERCENTAGE,
      DEBTOR,
      SITE,
      DISTRULES,*/
