#ifndef UPNPCONNECTION_H
#define UPNPCONNECTION_H

#include "upnqt_global.h"
#include <QObject>
#include <QUdpSocket>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QNetworkAddressEntry>
#include <QHostAddress>
#include <QTimer>
#include <QTextStream>
#include <QDateTime>
#include <QXmlStreamReader>

class UPNQTSHARED_EXPORT UPNPConnection : public QObject
{
    Q_OBJECT
public:
    enum State {
        Opened = 0,
        NotOpened,
        Closed
    };
private:
    QNetworkAddressEntry localAddress;
    int internalPort;
    QHostAddress externalAddress;
    int externalPort;
    QString info;
    QString pcol;
private:
    QHostAddress gateway;
    QUrl gatewayCtrlUrl;
    QString ctrlPort;
    QNetworkAccessManager *http_socket;
    QNetworkReply *http_reply;
    QUdpSocket *udp_socket;
private:
    QTimer *timer;
    State conn_state;
    int waitTime;

public:
    explicit UPNPConnection(QNetworkAddressEntry &local, QObject *parent = 0);
    ~UPNPConnection();
public:
    void makeTunnel(int internal, int external, QString protocol, QString text = "Tunnel ");
    void setTunnel();
    void removeTunnel();
    State getState() const;
    QHostAddress getExternalAddress() const;

private:
    void getExternalIP();
    void checkTunnels();
    void extractExternalIP(QString message);
    void extractUPNPError(QString message);
    void postSOAP(QString action, QString message);
public slots:

private slots:
    void getUdp();
    void processReq(QNetworkReply *reply);
    void getHttp();
    void getHttpError(QNetworkReply::NetworkError err);
    void timeExpired();
signals:
    void success();
    void stageSucceded(QString stage);
    void udpResponse();
    void extAddressExtracted();
    void upnp_error(QString message);
};

#endif // UPNPCONNECTION_H
