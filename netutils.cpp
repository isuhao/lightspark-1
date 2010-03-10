/**************************************************************************
    Lightspark, a free flash player implementation

    Copyright (C) 2009  Alessandro Pignotti (a.pignotti@sssup.it)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**************************************************************************/

#include "netutils.h"
#include <curl/curl.h>

using namespace lightspark;

CurlDownloader::CurlDownloader(const tiny_string& u):buffer(NULL),len(0),tail(0),url(u),failed(false),waiting(false)
{
	sem_init(&available,0,0);
	sem_init(&mutex,0,1);
	setg((char*)buffer,(char*)buffer,(char*)buffer);
}

void CurlDownloader::execute()
{
	if(url.len()==0)
	{
		failed=true;
		return;
	}
	CURL *curl;
	CURLcode res;
	curl = curl_easy_init();
	if(curl)
	{
		curl_easy_setopt(curl, CURLOPT_URL, url.raw_buf());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_header);
		curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);
		res = curl_easy_perform(curl);
		if(res!=0)
			failed=true;
		curl_easy_cleanup(curl);
	}
	else
		failed=true;

	return;
}

bool CurlDownloader::download()
{
	return !failed;
}

size_t CurlDownloader::write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
	CurlDownloader* th=static_cast<CurlDownloader*>(userp);
	sem_wait(&th->mutex);
	memcpy(th->buffer + th->tail,buffer,size*nmemb);
	th->tail+=(size*nmemb);
	if(th->waiting)
		sem_post(&th->available);
	sem_post(&th->mutex);
	return size*nmemb;
}

CurlDownloader::int_type CurlDownloader::underflow()
{
	sem_wait(&mutex);
	assert(gptr()==egptr());
	unsigned int firstIndex=tail;

	if((buffer+tail)==(uint8_t*)egptr()) //We have no more bytes
	{
		waiting=true;
		sem_post(&mutex);
		sem_wait(&available);
	}
	
	//We have some bytes now, let's use them
	setg((char*)buffer,(char*)buffer,(char*)buffer+tail);
	sem_post(&mutex);

	//Cast to signed, otherwise 0xff would become eof
	return (unsigned char)buffer[firstIndex];
}

size_t CurlDownloader::write_header(void *buffer, size_t size, size_t nmemb, void *userp)
{
	CurlDownloader* th=static_cast<CurlDownloader*>(userp);
	char* headerLine=(char*)buffer;
	if(strncmp(headerLine,"Content-Length: ",16)==0)
	{
		//Now read the length and allocate the byteArray
		assert(th->buffer==NULL);
		th->len=atoi(headerLine+16);
		th->buffer=new uint8_t[th->len];
	}
	return size*nmemb;
}
