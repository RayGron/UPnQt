#include "upnpconnection.h"

QHostAddress UPNPConnection::getExternalAddress() const
{
    return externalAddress;
}

UPNPConnection::UPNPConnection(QNetworkAddressEntry &local, QObject *parent) : QObject(parent)
{
    conn_state = State::NotOpened;
    localAddress = local;
    waitTime = 1000;
    udp_socket = new QUdpSocket();
    http_socket = new QNetworkAccessManager();
    http_reply = NULL;
    timer = new QTimer(this);
    udp_socket->bind(localAddress.ip(), 1900);
    QObject::connect(udp_socket, SIGNAL(readyRead()), this, SLOT(getUdp()));
    QObject::connect(timer, SIGNAL(timeout()), this, SLOT(timeExpired()));
}

UPNPConnection::~UPNPConnection()
{
    QObject::disconnect(udp_socket, SIGNAL(readyRead()), this, SLOT(getUdp()));
    QObject::disconnect(timer, SIGNAL(timeout()), this, SLOT(timeExpired()));
}

void UPNPConnection::makeTunnel(int internal, int external, QString protocol, QString text)
{
    if ((protocol == "TCP")||(protocol == "UDP"))
    {
        externalPort = external;
        internalPort = internal;
        info = text;
        pcol = protocol;
        QString discover_string = QString("M-SEARCH * HTTP/1.1\r\nHOST:239.255.255.250:1900\r\n")
                                 +QString("ST:urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n"/*upnp:rootdevice*/)
                                 +QString("Man:\"ssdp:discover\"\r\n")
                                 +QString("MX:3\r\n\r\n");
        QObject::connect(udp_socket, SIGNAL(readyRead()), this, SLOT(getUdp()));
        QObject::connect(timer, SIGNAL(timeout()), this, SLOT(timeExpired()));
        udp_socket->writeDatagram(discover_string.toLatin1(),discover_string.size(),localAddress.broadcast(), 1900);
        timer->start(waitTime);
    }
    else
    {
        emit upnp_error("Invalid protocol");
    }
}

void UPNPConnection::getUdp()
{
    QString vs;
    while (udp_socket->hasPendingDatagrams())
    {
        QByteArray datagram;
        datagram.resize(udp_socket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;
        udp_socket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
        QHostAddress st_addr(sender.toIPv4Address());
        if (st_addr != localAddress.ip())
        {
            timer->stop();
            gateway = st_addr;
            emit stageSucceded(st_addr.toString() + ": " + QString(senderPort) + ";\n" + datagram.data() + "\n");
            vs = datagram.data();
            int index = vs.indexOf("LOCATION: ");
            if (index != -1)
            {
                vs.remove(0, index + 10);
                index = 0;
                while (vs[index].isPrint())
                {
                    index++;
                }
                vs.remove(index, vs.size() - index);
                QString sport = vs;
                sport.remove(0, 5);
                index = 0;
                while (sport[index] != QChar(':'))
                {
                    index++;
                }
                sport.remove(0, index+1);
                index = 0;
                while (sport[index] != QChar('/'))
                {
                    index++;
                }
                sport.remove(index, sport.size() - index);
                ctrlPort = sport;
                QObject::connect(http_socket, SIGNAL(finished(QNetworkReply*)), this, SLOT(processReq(QNetworkReply*)));
                http_socket->get(QNetworkRequest(QUrl(vs)));
                udp_socket->close();
            }
        }
    }
}

void UPNPConnection::timeExpired()
{
    QObject::disconnect(udp_socket, SIGNAL(readyRead()), this, SLOT(getUdp()));
    QObject::disconnect(timer, SIGNAL(timeout()), this, SLOT(timeExpired()));
    timer->stop();
    emit upnp_error("Time expired!");
}

void UPNPConnection::processReq(QNetworkReply *reply)
{
    QObject::disconnect(http_socket, SIGNAL(finished(QNetworkReply*)), this, SLOT(processReq(QNetworkReply*)));
    QString response = reply->readAll();
    int i = 0;
    while (i < response.size()) {
        if (!response[i].isPrint())
            response.remove(i, 1);
        else
            i++;
    }
    QXmlStreamReader reader(response);
    reader.readNext();
    while (!reader.atEnd())
    {
        if (reader.name().toString() == QString("serviceType"))
        {
            if (reader.readElementText() == QString("urn:schemas-upnp-org:service:WANIPConnection:1"))
            {
                while ((!reader.atEnd())&&(reader.name().toString() != "controlURL"))
                {
                    reader.readNext();
                }
                if (reader.name().toString() == "controlURL")
                {
                    gatewayCtrlUrl = QString("http://") + gateway.toString() + QString(":") + ctrlPort + reader.readElementText();

                    break;
                }
            }
            else
            {
                reader.readNext();
            }
        }
        else
        {
            reader.readNext();
        }
    }
    getExternalIP();
}

void UPNPConnection::getExternalIP()
{
    QString message("<?xml version=\"1.0\"?>\r\n"
                                   "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
                                   "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
                                   "<s:Body>\r\n<u:GetExternalIPAddress xmlns:u=\"urn:schemas-upnp-org:service:WANIPConnection:1\">\r\n"
                                   "</u:GetExternalIPAddress>\r\n</s:Body>\r\n</s:Envelope>\r\n");
    this->postSOAP("GetExternalIPAddress", message);
}

void UPNPConnection::postSOAP(QString action, QString message)
{
    emit stageSucceded(QString("POST: \nAction: ") + action + QString("\nMessage: ") + message + QString("\n\n"));
    QNetworkRequest req(gatewayCtrlUrl);
    req.setRawHeader(QByteArray("Host"), (gateway.toString() + QString(":") + ctrlPort).toLocal8Bit());
    req.setRawHeader(QByteArray("Content-Type"), QByteArray("text/xml; charset=\"utf-8\""));
    req.setRawHeader(QByteArray("Content-Length"), QString::number(message.size()).toLatin1());
    req.setRawHeader(QByteArray("Soapaction"), (QString("\"urn:schemas-upnp-org:service:WANIPConnection:1#")+action+QString("\"")).toLatin1());
    http_reply = http_socket->post(req,message.toLatin1());
    QObject::connect(http_reply, SIGNAL(readyRead()), this, SLOT(getHttp()));
    QObject::connect(http_reply, SIGNAL(error(QNetworkReply::NetworkError)),
                     this, SLOT(getHttpError(QNetworkReply::NetworkError)));

}

void UPNPConnection::getHttp()
{
    QString reply = http_reply->readAll();
    QObject::disconnect(http_reply, SIGNAL(readyRead()), this, SLOT(getHttp()));
    QObject::disconnect(http_reply, SIGNAL(error(QNetworkReply::NetworkError)),
                     this, SLOT(getHttpError(QNetworkReply::NetworkError)));
    if (!reply.contains("UPnPError"))
    {
        if (reply.contains("<NewExternalIPAddress>"))
        {
            extractExternalIP(reply);
        }
        if (reply.contains("AddPortMappingResponse"))
        {
            conn_state = State::Opened;
            emit success();
        }
        if (reply.contains("DeletePortMappingResponse"))
        {
            conn_state = State::Closed;
        }
    }
    else
    {
        extractUPNPError(reply);
    }
    emit stageSucceded(reply + "\n");
}

void UPNPConnection::getHttpError(QNetworkReply::NetworkError err)
{
    emit upnp_error(http_reply->errorString());
}

void UPNPConnection::extractExternalIP(QString message)
{
    QXmlStreamReader reader(message);
    reader.readNext();
    while (reader.name().toString() != QString("NewExternalIPAddress"))
    {
        reader.readNext();
    }
    if (reader.name().toString() == QString("NewExternalIPAddress"))
    {
        externalAddress = QHostAddress(reader.readElementText());
    }
    emit extAddressExtracted();
    checkTunnels();
}

void UPNPConnection::extractUPNPError(QString message)
{
    QXmlStreamReader reader(message);
    reader.readNext();
    while (reader.name().toString() != QString("errorDescription"))
    {
        reader.readNext();
    }
    if (reader.name().toString() == QString("errorDescription"))
    {
        if (reader.readElementText() == QString("NoSuchEntryInArray"))
        {
            setTunnel();
        }
        else
        {
            emit upnp_error(reader.readElementText());
        }
    }
}

void UPNPConnection::checkTunnels()
{
    QString message("<?xml version=\"1.0\"?>\r\n"
                    "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
                    "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
                    "<s:Body>\r\n"
                    "<u:GetSpecificPortMappingEntry xmlns:u=\"urn:schemas-upnp-org:service:WANIPConnection:1\">\r\n"
                    "<NewRemoteHost></NewRemoteHost>"
                    "<NewExternalPort>");
    message+=QString::number(externalPort);
    message+=QString("</NewExternalPort>"
                    "<NewProtocol>");
              message+=pcol;
    message+=QString("</NewProtocol>"
                    "</u:GetSpecificPortMappingEntry>\r\n"
                    "</s:Body>\r\n"
                    "</s:Envelope>\r\n");
    this->postSOAP("GetSpecificPortMappingEntry", message);
}

void UPNPConnection::setTunnel()
{
    QString message("<?xml version=\"1.0\"?>\r\n"
                                    "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
                                    "<s:Body>\r\n"
                                    "<u:AddPortMapping xmlns:u=\"urn:schemas-upnp-org:service:WANIPConnection:1\">\r\n"
                                    "<NewRemoteHost></NewRemoteHost>"
                                    "<NewExternalPort>");
         message += QString::number(externalPort);
                message += QString("</NewExternalPort>"
                                    "<NewProtocol>");
                 message +=         pcol;
                 message += QString("</NewProtocol>"
                                    "<NewInternalPort>");
         message += QString::number(internalPort);
                 message += QString("</NewInternalPort>"
                                    "<NewInternalClient>");
         message += localAddress.ip().toString();
                 message += QString("</NewInternalClient>"
                                    "<NewEnabled>1</NewEnabled>"
                                    "<NewPortMappingDescription>");
                 message +=info+localAddress.ip().toString()+QString(":")+QString::number(internalPort);
                 message +=QString("</NewPortMappingDescription>"
                                    "<NewLeaseDuration>0</NewLeaseDuration>"
                                    "</u:AddPortMapping>\r\n"
                                    "</s:Body>\r\n"
                                    "</s:Envelope>\r\n");
    this->postSOAP("AddPortMapping", message);
}

void UPNPConnection::removeTunnel()
{
    QString message("<?xml version=\"1.0\"?>\r\n"
                                    "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
                                    "<s:Body>\r\n"
                                    "<u:DeletePortMapping xmlns:u=\"urn:schemas-upnp-org:service:WANIPConnection:1\">\r\n"
                                    "<NewRemoteHost></NewRemoteHost>"
                                    "<NewExternalPort>");
         message += QString::number(externalPort);
                message += QString("</NewExternalPort>"
                                    "<NewProtocol>");
                 message +=         pcol;
                 message += QString("</NewProtocol>"
                                    "</u:DeletePortMapping>\r\n"
                                    "</s:Body>\r\n"
                                    "</s:Envelope>\r\n");
    this->postSOAP("DeletePortMapping", message);
}

UPNPConnection::State UPNPConnection::getState() const
{
    return conn_state;
}
