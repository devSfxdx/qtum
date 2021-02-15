#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <qt/hardwaresigntx.h>
#include <qt/waitmessagebox.h>
#include <qt/hardwarekeystoredialog.h>
#include <qt/walletmodel.h>
#include <qt/qtumhwitool.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>


HardwareSignTx::HardwareSignTx(QWidget *_widget) : QObject(_widget)
{
    tool = new QtumHwiTool(this);
    widget = _widget;
}

HardwareSignTx::~HardwareSignTx()
{}

void HardwareSignTx::setModel(WalletModel *_model)
{
    model = _model;
    tool->setModel(_model);
}

bool HardwareSignTx::askDevice()
{
    // Check if the HWI tool exist
    QString hwiToolPath = GUIUtil::getHwiToolPath();
    if(!QFile::exists(hwiToolPath))
    {
        QMessageBox msgBox(widget);
        msgBox.setWindowTitle(tr("HWI tool not found"));
        msgBox.setTextFormat(Qt::RichText);
        msgBox.setText(tr("HWI tool not found at path \"%1\".<br>Please download it from %2 and add the path to the settings.").arg(hwiToolPath, QTUM_HWI_TOOL));
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
        return false;
    }

    // Ask for ledger
    QString fingerprint = model->getFingerprint();
    QString title = tr("Connect Ledger");
    QString message = tr("Please insert your Ledger (%1). Verify the cable is connected and that no other application is using it.\n\nTry to connect again?");
    if(HardwareKeystoreDialog::AskDevice(fingerprint, title, message.arg(fingerprint), widget))
    {
        model->setFingerprint(fingerprint);
        return true;
    }

    model->setFingerprint("");
    return false;
}

bool HardwareSignTx::sign()
{
    if(askDevice())
    {
        // Sign transaction with hardware
        WaitMessageBox dlg(tr("Ledger Status"), tr("Confirm Transaction on your Ledger device..."), [this]() {
            QString fingerprint = model->getFingerprint();
            QString tmpPsbt = psbt;
            hexTx = "";
            complete = false;
            bool ret = tool->signDelegate(fingerprint, tmpPsbt);
            if(ret) ret &= tool->signTx(fingerprint, tmpPsbt);
            if(ret) ret &= tool->finalizePsbt(tmpPsbt, hexTx, complete);
        }, widget);

        dlg.exec();

        if(!complete)
        {
            QMessageBox::warning(widget, tr("Sign failed"), tr("The transaction has no a complete set of signatures."));
        }
    }

    return complete;
}

bool HardwareSignTx::send(QVariantMap &result)
{
    if(tool->sendRawTransaction(hexTx, result))
    {
        return true;
    }
    else
    {
        // Display error message
        QString errorMessage = tool->errorMessage();
        if(errorMessage.isEmpty()) errorMessage = tr("Unknown transaction error");
        QMessageBox::warning(widget, tr("Broadcast transaction"), errorMessage);
    }

    return false;
}

void HardwareSignTx::setPsbt(const QString &_psbt)
{
    psbt = _psbt;
    hexTx = "";
    complete = false;
}