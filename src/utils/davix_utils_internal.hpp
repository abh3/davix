/*
 * This File is part of Davix, The IO library for HTTP based protocols
 * Copyright (C) CERN 2013
 * Author: Adrien Devresse <adrien.devresse@cern.ch>
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

#pragma once
#ifndef DAVIX_UTILS_INTERNAL_HPP
#define DAVIX_UTILS_INTERNAL_HPP

#include <utils/davix_uri.hpp>
#include <params/davixrequestparams.hpp>

namespace Davix {

bool isS3SignedURL(const Davix::Uri &url) {
    if(url.queryParamExists("AWSAccessKeyId") && url.queryParamExists("Signature")) {
    	return true;
    }

    if(url.queryParamExists("X-Amz-Credential") && url.queryParamExists("X-Amz-Signature")) {
    	return true;
    }

    return false;
}

}

#endif // DAVIX_UTILS_INTERNAL_HPP
