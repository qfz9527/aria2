#include "HttpRequest.h"

#include <sstream>

#include <cppunit/extensions/HelperMacros.h>

#include "prefs.h"
#include "AuthConfigFactory.h"
#include "PiecedSegment.h"
#include "Piece.h"
#include "Range.h"
#include "Request.h"
#include "Option.h"
#include "array_fun.h"
#include "CookieStorage.h"
#include "util.h"
#include "AuthConfig.h"
#include "TestUtil.h"

namespace aria2 {

class HttpRequestTest : public CppUnit::TestFixture {

  CPPUNIT_TEST_SUITE(HttpRequestTest);
  CPPUNIT_TEST(testGetStartByte);
  CPPUNIT_TEST(testGetEndByte);
  CPPUNIT_TEST(testCreateRequest);
  CPPUNIT_TEST(testCreateRequest_ftp);
  CPPUNIT_TEST(testCreateRequest_with_cookie);
  CPPUNIT_TEST(testCreateRequest_query);
  CPPUNIT_TEST(testCreateRequest_head);
  CPPUNIT_TEST(testCreateRequest_ipv6LiteralAddr);
  CPPUNIT_TEST(testCreateRequest_endOffsetOverride);
  CPPUNIT_TEST(testCreateProxyRequest);
  CPPUNIT_TEST(testIsRangeSatisfied);
  CPPUNIT_TEST(testUserAgent);
  CPPUNIT_TEST(testAddHeader);
  CPPUNIT_TEST(testAcceptMetalink);
  CPPUNIT_TEST(testEnableAcceptEncoding);
  CPPUNIT_TEST(testConditionalRequest);
  CPPUNIT_TEST_SUITE_END();
private:
  SharedHandle<Option> option_;
  SharedHandle<AuthConfigFactory> authConfigFactory_;
public:
  void setUp()
  {
    option_.reset(new Option());
    option_->put(PREF_HTTP_AUTH_CHALLENGE, A2_V_TRUE);
    authConfigFactory_.reset(new AuthConfigFactory());
  }

  void testGetStartByte();
  void testGetEndByte();
  void testCreateRequest();
  void testCreateRequest_ftp();
  void testCreateRequest_with_cookie();
  void testCreateRequest_query();
  void testCreateRequest_head();
  void testCreateRequest_ipv6LiteralAddr();
  void testCreateRequest_endOffsetOverride();
  void testCreateProxyRequest();
  void testIsRangeSatisfied();
  void testUserAgent();
  void testAddHeader();
  void testAcceptMetalink();
  void testEnableAcceptEncoding();
  void testConditionalRequest();
};


CPPUNIT_TEST_SUITE_REGISTRATION( HttpRequestTest );

void HttpRequestTest::testGetStartByte()
{
  HttpRequest httpRequest;
  SharedHandle<Piece> p(new Piece(1, 1024));
  SharedHandle<Segment> segment(new PiecedSegment(1024, p));
  SharedHandle<FileEntry> fileEntry(new FileEntry("file", 1024*10, 0));

  CPPUNIT_ASSERT_EQUAL((int64_t)0LL, httpRequest.getStartByte());

  httpRequest.setSegment(segment);
  httpRequest.setFileEntry(fileEntry);
  
  CPPUNIT_ASSERT_EQUAL((int64_t)1024LL, httpRequest.getStartByte());
}

void HttpRequestTest::testGetEndByte()
{
  size_t index = 1;
  size_t length = 1024*1024-1024;
  size_t segmentLength = 1024*1024;

  HttpRequest httpRequest;
  SharedHandle<Piece> piece(new Piece(index, length));
  SharedHandle<Segment> segment(new PiecedSegment(segmentLength, piece));
  SharedHandle<FileEntry> fileEntry(new FileEntry("file", segmentLength*10, 0));

  CPPUNIT_ASSERT_EQUAL((int64_t)0LL, httpRequest.getEndByte());

  httpRequest.setSegment(segment);

  CPPUNIT_ASSERT_EQUAL((int64_t)0LL, httpRequest.getEndByte());

  SharedHandle<Request> request(new Request());
  request->supportsPersistentConnection(true);
  request->setPipeliningHint(true);

  httpRequest.setRequest(request);
  httpRequest.setFileEntry(fileEntry);

  CPPUNIT_ASSERT_EQUAL((int64_t)(segmentLength*index+length-1),
                       httpRequest.getEndByte());

  // The end byte of FileEntry are placed inside segment
  fileEntry->setLength(segmentLength+100);

  CPPUNIT_ASSERT_EQUAL((int64_t)(segmentLength*index+100-1),
                       httpRequest.getEndByte());

  request->setPipeliningHint(false);

  CPPUNIT_ASSERT_EQUAL((int64_t)0LL, httpRequest.getEndByte());
}

void HttpRequestTest::testCreateRequest()
{
  SharedHandle<Piece> p;

  SharedHandle<Request> request(new Request());
  request->supportsPersistentConnection(true);

  request->setUri("http://localhost:8080/archives/aria2-1.0.0.tar.bz2");

  p.reset(new Piece(0, 1024));
  SharedHandle<Segment> segment(new PiecedSegment(1024, p));
  SharedHandle<FileEntry> fileEntry(new FileEntry("file", 1024*1024*10, 0));

  HttpRequest httpRequest;
  httpRequest.disableContentEncoding();
  httpRequest.setRequest(request);
  httpRequest.setSegment(segment);
  httpRequest.setFileEntry(fileEntry);
  httpRequest.setAuthConfigFactory(authConfigFactory_, option_.get());

  // remove "Connection: close" and add end byte range
  request->setPipeliningHint(true);  
  
  std::string expectedText = "GET /archives/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
    "User-Agent: aria2\r\n"
    "Accept: */*\r\n"
    "Host: localhost:8080\r\n"
    "Pragma: no-cache\r\n"
    "Cache-Control: no-cache\r\n"
    "Range: bytes=0-1023\r\n"
    "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  request->setPipeliningHint(false);

  expectedText = "GET /archives/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
    "User-Agent: aria2\r\n"
    "Accept: */*\r\n"
    "Host: localhost:8080\r\n"
    "Pragma: no-cache\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n"
    "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  p.reset(new Piece(1, 1024*1024));
  segment.reset(new PiecedSegment(1024*1024, p));
  httpRequest.setSegment(segment);

  expectedText = "GET /archives/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
    "User-Agent: aria2\r\n"
    "Accept: */*\r\n"
    "Host: localhost:8080\r\n"
    "Pragma: no-cache\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n"
    "Range: bytes=1048576-\r\n"
    "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  request->setPipeliningHint(true);

  expectedText = "GET /archives/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
    "User-Agent: aria2\r\n"
    "Accept: */*\r\n"
    "Host: localhost:8080\r\n"
    "Pragma: no-cache\r\n"
    "Cache-Control: no-cache\r\n"
    "Range: bytes=1048576-2097151\r\n"
    "\r\n";
  
  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  // redirection set persistent connection flag to true
  request->redirectUri("http://localhost:8080/archives/download/aria2-1.0.0.tar.bz2");

  expectedText = "GET /archives/download/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
    "User-Agent: aria2\r\n"
    "Accept: */*\r\n"
    "Host: localhost:8080\r\n"
    "Pragma: no-cache\r\n"
    "Cache-Control: no-cache\r\n"
    "Range: bytes=1048576-2097151\r\n"
    "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  request->supportsPersistentConnection(true);
  request->setPipeliningHint(false);

  // this only removes "Connection: close".
  request->setKeepAliveHint(true);

  expectedText = "GET /archives/download/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
    "User-Agent: aria2\r\n"
    "Accept: */*\r\n"
    "Host: localhost:8080\r\n"
    "Pragma: no-cache\r\n"
    "Cache-Control: no-cache\r\n"
    "Range: bytes=1048576-\r\n"
    "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  request->setKeepAliveHint(false);

  request->resetUri();

  p.reset(new Piece(0, 1024*1024));
  segment.reset(new PiecedSegment(1024*1024, p));
  httpRequest.setSegment(segment);

  // enable http auth
  option_->put(PREF_HTTP_USER, "aria2user");
  option_->put(PREF_HTTP_PASSWD, "aria2passwd");

  CPPUNIT_ASSERT(authConfigFactory_->activateBasicCred
                 ("localhost", 8080, "/", option_.get()));

  expectedText = "GET /archives/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
    "User-Agent: aria2\r\n"
    "Accept: */*\r\n"
    "Host: localhost:8080\r\n"
    "Pragma: no-cache\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n"
    "Authorization: Basic YXJpYTJ1c2VyOmFyaWEycGFzc3dk\r\n"
    "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  // enable http proxy auth
  SharedHandle<Request> proxyRequest(new Request());
  CPPUNIT_ASSERT(proxyRequest->setUri
                 ("http://aria2proxyuser:aria2proxypasswd@localhost:9000"));
  httpRequest.setProxyRequest(proxyRequest);

  expectedText = "GET http://localhost:8080/archives/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
    "User-Agent: aria2\r\n"
    "Accept: */*\r\n"
    "Host: localhost:8080\r\n"
    "Pragma: no-cache\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n"
    "Proxy-Connection: close\r\n"
    "Proxy-Authorization: Basic YXJpYTJwcm94eXVzZXI6YXJpYTJwcm94eXBhc3N3ZA==\r\n"
    "Authorization: Basic YXJpYTJ1c2VyOmFyaWEycGFzc3dk\r\n"
    "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  request->setPipeliningHint(true);

  expectedText = "GET http://localhost:8080/archives/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
    "User-Agent: aria2\r\n"
    "Accept: */*\r\n"
    "Host: localhost:8080\r\n"
    "Pragma: no-cache\r\n"
    "Cache-Control: no-cache\r\n"
    "Range: bytes=0-1048575\r\n"
    "Proxy-Connection: Keep-Alive\r\n"
    "Proxy-Authorization: Basic YXJpYTJwcm94eXVzZXI6YXJpYTJwcm94eXBhc3N3ZA==\r\n"
    "Authorization: Basic YXJpYTJ1c2VyOmFyaWEycGFzc3dk\r\n"
    "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  request->setPipeliningHint(false);

  // turn off proxy auth
  CPPUNIT_ASSERT(proxyRequest->setUri("http://localhost:9000"));

  expectedText = "GET http://localhost:8080/archives/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
    "User-Agent: aria2\r\n"
    "Accept: */*\r\n"
    "Host: localhost:8080\r\n"
    "Pragma: no-cache\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n"
    "Proxy-Connection: close\r\n"
    "Authorization: Basic YXJpYTJ1c2VyOmFyaWEycGFzc3dk\r\n"
    "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());  
}

void HttpRequestTest::testCreateRequest_ftp()
{
  option_->put(PREF_FTP_USER, "aria2user");
  option_->put(PREF_FTP_PASSWD, "aria2passwd");

  SharedHandle<Request> request(new Request());
  request->setUri("ftp://localhost:8080/archives/aria2-1.0.0.tar.bz2");

  SharedHandle<Request> proxyRequest(new Request());
  CPPUNIT_ASSERT(proxyRequest->setUri
                 ("http://localhost:9000"));

  HttpRequest httpRequest;
  SharedHandle<Piece> p(new Piece(0, 1024*1024));
  SharedHandle<Segment> segment
    (new PiecedSegment(1024*1024, p));
  SharedHandle<FileEntry> fileEntry(new FileEntry("file", 1024*1024*10, 0));

  httpRequest.disableContentEncoding();
  httpRequest.setRequest(request);
  httpRequest.setSegment(segment);
  httpRequest.setFileEntry(fileEntry);
  httpRequest.setAuthConfigFactory(authConfigFactory_, option_.get());
  httpRequest.setProxyRequest(proxyRequest);

  std::string expectedText =
    "GET ftp://aria2user@localhost:8080/archives/aria2-1.0.0.tar.bz2"
    " HTTP/1.1\r\n"
    "User-Agent: aria2\r\n"
    "Accept: */*\r\n"
    "Host: localhost:8080\r\n"
    "Pragma: no-cache\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n"
    "Proxy-Connection: close\r\n"
    "Authorization: Basic YXJpYTJ1c2VyOmFyaWEycGFzc3dk\r\n"
    "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  // test proxy authorization
  CPPUNIT_ASSERT(proxyRequest->setUri
                 ("http://aria2proxyuser:aria2proxypasswd@localhost:9000"));

  expectedText =
    "GET ftp://aria2user@localhost:8080/archives/aria2-1.0.0.tar.bz2"
    " HTTP/1.1\r\n"
    "User-Agent: aria2\r\n"
    "Accept: */*\r\n"
    "Host: localhost:8080\r\n"
    "Pragma: no-cache\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n"
    "Proxy-Connection: close\r\n"
    "Proxy-Authorization: Basic YXJpYTJwcm94eXVzZXI6YXJpYTJwcm94eXBhc3N3ZA==\r\n"
    "Authorization: Basic YXJpYTJ1c2VyOmFyaWEycGFzc3dk\r\n"
    "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());
}

void HttpRequestTest::testCreateRequest_with_cookie()
{
  SharedHandle<Request> request(new Request());
  request->setUri("http://localhost/archives/aria2-1.0.0.tar.bz2");
  SharedHandle<Piece> p(new Piece(0, 1024*1024));
  SharedHandle<Segment> segment
    (new PiecedSegment(1024*1024, p));
  SharedHandle<FileEntry> fileEntry(new FileEntry("file", 1024*1024*10, 0));

  Cookie cookies[] = {
    createCookie("name1", "value1", "localhost", true, "/archives", false),
    createCookie("name2", "value2", "localhost", true,
                 "/archives/download",  false),
    createCookie("name3", "value3", "aria2.org", false,
                 "/archives/download",  false),
    createCookie("name4", "value4", "aria2.org", false, "/archives/", true),
    createCookie("name5", "value5", "example.org", false, "/", false)
  };
  SharedHandle<CookieStorage> st(new CookieStorage());
  for(size_t i = 0; i < A2_ARRAY_LEN(cookies); ++i) {
    CPPUNIT_ASSERT(st->store(cookies[i], 0));
  }

  HttpRequest httpRequest;

  httpRequest.disableContentEncoding();
  httpRequest.setRequest(request);
  httpRequest.setSegment(segment);
  httpRequest.setFileEntry(fileEntry);
  httpRequest.setCookieStorage(st);
  httpRequest.setAuthConfigFactory(authConfigFactory_, option_.get());

  std::string expectedText = "GET /archives/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
    "User-Agent: aria2\r\n"
    "Accept: */*\r\n"
    "Host: localhost\r\n"
    "Pragma: no-cache\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n"
    "Cookie: name1=value1;\r\n"
    "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  request->setUri("http://localhost/archives/download/aria2-1.0.0.tar.bz2");

  expectedText = "GET /archives/download/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
    "User-Agent: aria2\r\n"
    "Accept: */*\r\n"
    "Host: localhost\r\n"
    "Pragma: no-cache\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n"
    "Cookie: name2=value2;name1=value1;\r\n"
    "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  request->setUri("http://www.aria2.org/archives/download/aria2-1.0.0.tar.bz2");

  expectedText = "GET /archives/download/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
    "User-Agent: aria2\r\n"
    "Accept: */*\r\n"
    "Host: www.aria2.org\r\n"
    "Pragma: no-cache\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n"
    "Cookie: name3=value3;\r\n"
    "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  request->setUri("https://www.aria2.org/archives/download/"
                  "aria2-1.0.0.tar.bz2");

  expectedText = "GET /archives/download/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
    "User-Agent: aria2\r\n"
    "Accept: */*\r\n"
    "Host: www.aria2.org\r\n"
    "Pragma: no-cache\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n"
    "Cookie: name3=value3;name4=value4;\r\n"
    "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  // The path of cookie4 ends with '/'
  request->setUri("https://www.aria2.org/archives/aria2-1.0.0.tar.bz2");

  expectedText = "GET /archives/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
    "User-Agent: aria2\r\n"
    "Accept: */*\r\n"
    "Host: www.aria2.org\r\n"
    "Pragma: no-cache\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n"
    "Cookie: name4=value4;\r\n"
    "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());  

  request->setUri("http://example.org/aria2-1.0.0.tar.bz2");

  expectedText = "GET /aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
    "User-Agent: aria2\r\n"
    "Accept: */*\r\n"
    "Host: example.org\r\n"
    "Pragma: no-cache\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n"
    "Cookie: name5=value5;\r\n"
    "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());
}

void HttpRequestTest::testCreateRequest_query()
{
  SharedHandle<Request> request(new Request());
  request->setUri("http://localhost/wiki?id=9ad5109a-b8a5-4edf-9373-56a1c34ae138");
  HttpRequest httpRequest;
  httpRequest.disableContentEncoding();
  httpRequest.setRequest(request);
  httpRequest.setAuthConfigFactory(authConfigFactory_, option_.get());

  std::string expectedText =
    "GET /wiki?id=9ad5109a-b8a5-4edf-9373-56a1c34ae138 HTTP/1.1\r\n"
    "User-Agent: aria2\r\n"
    "Accept: */*\r\n"
    "Host: localhost\r\n"
    "Pragma: no-cache\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n"
    "\r\n";
  
  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());
}

void HttpRequestTest::testCreateRequest_head()
{
  SharedHandle<Request> request(new Request());
  request->setMethod(Request::METHOD_HEAD);
  request->setUri("http://localhost/aria2-1.0.0.tar.bz2");

  HttpRequest httpRequest;
  httpRequest.setRequest(request);
  httpRequest.setAuthConfigFactory(authConfigFactory_, option_.get());
  
  std::stringstream result(httpRequest.createRequest());
  std::string line;
  CPPUNIT_ASSERT(getline(result, line));
  line = util::strip(line);
  CPPUNIT_ASSERT_EQUAL(std::string("HEAD /aria2-1.0.0.tar.bz2 HTTP/1.1"), line);
}

void HttpRequestTest::testCreateRequest_endOffsetOverride()
{
  SharedHandle<Request> request(new Request());
  request->setUri("http://localhost/myfile");
  HttpRequest httpRequest;
  httpRequest.disableContentEncoding();
  httpRequest.setRequest(request);
  httpRequest.setAuthConfigFactory(authConfigFactory_, option_.get());
  SharedHandle<Piece> p(new Piece(0, 1024*1024));
  SharedHandle<Segment> segment(new PiecedSegment(1024*1024, p));
  httpRequest.setSegment(segment);
  httpRequest.setEndOffsetOverride(1024*1024*1024);
  SharedHandle<FileEntry> fileEntry
    (new FileEntry("file", 1024*1024*1024*10LL, 0));
  httpRequest.setFileEntry(fileEntry);
  // End byte is not passed if start byte is 0.
  std::string expectedText =
    "GET /myfile HTTP/1.1\r\n"
    "User-Agent: aria2\r\n"
    "Accept: */*\r\n"
    "Host: localhost\r\n"
    "Pragma: no-cache\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n"
    "\r\n";
  
  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  segment->updateWrittenLength(1);

  expectedText =
    "GET /myfile HTTP/1.1\r\n"
    "User-Agent: aria2\r\n"
    "Accept: */*\r\n"
    "Host: localhost\r\n"
    "Pragma: no-cache\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n"
    "Range: bytes=1-1073741823\r\n"
    "\r\n";
  
  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());
}

void HttpRequestTest::testCreateProxyRequest()
{
  SharedHandle<Request> request(new Request());
  request->setUri("http://localhost/archives/aria2-1.0.0.tar.bz2");
  SharedHandle<Piece> p(new Piece(0, 1024*1024));
  SharedHandle<Segment> segment(new PiecedSegment(1024*1024, p));

  SharedHandle<Request> proxyRequest(new Request());
  CPPUNIT_ASSERT(proxyRequest->setUri("http://localhost:9000"));

  HttpRequest httpRequest;

  httpRequest.setRequest(request);
  httpRequest.setSegment(segment);
  httpRequest.setProxyRequest(proxyRequest);

  request->supportsPersistentConnection(true);

  std::string expectedText = "CONNECT localhost:80 HTTP/1.1\r\n"
    "User-Agent: aria2\r\n"
    "Host: localhost:80\r\n"
    //"Proxy-Connection: close\r\n"
    "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createProxyRequest());

  // adds Keep-Alive header.
  request->setKeepAliveHint(true);

  expectedText = "CONNECT localhost:80 HTTP/1.1\r\n"
    "User-Agent: aria2\r\n"
    "Host: localhost:80\r\n"
    //"Proxy-Connection: Keep-Alive\r\n"
    "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createProxyRequest());

  request->setKeepAliveHint(false);
  // pipelining also adds Keep-Alive header.
  request->setPipeliningHint(true);

  expectedText = "CONNECT localhost:80 HTTP/1.1\r\n"
    "User-Agent: aria2\r\n"
    "Host: localhost:80\r\n"
    //"Proxy-Connection: Keep-Alive\r\n"
    "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createProxyRequest());

  // test proxy authorization
  CPPUNIT_ASSERT(proxyRequest->setUri
                 ("http://aria2proxyuser:aria2proxypasswd@localhost:9000"));

  expectedText = "CONNECT localhost:80 HTTP/1.1\r\n"
    "User-Agent: aria2\r\n"
    "Host: localhost:80\r\n"
    //"Proxy-Connection: Keep-Alive\r\n"
    "Proxy-Authorization: Basic YXJpYTJwcm94eXVzZXI6YXJpYTJwcm94eXBhc3N3ZA==\r\n"
    "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createProxyRequest());
}

void HttpRequestTest::testIsRangeSatisfied()
{
  SharedHandle<Request> request(new Request());
  request->supportsPersistentConnection(true);
  request->setUri("http://localhost:8080/archives/aria2-1.0.0.tar.bz2");
  request->setPipeliningHint(false); // default: false
  SharedHandle<Piece> p(new Piece(0, 1024*1024));
  SharedHandle<Segment> segment(new PiecedSegment(1024*1024, p));
  SharedHandle<FileEntry> fileEntry(new FileEntry("file", 0, 0));

  HttpRequest httpRequest;

  httpRequest.setRequest(request);
  httpRequest.setSegment(segment);
  httpRequest.setFileEntry(fileEntry);

  SharedHandle<Range> range(new Range());

  CPPUNIT_ASSERT(httpRequest.isRangeSatisfied(range));

  p.reset(new Piece(1, 1024*1024));
  segment.reset(new PiecedSegment(1024*1024, p));
  httpRequest.setSegment(segment);

  CPPUNIT_ASSERT(!httpRequest.isRangeSatisfied(range));

  uint64_t entityLength = segment->getSegmentLength()*10;

  range.reset(new Range(segment->getPosition(), 0, entityLength));

  CPPUNIT_ASSERT(httpRequest.isRangeSatisfied(range));

  fileEntry->setLength(entityLength-1);

  CPPUNIT_ASSERT(!httpRequest.isRangeSatisfied(range));

  fileEntry->setLength(entityLength);

  CPPUNIT_ASSERT(httpRequest.isRangeSatisfied(range));

  request->setPipeliningHint(true);

  CPPUNIT_ASSERT(!httpRequest.isRangeSatisfied(range));

  range.reset(new Range(segment->getPosition(),
                        segment->getPosition()+segment->getLength()-1,
                        entityLength));

  CPPUNIT_ASSERT(httpRequest.isRangeSatisfied(range));

  range.reset(new Range(0, segment->getPosition()+segment->getLength()-1,
                        entityLength));

  CPPUNIT_ASSERT(!httpRequest.isRangeSatisfied(range));
}

void HttpRequestTest::testUserAgent()
{
  SharedHandle<Request> request(new Request());
  request->setUri("http://localhost:8080/archives/aria2-1.0.0.tar.bz2");

  //SharedHandle<Piece> p(new Piece(0, 1024));
  //SharedHandle<Segment> segment(new PiecedSegment(1024, p));

  HttpRequest httpRequest;
  httpRequest.disableContentEncoding();
  httpRequest.setRequest(request);
  //httpRequest.setSegment(segment);
  httpRequest.setUserAgent("aria2 (Linux)");
  httpRequest.setAuthConfigFactory(authConfigFactory_, option_.get());

  std::string expectedText = "GET /archives/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
    "User-Agent: aria2 (Linux)\r\n"
    "Accept: */*\r\n"
    "Host: localhost:8080\r\n"
    "Pragma: no-cache\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n"
    "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  SharedHandle<Request> proxyRequest(new Request());
  CPPUNIT_ASSERT(proxyRequest->setUri("http://localhost:9000"));
  
  httpRequest.setProxyRequest(proxyRequest);

  std::string expectedTextForProxy = "CONNECT localhost:8080 HTTP/1.1\r\n"
    "User-Agent: aria2 (Linux)\r\n"
    "Host: localhost:8080\r\n"
    //"Proxy-Connection: close\r\n"
    "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedTextForProxy, httpRequest.createProxyRequest());
}

void HttpRequestTest::testAddHeader()
{
  SharedHandle<Request> request(new Request());
  request->setUri("http://localhost/archives/aria2-1.0.0.tar.bz2");

  HttpRequest httpRequest;
  httpRequest.disableContentEncoding();
  httpRequest.setRequest(request);
  httpRequest.setAuthConfigFactory(authConfigFactory_, option_.get());
  httpRequest.addHeader("X-ARIA2: v0.13\nX-ARIA2-DISTRIBUTE: enabled\n");
  httpRequest.addHeader("Accept: text/html");
  std::string expectedText = "GET /archives/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
    "User-Agent: aria2\r\n"
    "Host: localhost\r\n"
    "Pragma: no-cache\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n"
    "X-ARIA2: v0.13\r\n"
    "X-ARIA2-DISTRIBUTE: enabled\r\n"
    "Accept: text/html\r\n"
    "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());
}

void HttpRequestTest::testAcceptMetalink()
{
  SharedHandle<Request> request(new Request());
  request->setUri("http://localhost/archives/aria2-1.0.0.tar.bz2");

  HttpRequest httpRequest;
  httpRequest.disableContentEncoding();
  httpRequest.setRequest(request);
  httpRequest.setAuthConfigFactory(authConfigFactory_, option_.get());
  httpRequest.setAcceptMetalink(true);

  std::string expectedText =
    "GET /archives/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
    "User-Agent: aria2\r\n"
    "Accept: */*,application/metalink4+xml,application/metalink+xml\r\n"
    "Host: localhost\r\n"
    "Pragma: no-cache\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n"
    "\r\n";

  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());
}

void HttpRequestTest::testEnableAcceptEncoding()
{
  SharedHandle<Request> request(new Request());
  request->setUri("http://localhost/archives/aria2-1.0.0.tar.bz2");

  HttpRequest httpRequest;
  httpRequest.setRequest(request);
  httpRequest.setAuthConfigFactory(authConfigFactory_, option_.get());

  std::string acceptEncodings;
#ifdef HAVE_ZLIB
  acceptEncodings += "deflate, gzip";
#endif // HAVE_ZLIB
  
  std::string expectedTextHead =
    "GET /archives/aria2-1.0.0.tar.bz2 HTTP/1.1\r\n"
    "User-Agent: aria2\r\n"
    "Accept: */*\r\n";
  std::string expectedTextTail =
    "Host: localhost\r\n"
    "Pragma: no-cache\r\n"
    "Cache-Control: no-cache\r\n"
    "Connection: close\r\n"
    "\r\n";

  std::string expectedText = expectedTextHead;
  expectedText += expectedTextTail;
  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());

  expectedText = expectedTextHead;
  if(!acceptEncodings.empty()) {
    expectedText += "Accept-Encoding: ";
    expectedText += acceptEncodings;
    expectedText += "\r\n";
  }
  expectedText += expectedTextTail;

  httpRequest.enableAcceptGZip();
  CPPUNIT_ASSERT_EQUAL(expectedText, httpRequest.createRequest());
}

void HttpRequestTest::testCreateRequest_ipv6LiteralAddr()
{
  SharedHandle<Request> request(new Request());
  request->setUri("http://[::1]/path");
  HttpRequest httpRequest;
  httpRequest.disableContentEncoding();
  httpRequest.setRequest(request);
  httpRequest.setAuthConfigFactory(authConfigFactory_, option_.get());

  CPPUNIT_ASSERT(httpRequest.createRequest().find("Host: [::1]") != std::string::npos);

  SharedHandle<Request> proxy(new Request());
  proxy->setUri("http://proxy");
  httpRequest.setProxyRequest(proxy);
  std::string proxyRequest = httpRequest.createProxyRequest();
  CPPUNIT_ASSERT(proxyRequest.find("Host: [::1]:80") != std::string::npos);
  CPPUNIT_ASSERT(proxyRequest.find("CONNECT [::1]:80 ") != std::string::npos);
}

void HttpRequestTest::testConditionalRequest()
{
  HttpRequest httpRequest;
  CPPUNIT_ASSERT(!httpRequest.conditionalRequest());
  httpRequest.setIfModifiedSinceHeader("dummy");
  CPPUNIT_ASSERT(httpRequest.conditionalRequest());
  httpRequest.setIfModifiedSinceHeader("");
  CPPUNIT_ASSERT(!httpRequest.conditionalRequest());
  httpRequest.addHeader("If-None-Match: *");
  CPPUNIT_ASSERT(httpRequest.conditionalRequest());
  httpRequest.clearHeader();
  httpRequest.addHeader("If-Modified-Since: dummy");
  CPPUNIT_ASSERT(httpRequest.conditionalRequest());
}

} // namespace aria2
