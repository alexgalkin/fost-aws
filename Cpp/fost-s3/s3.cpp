/*
    Copyright 2009-2014, Felspar Co Ltd. http://fost.3.felspar.com/
    Distributed under the Boost Software License, Version 1.0.
    See accompanying file LICENSE_1_0.txt or copy at
        http://www.boost.org/LICENSE_1_0.txt
*/


#include "fost-aws.hpp"
#include <fost/insert>
#include <fost/s3.hpp>

#include <boost/lambda/bind.hpp>


using namespace fostlib;
using namespace fostlib::aws::s3;


/*
    fostlib::aws::s3::bucket
*/

const setting< string > fostlib::aws::s3::bucket::s_account_name(
    "fost-internet/Cpp/fost-aws/s3.cpp",
    "Amazon S3", "Default account name", "default", true);

namespace {
    std::auto_ptr< http::user_agent::response > s3do(
        const http::user_agent &ua, http::user_agent::request &request
    ) {
        std::auto_ptr< http::user_agent::response > response = ua(request);
        if ( response->status() == 403 ) {
            exceptions::not_implemented exception("S3 response");
            insert(exception.data(), "ua", "base", ua.base());
            json rj;
            for (mime::mime_headers::const_iterator it(request.headers().begin());
                    it != request.headers().end(); ++it) {
                insert(rj, "headers", it->first, it->second.value());
            }
            insert(exception.data(), "request", rj);
            insert(exception.data(), "request", "url", request.address());
            rj = json();
            insert(rj, "status", response->status());
            insert(rj, "body", "size", response->body()->data().size());
            insert(rj, "body", "data",
                coerce<string>(response->body()->data()));
            for (mime::mime_headers::const_iterator it(response->headers().begin());
                    it != response->headers().end(); ++it) {
                insert(rj, "headers", it->first, it->second.value());
            }
            insert(exception.data(), "response", rj);
            throw exception;
        }
        return response;
    }
    url base_url(const ascii_printable_string &bucket) {
        nullable<string> base(account_setting< string >(
            aws::s3::bucket::s_account_name.value(),  L"Base URL",  null));
        return url(url(base.value("https://s3.amazonaws.com/")),
            url::filepath_string(bucket + "/"));
    }
}


fostlib::aws::s3::bucket::bucket(const ascii_printable_string &name)
: m_ua(base_url(name)), name( name ) {
    m_ua.authentication(boost::function< void ( http::user_agent::request & ) >(
            boost::lambda::bind(
                rest_authentication, s_account_name.value(), name, boost::lambda::_1)));
}


file_info fostlib::aws::s3::bucket::stat(const boost::filesystem::wpath &location) const {
    return file_info(m_ua, name(), location);
}


void fostlib::aws::s3::bucket::put(const boost::filesystem::wpath &file, const boost::filesystem::wpath &location) const {
    http::user_agent::request request("PUT", url(m_ua.base(), location), file);
    std::auto_ptr< http::user_agent::response > response(s3do(m_ua, request));
    switch ( response->status() ) {
        case 200:
            break;
        default:
            exceptions::not_implemented exception(L"fostlib::aws::s3::bucket::put(const boost::filesystem::wpath &file, const boost::filesystem::wpath &location) const -- with response status " + fostlib::coerce< fostlib::string >( response->status() ));
            exception.info() << response->body() << std::endl;
            throw exception;
    }
}


/*
    fostlib::aws::s3::file_info
*/


namespace {
    std::auto_ptr< http::user_agent::response > init_file_info(const http::user_agent &ua, const url &u) {
        http::user_agent::request r("HEAD", u);
        return s3do(ua, r);
    }
}
fostlib::aws::s3::file_info::file_info(const http::user_agent &ua, const ascii_printable_string &bucket, const boost::filesystem::wpath &location )
: m_response( init_file_info(ua, url(ua.base(), location)).release() ), path( location ) {
    switch ( m_response->status() ) {
        case 200:
        case 404:
            break;
        default:
            throw fostlib::exceptions::not_implemented("fostlib::aws::s3::file_info::file_info( const fostlib::aws::s3::bucket &bucket, const boost::filesystem::wpath &location ) -- with status code", fostlib::coerce< fostlib::string >( m_response->status() ));
    }
}


bool fostlib::aws::s3::file_info::exists() const {
    return m_response->status() == 200;
}
nullable< string > fostlib::aws::s3::file_info::md5() const {
    if ( exists() && m_response->body()->headers().exists(L"ETag") )
        return m_response->body()->headers()[L"ETag"].value();
    else
        return null;
}
