#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QMessageBox>
#include <fstream>
using namespace std;

static const char blankString[] = QT_TRANSLATE_NOOP("SettingsDialog", "N/A");
static QByteArray QString2Hex(QString str);
static char ConvertHexChar(char ch);
static int QString2Int(QString aString);
static QString Hex2QString(QByteArray data);

// 是否打印log到显示窗口
bool bDisp = true;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->groupBox_controlMode->setEnabled(false);
    ui->groupBox_flow->setEnabled(false);

    //固定窗口大小，禁止拖拉
    this->setFixedSize(496,696);

    // 得到按钮的初始背景色
    QPalette pal = ui->btn_local->palette();
    QBrush brush = pal.background();
    ivDefaultColor = brush.color();

    // 变量初始化
    ivGetFlowDisplayCount = 0;
    bFirstSetFlow = true;

    //
    // 遍历串口
    //
    ivSerial = new QSerialPort;
    QString description;
    QString manufacturer;
    QString serialNumber;

    // 获取可以用的串口
    QList<QSerialPortInfo> serialPortInfos = QSerialPortInfo::availablePorts();

    //输出当前系统可以使用的串口个数
    QString temp = "Total numbers of ports: " + QString::number(serialPortInfos.count());
    ui->textEdit_send->append(temp);

    for (const QSerialPortInfo &serialPortInfo : serialPortInfos)
    {
        QStringList list;
        description = serialPortInfo.description();
        manufacturer = serialPortInfo.manufacturer();
        serialNumber = serialPortInfo.serialNumber();
        list << serialPortInfo.portName()
             << (!description.isEmpty() ? description : blankString)
             << (!manufacturer.isEmpty() ? manufacturer : blankString)
             << (!serialNumber.isEmpty() ? serialNumber : blankString)
             << serialPortInfo.systemLocation()
             << (serialPortInfo.vendorIdentifier() ? QString::number(serialPortInfo.vendorIdentifier(), 16) : blankString)
             << (serialPortInfo.productIdentifier() ? QString::number(serialPortInfo.productIdentifier(), 16) : blankString);
        ui->comboBox_serialPort->addItem(list.first(), list);

        // 打印日志
        QString msg;
        for(int i = 0; i< list.size();++i)
        {
            msg.append(list.at(i) + " ");
        }
        ui->textEdit_send->append("PORT: " + msg);
    }

    // 系统无可用串口
    ivPortName = "";
    if( ui->comboBox_serialPort->count() == 0 )
    {
        return;
    }

    // 读取配置文件, 获得最近一次打开的串口名字
    ifstream infile("./port.config", ios::in);
    if (infile.is_open())
    {
        std::string temp;
        getline(infile, temp);
        infile.close();
        ivPortName = QString::fromStdString(temp);
    }

    // 设置comboBox 的值为最近一次打开的端口号，如果没有最近的值，则设置为第一个值
    int index;
    if( ivPortName != "" && (index=ui->comboBox_serialPort->findText(ivPortName)) != -1)
    {
        ui->comboBox_serialPort->setCurrentIndex(index);
    }
    else
    {
        ui->comboBox_serialPort->setCurrentIndex(0);
        ivPortName = ui->comboBox_serialPort->currentText();
    }
}

MainWindow::~MainWindow()
{
    closePort();

    // 保存串口名字
    ofstream outfile("./port.config", ios::out);
    if (outfile.is_open())
    {
        outfile << ivPortName.toStdString()<<endl;
        outfile.close();
    }

    delete ui;
}

void MainWindow::closePort()
{
    // 停止定时器
    if( ivGetFlowTimer.isActive() )
    {
        ivGetFlowTimer.stop();
    }
    if( ivSetFlowTimer.isActive() )
    {
        ivSetFlowTimer.stop();
    }

    //关闭串口
    //ivSerial->clear();
    ivSerial->close();
    //ivSerial->deleteLater();

    qDebug() << "Port RS232 has been closed.";
}

void MainWindow::on_btn_openPort_clicked()
{
    // 关闭端口的情况
    if( ui->btn_openPort->text()=="关闭端口")
    {
        // 停止定时器
        if( ivGetFlowTimer.isActive() )
        {
            ivGetFlowTimer.stop();
        }
        if( ivSetFlowTimer.isActive() )
        {
            ivSetFlowTimer.stop();
        }

        ivSerial->close();

        ui->comboBox_serialPort->setEnabled(true);
        ui->btn_openPort->setText("打开端口");
        ui->groupBox_controlMode->setEnabled(false);
        ui->groupBox_flow->setEnabled(false);
        return;
    }

    // 打开端口的情况
    ivPortName = ui->comboBox_serialPort->currentText();
    if( ivPortName == "" )
    {
        QString msg = "Not found any port, please check the environment or the comboBox contnet.";
        ui->textEdit_send->append(msg);
        return;
    }

    //设置串口名字
    ivSerial->setPortName(ivPortName);

    //设置波特率 9600
    ivSerial->setBaudRate(QSerialPort::Baud9600);

    //设置停止位
    ivSerial->setStopBits(QSerialPort::OneStop);

    //设置数据位
    ivSerial->setDataBits(QSerialPort::Data8);

    //设置奇偶校验位
    ivSerial->setParity(QSerialPort::NoParity);

    //设置流控
    //serial->setFlowControl(QSerialPort::NoFlowControl);

    //打开串口
    if (ivSerial->open(QIODevice::ReadWrite))
    {
        //信号与槽函数关联
        connect(ivSerial, &QSerialPort::readyRead, this, &MainWindow::readData);
    }
    else
    {
        QString msg = "\n Can not open port " + ivPortName + ", please check the environment.\n";
        ui->textEdit_send->append(msg);
        return;
    }

    ui->groupBox_controlMode->setEnabled(true);
    ui->groupBox_flow->setEnabled(true);
    ui->comboBox_serialPort->setEnabled(false);
    ui->btn_openPort->setText("关闭端口");

    QString temp = "打开串口" + ivPortName \
                  + " 波特率:" + QString::number(QSerialPort::Baud9600) + " 停止位:OneStop" \
                  + " 数据位:" + QString::number(QSerialPort::Data8) + " 校验位:NoParity";
    ui->textEdit_send->append(temp);

    connect(&ivGetFlowTimer, &QTimer::timeout, this, &MainWindow::handleGetFlowTimeout);
    ivGetFlowTimer.start(1000);

    // 设置工作模式为连续模式 判断返回帧的值
    ui->textEdit_send->append("\n设置工作模式为连续模式：");
    QString workMode = "7E01010101";
    writeData(workMode);

    return;
}

void MainWindow::handleGetFlowTimeout()
{
    ui->lineEdit_flowValue->clear();

    // 设置第一次和每5次(5s)打印日志消息
    ivGetFlowDisplayCount++;
    //if( ivGetFlowDisplayCount == 1 || ivGetFlowDisplayCount%5==0)
    {
        ui->textEdit_send->append("\n查询电流值：");
        bDisp = true;
    }

    // 查询电流值
    QString getFlowValue = "7E51010100";
    writeData(getFlowValue, bDisp);
}

void MainWindow::on_btn_local_clicked()
{
    // 停止定时器
    if( ivGetFlowTimer.isActive() )
    {
        ivGetFlowTimer.stop();
    }

    // 设置本地控制模式
    ui->textEdit_send->append("\n设置本地控制模式：");
    QString localMode = "7E01030101";
    writeData(localMode);

    ui->btn_local->setStyleSheet("background:rgb(0,255,0)");
    ui->btn_local->setEnabled(false);

    ui->btn_rs232->setEnabled(true);
    ui->btn_rs232->setStyleSheet(tr("background-color:%1").arg(ivDefaultColor.name()));

    // 开启查询定时器
    ivGetFlowTimer.start(1000);
}

void MainWindow::on_btn_rs232_clicked()
{
    // 停止定时器
    if( ivGetFlowTimer.isActive() )
    {
        ivGetFlowTimer.stop();
    }

    // 设置RS232控制模式
    ui->textEdit_send->append("\n设置RS232控制模式：");
    QString RS232Mode = "7E01030100";
    writeData(RS232Mode);

    ui->btn_rs232->setStyleSheet("background:rgb(0,255,0)");
    ui->btn_rs232->setEnabled(false);

    ui->btn_local->setEnabled(true);
    ui->btn_local->setStyleSheet(tr("background-color:%1").arg(ivDefaultColor.name()));

    // 开启查询定时器
    ivGetFlowTimer.start(1000);
}

void MainWindow::writeData(QString aString, bool aDisplay)
{
    QByteArray lvData = QString2Hex(aString);
    int result = ivSerial->write(lvData);

    if(ivSerial->waitForBytesWritten(5000))
    {
        // 打印日志
        if(!aDisplay)
        {
            return;
        }

        if( result == -1 )
        {
            ui->textEdit_send->append("Write " + aString + " failed!");
        }
        else
        {
            ui->textEdit_send->append("Write " + aString + " successful.");
        }
    }
}

QString MainWindow::readData()
{
    QByteArray buf;
    QString str;

    //if(ivSerial->waitForReadyRead(3000))
    {
        buf = ivSerial->readAll();
//        while(ivSerial->waitForReadyRead(100))
//        {
//            buf += ivSerial->readAll();
//        }
        if (!buf.isEmpty())
        {
            str = Hex2QString(buf);

            // 更新 ui recv 窗口信息
            if( bDisp == true )
            {
                // 清空之前显示的数据后，重新显示
                QString receive = ui->textEdit_recv->toPlainText();
                ui->textEdit_recv->clear();

                receive += QString(" " + str);
                ui->textEdit_recv->append(receive);
            }

            str.remove(QRegExp("\\s"));

            // 更新ui实时电流值信息
            int index = str.lastIndexOf("7F510102");
            if( index!=-1 )
            {
                // 取得后四位的电流值，实时显示在显示框
                int value = QString2Int(str.mid(index+8,4));
                ui->lineEdit_flowValue->setText(QString::number(value));
            }

            // 更新 ui send 窗口信息
            index = str.lastIndexOf("7F01010101");
            if( index!=-1 )
            {
                ui->textEdit_send->append("\n设置工作模式返回帧正确 7F01010101");
            }
            index = str.lastIndexOf("7F01030101");
            if( index!=-1 )
            {
                ui->textEdit_send->append("\n设置本地控制模式返回帧正确 7F01030101");
            }
            index = str.lastIndexOf("7F01030100");
            if( index!=-1 )
            {
                ui->textEdit_send->append("\n设置本地控制模式返回帧正确 7F01030100");
            }
            index = str.lastIndexOf("7F0401020000");
            if( index!=-1 )
            {
                ui->textEdit_send->append("\n设置电流为0返回帧正确 7F0401020000");
            }
            index = str.lastIndexOf("7F02010101");
            if( index!=-1 )
            {
                ui->textEdit_send->append("\n开启电流开关返回帧正确 7F02010101");
            }
            index = str.lastIndexOf("7F02010100");
            if( index!=-1 )
            {
                ui->textEdit_send->append("\n开启电流开关返回帧正确 7F02010100");
            }
            index = str.lastIndexOf("7F040102");
            if( index!=-1 )
            {
                QString vStr = str.mid(index+8,4);
                ui->textEdit_send->append("\n设置电流返回帧正确 7F02010100" + vStr);
            }
        }
    }

    return str;
}

void MainWindow::on_btn_standBy_clicked()
{
    // 停止定时器
    if( ivGetFlowTimer.isActive() )
    {
        ivGetFlowTimer.stop();
    }
    if( ivSetFlowTimer.isActive() )
    {
        ivSetFlowTimer.stop();
    }

    // 1. 待机模式 电流设置为0
    ui->textEdit_send->append("\n设置待机模式 电流为0：");
    QString flowValue = "7E0401020000";
    writeData(flowValue);

    // 2. 待机模式 开启电流开关
    ui->textEdit_send->append("\n设置待机模式 开启电流开关：");
    QString flowOn = "7E02010101";
    writeData(flowOn);

    ui->btn_standBy->setStyleSheet("background:rgb(0,255,0)");
    ui->btn_start->setStyleSheet(tr("background-color:%1").arg(ivDefaultColor.name()));


    // 开启查询定时器
    ivGetFlowTimer.start(1000);
}

void MainWindow::on_btn_start_clicked()
{
    // 启动
    if( ui->btn_start->text() == "启动" )
    {
        if( ui->lineEdit_maxFlow->text()=="")
        {
            QMessageBox::critical(NULL, "启动错误", "无法设置电流值, 请设置电流最大值!");
            return;
        }

        if( !checkMaxFlowValue() )
        {
            QMessageBox::critical(NULL, "启动错误", "电流最大值设置非法或超出范围9000mA");
            return;
        }

        // 停止定时器
        if( ivGetFlowTimer.isActive() )
        {
            ivGetFlowTimer.stop();
        }

        ivMaxFlowValue = ui->lineEdit_maxFlow->text().toInt();
        ivFlowValue = 0;

        // 打开电流
        ui->textEdit_send->append("\n打开电流：");
        QString flowOn = "7E02010101";
        writeData(flowOn);

        // 设置启动按钮文字为关闭电源，颜色为绿色
        ui->btn_start->setText("关闭电流");
        ui->btn_start->setStyleSheet("background:rgb(0,255,0)");
        ui->btn_standBy->setStyleSheet(tr("background-color:%1").arg(ivDefaultColor.name()));

        // 设置设置电流定时器
        connect(&ivSetFlowTimer, &QTimer::timeout, this, &MainWindow::handleSetFlowTimeout);
        ivSetFlowTimer.start(1000);

        // 开启查询定时器
        ivGetFlowTimer.start(1000);
    }

    // 关闭电流
    else
    {
        // 停止定时器
        if( ivSetFlowTimer.isActive() )
        {
            ivSetFlowTimer.stop();
        }
        if( ivGetFlowTimer.isActive() )
        {
            ivGetFlowTimer.stop();
        }

        ui->lineEdit_flowValue->clear();

        // 关闭电流
        ui->textEdit_send->append("\n关闭电流：");
        QString flowOn = "7E02010100";
        writeData(flowOn);

        // 设置电流
        ui->textEdit_send->append("\n设置电流值：0 ");
        QString flowValue = "7E0401020000";
        writeData(flowValue);

        ui->btn_start->setText("启动");
        ui->btn_start->setStyleSheet(tr("background-color:%1").arg(ivDefaultColor.name()));
        ui->btn_standBy->setStyleSheet(tr("background-color:%1").arg(ivDefaultColor.name()));

        // 开启查询定时器
        ivGetFlowTimer.start(1000);
    }
}

void MainWindow::handleSetFlowTimeout()
{
    if( ivFlowValue == ivMaxFlowValue )
    {
        // 已设置到峰值，结束定时器
        ivSetFlowTimer.stop();
        return;
    }

    // 计算得出这一秒的电流设置值
    ivFlowValue += ivMaxFlowValue/20;;
    if( ivFlowValue > ivMaxFlowValue )
    {
        ivFlowValue = ivMaxFlowValue;
    }
    QString value = QString("%1").arg(ivFlowValue,4,16,QLatin1Char('0'));

    // 第一次进来打印日志
    //if( bFirstSetFlow )
    {
        ui->textEdit_send->append("\n设置电流值： " + QString::number(ivFlowValue));
        bFirstSetFlow = false;
    }

    // 设置电流
    QString flowValue = "7E040102" + value;
    writeData(flowValue);
}

/*
void MainWindow::on_lineEdit_maxFlow_editingFinished()
{
    if( !checkMaxFlowValue() )
    {
        QMessageBox::critical(NULL, "输入错误", "电流最大值设置非法或超出范围9000mA");
    }
}
*/

bool MainWindow::checkMaxFlowValue()
{
    if( ui->lineEdit_maxFlow->text()=="")
    {
        return true;
    }

    // 表达式匹配 非负整数(正整数和0)
    QString pattern("^\\d+");
    QRegExp rx(pattern);

    if(!rx.exactMatch(ui->lineEdit_maxFlow->text()))
    {
        return false;
    }

    // 小于9000
    ivMaxFlowValue = ui->lineEdit_maxFlow->text().toInt();
    if(ivMaxFlowValue>9000)
    {
        return false;
    }

    return true;
}

static int QString2Int(QString aString)
{
    return (aString.toInt(0, 16));
}

static QByteArray QString2Hex(QString aString)
{
    QByteArray senddata;
    int hexdata,lowhexdata;
    int hexdatalen = 0;
    int len = aString.length();
    senddata.resize(len/2);
    char lstr,hstr;
    for(int i=0; i<len; )
    {
        hstr=aString[i].toLatin1();
        if(hstr == ' ')
        {
            i++;
            continue;
        }
        i++;
        if(i >= len)
            break;
        lstr = aString[i].toLatin1();
        hexdata = ConvertHexChar(hstr);
        lowhexdata = ConvertHexChar(lstr);
        if((hexdata == 16) || (lowhexdata == 16))
            break;
        else
            hexdata = hexdata*16+lowhexdata;
        i++;
        senddata[hexdatalen] = (char)hexdata;
        hexdatalen++;
    }
    senddata.resize(hexdatalen);
    return senddata;
}

static char ConvertHexChar(char ch)
{
    if((ch >= '0') && (ch <= '9'))
        return ch-0x30;
    else if((ch >= 'A') && (ch <= 'F'))
        return ch-'A'+10;
    else if((ch >= 'a') && (ch <= 'f'))
        return ch-'a'+10;
    else return (-1);
}

static QString Hex2QString(QByteArray data)
{
    QString ret(data.toHex().toUpper());
    int len = ret.length()/2;
    qDebug()<<len;
    for(int i=1;i<len;i++)
    {
        ret.insert(2*i+i-1," ");
    }

    return ret;
}

void MainWindow::on_comboBox_serialPort_currentIndexChanged(const QString &arg1)
{
    ivPortName = arg1;
}

void MainWindow::on_btn_clearMsg_clicked()
{
    ui->textEdit_send->clear();
}

void MainWindow::on_btn_clearMsg_recv_clicked()
{
    ui->textEdit_recv->clear();
}