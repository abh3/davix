#include <stdsoap2.h>
#include <gridsite.h>

#include "delegation.hpp"
#include "delegation2H.h"

using namespace Davix;
using namespace delegation2;

static void err2davix(struct soap* soap, DavixError** err, const std::string& prefix)
{
    char err_buffer[512];
    soap_sprint_fault(soap, err_buffer, sizeof(err_buffer));
    DavixError::setupError(err, DELEGATION_SCOPE, StatusCode::DelegationError,
            prefix + err_buffer);
}


static bool get_delegated_credentials(struct soap* soap,
        const std::string& endpoint, const std::string& dlg_id,
        time_t* lifetime)
{
    struct tns2__getTerminationTimeResponse resp;
    char *dlg_id_ptr = soap_strdup(soap, dlg_id.c_str());

    if (SOAP_OK != soap_call_tns2__getTerminationTime(soap, endpoint.c_str(), NULL, dlg_id_ptr, resp)) {
        char err_buffer[512];
        soap_sprint_fault(soap, err_buffer, sizeof(err_buffer));
        DAVIX_LOG(DAVIX_LOG_DEBUG, LOG_GRID, "Could not retrieve delegated credentials: %s", err_buffer);
        return false;
    }

    *lifetime = resp._getTerminationTimeReturn - time(NULL);
    return true;
}


static void renew_proxy(struct soap* soap, const std::string& endpoint,
        const std::string& ucred, const std::string& dlg_id, int lifetime, bool force, DavixError** err)
{
    int ret;
    char* dlg_id_ptr = soap_strdup(soap, dlg_id.c_str());
    //char *sdelegationID = (char *) "", *localproxy, *certtxt, *;
    char *certtxt, *scerttxt;
    std::string certreq;

    struct tns2__getProxyReqResponse get_resp;
    struct tns2__renewProxyReqResponse renew_resp;
    struct tns2__putProxyResponse put_resp;

    if (force) {
        DAVIX_LOG(DAVIX_LOG_VERBOSE, LOG_GRID, "Renew proxy request");

        ret = soap_call_tns2__renewProxyReq(soap, endpoint.c_str(), NULL, dlg_id_ptr, renew_resp);
        if (SOAP_OK != ret) {
            err2davix(soap, err, "Renewal failed");
            return;
        }
        certreq = renew_resp._renewProxyReqReturn;
    }

    /* if it was forced and failed, or if it was not forced at all */
    if (certreq.empty()) {
        DAVIX_LOG(DAVIX_LOG_VERBOSE, LOG_GRID, "Get proxy request");
        /* there was no proxy, or not forcing -- the normal path */
        ret = soap_call_tns2__getProxyReq(soap, endpoint.c_str(), NULL,
                dlg_id_ptr, get_resp);
        if (SOAP_OK != ret) {
            err2davix(soap, err, "Renewal failed");
            return;
        }
        certreq = get_resp._getProxyReqReturn;
    }

    /* generating a certificate from the request */
    if (!certreq.empty()) {
        DAVIX_LOG(DAVIX_LOG_VERBOSE, LOG_GRID, "Sign proxy request");
        ret = GRSTx509MakeProxyCert(&certtxt, stderr, (char*)certreq.c_str(),
                (char*)ucred.c_str(), (char*)ucred.c_str(), lifetime);
    }
    else {
        DavixError::setupError(err, DELEGATION_SCOPE, StatusCode::DelegationError, "Could not get a request");
        return;

    }
    if (ret != GRST_RET_OK) {
        DavixError::setupError(err, DELEGATION_SCOPE, StatusCode::DelegationError, "GRSTx509MakeProxyCert failed");
        return;
    }

    scerttxt = soap_strdup(soap, certtxt);
    if (!scerttxt) {
        DavixError::setupError(err, DELEGATION_SCOPE, StatusCode::DelegationError, "Could not duplicate");
        return;
    }

    DAVIX_LOG(DAVIX_LOG_VERBOSE, LOG_GRID, "Put new proxy");
    if (SOAP_OK != soap_call_tns2__putProxy(soap, endpoint.c_str(), NULL, dlg_id_ptr, scerttxt, put_resp)) {
        err2davix(soap, err, "Renewal failed");
        return;
    }
}


std::string DavixDelegation::delegate_v2(Context & context, const std::string &dlg_endpoint,
         const Davix::RequestParams& params,
         const std::string& ucred, const std::string& passwd,
         const std::string& capath,
         int lifetime, Davix::DavixError** err)
{
    (void) context;
    (void) params;
    struct soap* soap;
    soap = soap_new();

    std::string dlg_id = "1234";

    if (soap_ssl_client_context(soap, SOAP_SSL_DEFAULT, ucred.c_str(), passwd.c_str(),
                                  ucred.c_str(), capath.c_str(), NULL) != 0) {
        err2davix(soap, err, "Could not connect to the delegation endpoint: ");
        return std::string();
    }

    // Check if there is already a delegation done
    bool renew_delegation = false;
    bool new_delegation = false;
    time_t delegated_lifetime;

    if(!get_delegated_credentials(soap, dlg_endpoint, dlg_id, &delegated_lifetime)) {
        DAVIX_LOG(DAVIX_LOG_VERBOSE, LOG_GRID, "No delegated credentials on the storage");
        new_delegation = true;
    }
    else if (delegated_lifetime < lifetime) {
        DAVIX_LOG(DAVIX_LOG_VERBOSE, LOG_GRID, "Need to renew the credentials, %d > %d", lifetime, delegated_lifetime);
        renew_delegation = true;
    }
    else {
        DAVIX_LOG(DAVIX_LOG_VERBOSE, LOG_GRID, "Remaining life of the delegated credentials: %d", delegated_lifetime);
    }

    if (new_delegation || renew_delegation)
        renew_proxy(soap, dlg_endpoint, ucred, dlg_id, lifetime, renew_delegation, err);

    soap_free(soap);
    soap_done(soap);

    if (*err)
        return std::string();
    else
        return dlg_id;
}
