#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QList>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:

    QString readData();

    void on_btn_openPort_clicked();

    void on_btn_local_clicked();

    void on_btn_rs232_clicked();

    void on_btn_standBy_clicked();

    void on_btn_start_clicked();

    void handleSetFlowTimeout();

    void handleGetFlowTimeout();

//    void on_lineEdit_maxFlow_editingFinished();

    void on_btn_clearMsg_clicked();

    void on_comboBox_serialPort_currentIndexChanged(const QString &arg1);

    void on_btn_clearMsg_recv_clicked();

private:

    // 关闭串口
    void closePort();

    // 发送数据
    void writeData(QString aString, bool aDisplay=true);

    bool checkMaxFlowValue();

private:
    Ui::MainWindow *ui;
    QSerialPort *ivSerial;
    QString ivPortName;
    QColor ivDefaultColor;
    QTimer ivSetFlowTimer;
    QTimer ivGetFlowTimer;
    int ivMaxFlowValue;
    int ivFlowValue;
    uint ivGetFlowDisplayCount;
    bool bFirstSetFlow;
};

#endif // MAINWINDOW_H
