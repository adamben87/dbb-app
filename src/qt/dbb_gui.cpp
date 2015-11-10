// Copyright (c) 2015 Jonas Schnelli
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "dbb_gui.h"

#include <QAction>
#include <QApplication>
#include <QPushButton>
#include <QDebug>
#include <QInputDialog>
#include <QMessageBox>
#include <QSpacerItem>
#include <QToolBar>
#include <QFontDatabase>
#include <QGraphicsOpacityEffect>


#include "ui/ui_overview.h"
#include <dbb.h>

#include "dbb_util.h"

#include <cstdio>
#include <ctime>

#include <univalue.h>
#include <btc/bip32.h>
#include <btc/tx.h>


//TODO: move to util:
std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }
    return elems;
}


std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> elems;
    split(s, delim, elems);
    return elems;
}
/////////////////

void executeCommand(const std::string& cmd, const std::string& password, std::function<void(const std::string&, dbb_cmd_execution_status_t status)> cmdFinished);

bool DBBDaemonGui::QTexecuteCommandWrapper(const std::string& cmd, const dbb_process_infolayer_style_t layerstyle, std::function<void(const std::string&, dbb_cmd_execution_status_t status)> cmdFinished) {

    if (processComnand)
        return false;

    if (layerstyle == DBB_PROCESS_INFOLAYER_STYLE_TOUCHBUTTON)
    {
            ui->touchbuttonInfo->setVisible(true);
    }

    setLoading(true);
    processComnand = true;
    executeCommand(cmd, sessionPassword, cmdFinished);

    return true;
}


DBBDaemonGui::DBBDaemonGui(QWidget* parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    overviewAction(0),
    walletsAction(0),
    settingsAction(0),
    statusBarButton(0),
    statusBarLabelRight(0),
    statusBarLabelLeft(0),
    backupDialog(0),
    processComnand(0),
    deviceConnected(0),
    cachedWalletAvailableState(0),
    currentPaymentProposalWidget(0),
    loginScreenIndicatorOpacityAnimation(0),
    statusBarloadingIndicatorOpacityAnimation(0)
{
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    
    ui->setupUi(this);

    /////////// UI Styling
#if defined(Q_OS_MAC)
    std::string balanceFontSize = "24pt";
    std::string menuFontSize = "18pt";
    std::string stdFontSize = "16pt";
    std::string smallFontSize = "12pt";
#elif defined(Q_OS_WIN)
    std::string balanceFontSize = "24pt";
    std::string menuFontSize = "18pt";
    std::string stdFontSize = "16pt";
    std::string smallFontSize = "12pt";
#else
    std::string balanceFontSize = "20pt";
    std::string menuFontSize = "14pt";
    std::string stdFontSize = "12pt";
    std::string smallFontSize = "10pt";
#endif

    QFontDatabase::addApplicationFont(":/fonts/AlegreyaSans-Regular");
    QFontDatabase::addApplicationFont(":/fonts/AlegreyaSans-Bold");

    qApp->setStyleSheet("QWidget { font-family: Alegreya Sans; font-size:" + QString::fromStdString(stdFontSize) + "; }");
    this->setStyleSheet("DBBDaemonGui { background-image: url(:/theme/windowbackground);;  } QToolBar { background-color: white }");
    QString buttonCss("QPushButton::hover { } QPushButton:pressed { background-color: #444444; border:0; color: white; } QPushButton { font-family: Alegreya Sans; font-weight: bold; font-size:"+QString::fromStdString(menuFontSize)+"; background-color: black; border:0; color: white; };");
    QString msButtonCss("QPushButton::hover { } QPushButton:pressed { background-color: #444444; border:0; color: white; } QPushButton { font-family: Alegreya Sans; font-weight: bold; font-size:"+QString::fromStdString(menuFontSize)+"; background-color: #003366; border:0; color: white; };");

    QString labelCSS("QLabel { font-size: "+QString::fromStdString(smallFontSize)+"; }");

    this->ui->receiveButton->setStyleSheet(buttonCss);
    this->ui->overviewButton->setStyleSheet(buttonCss);
    this->ui->sendButton->setStyleSheet(buttonCss);
    this->ui->mainSettingsButton->setStyleSheet(buttonCss);
    this->ui->multisigButton->setStyleSheet(msButtonCss);

    this->ui->deviceNameKeyLabel->setStyleSheet(labelCSS);
    this->ui->deviceNameLabel->setStyleSheet(labelCSS);
    this->ui->versionKeyLabel->setStyleSheet(labelCSS);
    this->ui->versionLabel->setStyleSheet(labelCSS);


    this->ui->balanceLabel->setStyleSheet("font-size: "+QString::fromStdString(balanceFontSize)+";");
    this->ui->multisigBalance->setStyleSheet("font-size: "+QString::fromStdString(balanceFontSize)+";");

    this->ui->multisigBalanceKey->setStyleSheet(labelCSS);
    this->ui->multisigWalletNameKey->setStyleSheet(labelCSS);
    ////////////// END STYLING


    this->ui->dbbIcon->setVisible(false);

    ui->touchbuttonInfo->setVisible(false);
    // set light transparent background for touch button info layer
    this->ui->touchbuttonInfo->setStyleSheet("background-color: rgba(255, 255, 255, 240);");

    // allow serval signaling data types
    qRegisterMetaType<UniValue>("UniValue");
    qRegisterMetaType<std::string>("std::string");
    qRegisterMetaType<dbb_cmd_execution_status_t>("dbb_cmd_execution_status_t");
    qRegisterMetaType<dbb_response_type_t>("dbb_response_type_t");
    qRegisterMetaType<std::vector<std::string>>("std::vector<std::string>");

    // connect UI
    connect(ui->eraseButton, SIGNAL(clicked()), this, SLOT(eraseClicked()));
    connect(ui->ledButton, SIGNAL(clicked()), this, SLOT(ledClicked()));
    connect(ui->passwordButton, SIGNAL(clicked()), this, SLOT(setPasswordClicked()));
    connect(ui->seedButton, SIGNAL(clicked()), this, SLOT(seed()));
    connect(ui->createWallet, SIGNAL(clicked()), this, SLOT(seed()));
    connect(ui->createSingleWallet, SIGNAL(clicked()), this, SLOT(createSingleWallet()));
    connect(ui->getNewAddress, SIGNAL(clicked()), this, SLOT(getNewAddress()));
    connect(ui->joinCopayWallet, SIGNAL(clicked()), this, SLOT(JoinCopayWallet()));
    connect(ui->checkProposals, SIGNAL(clicked()), this, SLOT(MultisigUpdateWallets()));
    connect(ui->showBackups, SIGNAL(clicked()), this, SLOT(showBackupDialog()));
    connect(ui->getRand, SIGNAL(clicked()), this, SLOT(getRandomNumber()));
    connect(ui->lockDevice, SIGNAL(clicked()), this, SLOT(lockDevice()));

    // connect custom signals
    connect(this, SIGNAL(showCommandResult(const QString&)), this, SLOT(setResultText(const QString&)));
    connect(this, SIGNAL(XPubForCopayWalletIsAvailable(int)), this, SLOT(GetRequestXPubKey(int)));
    connect(this, SIGNAL(RequestXPubKeyForCopayWalletIsAvailable(int)), this, SLOT(JoinCopayWalletWithXPubKey(int)));
    connect(this, SIGNAL(gotResponse(const UniValue&, dbb_cmd_execution_status_t, dbb_response_type_t, int)), this, SLOT(parseResponse(const UniValue&, dbb_cmd_execution_status_t, dbb_response_type_t, int)));
    connect(this, SIGNAL(shouldVerifySigning(const UniValue&, int, const QString&)), this, SLOT(showEchoVerification(const UniValue&, int, const QString&)));
    connect(this, SIGNAL(signedProposalAvailable(const UniValue&, const std::vector<std::string> &)), this, SLOT(postSignedPaymentProposal(const UniValue&, const std::vector<std::string> &)));
    connect(this, SIGNAL(multisigWalletResponseAvailable(bool, const std::string&)), this, SLOT(MultisigParseWalletsResponse(bool, const std::string&)));

    // create backup dialog instance
    backupDialog = new BackupDialog(0);
    connect(backupDialog, SIGNAL(addBackup()), this, SLOT(addBackup()));
    connect(backupDialog, SIGNAL(eraseAllBackups()), this, SLOT(eraseAllBackups()));
    connect(backupDialog, SIGNAL(restoreFromBackup(const QString&)), this, SLOT(restoreBackup(const QString&)));


    //set window icon
    QApplication::setWindowIcon(QIcon(":/icons/dbb"));
    setWindowTitle(tr("The Digital Bitbox"));

    //set status bar
    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    spacer->setMinimumWidth(3);
    spacer->setMaximumHeight(10);
    statusBar()->addWidget(spacer);
    statusBar()->setStyleSheet("background: transparent;");
    this->statusBarButton = new QPushButton(QIcon(":/icons/connected"), "");
    this->statusBarButton->setEnabled(false);
    this->statusBarButton->setFlat(true);
    this->statusBarButton->setMaximumWidth(18);
    this->statusBarButton->setMaximumHeight(18);
    this->statusBarButton->setVisible(false);
    statusBar()->addWidget(this->statusBarButton);

    this->statusBarLabelLeft = new QLabel(tr("No Device Found"));
    statusBar()->addWidget(this->statusBarLabelLeft);

    this->statusBarLabelRight = new QLabel("");
    statusBar()->addPermanentWidget(this->statusBarLabelRight);
    if (!statusBarloadingIndicatorOpacityAnimation)
    {
        QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(this);
        this->statusBarLabelRight->setGraphicsEffect(eff);

        statusBarloadingIndicatorOpacityAnimation = new QPropertyAnimation(eff, "opacity");

        statusBarloadingIndicatorOpacityAnimation->setDuration(500);
        statusBarloadingIndicatorOpacityAnimation->setKeyValueAt(0, 0.3);
        statusBarloadingIndicatorOpacityAnimation->setKeyValueAt(0.5, 1.0);
        statusBarloadingIndicatorOpacityAnimation->setKeyValueAt(1, 0.3);
        statusBarloadingIndicatorOpacityAnimation->setEasingCurve(QEasingCurve::OutQuad);
        statusBarloadingIndicatorOpacityAnimation->setLoopCount(-1);
    }
    

    // tabbar
    QActionGroup *tabGroup = new QActionGroup(this);
    overviewAction = new QAction(QIcon(":/icons/home").pixmap(32), tr("&Overview"), this);
        overviewAction->setToolTip(tr("Show general overview of wallet"));
        overviewAction->setCheckable(true);
        overviewAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_1));
    tabGroup->addAction(overviewAction);
    
    walletsAction = new QAction(QIcon(":/icons/copay"), tr("&Wallets"), this);
        walletsAction->setToolTip(tr("Show Copay wallet screen"));
        walletsAction->setCheckable(true);
        walletsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_2));
    tabGroup->addAction(walletsAction);
    
    settingsAction = new QAction(QIcon(":/icons/settings"), tr("&Experts"), this);
        settingsAction->setToolTip(tr("Show Settings wallet screen"));
        settingsAction->setCheckable(true);
        settingsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_3));
    tabGroup->addAction(settingsAction);
     
//    QToolBar *toolbar = addToolBar(tr("Tabs toolbar"));
//            toolbar->setMovable(false);
//            toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
//            toolbar->addAction(overviewAction);
//            toolbar->addAction(walletsAction);
//            toolbar->addAction(settingsAction);
//            overviewAction->setChecked(true);
//    toolbar->setStyleSheet("QToolButton{margin:0px; padding: 3px; font-size:11pt;}");
//    toolbar->setIconSize(QSize(24,24));

    connect(this->ui->overviewButton, SIGNAL(clicked()), this, SLOT(mainOverviewButtonClicked()));
    connect(this->ui->multisigButton, SIGNAL(clicked()), this, SLOT(mainMultisigButtonClicked()));
    connect(this->ui->receiveButton, SIGNAL(clicked()), this, SLOT(mainReceiveButtonClicked()));
    connect(this->ui->sendButton, SIGNAL(clicked()), this, SLOT(mainSendButtonClicked()));
    connect(this->ui->mainSettingsButton, SIGNAL(clicked()), this, SLOT(mainSettingsButtonClicked()));

    connect(overviewAction, SIGNAL(triggered()), this, SLOT(gotoOverviewPage()));
    connect(walletsAction, SIGNAL(triggered()), this, SLOT(gotoMultisigPage()));
    connect(settingsAction, SIGNAL(triggered()), this, SLOT(gotoSettingsPage()));

    //login screen setup
    this->ui->blockerView->setVisible(false);
    connect(this->ui->passwordLineEdit, SIGNAL(returnPressed()),this,SLOT(passwordProvided()));

    //load local pubkeys

    DBBMultisigWallet singleCopayWallet;
    singleCopayWallet.client.setFilenameBase("copay_single");
    singleCopayWallet.baseKeyPath = "m/200'";
    singleCopayWallet.client.LoadLocalData();
    this->ui->currentAddress->setText(QString::fromStdString(singleCopayWallet.client.GetLastKnownAddress()));
    vMultisigWallets.push_back(singleCopayWallet);

    DBBMultisigWallet copayWallet;
    copayWallet.client.setFilenameBase("copay_multisig");
    copayWallet.client.LoadLocalData();
    vMultisigWallets.push_back(copayWallet);


    deviceConnected = false;
    resetInfos();
    //set status bar connection status
    checkDevice();
    changeConnectedState(DBB::isConnectionOpen());


    processComnand = false;

    //connect the device status update at very last point in init
    connect(this, SIGNAL(deviceStateHasChanged(bool)), this, SLOT(changeConnectedState(bool)));

    //update multisig wallet data
    MultisigUpdateWallets();
}

void DBBDaemonGui::setActiveArrow(int pos) {
    this->ui->activeArrow->move(pos*96+40,39);
}

void DBBDaemonGui::mainOverviewButtonClicked() {
    setActiveArrow(0);
    gotoOverviewPage();
}

void DBBDaemonGui::mainMultisigButtonClicked() {
    setActiveArrow(4);
    gotoMultisigPage();
}

void DBBDaemonGui::mainReceiveButtonClicked() {
    setActiveArrow(1);
    gotoReceivePage();
}

void DBBDaemonGui::mainSendButtonClicked() {
    setActiveArrow(2);
    gotoSendCoinsPage();
}

void DBBDaemonGui::mainSettingsButtonClicked() {
    setActiveArrow(3);
    gotoSettingsPage();
}

DBBDaemonGui::~DBBDaemonGui()
{

}

/*
 /////////////////////////////
 Plug / Unplug / GetInfo stack
 /////////////////////////////
*/
void DBBDaemonGui::changeConnectedState(bool state)
{
    bool stateChanged = deviceConnected != state;
    if (stateChanged)
    {
        if (state) {
            deviceConnected = true;
            this->statusBarLabelLeft->setText("Device Connected");
            this->statusBarButton->setVisible(true);
        } else {
            deviceConnected = false;
            this->statusBarLabelLeft->setText("No Device Found");
            this->statusBarButton->setVisible(false);
        }

        checkDevice();
    }
}

void DBBDaemonGui::setTabbarEnabled(bool status)
{
    this->ui->menuBlocker->setVisible(!status);
    this->ui->overviewButton->setEnabled(status);
    this->ui->receiveButton->setEnabled(status);
    this->ui->sendButton->setEnabled(status);
    this->ui->mainSettingsButton->setEnabled(status);
    this->ui->multisigButton->setEnabled(status);
}

void DBBDaemonGui::checkDevice()
{
    this->ui->verticalLayoutWidget->setVisible(deviceConnected);
    this->ui->balanceLabel->setVisible(deviceConnected);
    this->ui->noDeviceWidget->setVisible(!deviceConnected);

    if (!deviceConnected)
    {
        walletsAction->setEnabled(false);
        settingsAction->setEnabled(false);
        gotoOverviewPage();
        setActiveArrow(0);
        overviewAction->setChecked(true);
        resetInfos();
        sessionPassword.clear();
        hideSessionPasswordView();
        setTabbarEnabled(false);
    }
    else
    {
        walletsAction->setEnabled(true);
        settingsAction->setEnabled(true);
        askForSessionPassword();
    }
}

void DBBDaemonGui::setLoading(bool status)
{
    if (!status)
        ui->touchbuttonInfo->setVisible(false);

    this->statusBarLabelRight->setText((status) ? "processing..." : "");

    if (statusBarloadingIndicatorOpacityAnimation)
    {
        if (status)
            statusBarloadingIndicatorOpacityAnimation->start(QAbstractAnimation::KeepWhenStopped);
        else
            statusBarloadingIndicatorOpacityAnimation->stop();
    }

    this->ui->unlockingInfo->setText((status) ? "Unlocking Device..." : "");
    //TODO, subclass label and make it animated
}

void DBBDaemonGui::setNetLoading(bool status)
{
    this->statusBarLabelRight->setText((status) ? "loading..." : "");

    if (statusBarloadingIndicatorOpacityAnimation)
    {
        if (status)
            statusBarloadingIndicatorOpacityAnimation->start(QAbstractAnimation::KeepWhenStopped);
        else
            statusBarloadingIndicatorOpacityAnimation->stop();
    }
}

void DBBDaemonGui::resetInfos()
{
    this->ui->versionLabel->setText("loading info...");
    this->ui->deviceNameLabel->setText("loading info...");

    updateOverviewFlags(false,false,true);
}


/*
 /////////////////
 UI Action Stack
 /////////////////
*/

void DBBDaemonGui::createSingleWallet()
{
    if (!vMultisigWallets[0].client.IsSeeded())
    {
        GetXPubKey(0);
    }
    else
    {
        std::string walletsResponse;
        vMultisigWallets[0].client.GetWallets(walletsResponse);
        int i = 1;
    }
}

void DBBDaemonGui::getNewAddress()
{
    if (vMultisigWallets[0].client.IsSeeded())
    {
        std::string address;
        vMultisigWallets[0].client.GetNewAddress(address);
        this->ui->currentAddress->setText(QString::fromStdString(address));
    }
    else
    {
    }
}

void DBBDaemonGui::gotoOverviewPage()
{
    this->ui->stackedWidget->setCurrentIndex(0);
}

void DBBDaemonGui::gotoSendCoinsPage()
{
    this->ui->stackedWidget->setCurrentIndex(3);
}

void DBBDaemonGui::gotoReceivePage()
{
    this->ui->stackedWidget->setCurrentIndex(4);
}

void DBBDaemonGui::gotoMultisigPage()
{
    this->ui->stackedWidget->setCurrentIndex(1);
}

void DBBDaemonGui::gotoSettingsPage()
{
    this->ui->stackedWidget->setCurrentIndex(2);
}

void DBBDaemonGui::showEchoVerification(const UniValue &proposalData, int actionType, QString echoStr)
{
    QMessageBox::information(this, tr("Verify Transaction"),
                             tr("Use your Smartphone to verify the transaction integriry"),
                             QMessageBox::Ok);

    PaymentProposalAction(proposalData, actionType);
}

void DBBDaemonGui::passwordProvided()
{
    if (!loginScreenIndicatorOpacityAnimation)
    {
        QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(this);
        this->ui->unlockingInfo->setGraphicsEffect(eff);

        loginScreenIndicatorOpacityAnimation = new QPropertyAnimation(eff, "opacity");

        loginScreenIndicatorOpacityAnimation->setDuration(500);
        loginScreenIndicatorOpacityAnimation->setKeyValueAt(0, 0.3);
        loginScreenIndicatorOpacityAnimation->setKeyValueAt(0.5, 1.0);
        loginScreenIndicatorOpacityAnimation->setKeyValueAt(1, 0.3);
        loginScreenIndicatorOpacityAnimation->setEasingCurve(QEasingCurve::OutQuad);
        loginScreenIndicatorOpacityAnimation->setLoopCount(-1);
    }

    // to slide in call
    loginScreenIndicatorOpacityAnimation->start(QAbstractAnimation::KeepWhenStopped);

    sessionPassword = this->ui->passwordLineEdit->text().toStdString();
    getInfo();
}

void DBBDaemonGui::passwordAccepted()
{
    hideSessionPasswordView();
    this->ui->passwordLineEdit->setText("");
    setTabbarEnabled(true);
}


void DBBDaemonGui::askForSessionPassword()
{
    setLoading(false);
    this->ui->blockerView->setVisible(true);
    QWidget* slide = this->ui->blockerView;
    // setup slide
    slide->setGeometry(-slide->width(), 0, slide->width(), slide->height());

    // then a animation:
    QPropertyAnimation *animation = new QPropertyAnimation(slide, "pos");
    animation->setDuration(300);
    animation->setStartValue(slide->pos());
    animation->setEndValue(QPoint(0,0));
    animation->setEasingCurve(QEasingCurve::OutQuad);
    // to slide in call
    animation->start();
}

void DBBDaemonGui::hideSessionPasswordView()
{
    if (loginScreenIndicatorOpacityAnimation)
        loginScreenIndicatorOpacityAnimation->stop();

    QWidget* slide = this->ui->blockerView;
    // setup slide
    //slide->setGeometry(-slide->width(), 0, slide->width(), slide->height());

    // then a animation:
    QPropertyAnimation *animation = new QPropertyAnimation(slide, "pos");
    animation->setDuration(300);
    animation->setStartValue(slide->pos());
    animation->setEndValue(QPoint(-slide->width(),0));
    animation->setEasingCurve(QEasingCurve::OutQuad);
    // to slide in call
    animation->start();
}

//TODO: remove, not direct json result text in UI, add log
void DBBDaemonGui::setResultText(const QString& result)
{
    processComnand = false;
    qDebug() << "SetResultText Called\n";
    this->statusBarLabelRight->setText("");
}

void DBBDaemonGui::updateOverviewFlags(bool walletAvailable, bool lockAvailable, bool loading)
{
    this->ui->walletCheckmark->setIcon(QIcon(walletAvailable ? ":/icons/okay" : ":/icons/warning"));
    this->ui->walletLabel->setText(tr(walletAvailable ? "Wallet available" : "No Wallet"));
    this->ui->createWallet->setVisible(!walletAvailable);

    this->ui->lockCheckmark->setIcon(QIcon(lockAvailable ? ":/icons/okay" : ":/icons/warning"));
    this->ui->lockLabel->setText(lockAvailable ? "Device 2FA Lock" : "No 2FA set");

    if (loading)
    {
        this->ui->lockLabel->setText("loading info...");
        this->ui->walletLabel->setText("loading info...");

        this->ui->walletCheckmark->setIcon(QIcon(":/icons/warning")); //TODO change to loading...
        this->ui->lockCheckmark->setIcon(QIcon(":/icons/warning")); //TODO change to loading...
    }
}

/*
 //////////////////////////
 DBB USB Commands (General)
 //////////////////////////
*/

//TODO: remove sendCommand method
bool DBBDaemonGui::sendCommand(const std::string& cmd, const std::string& password, dbb_response_type_t tag)
{
    //ensure we don't fill the queue
    //at the moment the UI should only post one command into the queue

    if (processComnand) {
        qDebug() << "Already processing a command\n";
        return false;
    }
    processComnand = true;
    QTexecuteCommandWrapper(cmd, DBB_PROCESS_INFOLAYER_STYLE_NO_INFO, [this, tag](const std::string& cmdOut, dbb_cmd_execution_status_t status) {
        //send a signal to the main thread
        UniValue jsonOut;
        jsonOut.read(cmdOut);
        emit gotResponse(jsonOut, status, tag);
    });
    return true;
}

void DBBDaemonGui::eraseClicked()
{
    if (QTexecuteCommandWrapper("{\"reset\":\"__ERASE__\"}", DBB_PROCESS_INFOLAYER_STYLE_TOUCHBUTTON, [this](const std::string& cmdOut, dbb_cmd_execution_status_t status) {
            UniValue jsonOut;
            jsonOut.read(cmdOut);
            emit gotResponse(jsonOut, status, DBB_RESPONSE_TYPE_ERASE);
        }))
    {
        std::unique_lock<std::mutex> lock(this->cs_vMultisigWallets);

        vMultisigWallets[0].client.RemoveLocalData();
        vMultisigWallets[1].client.RemoveLocalData();

        sessionPasswordDuringChangeProcess = sessionPassword;
        sessionPassword.clear();
    }
}

void DBBDaemonGui::ledClicked()
{
    QTexecuteCommandWrapper("{\"led\" : \"toggle\"}", DBB_PROCESS_INFOLAYER_STYLE_NO_INFO, [this](const std::string& cmdOut, dbb_cmd_execution_status_t status) {
        UniValue jsonOut;
        jsonOut.read(cmdOut);

        emit gotResponse(jsonOut, status, DBB_RESPONSE_TYPE_LED_BLINK);
    });
}

void DBBDaemonGui::getInfo()
{
    QTexecuteCommandWrapper("{\"device\":\"info\"}", DBB_PROCESS_INFOLAYER_STYLE_NO_INFO, [this](const std::string& cmdOut, dbb_cmd_execution_status_t status) {
        UniValue jsonOut;
        jsonOut.read(cmdOut);
        emit gotResponse(jsonOut, status, DBB_RESPONSE_TYPE_INFO);
    });
}

void DBBDaemonGui::setPasswordClicked(bool showInfo)
{
    bool ok;
    QString text = QInputDialog::getText(this, tr("Set New Password"), tr("Password"), QLineEdit::Normal, "0000", &ok);
    if (ok && !text.isEmpty()) {
        std::string command = "{\"password\" : \"" + text.toStdString() + "\"}";

        if (QTexecuteCommandWrapper(command, (showInfo) ? DBB_PROCESS_INFOLAYER_STYLE_TOUCHBUTTON : DBB_PROCESS_INFOLAYER_STYLE_NO_INFO, [this](const std::string& cmdOut, dbb_cmd_execution_status_t status) {
            UniValue jsonOut;
            jsonOut.read(cmdOut);
            emit gotResponse(jsonOut, status, DBB_RESPONSE_TYPE_PASSWORD);
        }))
        {
            sessionPasswordDuringChangeProcess = sessionPassword;
            sessionPassword = text.toStdString();
        }
    }

}

void DBBDaemonGui::seed()
{
    std::string command = "{\"seed\" : {\"source\" :\"create\","
    "\"decrypt\": \"no\","
    "\"salt\" : \"\"} }";

    QTexecuteCommandWrapper(command, (cachedWalletAvailableState) ? DBB_PROCESS_INFOLAYER_STYLE_TOUCHBUTTON : DBB_PROCESS_INFOLAYER_STYLE_NO_INFO, [this](const std::string& cmdOut, dbb_cmd_execution_status_t status) {
        UniValue jsonOut;
        jsonOut.read(cmdOut);
        emit gotResponse(jsonOut, status, DBB_RESPONSE_TYPE_CREATE_WALLET);
    });
}

/*
 /////////////////
 Utils
 /////////////////
*/
void DBBDaemonGui::getRandomNumber()
{

    std::string command = "{\"random\" : \"true\" }";

    QTexecuteCommandWrapper(command, DBB_PROCESS_INFOLAYER_STYLE_NO_INFO, [this](const std::string& cmdOut, dbb_cmd_execution_status_t status) {
        UniValue jsonOut;
        jsonOut.read(cmdOut);
        emit gotResponse(jsonOut, status, DBB_RESPONSE_TYPE_RANDOM_NUM);
    });
}

void DBBDaemonGui::lockDevice()
{

    std::string command = "{\"device\" : \"lock\" }";

    QTexecuteCommandWrapper(command, DBB_PROCESS_INFOLAYER_STYLE_NO_INFO, [this](const std::string& cmdOut, dbb_cmd_execution_status_t status) {
        UniValue jsonOut;
        jsonOut.read(cmdOut);
        emit gotResponse(jsonOut, status, DBB_RESPONSE_TYPE_DEVICE_LOCK);
    });
}

/*
 /////////////////
 Backup Stack
 /////////////////
*/
void DBBDaemonGui::showBackupDialog()
{
    backupDialog->show();
    listBackup();
}

void DBBDaemonGui::addBackup()
{

    std::time_t rawtime;
    std::tm* timeinfo;
    char buffer [80];

    std::time(&rawtime);
    timeinfo = std::localtime(&rawtime);

    std::strftime(buffer,80,"%Y-%m-%d-%H-%M-%S",timeinfo);
    std::string timeStr(buffer);

    std::string command = "{\"backup\" : {\"encrypt\" :\"no\","
    "\"filename\": \"backup-"+timeStr+".bak\"} }";

    QTexecuteCommandWrapper(command, DBB_PROCESS_INFOLAYER_STYLE_NO_INFO, [this](const std::string& cmdOut, dbb_cmd_execution_status_t status) {
        UniValue jsonOut;
        jsonOut.read(cmdOut);
        emit gotResponse(jsonOut, status, DBB_RESPONSE_TYPE_ADD_BACKUP);
    });
}

void DBBDaemonGui::listBackup()
{
    std::string command = "{\"backup\" : \"list\" }";

    QTexecuteCommandWrapper(command, DBB_PROCESS_INFOLAYER_STYLE_NO_INFO, [this](const std::string& cmdOut, dbb_cmd_execution_status_t status) {
        UniValue jsonOut;
        jsonOut.read(cmdOut);
        emit gotResponse(jsonOut, status, DBB_RESPONSE_TYPE_LIST_BACKUP);
    });

    backupDialog->showLoading();
}

void DBBDaemonGui::eraseAllBackups()
{
    QMessageBox::StandardButton reply = QMessageBox::question(this, tr("Erase All Backups?"), tr("Are your sure you want to erase all backups"), QMessageBox::Yes|QMessageBox::No);
    if (reply == QMessageBox::No)
        return;

    std::string command = "{\"backup\" : \"erase\" }";

    QTexecuteCommandWrapper(command, DBB_PROCESS_INFOLAYER_STYLE_NO_INFO, [this](const std::string& cmdOut, dbb_cmd_execution_status_t status) {
        UniValue jsonOut;
        jsonOut.read(cmdOut);
        emit gotResponse(jsonOut, status, DBB_RESPONSE_TYPE_ERASE_BACKUP);
    });

    backupDialog->showLoading();
}

void DBBDaemonGui::restoreBackup(const QString& backupFilename)
{
    std::string command = "{\"seed\" : {\"source\" :\""+backupFilename.toStdString()+"\","
    "\"decrypt\": \"no\","
    "\"salt\" : \"\"} }";

    QTexecuteCommandWrapper(command, DBB_PROCESS_INFOLAYER_STYLE_TOUCHBUTTON, [this](const std::string& cmdOut, dbb_cmd_execution_status_t status) {
        UniValue jsonOut;
        jsonOut.read(cmdOut);
        emit gotResponse(jsonOut, status, DBB_RESPONSE_TYPE_CREATE_WALLET);
    });
    
    backupDialog->close();
}

/*
 ///////////////////////////////////
 DBB USB Commands (Response Parsing)
 ///////////////////////////////////
*/
void DBBDaemonGui::parseResponse(const UniValue &response, dbb_cmd_execution_status_t status, dbb_response_type_t tag, int subtag)
{
    processComnand = false;
    setLoading(false);

    if (response.isObject())
    {
        UniValue errorObj = find_value(response, "error");
        UniValue touchbuttonObj = find_value(response, "touchbutton");
        bool touchErrorShowed = false;

        if (touchbuttonObj.isStr())
        {
            QMessageBox::information(this, tr("Touchbutton"), QString::fromStdString(touchbuttonObj.get_str()), QMessageBox::Ok);
            touchErrorShowed = true;
        }

        if (errorObj.isObject())
        {
            //error found
            UniValue errorCodeObj = find_value(errorObj, "code");
            UniValue errorMessageObj = find_value(errorObj, "message");
            if (errorCodeObj.isNum() && errorCodeObj.get_int() == 108)
            {
                QMessageBox::warning(this, tr("Password Error"), tr("Password Wrong. %1").arg(QString::fromStdString(errorMessageObj.get_str())), QMessageBox::Ok);

                //try again
                askForSessionPassword();
            }
            else if (errorCodeObj.isNum() && errorCodeObj.get_int() == 110)
            {
                QMessageBox::critical(this, tr("Password Error"), tr("Device Reset. %1").arg(QString::fromStdString(errorMessageObj.get_str())), QMessageBox::Ok);
            }
            else if (errorCodeObj.isNum() && errorCodeObj.get_int() == 101)
            {
                QMessageBox::warning(this, tr("Password Error"), QString::fromStdString(errorMessageObj.get_str()), QMessageBox::Ok);

                sessionPassword.clear();
                setPasswordClicked(false);
            }
            else
            {
                //password wrong
                QMessageBox::warning(this, tr("Error"), QString::fromStdString(errorMessageObj.get_str()), QMessageBox::Ok);
            }
        }
        else if (tag == DBB_RESPONSE_TYPE_INFO)
        {
            UniValue deviceObj = find_value(response, "device");
            if (deviceObj.isObject())
            {
                UniValue version = find_value(deviceObj, "version");
                UniValue name = find_value(deviceObj, "name");
                UniValue seeded = find_value(deviceObj, "seeded");
                UniValue lock = find_value(deviceObj, "lock");
                bool walletAvailable = (seeded.isBool() && seeded.isTrue());
                bool lockAvailable = (lock.isBool() && lock.isTrue());

                if (version.isStr())
                    this->ui->versionLabel->setText(QString::fromStdString(version.get_str()));
                if (name.isStr())
                    this->ui->deviceNameLabel->setText(QString::fromStdString(name.get_str()));

                updateOverviewFlags(walletAvailable,lockAvailable,false);

                //enable UI
                passwordAccepted();
            }
        }
        else if (tag == DBB_RESPONSE_TYPE_CREATE_WALLET)
        {
            UniValue touchbuttonObj = find_value(response, "touchbutton");
            UniValue seedObj = find_value(response, "seed");
            UniValue errorObj = find_value(response, "error");
            QString errorString;

            if (errorObj.isObject())
            {
                UniValue errorMsgObj = find_value(errorObj, "message");
                if (errorMsgObj.isStr())
                    errorString = QString::fromStdString(errorMsgObj.get_str());
            }
            if (!touchbuttonObj.isNull() && touchbuttonObj.isObject())
            {
                UniValue errorObj = find_value(touchbuttonObj, "error");
                if (!errorObj.isNull() && errorObj.isStr())
                    errorString = QString::fromStdString(errorObj.get_str());
            }
            if (!seedObj.isNull() && seedObj.isStr() && seedObj.get_str() == "success")
            {
                QMessageBox::information(this, tr("Wallet Created"), tr("Your wallet has been created successfully!"), QMessageBox::Ok);
                getInfo();
            }
            else
            {
                if (!touchErrorShowed)
                    QMessageBox::warning(this, tr("Wallet Error"), errorString, QMessageBox::Ok);
            }
        }
        else if (tag == DBB_RESPONSE_TYPE_PASSWORD)
        {
            UniValue passwordObj = find_value(response, "password");
            if (status != DBB_CMD_EXECUTION_STATUS_OK || (passwordObj.isStr() && passwordObj.get_str() == "success"))
            {
                sessionPasswordDuringChangeProcess.clear();

                //could not decrypt, password was changed successfully
                QMessageBox::information(this, tr("Password Set"), tr("Password has been set successfully!"), QMessageBox::Ok);
                getInfo();
            }
            else {
                QString errorString;
                UniValue touchbuttonObj = find_value(response, "touchbutton");
                if (!touchbuttonObj.isNull() && touchbuttonObj.isObject())
                {
                    UniValue errorObj = find_value(touchbuttonObj, "error");
                    if (!errorObj.isNull() && errorObj.isStr())
                        errorString = QString::fromStdString(errorObj.get_str());
                }

                //reset password in case of an error
                sessionPassword = sessionPasswordDuringChangeProcess;
                sessionPasswordDuringChangeProcess.clear();

                QMessageBox::warning(this, tr("Password Error"), tr("Could not set password (error: %1)!").arg(errorString), QMessageBox::Ok);
            }
        }
        else if(tag == DBB_RESPONSE_TYPE_XPUB_MS_MASTER)
        {
            UniValue xPubKeyUV = find_value(response, "xpub");
            QString errorString;

            if (!xPubKeyUV.isNull() && xPubKeyUV.isStr())
            {

                btc_hdnode node;
                bool r = btc_hdnode_deserialize(xPubKeyUV.get_str().c_str(), &btc_chain_main, &node);

                char outbuf[112];
                btc_hdnode_serialize_public(&node, &btc_chain_test, outbuf, sizeof(outbuf));

                std::string xPubKeyNew(outbuf);

                {
                    std::unique_lock<std::mutex> lock(this->cs_vMultisigWallets);
                    vMultisigWallets[subtag].client.setMasterPubKey(xPubKeyNew);
                }

                emit XPubForCopayWalletIsAvailable(subtag);
            }
            else
            {
                if (xPubKeyUV.isObject())
                {
                    UniValue errorObj = find_value(xPubKeyUV, "error");
                    if (!errorObj.isNull() && errorObj.isStr())
                        errorString = QString::fromStdString(errorObj.get_str());
                }

                QMessageBox::warning(this, tr("Join Wallet Error"), tr("Error joining Copay Wallet (%1)").arg(errorString), QMessageBox::Ok);
            }
        }
        else if(tag == DBB_RESPONSE_TYPE_XPUB_MS_REQUEST)
        {
            UniValue requestXPubKeyUV = find_value(response, "xpub");
            QString errorString;
            
            if (!requestXPubKeyUV.isNull() && requestXPubKeyUV.isStr())
            {
                btc_hdnode node;
                bool r = btc_hdnode_deserialize(requestXPubKeyUV.get_str().c_str(), &btc_chain_main, &node);

                char outbuf[112];
                btc_hdnode_serialize_public(&node, &btc_chain_test, outbuf, sizeof(outbuf));

                std::string xRequestKeyNew(outbuf);

                {
                    std::unique_lock<std::mutex> lock(this->cs_vMultisigWallets);
                    vMultisigWallets[subtag].client.setRequestPubKey(xRequestKeyNew);
                }

                emit RequestXPubKeyForCopayWalletIsAvailable(subtag);
            }
            else
            {
                if (requestXPubKeyUV.isObject())
                {
                    UniValue errorObj = find_value(requestXPubKeyUV, "error");
                    if (!errorObj.isNull() && errorObj.isStr())
                        errorString = QString::fromStdString(errorObj.get_str());
                }

                QMessageBox::warning(this, tr("Join Wallet Error"), tr("Error joining Copay Wallet (%1)").arg(errorString), QMessageBox::Ok);
            }
        }
        else if(tag == DBB_RESPONSE_TYPE_ERASE)
        {
            UniValue resetObj = find_value(response, "reset");
            if (resetObj.isStr() && resetObj.get_str() == "success")
            {
                QMessageBox::information(this, tr("Erase"), tr("Device was erased successfully"), QMessageBox::Ok);
                sessionPasswordDuringChangeProcess.clear();

                resetInfos();
                getInfo();
            }
            else
            {
                //reset password in case of an error
                sessionPassword = sessionPasswordDuringChangeProcess;
                sessionPasswordDuringChangeProcess.clear();

                if (!touchErrorShowed)
                    QMessageBox::warning(this, tr("Erase error"), tr("Could not reset device"), QMessageBox::Ok);
            }
        }
        else if(tag == DBB_RESPONSE_TYPE_LIST_BACKUP && backupDialog)
        {
            UniValue backupObj = find_value(response, "backup");
            if (backupObj.isStr())
            {
                std::vector<std::string> data = split(backupObj.get_str(), ',');
                backupDialog->showList(data);
            }
        }
        else if(tag == DBB_RESPONSE_TYPE_ADD_BACKUP && backupDialog)
        {
            listBackup();
        }
        else if(tag == DBB_RESPONSE_TYPE_ERASE_BACKUP && backupDialog)
        {
            listBackup();
        }
        else if(tag == DBB_RESPONSE_TYPE_RANDOM_NUM)
        {
            UniValue randomNumObj = find_value(response, "random");
            if (randomNumObj.isStr())
            {
                QMessageBox::information(this, tr("Random Number"), QString::fromStdString(randomNumObj.get_str()), QMessageBox::Ok);
            }
        }
        else if(tag == DBB_RESPONSE_TYPE_DEVICE_LOCK)
        {
            bool suc = false;

            //check device:lock response and give appropriate user response
            UniValue deviceObj = find_value(response, "device");
            if (deviceObj.isObject())
            {
                UniValue lockObj = find_value(deviceObj, "lock");
                if (lockObj.isBool() && lockObj.isTrue())
                    suc = true;

            }
            if (suc)
                QMessageBox::information(this, tr("Success"), tr("Your device is now locked"), QMessageBox::Ok);
            else
                QMessageBox::warning(this, tr("Error"), tr("Could not lock your device"), QMessageBox::Ok);

            //reload device infos
            resetInfos();
            getInfo();
        }
        else
        {

        }
    }
}

/*
/////////////////
copay stack
/////////////////
*/

void DBBDaemonGui::JoinCopayWallet(int walletIndex)
{
    
    if (walletIndex == -1)
        walletIndex = 1;

    setResultText(QString::fromStdString(""));

    bool isSeeded = false;
    {
        std::unique_lock<std::mutex> lock(this->cs_vMultisigWallets);
        isSeeded = vMultisigWallets[walletIndex].client.IsSeeded();
    }

    if (!isSeeded) {
        //if there is no xpub and request key, seed over DBB
        setResultText("Initializing Copay Client... this might take some seconds.");
        GetXPubKey(walletIndex);
    } else {
        //send a join request
        _JoinCopayWallet(walletIndex);
    }
}

void DBBDaemonGui::_JoinCopayWallet(int walletIndex)
{
    bool ok;
    QString text = QInputDialog::getText(this, tr("Join Copay Wallet"), tr("Wallet Invitation Code"), QLineEdit::Normal, "", &ok);
    if (!ok || text.isEmpty())
        return;

    // parse invitation code
    std::unique_lock<std::mutex> lock(this->cs_vMultisigWallets);

    BitpayWalletInvitation invitation;
    if (!vMultisigWallets[walletIndex].client.ParseWalletInvitation(text.toStdString(), invitation))
    {
        QMessageBox::warning(this, tr("Invalid Invitation"), tr("Your Copay Wallet Invitation is invalid"), QMessageBox::Ok);
        return;
    }

    std::string result;
    bool ret = vMultisigWallets[walletIndex].client.JoinWallet(vMultisigWallets[walletIndex].participationName, invitation, result);

    if (!ret) {
        UniValue responseJSON;
        std::string additionalErrorText = "unknown";
        if (responseJSON.read(result)) {
            UniValue errorText;
            errorText = find_value(responseJSON, "message");
            if (!errorText.isNull() && errorText.isStr())
                additionalErrorText = errorText.get_str();
        }
        setResultText(QString::fromStdString(result));

        int ret = QMessageBox::warning(this, tr("Copay Wallet Response"), tr("Joining the wallet failed (%1)").arg(QString::fromStdString(additionalErrorText)), QMessageBox::Ok);
    } else {
        QMessageBox::information(this, tr("Copay Wallet Response"), tr("Successfull joined Copay Wallet"), QMessageBox::Ok);
    }
}

void DBBDaemonGui::GetXPubKey(int walletIndex)
{
    std::string baseKeyPath;
    {
        std::unique_lock<std::mutex> lock(this->cs_vMultisigWallets);
        baseKeyPath = vMultisigWallets[walletIndex].baseKeyPath;
    }

    QTexecuteCommandWrapper("{\"xpub\":\"" + baseKeyPath + "/45'\"}", DBB_PROCESS_INFOLAYER_STYLE_NO_INFO, [this, walletIndex](const std::string& cmdOut, dbb_cmd_execution_status_t status) {
        UniValue jsonOut;
        jsonOut.read(cmdOut);
        emit gotResponse(jsonOut, status, DBB_RESPONSE_TYPE_XPUB_MS_MASTER, walletIndex);
    });
}

void DBBDaemonGui::GetRequestXPubKey(int walletIndex)
{
    std::string baseKeyPath;
    {
        std::unique_lock<std::mutex> lock(this->cs_vMultisigWallets);
        baseKeyPath = vMultisigWallets[walletIndex].baseKeyPath;
    }

    //try to get the xpub for seeding the request private key (ugly workaround)
    //we cannot export private keys from a hardware wallet
    QTexecuteCommandWrapper("{\"xpub\":\"" + baseKeyPath + "/1'/0\"}", DBB_PROCESS_INFOLAYER_STYLE_NO_INFO, [this, walletIndex](const std::string& cmdOut, dbb_cmd_execution_status_t status) {
        UniValue jsonOut;
        jsonOut.read(cmdOut);
        emit gotResponse(jsonOut, status, DBB_RESPONSE_TYPE_XPUB_MS_REQUEST, walletIndex);
    });
}

void DBBDaemonGui::JoinCopayWalletWithXPubKey(int walletIndex)
{
    if (walletIndex == 0)
    {
        //single wallet, create wallet first
        vMultisigWallets[walletIndex].client.CreateWallet(vMultisigWallets[walletIndex].participationName);
        vMultisigWallets[walletIndex].client.CreateWallet(vMultisigWallets[walletIndex].participationName);
    }
    //set the keys and try to join the wallet
    _JoinCopayWallet(walletIndex);
}


void DBBDaemonGui::hidePaymentProposalsWidget()
{
    if(currentPaymentProposalWidget)
    {
        currentPaymentProposalWidget->hide();
        delete currentPaymentProposalWidget;
        currentPaymentProposalWidget = NULL;
    }

    this->ui->noProposalsAvailable->setVisible(true);
}

void DBBDaemonGui::updateUIMultisigWallets(const UniValue &walletResponse)
{
    std::unique_lock<std::mutex> lock(this->cs_vMultisigWallets);

    // get balance and name
    UniValue balanceObj = find_value(walletResponse, "balance");
    if (balanceObj.isObject())
    {
        UniValue availableAmountUni = find_value(balanceObj, "availableAmount");
        vMultisigWallets[1].availableBalance = availableAmountUni.get_int64();
    }

    UniValue walletObj = find_value(walletResponse, "wallet");
    if (walletObj.isObject())
    {
        UniValue nameUni = find_value(walletObj, "name");
        if (nameUni.isStr())
            vMultisigWallets[1].walletRemoteName = nameUni.get_str();


        std::string mStr;
        std::string nStr;

        UniValue mUni = find_value(walletObj, "m");
        if (mUni.isNum())
            mStr = std::to_string(mUni.get_int());

        UniValue nUni = find_value(walletObj, "n");
        if (nUni.isNum())
            nStr = std::to_string(nUni.get_int());

        if (mStr.size() > 0 && nStr.size() > 0)
            vMultisigWallets[1].walletRemoteName += " ("+mStr+" of "+nStr+")";
    }

    UniValue pendingTxps;
    pendingTxps = find_value(walletResponse, "pendingTxps");
    if (pendingTxps.isArray()) {
        this->ui->proposalsLabel->setText(tr("Current Payment Proposals (%1)").arg(pendingTxps.size()));
    }



    //TODO, add a monetary amount / unit helper function
    this->ui->multisigBalance->setText(tr("%1 Bits").arg(vMultisigWallets[1].availableBalance));
    this->ui->multisigWalletName->setText(QString::fromStdString(vMultisigWallets[1].walletRemoteName));
}

void DBBDaemonGui::MultisigUpdateWallets()
{
    if (netThreadBusy)
        return;

    //join the tread in case if one is already running
    if (netThread.joinable())
        netThread.join();

    //TODO: find a better more generic way to detach net threads
    netThread = std::thread([this]() {
        std::string walletsResponse;

        std::unique_lock<std::mutex> lock(this->cs_vMultisigWallets);
        bool walletsAvailable = this->vMultisigWallets[1].client.GetWallets(walletsResponse);
        emit multisigWalletResponseAvailable(walletsAvailable, walletsResponse);
    });
    netThreadBusy = true;
    setNetLoading(true);

    int i = 1;
}

void DBBDaemonGui::MultisigParseWalletsResponse(bool walletsAvailable, const std::string &walletsResponse)
{
    netThreadBusy = false;
    setNetLoading(false);

    if (!walletsAvailable)
    {
        QMessageBox::warning(this, tr("No Wallet"),
                             tr("No Copay Wallet Available"),
                             QMessageBox::Ok);
        return;
    }

    UniValue response;
    if (response.read(walletsResponse)) {
        if (response.isObject()) {
            printf("Wallet: %s\n", response.write(true, 2).c_str());

            updateUIMultisigWallets(response);
            MultisigUpdatePaymentProposals(response);
        }
    }
}

bool DBBDaemonGui::MultisigDisplayPaymentProposal(const UniValue &pendingTxps, const std::string &targetID)
{
    if (pendingTxps.isArray()) {
        std::vector<UniValue> values = pendingTxps.getValues();
        if (values.size() == 0)
        {
            hidePaymentProposalsWidget();
            return false;
        }

        size_t cnt = 0;
        for (const UniValue &oneProposal : values)
        {

            UniValue idUni = find_value(oneProposal, "id");
            if (!idUni.isStr() || idUni.get_str() != targetID)
            {
                cnt++;
                continue;
            }

            std::string prevProposalID;
            std::string nextProposalID;

            if (cnt > 0)
            {
                UniValue idUni = find_value(values[cnt-1], "id");
                if (idUni.isStr())
                    prevProposalID = idUni.get_str();
            }

            if (cnt < values.size()-1)
            {
                UniValue idUni = find_value(values[cnt+1], "id");
                if (idUni.isStr())
                    nextProposalID = idUni.get_str();
            }

            if (!currentPaymentProposalWidget)
            {
                currentPaymentProposalWidget = new PaymentProposal(this->ui->copay);
                connect(currentPaymentProposalWidget, SIGNAL(processProposal(const UniValue &, int )), this, SLOT(PaymentProposalAction(const UniValue &, int)));
                connect(currentPaymentProposalWidget, SIGNAL(shouldDisplayProposal(const UniValue &, const std::string &)), this, SLOT(MultisigDisplayPaymentProposal(const UniValue &, const std::string &)));
            }

            currentPaymentProposalWidget->move(15,115);
            currentPaymentProposalWidget->show();
            currentPaymentProposalWidget->SetData(vMultisigWallets[1].client.GetCopayerId(), pendingTxps, oneProposal, prevProposalID, nextProposalID);

            this->ui->noProposalsAvailable->setVisible(false);

            cnt++;
        }
    }
}

bool DBBDaemonGui::MultisigUpdatePaymentProposals(const UniValue &response)
{
    bool ret = false;
    int copayerIndex = INT_MAX;

    UniValue pendingTxps;
    pendingTxps = find_value(response, "pendingTxps");
    if (!pendingTxps.isNull() && pendingTxps.isArray()) {

        std::unique_lock<std::mutex> lock(this->cs_vMultisigWallets);
        vMultisigWallets[1].currentPaymentProposals = pendingTxps;

        printf("pending txps: %s", pendingTxps.write(2, 2).c_str());
        std::vector<UniValue> values = pendingTxps.getValues();
        if (values.size() == 0)
        {
            hidePaymentProposalsWidget();
            return false;
        }

        size_t cnt = 0;

        for (const UniValue &oneProposal : values)
        {

            QString amount;
            QString toAddress;

            UniValue toAddressUni = find_value(oneProposal, "toAddress");
            UniValue amountUni = find_value(oneProposal, "amount");
            UniValue actions = find_value(oneProposal, "actions");

            bool skipProposal = false;
            if (actions.isArray())
            {
                for (const UniValue &oneAction : actions.getValues())
                {
                    UniValue copayerId = find_value(oneAction, "copayerId");
                    UniValue actionType = find_value(oneAction, "type");

                    if (!copayerId.isStr() || !actionType.isStr())
                        continue;

                    if (vMultisigWallets[1].client.GetCopayerId() == copayerId.get_str() && actionType.get_str() == "accept") {
                        skipProposal = true;
                        break;
                    }
                }
            }
//                    if (skipProposal)
//                        continue;

            UniValue isUni = find_value(oneProposal, "id");
            if (isUni.isStr())
                MultisigDisplayPaymentProposal(pendingTxps, isUni.get_str());
            
            return true;
            
            QMessageBox::StandardButton reply = QMessageBox::question(this, tr("Payment Proposal Available"), tr("Do you want to sign: pay %1BTC to %2").arg(amount, toAddress), QMessageBox::Yes|QMessageBox::No);
            if (reply == QMessageBox::No)
                return false;
        } //end proposal loop
    }

    return ret;
}

void DBBDaemonGui::PaymentProposalAction(const UniValue &paymentProposal, int actionType)
{
    std::unique_lock<std::mutex> lock(this->cs_vMultisigWallets);

    if (actionType == ProposalActionTypeReject)
    {
        vMultisigWallets[1].client.RejectTxProposal(paymentProposal);
        MultisigUpdateWallets();
        return;
    }
    std::vector<std::pair<std::string, std::vector<unsigned char> > > inputHashesAndPaths;
    vMultisigWallets[1].client.ParseTxProposal(paymentProposal, inputHashesAndPaths);

    //build sign command
    std::string hashCmd;
    for (const std::pair<std::string, std::vector<unsigned char> > &hashAndPathPair : inputHashesAndPaths)
    {
        std::string hexHash = DBB::HexStr((unsigned char *)&hashAndPathPair.second[0], (unsigned char *)&hashAndPathPair.second[0]+32);

        hashCmd += "{ \"hash\" : \"" + hexHash + "\", \"keypath\" : \"" + vMultisigWallets[1].baseKeyPath + "/45'/" + hashAndPathPair.first + "\" }, ";
    }
    hashCmd.pop_back(); hashCmd.pop_back(); // remove ", "

    std::string hexHash = DBB::HexStr(&inputHashesAndPaths[0].second[0], &inputHashesAndPaths[0].second[0]+32);


    std::string command = "{\"sign\": { \"type\": \"meta\", \"meta\" : \"somedata\", \"data\" : [ " + hashCmd + " ] } }";
    printf("Command: %s\n", command.c_str());

    bool ret = false;
    QTexecuteCommandWrapper(command, DBB_PROCESS_INFOLAYER_STYLE_NO_INFO, [&ret, actionType, paymentProposal, inputHashesAndPaths, this](const std::string& cmdOut, dbb_cmd_execution_status_t status) {
        //send a signal to the main thread
        processComnand = false;
        setLoading(false);

        printf("cmd back: %s\n", cmdOut.c_str());
        UniValue jsonOut(UniValue::VOBJ);
        jsonOut.read(cmdOut);

        UniValue echoStr = find_value(jsonOut, "echo");
        if (!echoStr.isNull() && echoStr.isStr())
        {

            emit shouldVerifySigning(paymentProposal, actionType, QString::fromStdString(echoStr.get_str()));
        }
        else
        {
            UniValue signObject = find_value(jsonOut, "sign");
            if (signObject.isArray()) {
                std::vector<UniValue> vSignatureObjects;
                vSignatureObjects = signObject.getValues();
                if (vSignatureObjects.size() > 0) {
                    std::vector<std::string> sigs;

                    for (const UniValue &oneSig : vSignatureObjects)
                    {
                        UniValue sigObject = find_value(oneSig, "sig");
                        UniValue pubKey = find_value(oneSig, "pubkey");
                        if (!sigObject.isNull() && sigObject.isStr())
                        {
                            sigs.push_back(sigObject.get_str());
                            //client.BroadcastProposal(values[0]);
                        }
                    }

                    emit signedProposalAvailable(paymentProposal, sigs);
                    ret = true;
                }
            }

        }
    });
}

void DBBDaemonGui::postSignedPaymentProposal(const UniValue& proposal, const std::vector<std::string> &vSigs)
{
    std::unique_lock<std::mutex> lock(this->cs_vMultisigWallets);

    vMultisigWallets[0].client.PostSignaturesForTxProposal(proposal, vSigs);
    MultisigUpdateWallets();
}
