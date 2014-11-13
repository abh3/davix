/*
 * This File is part of Davix, The IO library for HTTP based protocols
 * Copyright (C) 2013  Adrien Devresse <adrien.devresse@cern.ch>, CERN
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
*/


#include <davix_internal.hpp>
#include "davix_tool_params.hpp"
#include "davix_tool_util.hpp"
#include <getopt.h>
#include <string_utils/stringutils.hpp>
#include <utils/davix_logger.hpp>

namespace Davix{

namespace Tool{


const std::string scope_params = "Davix::Tools::Params";


#define CAPATH_OPT          1000
#define DEBUG_OPT           1001
#define USER_LOGIN          1002
#define USER_PASSWORD       1003
#define DATA_CONTENT        1004
#define S3_SECRET_KEY       1005
#define S3_ACCESS_KEY       1006
#define X509_PRIVATE_KEY    1007
#define TRACE_OPTIONS       1008
#define REDIRECTION_OPT     1009
#define METALINK_OPT        1010
#define CONN_TIMEOUT        1011
#define TIMEOUT_OPS         1012

// LONG OPTS

#define COMMON_LONG_OPTIONS \
{"debug", no_argument, 0,  DEBUG_OPT }, \
{"header",  required_argument, 0,  'H' }, \
{"help", no_argument, 0,'?'}, \
{"metalink", required_argument, 0, METALINK_OPT }, \
{"module", required_argument, 0, 'P'}, \
{"proxy", required_argument, 0, 'x'}, \
{"redirection", required_argument, 0, REDIRECTION_OPT }, \
{"conn-timeout", required_argument, 0, CONN_TIMEOUT }, \
{"timeout", required_argument, 0, TIMEOUT_OPS }, \
{"trace", required_argument, 0, TRACE_OPTIONS }, \
{"verbose", no_argument, 0,  0 }, \
{"version", no_argument, 0, 'V'}

#define SECURITY_LONG_OPTIONS \
{"cert",  required_argument,       0, 'E' }, \
{"capath",  required_argument, 0, CAPATH_OPT }, \
{"key", required_argument, 0, X509_PRIVATE_KEY}, \
{"userlogin", required_argument, 0, USER_LOGIN}, \
{"userpass", required_argument, 0, USER_PASSWORD}, \
{"s3secretkey", required_argument, 0, S3_SECRET_KEY}, \
{"s3accesskey", required_argument, 0, S3_ACCESS_KEY}, \
{"insecure", no_argument, 0,  'k' }

#define REQUEST_LONG_OPTIONS \
{"request",  required_argument, 0,  'X' }, \
{"data", required_argument, 0, DATA_CONTENT}, \
{"verbose", no_argument, 0,  0 }

#define LISTING_LONG_OPTIONS \
{"long-list", no_argument, 0,  'l' }

OptParams::OptParams() :
    params(),
    vec_arg(),
    verbose(false),
    debug(false),
    req_type(),
    help_msg(),
    cred_path(),
    priv_key(),
    output_file_path(),
    input_file_path(),
    userlogpasswd(),
    req_content(),
    aws_auth(),
    pres_flag(0),
    shell_flag(0)
{

}


static void option_abort(char** argv){
    std::cerr << argv[0] <<", Error: Wrong argument format\n";
    std::cerr << "Try '" << argv[0] <<" --help' for more informations" << std::endl;
    exit(-1);
}

static void display_version(){
    std::cout << "Davix Version: " << version() << std::endl;
    exit(0);
}

template <typename T, typename Y, typename Z>
Y match_option(T begin, T end,
               Y begin_res, Y end_res,
               Z val, char** argv){
    Y res = match_array(begin, end, begin_res, end_res, val);
    if(res == end_res){
        option_abort(argv);
    }
    return res;
}

static int set_header_field(const std::string & arg, OptParams & p, DavixError** err){
    dav_size_t pos;
    if( (pos = arg.find(':') ) == std::string::npos){
        DavixError::setupError(err, scope_params, StatusCode::InvalidArgument, " Invalid header field argument");
        return -1;
    }
    p.params.addHeader(arg.substr(0,pos), arg.substr(pos+1));
    return 0;
}

static void set_metalink_opt(RequestParams & params, const std::string & metalink_opt, char** argv){
    const std::string str_opt[] = { "no" , "disable", "auto", "failover" };
    const Davix::MetalinkMode::MetalinkMode mode_opt[] = { MetalinkMode::Disable, MetalinkMode::Disable, MetalinkMode::Auto, MetalinkMode::FailOver };
    params.setMetalinkMode(*match_option(str_opt, str_opt+sizeof(str_opt)/sizeof(str_opt[0]),
                                               mode_opt, mode_opt + sizeof(mode_opt)/sizeof(mode_opt[0]),
                                               metalink_opt, argv));
}


static void set_redirection_opt(RequestParams & params, const std::string & redir_opt, char** argv){
    const std::string str_opt[] = { "no" , "yes", "auto"};
    const bool mode_opt[] = { false, true, true };
    params.setTransparentRedirectionSupport(*match_option(str_opt, str_opt+sizeof(str_opt)/sizeof(str_opt[0]),
                                               mode_opt, mode_opt + sizeof(mode_opt)/sizeof(mode_opt[0]),
                                               redir_opt, argv));
}

static struct timespec parse_timeout(const std::string & opt, char** argv){
    int t;
    std::istringstream ss(opt);
    ss >> t;
    if( ss.fail() || t < 0){
        std::cerr << "Invalid timeout " << opt << std::endl;
        option_abort(argv);
    }
    struct timespec timelapse;
    timelapse.tv_sec =t;
    timelapse.tv_nsec =0;
    return timelapse;
}

int parse_davix_options_generic(const std::string &opt_filter,
                                const struct option* long_options,
                                int argc, char** argv, OptParams & p, DavixError** err){
    int ret =0;
    int option_index=0;

    while( (ret =  getopt_long(argc, argv, opt_filter.c_str(),
                               long_options, &option_index)) > 0){

        switch(ret){
            case DEBUG_OPT:
                p.debug = true;
                davix_set_log_level(LOG_ALL ^ LOG_BODY ^ LOG_XML);
                davix_set_log_debug(true);
                break;
            case 'E':
                 p.cred_path = SanitiseTildedPath(optarg);
                 break;
            case 'k':
                p.params.setSSLCAcheck(false);
                break;
            case 'H':
                if( set_header_field(optarg, p, err) <0)
                    return -1;
                break;
            case CAPATH_OPT:
                p.params.addCertificateAuthorityPath(optarg);
                break;
            case USER_LOGIN:
                p.userlogpasswd.first = optarg;
                break;
            case X509_PRIVATE_KEY:
                p.priv_key = SanitiseTildedPath(optarg).c_str();
                break;
            case USER_PASSWORD:
                p.userlogpasswd.second = optarg;
                break;
            case DATA_CONTENT:
                p.req_content = optarg;
                break;
            case S3_ACCESS_KEY:
                p.aws_auth.second = optarg;
                break;
            case S3_SECRET_KEY:
                p.aws_auth.first = optarg;
                break;
            case 'l':
                p.pres_flag |= LONG_LISTING_FLAG;
                break;
            case 'o':
                p.output_file_path = optarg;
                break;
            case 'P':
                p.modules_list = StrUtil::tokenSplit(std::string(optarg), ",");
                break;
            case 'V':
                display_version();
                return 1;
            case TRACE_OPTIONS:
                p.trace_list = StrUtil::tokenSplit(std::string(optarg), ",");

                unsigned int i;

                if(is_number(p.trace_list[0]) ){
                        if(atoi(p.trace_list[0].c_str() ) > DAVIX_LOG_ALL){
                            std::cerr << "Trace level must be a decimal digit up to " << DAVIX_LOG_ALL << std::endl;
                            return -1;
                        }else{ // is a number <= max log level
                            i = 1;
                            davix_set_trace_level(atoi(p.trace_list[0].c_str()));
                        }
                }else{ // not a number
                    i = 0;
                }
                
                for(; i < p.trace_list.size(); ++i)
                    davix_set_log_scope(p.trace_list[i]);
                break;
            case 'x':
                p.params.setProxyServer(std::string(optarg, 0, 2048));
                break;
            case 'X':
                p.req_type = std::string(optarg, 0, 255);
                break;
            case METALINK_OPT:
                 set_metalink_opt(p.params, std::string(optarg), argv);
                 break;
            case REDIRECTION_OPT:
                 set_redirection_opt(p.params, std::string(optarg), argv);
                 break;
            case CONN_TIMEOUT:{
                struct timespec s =parse_timeout(std::string(optarg), argv);
                p.params.setConnectionTimeout(&s);
                }
                break;
            case TIMEOUT_OPS:{
                struct timespec s =parse_timeout(std::string(optarg), argv);
                p.params.setOperationTimeout(&s);
                break;
             }
            case '?':
            printf(p.help_msg.c_str(), argv[0]);
                return -1;
          break;
            default:
                option_abort(argv);
        }
    }


   ret =-1;
   for(int i = optind; i < argc; ++i){
            p.vec_arg.push_back(argv[i]);
            ret =0;
    }

    if(ret != 0){
        option_abort(argv);
    }

    return ret;
}



int parse_davix_options(int argc, char** argv, OptParams & p, DavixError** err){
    const std::string arg_tool_main= "P:x:H:E:X:o:kV";
    const struct option long_options[] = {
        COMMON_LONG_OPTIONS,
        SECURITY_LONG_OPTIONS,
        REQUEST_LONG_OPTIONS,
        {0,         0,                 0,  0 }
     };

    return parse_davix_options_generic(arg_tool_main, long_options,
                                       argc, argv,
                                       p, err);
}


int parse_davix_ls_options(int argc, char** argv, OptParams & p, DavixError** err){
    const std::string arg_tool_main= "P:x:H:E:vkVl";
    const struct option long_options[] = {
        COMMON_LONG_OPTIONS,
        SECURITY_LONG_OPTIONS,
        LISTING_LONG_OPTIONS,
        {0,         0,                 0,  0 }
     };

    if( parse_davix_options_generic(arg_tool_main, long_options,
                                       argc, argv,
                                       p, err) <0
            || p.vec_arg.size() != 1){
        option_abort(argv);
        return -1;
    }
    return 0;
}


int parse_davix_get_options(int argc, char** argv, OptParams & p, DavixError** err){
    const std::string arg_tool_main= "P:x:H:E:o:OvkV";
    const struct option long_options[] = {
        COMMON_LONG_OPTIONS,
        SECURITY_LONG_OPTIONS,
        {0,         0,                 0,  0 }
     };

    if( parse_davix_options_generic(arg_tool_main, long_options,
                                       argc, argv,
                                       p, err) < 0
            || p.vec_arg.size() > 2){
        option_abort(argv);
        return -1;
    }

    if(p.vec_arg.size() == 2){
        p.output_file_path = p.vec_arg[1];
    }
    return 0;
}

int parse_davix_put_options(int argc, char** argv, OptParams & p, DavixError** err){
    const std::string arg_tool_main= "P:x:H:E:o:vkV";
    const struct option long_options[] = {
        COMMON_LONG_OPTIONS,
        SECURITY_LONG_OPTIONS,
        {0,         0,                 0,  0 }
     };



    if( parse_davix_options_generic(arg_tool_main,
                                    long_options,
                                    argc,
                                    argv,
                                    p, err) < 0
        || p.vec_arg.size() != 2){

        option_abort(argv);
        return -1;
    }
    p.input_file_path = p.vec_arg[0];
    return 0;
}


const std::string  & get_common_options(){
    static const std::string s(
            "  Common Options:\n"
            "\t--conn-timeout TIME:      Connection timeout in seconds. default: 30\n"
            "\t--debug:                  Debug mode\n"
            "\t--header, -H:             Add a header field to the request\n"
            "\t--help, -h:               Display this help message\n"
            "\t--metalink OPT:           Metalink support. value=failover|no. default=failover) \n"
            "\t--module, -P NAME:        Load a plugin or profile by name\n"
            "\t--proxy, -x URL:          SOCKS5 proxy server URL. (Ex: socks5://login:pass@socks.example.org)\n"
            "\t--redirection OPT:        Transparent redirection support. value=yes|no. default=yes)\n"
            "\t--timeout TIME:           Global timeout for the operation in seconds. default: infinite\n"
            "\t--trace:                  Specify one or more scpoes to trace. (Ex: --trace log level(optional),header,file)\n"
            "\t--verbose:                Verbose mode\n"
            "\t--version, -V:            Display version\n"
            "  Security Options:\n"
            "\t--capath CA_PATH:         Add an additional certificate authority directory\n"
            "\t--cert, -E CRED_PATH:     Client Certificate in PEM format\n"
            "\t--key KEY_PATH:           Private key in PEM format\n"
            "\t--insecure, -k:           Disable SSL credential checks\n"
            "\t--userlogin:              User login for login/password authentication\n"
            "\t--userpass:               User password for login/password authentication\n"
            "\t--s3secretkey SEC_KEY:    S3 authentication: secret key\n"
            "\t--s3accesskey ACC_KEY:    S3 authentication: access key\n"
            );
    return s;
}



const std::string  & get_base_description_options(){
    static const std::string s("Usage: %s [OPTIONS ...] <url>\n"
            );
    return s;
}


const std::string  & get_put_description_options(){
    static const std::string s("Usage: %s [OPTIONS ...] <local_file> <remote_file_url> \n"
            );
    return s;
}


const std::string  & get_copy_description_options(){
    static const std::string s("Usage: %s [OPTIONS ...] <src_url> <dst_url>\n"
            );
    return s;
}

}

}
