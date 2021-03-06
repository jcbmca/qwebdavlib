/****************************************************************************
** QWebDAV Library (qwebdavlib) - LGPL v2.1
**
** HTTP Extensions for Web Distributed Authoring and Versioning (WebDAV)
** from June 2007
**      http://tools.ietf.org/html/rfc4918
**
** Web Distributed Authoring and Versioning (WebDAV) SEARCH
** from November 2008
**      http://tools.ietf.org/html/rfc5323
**
** Missing:
**      - LOCK support
**      - process WebDAV SEARCH responses
**
** Copyright (C) 2012 Martin Haller <martin.haller@rebnil.com>
** for QWebDAV library (qwebdavlib) version 1.0
**      https://github.com/mhaller/qwebdavlib
**
** Copyright (C) 2012 Timo Zimmermann <meedav@timozimmermann.de>
** for portions from QWebdav plugin for MeeDav (LGPL v2.1)
**      http://projects.developer.nokia.com/meedav/
**
** Copyright (C) 2009-2010 Corentin Chary <corentin.chary@gmail.com>
** for portions from QWebdav - WebDAV lib for Qt4 (LGPL v2.1)
**      http://xf.iksaif.net/dev/qwebdav.html
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** for naturalCompare() (LGPL v2.1)
**      http://qt.gitorious.org/qt/qt/blobs/4.7/src/gui/dialogs/qfilesystemmodel.cpp
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Library General Public
** License as published by the Free Software Foundation; either
** version 2 of the License, or (at your option) any later version.
**
** This library is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** Library General Public License for more details.
**
** You should have received a copy of the GNU Library General Public License
** along with this library; see the file COPYING.LIB.  If not, write to
** the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
** Boston, MA 02110-1301, USA.
**
** http://www.gnu.org/licenses/lgpl-2.1-standalone.html
**
****************************************************************************/

#include "qwebdav.h"

QWebdav::QWebdav (QObject *parent) : QObject(parent)
  ,m_username()
  ,m_password()
  ,m_baseUrl()
  ,m_authenticator_lastReply(0)
  ,m_sslCertDigestMd5("")
  ,m_sslCertDigestSha1("")

{
    qRegisterMetaType<QNetworkReply*>("QNetworkReply*");

    connect(&m_nam, SIGNAL(authenticationRequired(QNetworkReply*,QAuthenticator*)), this, SLOT(provideAuthenication(QNetworkReply*,QAuthenticator*)));
    connect(&m_nam, SIGNAL(sslErrors(QNetworkReply*,QList<QSslError>)), this, SLOT(sslErrors(QNetworkReply*,QList<QSslError>)));
}

QWebdav::~QWebdav()
{
}

QString QWebdav::hostname() const
{
    return m_baseUrl.host();
}

int QWebdav::port() const
{
    return m_baseUrl.port();
}

QString QWebdav::rootPath() const
{
    return m_baseUrl.path();
}

QString QWebdav::username() const
{
    return m_username;
}

QString QWebdav::password() const
{
    return m_password;
}

bool QWebdav::isSSL() const
{
    return m_baseUrl.scheme() == QLatin1String("https");
}

void QWebdav::setConnectionSettings(const QWebdavConnectionType connectionType,
                                    const QString& hostname,
                                    const QString& rootPath,
                                    const QString& username,
                                    const QString& password,
                                    int port,
                                    const QString &sslCertDigestMd5,
                                    const QString &sslCertDigestSha1)
{
    QUrl url;

    QString uriScheme;
    switch (connectionType)
    {
    case QWebdav::HTTP:
        uriScheme = "http";
        break;
    case QWebdav::HTTPS:
        uriScheme = "https";
        break;
    }

    url.setScheme(uriScheme);
    url.setHost(hostname);
    url.setPath(rootPath);

    if (port != 0) {

        // use user-defined port number - TG: WHAT THE FUCK???
        if ( ! ( ( (port == 80) && (connectionType==QWebdav::HTTP) ) ||
               ( (port == 443) && (connectionType==QWebdav::HTTPS) ) ) )
            url.setPort(port);
    }
    setConnectionSettings(url, username, password, sslCertDigestMd5, sslCertDigestSha1);

}

void QWebdav::setConnectionSettings(const QUrl &url, const QString &username, const QString &password, const QString &sslCertDigestMd5, const QString &sslCertDigestSha1)
{
    m_baseUrl = url;
    m_sslCertDigestMd5 = hexToDigest(sslCertDigestMd5);
    m_sslCertDigestSha1 = hexToDigest(sslCertDigestSha1);

    m_username = username;
    m_password = password;
}

void QWebdav::acceptSslCertificate(const QString &sslCertDigestMd5,
                                   const QString &sslCertDigestSha1)
{
    m_sslCertDigestMd5 = hexToDigest(sslCertDigestMd5);
    m_sslCertDigestSha1 = hexToDigest(sslCertDigestSha1);
}

void QWebdav::provideAuthenication(QNetworkReply *reply, QAuthenticator *authenticator)
{
#ifdef DEBUG_WEBDAV
    qDebug() << "QWebdav::authenticationRequired()";
    QVariantHash opts = authenticator->options();
    QVariant optVar;
    foreach(optVar, opts) {
        qDebug() << "QWebdav::authenticationRequired()  option == " << optVar.toString();
    }
#endif

    if (reply == m_authenticator_lastReply) {
        reply->abort();
        emit errorChanged("WebDAV server requires authentication. Check WebDAV share settings!");
        reply->deleteLater();
        reply=0;
    }

    m_authenticator_lastReply = reply;

    authenticator->setUser(m_username);
    authenticator->setPassword(m_password);
}

void QWebdav::sslErrors(QNetworkReply *reply, const QList<QSslError> &errors)
{
#ifdef DEBUG_WEBDAV
    qDebug() << "QWebdav::sslErrors()   reply->url == " << reply->url().toString(QUrl::RemoveUserInfo);
#endif

    QSslCertificate sslcert = errors[0].certificate();

    if ( ( sslcert.digest(QCryptographicHash::Md5) == m_sslCertDigestMd5 ) &&
         ( sslcert.digest(QCryptographicHash::Sha1) == m_sslCertDigestSha1) )
    {
        // user accepted this SSL certifcate already ==> ignore SSL errors
        reply->ignoreSslErrors();
    } else {
        // user has to check the SSL certificate and has to accept manually
        emit checkSslCertifcate(errors);
        reply->abort();
    }
}

QString QWebdav::digestToHex(const QByteArray &input)
{
    QByteArray inputHex = input.toHex();

    QString result = "";
    for (int i = 0; i<inputHex.size(); i+=2) {
        result.append(inputHex.at(i));
        result.append(inputHex.at(i+1));
        result.append(":");
    }
    result.chop(1);
    result = result.toUpper();

    return result;
}

QByteArray QWebdav::hexToDigest(const QString &input)
{
    QByteArray result;
    int i = 2;
    int l = input.size();
    result.append(input.left(2).toLatin1());
    while ((i<l) && (input.at(i) == ':')) {
        ++i;
        result.append(input.mid(i,2).toLatin1());
        i+=2;
    }
    return QByteArray::fromHex(result);
}

QString QWebdav::absolutePath(const QString &relPath)
{
    return QString(rootPath() + relPath);

}

QUrl QWebdav::urlForPath(const QString &path)
{
    QUrl ret(m_baseUrl);
    ret.setPath(absolutePath(path));
    return ret;
}

QNetworkReply* QWebdav::createRequest(const char* method, QNetworkRequest req, QIODevice* outgoingData)
{
    if(outgoingData != 0 && outgoingData->size() !=0) {
        req.setHeader(QNetworkRequest::ContentLengthHeader, outgoingData->size());
        req.setHeader(QNetworkRequest::ContentTypeHeader, "text/xml; charset=utf-8");
    }

#ifdef DEBUG_WEBDAV
    qDebug() << " QWebdav::createWebdavRequest1";
    qDebug() << "   " << method << " " << req.url().toString();
    QList<QByteArray> rawHeaderList = req.rawHeaderList();
    QByteArray rawHeaderItem;
    foreach(rawHeaderItem, rawHeaderList) {
        qDebug() << "   " << rawHeaderItem << ": " << req.rawHeader(rawHeaderItem);
    }
#endif

    return m_nam.sendCustomRequest(req, method, outgoingData);
}

QNetworkReply* QWebdav::createRequest(const char* method, QNetworkRequest req, const QByteArray& outgoingData )
{
    QBuffer* dataIO = new QBuffer;
    dataIO->setData(outgoingData);
    dataIO->open(QIODevice::ReadOnly);

#ifdef DEBUG_WEBDAV
    qDebug() << " QWebdav::createWebdavRequest2";
    qDebug() << "   " << method << " " << req.url().toString();
    QList<QByteArray> rawHeaderList = req.rawHeaderList();
    QByteArray rawHeaderItem;
    foreach(rawHeaderItem, rawHeaderList) {
        qDebug() << "   " << rawHeaderItem << ": " << req.rawHeader(rawHeaderItem);
    }
#endif

    QNetworkReply* reply = createRequest(method, req, dataIO);
    dataIO->setParent(reply);
    return reply;
}

QNetworkReply* QWebdav::list(const QString& path)
{
#ifdef DEBUG_WEBDAV
    qDebug() << "QWebdav::list() path = " << path;
#endif
    return list(path, 1);
}

QNetworkReply* QWebdav::list(const QString& path, int depth)
{
    QWebdav::PropNames query;
    QStringList props;

    // Small set of properties
    // href in response contains also the name
    // e.g. /container/front.html
    props << "getlastmodified";         // http://www.webdav.org/specs/rfc4918.html#PROPERTY_getlastmodified
    // e.g. Mon, 12 Jan 1998 09:25:56 GMT
    props << "getcontentlength";        // http://www.webdav.org/specs/rfc4918.html#PROPERTY_getcontentlength
    // e.g. "4525"
    props << "resourcetype";            // http://www.webdav.org/specs/rfc4918.html#PROPERTY_resourcetype
    // e.g. "collection" for a directory

#ifdef QWEBDAVITEM_EXTENDED_PROPERTIES
    // Following properties are available as well.
    props << "creationdate";          // http://www.webdav.org/specs/rfc4918.html#PROPERTY_creationdate
    // e.g. "1997-12-01T18:27:21-08:00"
    props << "displayname";           // http://www.webdav.org/specs/rfc4918.html#PROPERTY_displayname
    // e.g. "Example HTML resource"
    props << "getcontentlanguage";    // http://www.webdav.org/specs/rfc4918.html#PROPERTY_getcontentlanguage
    // e.g. "en-US"
    props << "getcontenttype";        // http://www.webdav.org/specs/rfc4918.html#PROPERTY_getcontenttype
    // e.g "text/html"
    props << "getetag";               // http://www.webdav.org/specs/rfc4918.html#PROPERTY_getetag
    // e.g. "zzyzx"
    // Additionally, there are also properties for locking
#endif

    query["DAV:"] = props;

    return propfind(path, query, depth);
}

QNetworkReply* QWebdav::search(const QString& path, const QString& q )
{
    QByteArray query;
    QTextStream str(&query);
    str.setCodec("UTF-8");
    str<<"<?xml version=\"1.0\"?>\r\n"
         "<D:searchrequest xmlns:D=\"DAV:\">\r\n";
    str<<q;
    str<<"</D:searchrequest>\r\n";
    str.flush();

    return createRequest("SEARCH", QNetworkRequest(urlForPath(path)), query);
}

QNetworkReply* QWebdav::get(const QString& path)
{

#ifdef DEBUG_WEBDAV
    qDebug() << "QWebdav::get() url = " << req.url().toString(QUrl::RemoveUserInfo);
#endif

    return m_nam.get(QNetworkRequest(urlForPath(path)));
}

QNetworkReply* QWebdav::get(const QString& path, QIODevice* data)
{
    return get(path, data, 0);
}

QNetworkReply* QWebdav::get(const QString& path, QIODevice* data, quint64 fromRangeInBytes)
{

    QNetworkRequest req(urlForPath(path));

#ifdef DEBUG_WEBDAV
    qDebug() << "QWebdav::get() url = " << req.url().toString(QUrl::RemoveUserInfo);
#endif

    if (fromRangeInBytes>0) {
        // RFC2616 http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html
        QByteArray fromRange = "bytes=" + QByteArray::number(fromRangeInBytes) + "-";   // byte-ranges-specifier
        req.setRawHeader("Range",fromRange);
    }

    QNetworkReply* reply = m_nam.get(req);
    connect(reply, &QNetworkReply::readyRead, [reply, data](){
        //if (reply->bytesAvailable() < 256000)//WTH? where does this constant come from?
        //    return;

        data->write(reply->readAll());

    });
    connect(reply, &QNetworkReply::finished, [reply, data] () {
        data->write(reply->readAll());
        data->close();//WTF
        delete data;//WTF
    });
    connect(reply, (void(QNetworkReply::*)(QNetworkReply::NetworkError))&QNetworkReply::error, [this, reply, data] (QNetworkReply::NetworkError err) {
        if(err == QNetworkReply::OperationCanceledError) {
            data->close();//WTF
            delete data;//WTF
        }
        emit errorChanged(reply->errorString());
    });

    return reply;
}

QNetworkReply* QWebdav::put(const QString& path, QIODevice* data)
{
#ifdef DEBUG_WEBDAV
    qDebug() << "QWebdav::put() url = " << req.url().toString(QUrl::RemoveUserInfo);
#endif

    return m_nam.put(QNetworkRequest(urlForPath(path)), data);
}

QNetworkReply* QWebdav::put(const QString& path, const QByteArray& data)
{  
#ifdef DEBUG_WEBDAV
    qDebug() << "QWebdav::put() url = " << req.url().toString(QUrl::RemoveUserInfo);
#endif

    return m_nam.put(QNetworkRequest(urlForPath(path)), data);
}


QNetworkReply* QWebdav::propfind(const QString& path, const QWebdav::PropNames& props, int depth)
{
    QByteArray query;
    QTextStream str(&query);
    str.setCodec("UTF-8");

    str<<"<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
         "<D:propfind xmlns:D=\"DAV:\" >"
         "<D:prop>";
    QMapIterator<QString, QStringList> iter(props);
    while(iter.hasNext()) {
        iter.next();
        QString ns(iter.key());
        foreach(const QString& key, iter.value()) {
            if (ns == "DAV:")
                str<<"<D:"<<key<<"/>";
            else
                str<<"<"<<key<<" xmlns=\""<<ns<<"\"/>";
        }
    }
    str<<"</D:prop>"
         "</D:propfind>";
    str.flush();
    return propfind(path, query, depth);
}


QNetworkReply* QWebdav::propfind(const QString& path, const QByteArray& query, int depth)
{
    QNetworkRequest req(urlForPath(path));
    req.setRawHeader("Depth", depth == 2 ? QString("infinity").toUtf8() : QString::number(depth).toUtf8());

    return createRequest("PROPFIND", req, query);
}

QNetworkReply* QWebdav::proppatch(const QString& path, const QWebdav::PropValues& props)
{
    QByteArray query;
    QTextStream str(&query);
    str.setCodec("UTF-8");

    str<<"<?xml version=\"1.0\" encoding=\"utf-8\" ?>"
         "<D:proppatch xmlns:D=\"DAV:\" >"
         "<D:prop>";
    QMapIterator<PropValues::key_type, PropValues::mapped_type> propIter(props);
    while(propIter.hasNext()) {
        propIter.next();
        QString ns(propIter.key());
        QMapIterator<PropValues::mapped_type::key_type, PropValues::mapped_type::mapped_type> iter(propIter.value());
        while(iter.hasNext()) {
            iter.next();
            if (ns == "DAV:") {
                str<<"<D:"<<iter.key()<<">";
                str<<iter.value().toString();
                str<<"</D:"<<iter.key()<<">" ;
            } else {
                str<<"<"<<iter.key()<<" xmlns=\""<<ns<<"\">";
                str<<iter.value().toString();
                str<<"</"<<iter.key()<<" xmlns=\""<<ns<<"\"/>";
            }
        }
    }
    str<<"</D:prop>"
         "</D:propfind>";
    str.flush();
    return proppatch(path, query);
}

QNetworkReply* QWebdav::proppatch(const QString& path, const QByteArray& query)
{
    return createRequest("PROPPATCH", QNetworkRequest(urlForPath(path)), query);
}

QNetworkReply* QWebdav::mkdir (const QString& path)
{
    return createRequest("MKCOL", QNetworkRequest(urlForPath(path)));
}

QNetworkReply* QWebdav::copy(const QString& pathFrom, const QString& pathTo, bool overwrite)
{
    QNetworkRequest req(urlForPath(pathFrom));

    // RFC4918 Section 10.3 requires an absolute URI for destination raw header
    //  http://tools.ietf.org/html/rfc4918#section-10.3
    // RFC3986 Section 4.3 specifies the term absolute URI
    //  http://tools.ietf.org/html/rfc3986#section-4.3
    QUrl dstUrl(m_baseUrl);
    //dstUrl.setUserInfo("");
    dstUrl.setPath(absolutePath(pathTo));
    req.setRawHeader("Destination", dstUrl.toString().toUtf8());

    req.setRawHeader("Depth", "infinity");
    req.setRawHeader("Overwrite", overwrite ? "T" : "F");

    return createRequest("COPY", req);
}

QNetworkReply* QWebdav::move(const QString& pathFrom, const QString& pathTo, bool overwrite)
{
    QNetworkRequest req(urlForPath(pathFrom));

    // RFC4918 Section 10.3 requires an absolute URI for destination raw header
    //  http://tools.ietf.org/html/rfc4918#section-10.3
    // RFC3986 Section 4.3 specifies the term absolute URI
    //  http://tools.ietf.org/html/rfc3986#section-4.3
    QUrl dstUrl(m_baseUrl);
    //dstUrl.setUserInfo("");
    dstUrl.setPath(absolutePath(pathTo));
    req.setRawHeader("Destination", dstUrl.toString().toUtf8());

    req.setRawHeader("Depth", "infinity");
    req.setRawHeader("Overwrite", overwrite ? "T" : "F");

    return createRequest("MOVE", req);
}

QNetworkReply* QWebdav::remove(const QString& path)
{
    return createRequest("DELETE", QNetworkRequest(urlForPath(path)));
}
