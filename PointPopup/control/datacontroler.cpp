/***********************************************************************
*PointDownload
*Copyright (C) 2014  PointTeam
*
* Author:     Match <2696627729@qq.com>
* Maintainer: Match <2696627729@qq.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ***********************************************************************/


#include "datacontroler.h"
#include <QtQml>

DataControler::DataControler(QObject *parent) :
    QObject(parent)
{
    //import时使用Singleton.DataControler，在获取内容或调用函数时使用PEventFilter
    qmlRegisterSingletonType<DataControler>("Singleton.DataControler", 1, 0, "DataControler", dataObj);

    startMainProgram();

    fileURL = "";
    fileNameList = "";
    fileSavePath = gSettingHandler.getChildElement(GeneralSettings,"SavePath");
    freeSpace = getLinuxFreeSpace(fileSavePath);
    maxThread = gSettingHandler.getChildElement(GeneralSettings,"DefaultThreadCount");
    maxSpeed = gSettingHandler.getChildElement(GeneralSettings,"MaxDownloadSpeed");

    toolsType = gSettingHandler.getChildElement(GeneralSettings,"PriorityTool");
    setDefaultToolType(gSettingHandler.getChildElement(GeneralSettings,"PriorityTool"));
    setIsYouGetEnable(gSettingHandler.getChildElement(YouGetSetting,"State")=="Enable"?true:false);
    setIsAria2Enable(gSettingHandler.getChildElement(Aria2Setting,"State")=="Enable"?true:false);
    setIsXwareEnable(gSettingHandler.getChildElement(XwareSetting,"State")=="Enable"?true:false);

    yougetProcess = NULL;

    urlInfoGeter = new URLInfoGeter(localSocket,fileURL,0);
}

//保证单例对象先于qml组件之前构建
DataControler * DataControler::dataControler = new DataControler();
DataControler * DataControler::getInstance()
{
    return dataControler;
}

void DataControler::selectSavePath(QString buttonName)
{
    if (buttonName == "Default")
    {
        setFileSavePath(gSettingHandler.getChildElement(GeneralSettings,"SavePath"));
        setFreeSpace(getLinuxFreeSpace(fileSavePath));
    }
    else if (buttonName == "Desktop")
    {
        setFileSavePath(QStandardPaths::standardLocations(QStandardPaths::DesktopLocation).at(0));
        setFreeSpace(getLinuxFreeSpace(fileSavePath));
    }
    else
    {
        QString tmpPath =  QFileDialog::getExistingDirectory(0,
                                                          tr("Open Folder"),
                                                          gSettingHandler.getChildElement(GeneralSettings,"SavePath"),
                                                          QFileDialog::ShowDirsOnly);
        if (tmpPath != "")
        {
            setFileSavePath(tmpPath);
            setFreeSpace(getLinuxFreeSpace(fileSavePath));
        }
    }
}

void DataControler::selectBTFile()
{
    QString fileName = QFileDialog::getOpenFileName(0, tr("Open File"),
                                                    gSettingHandler.getChildElement(GeneralSettings,"SavePath"),
                                                    tr("BitTorrent (*.torrent)"));
    if (fileName == NULL)
        return;

    MetaInfo metaInfo;
    QFile torrent(fileName);
    if (!torrent.open(QFile::ReadOnly) || !metaInfo.parse(torrent.readAll()))
        return;
    else
        getURLFromBrowser(metaInfo.toMagnetLink());
}

void DataControler::sendToMainServer(QString threads, QString speed, QString savePath,QString newToolType)
{
    if (checkIsInDownloading(fileURL))
    {
        return;//no need to do anything
    }
    else if (checkIsInDownloaded(fileURL))
    {
        //delete xml item
        QFile::remove(gDownloadHandler.getDownloadedNode(fileURL).savePath + "/"
                      + gDownloadHandler.getDownloadedNode(fileURL).name);
        gDownloadHandler.removeDownloadedFileNode(fileURL);

    }
    else if (checkIsInDownloadTrash(fileURL))
    {
        gDownloadHandler.removeDownloadTrashFileNode(fileURL);
    }

    //info: toolsType?:?fileNameList?:?URL?:?RedirectURL?:?iconName?:?savePath?:?threadCount?:?maxSpeed
    QString info = newToolType + "?:?"
            + mergeFileNameList(fileNameList) + "?:?"
            + fileURL + "?:?"
            + redirectURL + "?:?"
            + "qrc:/images/right/filetype/" +getIconName() + "?:?"
            + savePath + "?:?"
            + threads + "?:?"
            + speed;

    localSocket->write(info.toStdString().c_str());
    localSocket->flush();

    qApp->quit();
}

void DataControler::getURLFromBrowser(QString URL)
{
    setFileURL(URL);
    redirectURL = fileURL;
    setToolsType(getDLToolsTypeFromURL(URL));

    if (toolsType == "")
    {
        emit sIsWrongURL();
        return;
    }

    if(toolsType == "YouGet")
    {
        QTimer::singleShot(100,this,SLOT(getURLInfoFromYouget()));
    }
    else if(toolsType == "Point")
    {
        //此处会导致警告：QString::arg: Argument missing: 无法解析res_nclose中的符号“res_nclose”：libresolv.so.2,
        //获取http、ftp、Bt等类型的信息
        //延迟时间，防止qml组件未初始化前就发送了信号
        QTimer::singleShot(100,this,SLOT(getURLInfo()));
    }
    else if(toolsType == "Xware")
    {
        QTimer::singleShot(100,this,SLOT(getXwareURLOrBtInfo()));
    }

    emit sGettingInfo(true);
}

QString DataControler::getFileURL()
{
    return fileURL;
}

QString DataControler::getFileNameList()
{
    return fileNameList;
}

QString DataControler::getMaxSpeed()
{
    return maxSpeed;
}

QString DataControler::getFreeSpace()
{
    return freeSpace;
}

QString DataControler::getToolsType()
{
    return toolsType;
}

QString DataControler::getDefaultToolType()
{
    return defaultTool;
}

bool DataControler::getIsYouGetEnable()
{
    return isYouGetEnable;
}

bool DataControler::getIsAria2Enable()
{
    return isAria2Enable;
}

bool DataControler::getIsXwareEnable()
{
    return isXwareEnable;
}

QString DataControler::getFileSavePath()
{
    return fileSavePath;
}

QString DataControler::getMaxThread()
{
    return maxThread;
}

void DataControler::setFileURL(QString URL)
{
    if (fileURL == URL)
        return;
    fileURL = URL;
    emit sFileURLChange();
}

void DataControler::setFileNameList(QString nameList)
{
    fileNameList = nameList;
    emit sFileNameListChange();
    emit sGettingInfo(false);
}

void DataControler::setFileSavePath(QString path)
{
    fileSavePath = path;
    emit sFileSavePathChange();
}

void DataControler::setMaxSpeed(QString speed)
{
    maxSpeed = speed;
    emit sMaxSpeedChange();
}

void DataControler::setMaxThread(QString count)
{
    maxThread = count;
    emit sMaxThreadChange();
}

void DataControler::setFreeSpace(QString space)
{
    freeSpace = space;
    emit sFreeSpaceChange();
}

void DataControler::setToolsType(QString tType)
{
    toolsType = tType;
    //加延时是因为在初始化的时候获取到toolType界面却还未完成初始化
    QTimer::singleShot(500,this,SIGNAL(sToolsTypeChange()));
}

void DataControler::setDefaultToolType(QString tType)
{
    defaultTool = tType;
    emit sDefaultToolTypeChange();
}

void DataControler::setIsYouGetEnable(bool flag)
{
    isYouGetEnable = flag;
    emit sIsYouGetEnableChange();
}

void DataControler::setIsAria2Enable(bool flag)
{
    isAria2Enable = flag;
    emit sIsAria2EnableChange();
}

void DataControler::setIsXwareEnable(bool flag)
{
    isXwareEnable = flag;
    emit sIsXwareEnableChange();
}

QString DataControler::getLinuxFreeSpace(QString path)
{
    struct statfs lv_fs;

    if(statfs(path.toUtf8().data(),&lv_fs) < 0)
    {
        return "";
    }
    else
    {
        double freeSize = lv_fs.f_bfree * lv_fs.f_bsize; // Byte
        if (freeSize > 1024 * 1024 * 1024)//GB
            return QString::number(freeSize / 1024 / 1024 / 1024,'f',1) + "GB";
        else if (freeSize > 1024 * 1024)//MB
            return QString::number(freeSize / 1024 / 1024,'f',1) + "MB";
        else if (freeSize > 1024)//KB
            return QString::number(freeSize/ 1024,'f',1) + "KB";
        else
            return QString::number(freeSize,'f',1) + "B";
    }
}

void DataControler::getURLInfoFromYouget()
{

    //该指针指向另外一个被启动的程序，所以 绝对不能被delete
    yougetProcess = new QProcess(this);
    connect(yougetProcess,SIGNAL(readyReadStandardOutput()),this,SLOT(getYougetFeedBack()));
    connect(yougetProcess,SIGNAL(readyReadStandardError()),
            this,SLOT(getYougetError()));

    SettingXMLHandler tmpHandler;
    QStringList arguments;
    arguments << tmpHandler.getChildElement(YouGetSetting,"ExecutePath");
    arguments << "-i";
    arguments << fileURL;
    yougetProcess->start("python3",arguments);

}

void DataControler::getURLInfo()
{
    if (fileURL.contains("http://") || fileURL.contains("https"))
    {
        //必须要执行getHttpFileTypeSize函数，保证如果有重定向可以根据重定向后再去取名字
        QString tmpNameList = getHttpFileTypeSize(fileURL) + QString(ITEM_INFO_SPLIT_CHAR);
        QString tmpName =  getHttpFtpFileName(redirectURL);
        tmpNameList += tmpName;

        setFileNameList(tmpNameList);
        emit sFnishGetAllInfo();
    }

//    else if (fileURL.contains(".torrent"))
//    {

//    }
    //确保toolsType参数在获取信息后一定会发送至界面
    setToolsType(toolsType);
}

void DataControler::getYougetFeedBack()
{
    QString outPut = QString(yougetProcess->readAllStandardOutput());
    if (outPut == "")
        return;

    QStringList tmpList;
    if (outPut.contains("streams"))
        tmpList = getMovieYouGetFeedBackInfo(outPut);
    else
        tmpList = getNormalYouGetFeedBackInfo(outPut);

    if (tmpList.count() != 3)
        return;

     setFileNameList(tmpList.at(0) + ITEM_INFO_SPLIT_CHAR + tmpList.at(1) + ITEM_INFO_SPLIT_CHAR + tmpList.at(2));
     emit sFnishGetAllInfo();
}

void DataControler::getYougetError()
{
    qDebug() << "++++++++++++++++++++++++++++++++++++++++++++++++++";
    qDebug() << yougetProcess->readAllStandardError();
    qDebug() << "++++++++++++++++++++++++++++++++++++++++++++++++++";
}

QStringList DataControler::getNormalYouGetFeedBackInfo(QString data)
{
    QStringList tmpList;
    int siteIndex = data.indexOf("Video Site:");
    int titleIndex = data.indexOf("Title:");
    int typeIndex = data.indexOf("Type:");
    int sizeIndex = data.indexOf("Size:");

    QString tmpSite = data.mid(siteIndex + 11, titleIndex - siteIndex - 11);
    QString tmpTitle = data.mid(titleIndex + 12,typeIndex - titleIndex - 13);
    fileType = data.mid(data.indexOf("(") + 1,data.indexOf(")") - data.indexOf("(") - 1);
    QString tmpSize = data.mid(sizeIndex + 12,data.lastIndexOf("(") - sizeIndex - 17);

    //youget返回大小都以MiB计算,要转换成B
    tmpSize = QString::number(qint64(tmpSize.toDouble() * 1024 * 1024));

    tmpList.append(fileType);
    tmpList.append(tmpSize);
    tmpList.append(tmpTitle);

    return tmpList;
}

QStringList DataControler::getMovieYouGetFeedBackInfo(QString data)
{
    QStringList tmpList;

    QString tmpSite = data.mid(data.indexOf("site:") + 20, data.indexOf("title:") - data.indexOf("site") - 21);
    QString tmpTitle = data.mid(data.indexOf("title:") + 20,data.indexOf("streams:") - data.indexOf("title:") - 21);
    fileType = data.mid(data.indexOf("container:") + 15,data.indexOf("video-profile:") - data.indexOf("container:") - 22);
    QString tmpSize = data.mid(data.indexOf("size:") + 15,data.indexOf("MiB") - data.indexOf("size:") - 16);

    //youget返回大小都以MiB计算,要转换成B
    tmpSize = QString::number(qint64(tmpSize.toDouble() * 1024 * 1024));

    tmpList.append(fileType);
    tmpList.append(tmpSize);
    tmpList.append(tmpTitle);

    return tmpList;
}

bool isConnected = false;
void DataControler::mainProgramStarted()
{
    //要保证主程序启动才连接到主程序
    if (!isConnected)
    {
        localSocket = new QLocalSocket();
        connectToMainProgram();
        isConnected = true;
    }
}


QString DataControler::getHttpFileTypeSize(QString URL)
{
    qint64 totalSize = -1;
    fileType = "";
    QNetworkAccessManager * tmpManager = new QNetworkAccessManager;
    QNetworkRequest headReq(URL);
    headReq.setRawHeader("User-Agent", "");  //Content-Length

    QNetworkReply*  headReply = NULL;
    bool connectError = false;
    int tryTimes = 1;
    //如果失败,连接尝试1次;
    do{
        connectError = false;
        headReply =  tmpManager->head(headReq);
        if(!headReply)
        {
            connectError = true;
            continue;
        }
        QEventLoop loop;
        connect(headReply, SIGNAL(finished()), &loop, SLOT(quit()));
        loop.exec();
        connectError = (headReply->error() != QNetworkReply::NoError);
        if(connectError)
        {
            qDebug()<<"connect failed!";
            qDebug() << headReply->errorString();
        }
        headReply->deleteLater();
    } while (connectError && --tryTimes);

    int statusCode = headReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

//    qDebug() << statusCode;
    if(statusCode == 302)
    {
        QUrl newUrl = headReply->header(QNetworkRequest::LocationHeader).toUrl();
        if(newUrl.isValid())
        {
//            qDebug()<<"Redirect："<<newUrl;
            URL = newUrl.toString();
            redirectURL = URL;
            return getHttpFileTypeSize(URL);
        }
    }

    else
    {
        totalSize = headReply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
        fileType =  headReply->header(QNetworkRequest::ContentTypeHeader).toString();
    }
    headReply->deleteLater();
    delete tmpManager;

    return fileType + ITEM_INFO_SPLIT_CHAR + QString::number(totalSize);

}

QString DataControler::getFtpFileTypeSize(QString URL)
{
    return "unknown@unknown";
}

QString DataControler::getIconName()
{
    QString headerString = fileType.mid(0, fileType.indexOf("/"));
    QString behindString = fileType.mid(fileType.indexOf("/") + 1, fileType.length());

    QString iconName  = "noicon";
    if (headerString == "application")
    {
        if (behindString.contains("pdf"))
        {
            //MIME类型@资源文件别名
            iconName = "pdf";
        }
        else if (behindString.contains("msword"))
            iconName = "ms-doc";
        else if (behindString.contains("excel"))
            iconName = "ms-xls";
        else if (behindString.contains("powerpoint"))
            iconName = "ms-ppt";
        else if (behindString.contains("octet-stream"))
            iconName = "binary";
        else if (behindString.contains("x-") ||
                 behindString.contains("zip") ||
                 behindString.contains("tar") ||
                 behindString.contains("rar"))
            iconName = "archive";
        else
            iconName = "noicon";
    }
    else if (behindString.contains("adobe"))
        iconName = "adobe";
    else if (headerString == "image")
        iconName = "image";
    else if (headerString == "audio")
        iconName = "audio";
    else if (headerString == "video")
        iconName = "video";
    else if (headerString == "text" && behindString == "plain")
        iconName = "text";
    else
        iconName = "noicon";

    //some video site,you-get will got this two type
    if (fileType == "flv" || fileType == "mp4")
    {
        iconName = "video";
    }

    return iconName;
}

QString DataControler::getHttpFtpFileName(QString URL)
{
    QRegExp rx;
    rx.setPatternSyntax(QRegExp::RegExp);
    rx.setCaseSensitivity(Qt::CaseSensitive); //大小写敏感
    rx.setPattern(QString("[^/]*$"));
    int pos = URL.indexOf(rx);

    if ( pos >= 0 )
    {
        QString tmpStr = rx.capturedTexts().at(0);
        rx.setPattern(QString("\.[^?@#$%&]+"));
        tmpStr.indexOf(rx);
        QString tmpName = rx.capturedTexts().at(0);

        if (tmpName == "")
            return "UnknownName";
        else
            return tmpName;
    }
    else
        return "";
}


void DataControler::startMainProgram()
{
    //每次启动前先尝试启动主程序
    QObject * mparent;
    QStringList arguments;
    arguments << "-c";

    //该指针指向另外一个被启动的程序，所以 绝对不能被delete
    QProcess * myProcess = new QProcess(mparent);
    myProcess->start(MAIN_PROGRAM_PATH,arguments);
    mainProgramStarted();
//    connect(myProcess,SIGNAL(started()),this,SLOT(mainProgramStarted()));
}

void DataControler::connectToMainProgram()
{
    // 服务端的serverNewConnectionHandler成员方法将被调用
    localSocket->connectToServer("PointURLServer");
    if (localSocket->waitForConnected())
    {
        qDebug() << "connect to main program success";
        return;
        // TODO:
    }
    else
    {
        //不成功就循环地尝试连接，这里要保证此函数在主程序启动后再调用
        connectToMainProgram();
    }
}

bool DataControler::checkIsInDownloading(QString URL)
{
    QList<SDownloading> tmpList = gDownloadHandler.getDownloadingNodes();

    for (int i = 0; i < tmpList.count(); i ++)
    {
        if (tmpList.at(i).URL == URL)//if exit,when user click ok button ,this window will just close and do nothing
        {
            return true;
        }
    }

    return false;//at the end got nothing will return false
}

bool DataControler::checkIsInDownloaded(QString URL)
{
    QList<SDownloaded> tmpList = gDownloadHandler.getDownloadedNodes();

    for (int i = 0; i < tmpList.count(); i ++)
    {
        if (tmpList.at(i).URL == URL)//if exit,when user click ok button ,this window will just close and do nothing
        {
            return true;
        }
    }

    return false;//at the end got nothing will return false
}

bool DataControler::checkIsInDownloadTrash(QString URL)
{
    QList<SDownloadTrash> tmpList = gDownloadHandler.getDownloadTrashNodes();

    for (int i = 0; i < tmpList.count(); i ++)
    {
        if (tmpList.at(i).URL == URL)//if exit,when user click ok button ,this window will just close and do nothing
        {
            return true;
        }
    }

    return false;//at the end got nothing will return false
}

bool DataControler::isXwareParseType(QString task)
{
    QString str = QString("^(ftp|magnet|ed2k|thunder|mms|rtsp)?:.*");
    QRegExp rex(str);
    return rex.exactMatch(task);
}

void DataControler::getXwareURLOrBtInfo()
{
    if (urlInfoGeter->isRunning())
        return;
    else
    {
        delete urlInfoGeter ;
        urlInfoGeter = new URLInfoGeter(localSocket,fileURL,0);
        connect(urlInfoGeter, SIGNAL(sGetAllFileInfo(QString)),
                this ,SLOT(receiveXwareNameInfo(QString)),Qt::UniqueConnection);
        urlInfoGeter->start();
    }
}

void DataControler::receiveXwareNameInfo(QString nameList)
{
    fileType = "application/x-gzip";
    setFileNameList(nameList);
}


QString DataControler::getDLToolsTypeFromURL(QString URL)
{
    QRegExp rx;
    rx.setPatternSyntax(QRegExp::RegExp);

    QString normalURLRegex = QString("^((https|http|chrome)?://)")
            + QString(".*\.(exe|asf|avi|exe|iso|mp3|mpeg|mpg|mpga|ra|rar|rm|rmvb|tar|wma|wmp|wmv|mov|zip|3gp|")
            + QString("chm|mdf|torrent|jar|msi|arj|bin|dll|psd|hqx|sit|lzh|gz|tgz|xlsx|xls|doc|docx|ppt|pptx|flv|swf|mkv|")
            + QString("tp|ts|flac|ape|wav|aac|txt|dat|7z|ttf|bat|xv|xvx|pdf|mp4|apk|ipa|epub|mobi|deb|sisx|cab|pxl|run|rpm|deb|dmg)")
            + QString("($|[?]{1}.*$)");

    QString videoURLRegex = QString("^(http://www.tudou.com/|") +
            QString("http://v.yinyuetai.com/|") +
            QString("http://v.youku.com/| ")+
            QString(" http://v.ku6.com/|")+
            QString("http://v.163.com/|") +
            QString("http://v.qq.com/|") +
            QString("http://www.acfun.com/v/|")+
            QString("http://bilibili.kankanews.com/video/av|")+
            QString("http://www.jpopsuki.tv/video/|")+
            QString("http://video.sina.com.cn/|")+
            QString("http://tv.sohu.com/|")+
            QString("http://www.56.com/w|")+
            QString("http://www.56.com/u|")+
            QString("http://www.songtaste.com/song/).+");

    rx.setPattern(normalURLRegex);
    int normalPos = URL.indexOf(rx);

    if (normalPos >= 0)
        return "Point";

    rx.setPattern(videoURLRegex);
    int videoPos = URL.indexOf(rx);

    if (videoPos >= 0)
        return "YouGet";

    if(isXwareParseType(URL))
    {
        return "Xware";
    }

        return "";                  //排除两种可能性外,就是不合法的链接
}

QString DataControler::mergeFileNameList(QString nameList)
{
    if (!nameList.contains(NAME_LIST_SPLIT_CHAR))
        return nameList;

    qint64 totalSize = 0;
    qint64 maxSize = 0;
    QString maxName = "";
    bool getDirName = false;
    QStringList itemList = nameList.split(NAME_LIST_SPLIT_CHAR);
    for (int i = 0; i < itemList.size(); i++)
    {
        QStringList infoList = itemList.at(i).split(ITEM_INFO_SPLIT_CHAR);
        if (infoList.size() != 3)
            continue;
        else
        {
            qint64 tmpSize = infoList.at(1).toLongLong();
            totalSize += tmpSize;
            if (tmpSize == -1)
            {
                getDirName = true;
                maxName = infoList.at(2);
                totalSize += 1;
            }
            if (!getDirName)
            {
                maxName = maxSize > tmpSize?maxName:infoList.at(2);
                maxSize = maxSize > tmpSize?maxSize:tmpSize;
            }
        }
    }

    return "application/x-gzip" + ITEM_INFO_SPLIT_CHAR + QString::number(totalSize) +
            ITEM_INFO_SPLIT_CHAR + maxName;
}





