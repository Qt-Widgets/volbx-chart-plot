#include <QDebug>
#include <QMessageBox>
#include <QVariant>
#include <QDesktopServices>
#include <QUrl>
#include <QLabel>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QProcess>
#include <QStyleFactory>
#include <QDir>

#include "Shared/Logger.h"
#include "ModelsAndViews/FilteringProxyModel.h"
#include "Common/Configuration.h"
#include "Common/ExportData.h"
#include "DataSets/Dataset.h"
#include "DataSets/DatasetSpreadsheet.h"
#include "DataSets/DatasetInner.h"
#include "DataSets/DatasetDefinitionInner.h"
#include "DataSets/DatasetDefinitionXlsx.h"
#include "DataSets/DatasetDefinitionOds.h"
#include "Common/ProgressBar.h"
#include "Common/Constants.h"
#include "ModelsAndViews/TableModel.h"
#include "Shared/Debugging.h"
#include "Shared/Networking.h"
#include "ModelsAndViews/DataView.h"
#include "Shared/Application.h"
#include "Filters/FiltersDock.h"

#include "SaveDatasetAs.h"
#include "MainTabWidget.h"
#include "Export.h"
#include "Import/ImportData.h"
#include "VolbxMain.h"
#include "ui_VolbxMain.h"
#include "ViewDockWidget.h"
#include "About.h"
#include "CheckUpdatesDialog.h"
#include "MainTab.h"

VolbxMain::VolbxMain(QWidget *parent) :
    QMainWindow(parent),
    filters_(NULL),
    ui(new Ui::VolbxMain),
    tabWidget_(NULL)
{
    ui->setupUi(this);

    setStandardIcons();

    setupStatusBar();

    filters_ = new FiltersDock(this);

    tabWidget_ = new MainTabWidget(this);

    connect(filters_,
            SIGNAL(newNamesFiltering(int, QSet<QString>)),
            tabWidget_,
            SLOT(setTextFilterInProxy(int, QSet<QString>)));

    connect(filters_,
            SIGNAL(newDateFiltering(int,QDate,QDate,bool)),
            tabWidget_,
            SLOT(setDateFilterInProxy(int,QDate,QDate,bool)));

    connect(filters_,
            SIGNAL(newNumbersFiltering(int,double,double)),
            tabWidget_,
            SLOT(setNumericFilterInProxy(int,double,double)));

    connect(ui->actionBasic_plot,
            SIGNAL(triggered()),
            tabWidget_,
            SLOT(addBasicPlot()));

    connect(ui->actionHistogram,
            SIGNAL(triggered()),
            tabWidget_,
            SLOT(addHistogramPlot()));

    connect(ui->actionGroup_plot,
            SIGNAL(triggered()),
            tabWidget_,
            SLOT(addGroupingPlot()));

    ui->verticalLayout->addWidget(tabWidget_);

    addDockWidget(Qt::LeftDockWidgetArea, filters_);
    filters_->titleBarWidget()->resize(200, filters_->titleBarWidget()->height());

    connect(tabWidget_, SIGNAL(currentChanged(int)), this, SLOT(tabWasChanged(int)));

    connect(tabWidget_, SIGNAL(tabCloseRequested(int)), this, SLOT(closeTab(int)));

#ifndef DEBUGGING
    ui->actionLogs->setVisible(false);
#endif

    ui->actionBugReport->setVisible(false);
    ui->actionSendErrorOrIdea->setVisible(false);
    ui->actionTutorials->setVisible(false);

    connect(&networkManager_,
            SIGNAL(finished(QNetworkReply*)),
            this,
            SLOT(updateCheckReplyFinished(QNetworkReply*)));

    createOptionsMenu();
}

VolbxMain::~VolbxMain()
{
    delete ui;
    delete filters_;
}

void VolbxMain::setStandardIcons()
{
    QStyle* style = QApplication::style();
    ui->actionImportData->setIcon(style->standardIcon(QStyle::SP_DialogOpenButton));
    ui->actionSaveDatasetAs->setIcon(style->standardIcon(QStyle::SP_DialogSaveButton));
    ui->actionExit->setIcon(style->standardIcon(QStyle::SP_DialogCloseButton));
    ui->actionLogs->setIcon(style->standardIcon(QStyle::SP_FileDialogContentsView));
    ui->actionCheckForNewVersion->setIcon(style->standardIcon(QStyle::SP_BrowserReload));
    ui->actionAbout->setIcon(style->standardIcon(QStyle::QStyle::SP_FileDialogInfoView));
}

void VolbxMain::createOptionsMenu()
{
    ui->menuOptions->addSection(tr("Updates"));
    ui->menuOptions->addAction(ui->actionCheckForNewVersion);
    ui->menuOptions->addAction(ui->actionUpdateAuto);

    ui->menuOptions->addSection(tr("Styles"));

    QString activeStyl = Configuration::getInstance().getStyle();

    QActionGroup* actionsGroup = new QActionGroup(this);

    //Add orange style.
    QString styleName("Dark Orange");
    QAction* action = new QAction(styleName, ui->menuOptions);
    action->setCheckable(true);
    if( activeStyl == styleName )
    {
        action->setChecked(true);
    }
    connect(action, SIGNAL(triggered()), this, SLOT(customStylePicked()));
    actionsGroup->addAction(action);

    //Add blue style.
    styleName  = "Rounded Blue";
    action = new QAction(styleName, ui->menuOptions);
    action->setCheckable(true);
    if( activeStyl == styleName )
    {
        action->setChecked(true);
    }
    connect(action, SIGNAL(triggered()), this, SLOT(customStylePicked()));
    actionsGroup->addAction(action);

    //Add styles found in app dir.
    QStringList nameFilter("*.css");
    QDir directory(QCoreApplication::applicationDirPath());
    QFileInfoList styleFiles = directory.entryInfoList(nameFilter);

    foreach( QFileInfo styleFile, styleFiles )
    {
        QAction* action = new QAction(styleFile.baseName(), ui->menuOptions);
        action->setCheckable(true);
        if( activeStyl == styleFile.baseName() )
        {
            action->setChecked(true);
        }
        connect(action, SIGNAL(triggered()), this, SLOT(customStylePicked()));
        actionsGroup->addAction(action);
    }

    //Add qt available styles.
    QStringList qtStylesList = QStyleFactory::keys();
    foreach (QString style, qtStylesList)
    {
        QAction* action = new QAction(style, ui->menuOptions);
        action->setCheckable(true);
        if( activeStyl == style )
        {
            action->setChecked(true);
        }
        connect(action, SIGNAL(triggered()), this, SLOT(qtStylePicked()));
        actionsGroup->addAction(action);
    }

    ui->menuOptions->addActions(actionsGroup->actions());
}

void VolbxMain::checkForUpdates()
{
    bool checkForUpdates = false;

    if( Configuration::getInstance().needToShowUpdatePickerDialog() )
    {
        CheckUpdatesDialog dialog(this);

        int reply = dialog.exec();

        if( QDialog::Accepted == reply )
        {
            checkForUpdates = true;
        }

        //Remember if choice was checked.
        if( true == dialog.saveFlagSet() )
        {
            Configuration::getInstance().setUpdatesCheckingOption(checkForUpdates);
        }
    }
    else
    {
        checkForUpdates = Configuration::getInstance().needToCheckForUpdates();
    }

    if( true == checkForUpdates )
    {
        on_actionCheckForNewVersion_triggered();
    }

    ui->actionUpdateAuto->setChecked(Configuration::getInstance().needToCheckForUpdates());
}

void VolbxMain::on_actionExit_triggered()
{
    close();
}

void VolbxMain::on_actionFilters_triggered()
{
    if(filters_->isVisible())
    {
        filters_->hide();
    }
    else
    {
        filters_->show();
    }
}

void VolbxMain::on_actionLogs_triggered()
{
#ifdef DEBUGGING
    Logger::getInstance()->switchVisibility();
#endif
}

void VolbxMain::tabWasChanged(int index)
{
    if ( -1 != index )
    {
        const FilteringProxyModel* model = tabWidget_->getCurrentProxyModel();
        if ( NULL == model )
        {
            return;
        }
        filters_->activateFiltersForModel(model);
        manageActions(true);
    }
    else
    {
        manageActions(false);
    }
}

void VolbxMain::closeEvent(QCloseEvent *event)
{
    Configuration::getInstance().save();

    QMainWindow::closeEvent(event);

    //If logger window is shown closing mainWindow do not close app.
    QApplication::closeAllWindows();
}

void VolbxMain::on_actionAbout_triggered()
{
    About about(this);

    about.exec();
}

void VolbxMain::closeTab(int tab)
{
    QWidget* tabToDelete = tabWidget_->widget(tab);
    const FilteringProxyModel* model = tabWidget_->getCurrentProxyModel();
    filters_->removeModel(model);
    tabWidget_->removeTab(tab);
    delete tabToDelete;

    manageActions(0 != tabWidget_->count());
}

void VolbxMain::on_actionExport_triggered()
{
    Export exportDialog(dynamic_cast<QMainWindow*>(tabWidget_->currentWidget()),
                        this);

    exportDialog.exec();
}

void VolbxMain::manageActions(bool tabExists)
{
    ui->actionExport->setEnabled(tabExists);
    ui->actionSaveDatasetAs->setEnabled(tabExists);

	//Check if plot icons should be enabled.
    bool activateCharts = tabExists;
    if( true == tabExists )
    {
        const TableModel* dataModel = tabWidget_->getCurrentDataModel();
        if( NULL != dataModel )
        {
            activateCharts = (true == activateCharts &&
                              true == dataModel->isSpecialColumnsSet());
        }
    }

	//Enable/disable plot icons.
    ui->actionBasic_plot->setEnabled(activateCharts);
    ui->actionHistogram->setEnabled(activateCharts);
    ui->actionGroup_plot->setEnabled(activateCharts);

	//Set tooltips for plot icons.
    if ( false == activateCharts )
    {
        const static QString cannotCreateCharts =
            tr("Time and examined variable columns are not specified.");
        ui->actionBasic_plot->setToolTip(cannotCreateCharts);
        ui->actionHistogram->setToolTip(cannotCreateCharts);
        ui->actionGroup_plot->setToolTip(cannotCreateCharts);
    }
    else
    {
        ui->actionBasic_plot->setToolTip(ui->actionBasic_plot->text());
        ui->actionHistogram->setToolTip(ui->actionHistogram->text());
        ui->actionGroup_plot->setToolTip(ui->actionGroup_plot->text());
    }
}

void VolbxMain::on_actionSaveDatasetAs_triggered()
{
    if ( false == DatasetInner::datasetDirExistAndUserHavePermisions() )
    {
        QString msg(tr("Can not access folder "));
        msg.append(DatasetInner::getDatasetsDir());
        msg.append(tr(" needed for saving dataset."));
        QMessageBox::critical(this,
                              QString(tr("Access denied")),
                              msg);

        return;
    }

    SaveDatasetAs save(DatasetInner::getListOfAvailableDatasets());

    if(QDialog::Accepted == save.exec())
    {
        QTime performanceTimer;
        performanceTimer.start();

        DataView* view = tabWidget_->getCurrentDataView();
        if( NULL == view )
        {
            return;
        }

        ExportData::saveDataset(save.getChosenDatasetName(), view);

        LOG(LOG_IMPORT_EXPORT, "File saved in total time " +
                               QString::number(performanceTimer.elapsed()*1.0/1000) +
                               " seconds.");
    }
}

bool VolbxMain::loadDataset(Dataset* dataset)
{
    //TODO 26/08/2012 Currently try catches part of no memory problems.
    //Problem is in creating 2d array.
    //Try to create vector of pointers to 1d arrays.
    try
    {
        dataset->init();
    }
    catch (std::bad_alloc&)
    {
        QString message(tr("Not enough memory to open data. "));
        message.append(tr("Close not needed data"));
        message.append(tr(" pick smaller set or use another instance of application."));
        QMessageBox::critical(this, tr("Memory problem"), message);

        return false;
    }

    if ( NULL != dataset && false == dataset->isValid() )
    {
        return false;
    }

    return true;
}

void VolbxMain::on_actionImportData_triggered()
{
    ImportData import(this);

    if ( QDialog::Accepted == import.exec() )
    {
        DatasetDefinition* datasetDefinition = import.getSelectedDataset();

        if ( NULL == datasetDefinition ||
             (NULL != datasetDefinition && false == datasetDefinition->isValid()) )
        {
            QMessageBox::critical(this,
                                  tr("Import error"),
                                  tr("Import error encountered, can not continue operation."));
            return;
        }

        //Sample data is not needed anymore.
        datasetDefinition->clearSampleData();

        Dataset* dataset = NULL;

        switch( import.getImportDataType() )
        {
            case ImportData::IMPORT_TYPE_INNER:
            {
                DatasetDefinitionInner* innerDataset =
                        dynamic_cast<DatasetDefinitionInner*>(datasetDefinition);
                if ( NULL != innerDataset )
                {
                    dataset = new DatasetInner(innerDataset);
                }

                break;
            }

            case ImportData::IMPORT_TYPE_SPREADSHEET:
            {
                DatasetDefinitionSpreadsheet* definitionSpreadsheet =
                        dynamic_cast<DatasetDefinitionSpreadsheet*>(datasetDefinition);
                if ( NULL != definitionSpreadsheet )
                {
                    dataset = new DatasetSpreadsheet(definitionSpreadsheet);
                }

                break;
            }

            default:
            {
                Q_ASSERT(false);
                break;
            }
        }

        if( NULL == dataset )
        {
            QMessageBox::critical(this,
                                  tr("Import error"),
                                  datasetDefinition->getError());
            return;
        }

        if ( false == loadDataset(dataset) )
        {
            delete dataset;
            return;
        }

        addMainTabForDataset(dataset);
    }
}


void VolbxMain::addMainTabForDataset(Dataset* dataset)
{
    MainTab* mainTab = new MainTab(dataset, tabWidget_);
    const FilteringProxyModel* proxyModel = mainTab->getCurrentProxyModel();
    if( NULL != proxyModel )
    {
        filters_->addModel(proxyModel);
    }

    int newTabIndex = tabWidget_->addTab(mainTab, dataset->getNameForTabBar());
    tabWidget_->setCurrentIndex(newTabIndex);

    manageActions(true);

    ui->statusBar->showMessage(dataset->getName() + " " + tr("loaded"));
}

void VolbxMain::setupStatusBar()
{
    ui->statusBar->showMessage(tr("Ready") + "...");
}

void VolbxMain::updateCheckReplyFinished(QNetworkReply* reply)
{
    reply->deleteLater();

    //Check errors.
    if( true == Networking::errorsOccuredCheck(reply) )
    {
        ui->statusBar->showMessage(tr("Connection error encountered."));
        return;
    }

    QStringList notNeededHereList;
    QString newestVersion =
        Networking::checkReplyAndReturnAvailableVersion(reply, notNeededHereList);

    if( true == newestVersion.isEmpty() )
    {
        ui->statusBar->showMessage(tr("Wrong answer received from server."));
        return;
    }

    if( newestVersion != QApplication::applicationVersion() )
    {
         QMessageBox::StandardButton answer =
                QMessageBox::question(NULL,
                                      tr("New version"),
                                      tr("New version is available. Download and install it now?"));

        //When user want to update.
        if( QMessageBox::Yes == answer )
        {
            if( QFile::exists(QCoreApplication::applicationDirPath() +
                              '/' +
                              Constants::updaterName_ +
                              Constants::exeFileSuffix_) )
            {
                QProcess::startDetached(QString(Constants::updaterName_));
                QCoreApplication::quit();
            }
            else
            {
                QMessageBox::critical(NULL,
                                      tr("Wrong installation"),
                                      tr("Installation is corrupted. Could not find file ") +
                                      Constants::updaterName_ + Constants::exeFileSuffix_ + ".\n" +
                                      tr("Can not use update functionality."));
            }
        }
    }
    else
    {
        ui->statusBar->showMessage(tr("No updates available"));
    }
}

void VolbxMain::on_actionCheckForNewVersion_triggered()
{
    networkManager_.get(Networking::getCurrentVersionRequest());
}

void VolbxMain::on_actionUpdateAuto_toggled(bool alwaysCheck)
{
    Configuration::getInstance().setUpdatesCheckingOption(alwaysCheck);
}

void VolbxMain::qtStylePicked()
{
    QAction* action = dynamic_cast<QAction*>(sender());
    if( NULL != action)
    {
        QString style = action->text();
        Application::setQtStyle(style);
        Configuration::getInstance().setStyle(style);
    }
}

void VolbxMain::customStylePicked()
{
    QAction* action = dynamic_cast<QAction*>(sender());
    if( NULL != action)
    {
        QString styleName = action->text();
        Application::setCssStyle(styleName);
        Configuration::getInstance().setStyle(styleName);
    }
}