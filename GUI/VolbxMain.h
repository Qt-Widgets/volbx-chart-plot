#ifndef VOLBXMAIN_H
#define VOLBXMAIN_H

#include <QMainWindow>
#include <QDate>
#include <QSet>
#include <QNetworkAccessManager>

namespace Ui {
    class VolbxMain;
}

class FiltersDock;
class DataView;
class ViewDockWidget;
class TableModel;
class Dataset;
class QNetworkReply;
class FilteringProxyModel;
class MainTabWidget;

/**
 * @brief Volbx main window.
 */
class VolbxMain : public QMainWindow
{
    Q_OBJECT
public:
    explicit VolbxMain(QWidget *parent = 0);

    virtual ~VolbxMain();

    void checkForUpdates();

protected:
    virtual void closeEvent(QCloseEvent *event);

private:
    Q_DISABLE_COPY(VolbxMain)

    /**
     * @brief add new tab for given dataset.
     * @param dataset dataset.
     */
    void addMainTabForDataset(Dataset* dataset);

    ///Side dock with filters.
    FiltersDock* filters_;

    void manageActions(bool tabExists);

    bool loadDataset(Dataset* dataset);

    Ui::VolbxMain *ui;

    void setupStatusBar();

    ///Main tab widget/center widget.
    MainTabWidget* tabWidget_;

    ///Network manager used to retrieve current version available.
    QNetworkAccessManager networkManager_;

    void createOptionsMenu();

    void setStandardIcons();

private slots:
    void on_actionExit_triggered();

    void on_actionFilters_triggered();

    void on_actionLogs_triggered();

    void tabWasChanged(int index);

    void on_actionAbout_triggered();

    void closeTab(int tab);

    void on_actionExport_triggered();

    void on_actionSaveDatasetAs_triggered();

    void on_actionImportData_triggered();

    void updateCheckReplyFinished(QNetworkReply* reply);

    void on_actionCheckForNewVersion_triggered();

    void on_actionUpdateAuto_toggled(bool alwaysCheck);

    /**
     * @brief picked qt built in style.
     */
    void qtStylePicked();

    /**
     * @brief picked custom style (found in resoures or application dir).
     */
    void customStylePicked();
};

#endif // VOLBXMAIN_H