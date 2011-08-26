// Fast HTTP module for FFmpeg
// Author: Max Schwarz <Max@x-quadraht.de>

#include "io_http.h"

#include <stdlib.h>

#include <QtCore/QUrl>
#include <QtNetwork/QTcpSocket>

struct IOHTTPContext
{
	AVIOContext* avio;
	QUrl url;
	QTcpSocket* socket;
	
	int64_t file_size;
};

static bool io_http_connect(IOHTTPContext* ctx, QTcpSocket* sock, int64_t offset = -1)
{
	sock->connectToHost(ctx->url.host(), ctx->url.port(80));
	if(!sock->waitForConnected(-1))
	{
		fprintf(stderr, "io_http: Could not connect to URL '%s': %s\n",
			ctx->url.toString().toAscii().constData(),
			sock->errorString().toAscii().constData()
		);
		return false;
	}
	
	QString request_tpl(
		"GET %1 HTTP/1.1\r\n"
		"Host: %2\r\n"
	);
	
	QString request;
	request += request_tpl
		.arg(QString(ctx->url.encodedPath()))
		.arg(QString(ctx->url.encodedHost()));
		
	if(offset != -1)
		request += QString("Range: bytes=%1-\r\n").arg(offset);
	
	request += "\r\n";
	
	if(sock->write(request.toAscii()) <= 0)
		return false;
	
	if(!sock->waitForReadyRead(-1))
		return false;
	
	QString response = sock->readLine();
	if(!response.endsWith("200 OK\r\n") && !response.endsWith("206 Partial Content\r\n"))
	{
		fprintf(stderr, "io_http: unknown response '%s'\n",
			response.toAscii().constData()
		);
		return false;
	}
	
	if(ctx->file_size == -1)
	{
		QString header_line;
		do
		{
			header_line = sock->readLine();
			
			if(offset == -1 && header_line.startsWith("Content-Length", Qt::CaseInsensitive))
			{
				QString arg = header_line.section(":", -1).trimmed();
				ctx->file_size = arg.toLongLong();
			}
		}
		while(header_line != "\r\n");
	}
	else
	{
		QByteArray header_line;
		do
			header_line = sock->readLine();
		while(header_line != "\r\n");
	}
	
	return true;
}

static int io_http_read_packet(void* opaque, uint8_t* buf, int buf_size)
{
	IOHTTPContext* d = (IOHTTPContext*)opaque;
	
	if(!d->socket->waitForReadyRead(100*1000))
	{
		fprintf(stderr, "io_http: Could not wait for new data\n",
			d->socket->errorString().toAscii().constData()
		);
		
		if(d->socket->state() != QAbstractSocket::ConnectedState)
			return 0;
		else
			return -1;
	}
	
	return d->socket->read((char*)buf, buf_size);
}

static int64_t io_http_seek(void* opaque, int64_t offset, int whence)
{
	IOHTTPContext* d = (IOHTTPContext*)opaque;
	
	printf("seek: offset = %20lld, whence = %d\n", offset, whence);
	
	if(whence == AVSEEK_SIZE)
	{
		printf("File size is %lld\n", d->file_size);
		return d->file_size;
	}
	
	QTcpSocket* new_socket = new QTcpSocket;
	if(!io_http_connect(d, new_socket, offset))
		return -1;
	
	
	d->socket->disconnectFromHost();
	if(d->socket->state() == QAbstractSocket::ConnectedState && !d->socket->waitForDisconnected(-1))
		fprintf(stderr, "io_http: Could not disconnect\n");
	
	delete d->socket;
	d->socket = new_socket;
	
	return 0;
}

AVIOContext* io_http_create(const char* str_url)
{
	const int BUFSIZE = 4096;
	unsigned char* buf = (unsigned char*)av_malloc(BUFSIZE);
	
	IOHTTPContext* d = new IOHTTPContext;
	
	d->file_size = -1;
	d->socket = new QTcpSocket();
	d->url = QUrl(str_url);
	if(!io_http_connect(d, d->socket))
	{
		delete d;
		return 0;
	}
	
	AVIOContext* ctx = avio_alloc_context(
		buf, BUFSIZE,
		0,
		(void*)d,
		&io_http_read_packet,
		NULL,
		&io_http_seek
	);
	
	d->avio = ctx;
	
	return ctx;
}
