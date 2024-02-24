/*
 * Copyright 1995-2022 The OpenSSL Project Authors. All Rights Reserved.
 * Copyright (c) 2002, Oracle and/or its affiliates. All rights reserved
 * Copyright 2005 Nokia. All rights reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <openssl/objects.h>
#include "internal/nelem.h"
#include "ssl_local.h"
#include <openssl/md5.h>
#include <openssl/dh.h>
#include <openssl/rand.h>
#include <openssl/trace.h>
#include <openssl/x509v3.h>
#include <openssl/core_names.h>
#include "internal/cryptlib.h"
#include "btls.h"

#define TLS13_NUM_CIPHERS OSSL_NELEM(tls13_ciphers)
#define SSL3_NUM_CIPHERS OSSL_NELEM(ssl3_ciphers)
#define SSL3_NUM_SCSVS OSSL_NELEM(ssl3_scsvs)

/* TLSv1.3 downgrade protection sentinel values */
const unsigned char tls11downgrade[] = {
    0x44, 0x4f, 0x57, 0x4e, 0x47, 0x52, 0x44, 0x00};
const unsigned char tls12downgrade[] = {
    0x44, 0x4f, 0x57, 0x4e, 0x47, 0x52, 0x44, 0x01};

/* The list of available TLSv1.3 ciphers */
static SSL_CIPHER tls13_ciphers[] = {
    {
        1,
        TLS1_3_RFC_AES_128_GCM_SHA256,
        TLS1_3_RFC_AES_128_GCM_SHA256,
        TLS1_3_CK_AES_128_GCM_SHA256,
        SSL_kANY,
        SSL_aANY,
        SSL_AES128GCM,
        SSL_AEAD,
        TLS1_3_VERSION,
        TLS1_3_VERSION,
        0,
        0,
        SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_3_RFC_AES_256_GCM_SHA384,
        TLS1_3_RFC_AES_256_GCM_SHA384,
        TLS1_3_CK_AES_256_GCM_SHA384,
        SSL_kANY,
        SSL_aANY,
        SSL_AES256GCM,
        SSL_AEAD,
        TLS1_3_VERSION,
        TLS1_3_VERSION,
        0,
        0,
        SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA384,
        256,
        256,
    },
    {
        1,
        TLS1_3_RFC_CHACHA20_POLY1305_SHA256,
        TLS1_3_RFC_CHACHA20_POLY1305_SHA256,
        TLS1_3_CK_CHACHA20_POLY1305_SHA256,
        SSL_kANY,
        SSL_aANY,
        SSL_CHACHA20POLY1305,
        SSL_AEAD,
        TLS1_3_VERSION,
        TLS1_3_VERSION,
        0,
        0,
        SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256,
        256,
        256,
    },
    {
        1,
        TLS1_3_RFC_AES_128_CCM_SHA256,
        TLS1_3_RFC_AES_128_CCM_SHA256,
        TLS1_3_CK_AES_128_CCM_SHA256,
        SSL_kANY,
        SSL_aANY,
        SSL_AES128CCM,
        SSL_AEAD,
        TLS1_3_VERSION,
        TLS1_3_VERSION,
        0,
        0,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_3_RFC_AES_128_CCM_8_SHA256,
        TLS1_3_RFC_AES_128_CCM_8_SHA256,
        TLS1_3_CK_AES_128_CCM_8_SHA256,
        SSL_kANY,
        SSL_aANY,
        SSL_AES128CCM8,
        SSL_AEAD,
        TLS1_3_VERSION,
        TLS1_3_VERSION,
        0,
        0,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256,
        128,
        128,
    }};

/*
 * The list of available ciphers, mostly organized into the following
 * groups:
 *      Always there
 *      EC
 *      PSK
 *      SRP (within that: RSA EC PSK)
 *      Cipher families: Chacha/poly, Camellia, Gost, IDEA, SEED
 *      Weak ciphers
 */
static SSL_CIPHER ssl3_ciphers[] = {
    {
        1,
        SSL3_TXT_RSA_NULL_MD5,
        SSL3_RFC_RSA_NULL_MD5,
        SSL3_CK_RSA_NULL_MD5,
        SSL_kRSA,
        SSL_aRSA,
        SSL_eNULL,
        SSL_MD5,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_STRONG_NONE,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        0,
        0,
    },
    {
        1,
        SSL3_TXT_RSA_NULL_SHA,
        SSL3_RFC_RSA_NULL_SHA,
        SSL3_CK_RSA_NULL_SHA,
        SSL_kRSA,
        SSL_aRSA,
        SSL_eNULL,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_STRONG_NONE | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        0,
        0,
    },
#ifndef OPENSSL_NO_WEAK_SSL_CIPHERS
    {
        1,
        SSL3_TXT_RSA_DES_192_CBC3_SHA,
        SSL3_RFC_RSA_DES_192_CBC3_SHA,
        SSL3_CK_RSA_DES_192_CBC3_SHA,
        SSL_kRSA,
        SSL_aRSA,
        SSL_3DES,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_MEDIUM | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        112,
        168,
    },
    {
        1,
        SSL3_TXT_DHE_DSS_DES_192_CBC3_SHA,
        SSL3_RFC_DHE_DSS_DES_192_CBC3_SHA,
        SSL3_CK_DHE_DSS_DES_192_CBC3_SHA,
        SSL_kDHE,
        SSL_aDSS,
        SSL_3DES,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_MEDIUM | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        112,
        168,
    },
    {
        1,
        SSL3_TXT_DHE_RSA_DES_192_CBC3_SHA,
        SSL3_RFC_DHE_RSA_DES_192_CBC3_SHA,
        SSL3_CK_DHE_RSA_DES_192_CBC3_SHA,
        SSL_kDHE,
        SSL_aRSA,
        SSL_3DES,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_MEDIUM | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        112,
        168,
    },
    {
        1,
        SSL3_TXT_ADH_DES_192_CBC_SHA,
        SSL3_RFC_ADH_DES_192_CBC_SHA,
        SSL3_CK_ADH_DES_192_CBC_SHA,
        SSL_kDHE,
        SSL_aNULL,
        SSL_3DES,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_MEDIUM | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        112,
        168,
    },
#endif
    {
        1,
        TLS1_TXT_RSA_WITH_AES_128_SHA,
        TLS1_RFC_RSA_WITH_AES_128_SHA,
        TLS1_CK_RSA_WITH_AES_128_SHA,
        SSL_kRSA,
        SSL_aRSA,
        SSL_AES128,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_DHE_DSS_WITH_AES_128_SHA,
        TLS1_RFC_DHE_DSS_WITH_AES_128_SHA,
        TLS1_CK_DHE_DSS_WITH_AES_128_SHA,
        SSL_kDHE,
        SSL_aDSS,
        SSL_AES128,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_DHE_RSA_WITH_AES_128_SHA,
        TLS1_RFC_DHE_RSA_WITH_AES_128_SHA,
        TLS1_CK_DHE_RSA_WITH_AES_128_SHA,
        SSL_kDHE,
        SSL_aRSA,
        SSL_AES128,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_ADH_WITH_AES_128_SHA,
        TLS1_RFC_ADH_WITH_AES_128_SHA,
        TLS1_CK_ADH_WITH_AES_128_SHA,
        SSL_kDHE,
        SSL_aNULL,
        SSL_AES128,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_RSA_WITH_AES_256_SHA,
        TLS1_RFC_RSA_WITH_AES_256_SHA,
        TLS1_CK_RSA_WITH_AES_256_SHA,
        SSL_kRSA,
        SSL_aRSA,
        SSL_AES256,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_DHE_DSS_WITH_AES_256_SHA,
        TLS1_RFC_DHE_DSS_WITH_AES_256_SHA,
        TLS1_CK_DHE_DSS_WITH_AES_256_SHA,
        SSL_kDHE,
        SSL_aDSS,
        SSL_AES256,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_DHE_RSA_WITH_AES_256_SHA,
        TLS1_RFC_DHE_RSA_WITH_AES_256_SHA,
        TLS1_CK_DHE_RSA_WITH_AES_256_SHA,
        SSL_kDHE,
        SSL_aRSA,
        SSL_AES256,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_ADH_WITH_AES_256_SHA,
        TLS1_RFC_ADH_WITH_AES_256_SHA,
        TLS1_CK_ADH_WITH_AES_256_SHA,
        SSL_kDHE,
        SSL_aNULL,
        SSL_AES256,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_RSA_WITH_NULL_SHA256,
        TLS1_RFC_RSA_WITH_NULL_SHA256,
        TLS1_CK_RSA_WITH_NULL_SHA256,
        SSL_kRSA,
        SSL_aRSA,
        SSL_eNULL,
        SSL_SHA256,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_STRONG_NONE | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        0,
        0,
    },
    {
        1,
        TLS1_TXT_RSA_WITH_AES_128_SHA256,
        TLS1_RFC_RSA_WITH_AES_128_SHA256,
        TLS1_CK_RSA_WITH_AES_128_SHA256,
        SSL_kRSA,
        SSL_aRSA,
        SSL_AES128,
        SSL_SHA256,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_RSA_WITH_AES_256_SHA256,
        TLS1_RFC_RSA_WITH_AES_256_SHA256,
        TLS1_CK_RSA_WITH_AES_256_SHA256,
        SSL_kRSA,
        SSL_aRSA,
        SSL_AES256,
        SSL_SHA256,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_DHE_DSS_WITH_AES_128_SHA256,
        TLS1_RFC_DHE_DSS_WITH_AES_128_SHA256,
        TLS1_CK_DHE_DSS_WITH_AES_128_SHA256,
        SSL_kDHE,
        SSL_aDSS,
        SSL_AES128,
        SSL_SHA256,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_DHE_RSA_WITH_AES_128_SHA256,
        TLS1_RFC_DHE_RSA_WITH_AES_128_SHA256,
        TLS1_CK_DHE_RSA_WITH_AES_128_SHA256,
        SSL_kDHE,
        SSL_aRSA,
        SSL_AES128,
        SSL_SHA256,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_DHE_DSS_WITH_AES_256_SHA256,
        TLS1_RFC_DHE_DSS_WITH_AES_256_SHA256,
        TLS1_CK_DHE_DSS_WITH_AES_256_SHA256,
        SSL_kDHE,
        SSL_aDSS,
        SSL_AES256,
        SSL_SHA256,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_DHE_RSA_WITH_AES_256_SHA256,
        TLS1_RFC_DHE_RSA_WITH_AES_256_SHA256,
        TLS1_CK_DHE_RSA_WITH_AES_256_SHA256,
        SSL_kDHE,
        SSL_aRSA,
        SSL_AES256,
        SSL_SHA256,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_ADH_WITH_AES_128_SHA256,
        TLS1_RFC_ADH_WITH_AES_128_SHA256,
        TLS1_CK_ADH_WITH_AES_128_SHA256,
        SSL_kDHE,
        SSL_aNULL,
        SSL_AES128,
        SSL_SHA256,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_ADH_WITH_AES_256_SHA256,
        TLS1_RFC_ADH_WITH_AES_256_SHA256,
        TLS1_CK_ADH_WITH_AES_256_SHA256,
        SSL_kDHE,
        SSL_aNULL,
        SSL_AES256,
        SSL_SHA256,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_RSA_WITH_AES_128_GCM_SHA256,
        TLS1_RFC_RSA_WITH_AES_128_GCM_SHA256,
        TLS1_CK_RSA_WITH_AES_128_GCM_SHA256,
        SSL_kRSA,
        SSL_aRSA,
        SSL_AES128GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_RSA_WITH_AES_256_GCM_SHA384,
        TLS1_RFC_RSA_WITH_AES_256_GCM_SHA384,
        TLS1_CK_RSA_WITH_AES_256_GCM_SHA384,
        SSL_kRSA,
        SSL_aRSA,
        SSL_AES256GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_DHE_RSA_WITH_AES_128_GCM_SHA256,
        TLS1_RFC_DHE_RSA_WITH_AES_128_GCM_SHA256,
        TLS1_CK_DHE_RSA_WITH_AES_128_GCM_SHA256,
        SSL_kDHE,
        SSL_aRSA,
        SSL_AES128GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_DHE_RSA_WITH_AES_256_GCM_SHA384,
        TLS1_RFC_DHE_RSA_WITH_AES_256_GCM_SHA384,
        TLS1_CK_DHE_RSA_WITH_AES_256_GCM_SHA384,
        SSL_kDHE,
        SSL_aRSA,
        SSL_AES256GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_DHE_DSS_WITH_AES_128_GCM_SHA256,
        TLS1_RFC_DHE_DSS_WITH_AES_128_GCM_SHA256,
        TLS1_CK_DHE_DSS_WITH_AES_128_GCM_SHA256,
        SSL_kDHE,
        SSL_aDSS,
        SSL_AES128GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_DHE_DSS_WITH_AES_256_GCM_SHA384,
        TLS1_RFC_DHE_DSS_WITH_AES_256_GCM_SHA384,
        TLS1_CK_DHE_DSS_WITH_AES_256_GCM_SHA384,
        SSL_kDHE,
        SSL_aDSS,
        SSL_AES256GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_ADH_WITH_AES_128_GCM_SHA256,
        TLS1_RFC_ADH_WITH_AES_128_GCM_SHA256,
        TLS1_CK_ADH_WITH_AES_128_GCM_SHA256,
        SSL_kDHE,
        SSL_aNULL,
        SSL_AES128GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_ADH_WITH_AES_256_GCM_SHA384,
        TLS1_RFC_ADH_WITH_AES_256_GCM_SHA384,
        TLS1_CK_ADH_WITH_AES_256_GCM_SHA384,
        SSL_kDHE,
        SSL_aNULL,
        SSL_AES256GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_RSA_WITH_AES_128_CCM,
        TLS1_RFC_RSA_WITH_AES_128_CCM,
        TLS1_CK_RSA_WITH_AES_128_CCM,
        SSL_kRSA,
        SSL_aRSA,
        SSL_AES128CCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_RSA_WITH_AES_256_CCM,
        TLS1_RFC_RSA_WITH_AES_256_CCM,
        TLS1_CK_RSA_WITH_AES_256_CCM,
        SSL_kRSA,
        SSL_aRSA,
        SSL_AES256CCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_DHE_RSA_WITH_AES_128_CCM,
        TLS1_RFC_DHE_RSA_WITH_AES_128_CCM,
        TLS1_CK_DHE_RSA_WITH_AES_128_CCM,
        SSL_kDHE,
        SSL_aRSA,
        SSL_AES128CCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_DHE_RSA_WITH_AES_256_CCM,
        TLS1_RFC_DHE_RSA_WITH_AES_256_CCM,
        TLS1_CK_DHE_RSA_WITH_AES_256_CCM,
        SSL_kDHE,
        SSL_aRSA,
        SSL_AES256CCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_RSA_WITH_AES_128_CCM_8,
        TLS1_RFC_RSA_WITH_AES_128_CCM_8,
        TLS1_CK_RSA_WITH_AES_128_CCM_8,
        SSL_kRSA,
        SSL_aRSA,
        SSL_AES128CCM8,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_RSA_WITH_AES_256_CCM_8,
        TLS1_RFC_RSA_WITH_AES_256_CCM_8,
        TLS1_CK_RSA_WITH_AES_256_CCM_8,
        SSL_kRSA,
        SSL_aRSA,
        SSL_AES256CCM8,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_DHE_RSA_WITH_AES_128_CCM_8,
        TLS1_RFC_DHE_RSA_WITH_AES_128_CCM_8,
        TLS1_CK_DHE_RSA_WITH_AES_128_CCM_8,
        SSL_kDHE,
        SSL_aRSA,
        SSL_AES128CCM8,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_DHE_RSA_WITH_AES_256_CCM_8,
        TLS1_RFC_DHE_RSA_WITH_AES_256_CCM_8,
        TLS1_CK_DHE_RSA_WITH_AES_256_CCM_8,
        SSL_kDHE,
        SSL_aRSA,
        SSL_AES256CCM8,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_PSK_WITH_AES_128_CCM,
        TLS1_RFC_PSK_WITH_AES_128_CCM,
        TLS1_CK_PSK_WITH_AES_128_CCM,
        SSL_kPSK,
        SSL_aPSK,
        SSL_AES128CCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_PSK_WITH_AES_256_CCM,
        TLS1_RFC_PSK_WITH_AES_256_CCM,
        TLS1_CK_PSK_WITH_AES_256_CCM,
        SSL_kPSK,
        SSL_aPSK,
        SSL_AES256CCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_DHE_PSK_WITH_AES_128_CCM,
        TLS1_RFC_DHE_PSK_WITH_AES_128_CCM,
        TLS1_CK_DHE_PSK_WITH_AES_128_CCM,
        SSL_kDHEPSK,
        SSL_aPSK,
        SSL_AES128CCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_DHE_PSK_WITH_AES_256_CCM,
        TLS1_RFC_DHE_PSK_WITH_AES_256_CCM,
        TLS1_CK_DHE_PSK_WITH_AES_256_CCM,
        SSL_kDHEPSK,
        SSL_aPSK,
        SSL_AES256CCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_PSK_WITH_AES_128_CCM_8,
        TLS1_RFC_PSK_WITH_AES_128_CCM_8,
        TLS1_CK_PSK_WITH_AES_128_CCM_8,
        SSL_kPSK,
        SSL_aPSK,
        SSL_AES128CCM8,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_PSK_WITH_AES_256_CCM_8,
        TLS1_RFC_PSK_WITH_AES_256_CCM_8,
        TLS1_CK_PSK_WITH_AES_256_CCM_8,
        SSL_kPSK,
        SSL_aPSK,
        SSL_AES256CCM8,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_DHE_PSK_WITH_AES_128_CCM_8,
        TLS1_RFC_DHE_PSK_WITH_AES_128_CCM_8,
        TLS1_CK_DHE_PSK_WITH_AES_128_CCM_8,
        SSL_kDHEPSK,
        SSL_aPSK,
        SSL_AES128CCM8,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_DHE_PSK_WITH_AES_256_CCM_8,
        TLS1_RFC_DHE_PSK_WITH_AES_256_CCM_8,
        TLS1_CK_DHE_PSK_WITH_AES_256_CCM_8,
        SSL_kDHEPSK,
        SSL_aPSK,
        SSL_AES256CCM8,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_ECDHE_ECDSA_WITH_AES_128_CCM,
        TLS1_RFC_ECDHE_ECDSA_WITH_AES_128_CCM,
        TLS1_CK_ECDHE_ECDSA_WITH_AES_128_CCM,
        SSL_kECDHE,
        SSL_aECDSA,
        SSL_AES128CCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_ECDHE_ECDSA_WITH_AES_256_CCM,
        TLS1_RFC_ECDHE_ECDSA_WITH_AES_256_CCM,
        TLS1_CK_ECDHE_ECDSA_WITH_AES_256_CCM,
        SSL_kECDHE,
        SSL_aECDSA,
        SSL_AES256CCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_ECDHE_ECDSA_WITH_AES_128_CCM_8,
        TLS1_RFC_ECDHE_ECDSA_WITH_AES_128_CCM_8,
        TLS1_CK_ECDHE_ECDSA_WITH_AES_128_CCM_8,
        SSL_kECDHE,
        SSL_aECDSA,
        SSL_AES128CCM8,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_ECDHE_ECDSA_WITH_AES_256_CCM_8,
        TLS1_RFC_ECDHE_ECDSA_WITH_AES_256_CCM_8,
        TLS1_CK_ECDHE_ECDSA_WITH_AES_256_CCM_8,
        SSL_kECDHE,
        SSL_aECDSA,
        SSL_AES256CCM8,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_ECDHE_ECDSA_WITH_NULL_SHA,
        TLS1_RFC_ECDHE_ECDSA_WITH_NULL_SHA,
        TLS1_CK_ECDHE_ECDSA_WITH_NULL_SHA,
        SSL_kECDHE,
        SSL_aECDSA,
        SSL_eNULL,
        SSL_SHA1,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_STRONG_NONE | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        0,
        0,
    },
#ifndef OPENSSL_NO_WEAK_SSL_CIPHERS
    {
        1,
        TLS1_TXT_ECDHE_ECDSA_WITH_DES_192_CBC3_SHA,
        TLS1_RFC_ECDHE_ECDSA_WITH_DES_192_CBC3_SHA,
        TLS1_CK_ECDHE_ECDSA_WITH_DES_192_CBC3_SHA,
        SSL_kECDHE,
        SSL_aECDSA,
        SSL_3DES,
        SSL_SHA1,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_MEDIUM | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        112,
        168,
    },
#endif
    {
        1,
        TLS1_TXT_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
        TLS1_RFC_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
        TLS1_CK_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
        SSL_kECDHE,
        SSL_aECDSA,
        SSL_AES128,
        SSL_SHA1,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
        TLS1_RFC_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
        TLS1_CK_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
        SSL_kECDHE,
        SSL_aECDSA,
        SSL_AES256,
        SSL_SHA1,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_ECDHE_RSA_WITH_NULL_SHA,
        TLS1_RFC_ECDHE_RSA_WITH_NULL_SHA,
        TLS1_CK_ECDHE_RSA_WITH_NULL_SHA,
        SSL_kECDHE,
        SSL_aRSA,
        SSL_eNULL,
        SSL_SHA1,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_STRONG_NONE | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        0,
        0,
    },
#ifndef OPENSSL_NO_WEAK_SSL_CIPHERS
    {
        1,
        TLS1_TXT_ECDHE_RSA_WITH_DES_192_CBC3_SHA,
        TLS1_RFC_ECDHE_RSA_WITH_DES_192_CBC3_SHA,
        TLS1_CK_ECDHE_RSA_WITH_DES_192_CBC3_SHA,
        SSL_kECDHE,
        SSL_aRSA,
        SSL_3DES,
        SSL_SHA1,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_MEDIUM | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        112,
        168,
    },
#endif
    {
        1,
        TLS1_TXT_ECDHE_RSA_WITH_AES_128_CBC_SHA,
        TLS1_RFC_ECDHE_RSA_WITH_AES_128_CBC_SHA,
        TLS1_CK_ECDHE_RSA_WITH_AES_128_CBC_SHA,
        SSL_kECDHE,
        SSL_aRSA,
        SSL_AES128,
        SSL_SHA1,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_ECDHE_RSA_WITH_AES_256_CBC_SHA,
        TLS1_RFC_ECDHE_RSA_WITH_AES_256_CBC_SHA,
        TLS1_CK_ECDHE_RSA_WITH_AES_256_CBC_SHA,
        SSL_kECDHE,
        SSL_aRSA,
        SSL_AES256,
        SSL_SHA1,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_ECDH_anon_WITH_NULL_SHA,
        TLS1_RFC_ECDH_anon_WITH_NULL_SHA,
        TLS1_CK_ECDH_anon_WITH_NULL_SHA,
        SSL_kECDHE,
        SSL_aNULL,
        SSL_eNULL,
        SSL_SHA1,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_STRONG_NONE | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        0,
        0,
    },
#ifndef OPENSSL_NO_WEAK_SSL_CIPHERS
    {
        1,
        TLS1_TXT_ECDH_anon_WITH_DES_192_CBC3_SHA,
        TLS1_RFC_ECDH_anon_WITH_DES_192_CBC3_SHA,
        TLS1_CK_ECDH_anon_WITH_DES_192_CBC3_SHA,
        SSL_kECDHE,
        SSL_aNULL,
        SSL_3DES,
        SSL_SHA1,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_MEDIUM | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        112,
        168,
    },
#endif
    {
        1,
        TLS1_TXT_ECDH_anon_WITH_AES_128_CBC_SHA,
        TLS1_RFC_ECDH_anon_WITH_AES_128_CBC_SHA,
        TLS1_CK_ECDH_anon_WITH_AES_128_CBC_SHA,
        SSL_kECDHE,
        SSL_aNULL,
        SSL_AES128,
        SSL_SHA1,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_ECDH_anon_WITH_AES_256_CBC_SHA,
        TLS1_RFC_ECDH_anon_WITH_AES_256_CBC_SHA,
        TLS1_CK_ECDH_anon_WITH_AES_256_CBC_SHA,
        SSL_kECDHE,
        SSL_aNULL,
        SSL_AES256,
        SSL_SHA1,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_ECDHE_ECDSA_WITH_AES_128_SHA256,
        TLS1_RFC_ECDHE_ECDSA_WITH_AES_128_SHA256,
        TLS1_CK_ECDHE_ECDSA_WITH_AES_128_SHA256,
        SSL_kECDHE,
        SSL_aECDSA,
        SSL_AES128,
        SSL_SHA256,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_ECDHE_ECDSA_WITH_AES_256_SHA384,
        TLS1_RFC_ECDHE_ECDSA_WITH_AES_256_SHA384,
        TLS1_CK_ECDHE_ECDSA_WITH_AES_256_SHA384,
        SSL_kECDHE,
        SSL_aECDSA,
        SSL_AES256,
        SSL_SHA384,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_ECDHE_RSA_WITH_AES_128_SHA256,
        TLS1_RFC_ECDHE_RSA_WITH_AES_128_SHA256,
        TLS1_CK_ECDHE_RSA_WITH_AES_128_SHA256,
        SSL_kECDHE,
        SSL_aRSA,
        SSL_AES128,
        SSL_SHA256,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_ECDHE_RSA_WITH_AES_256_SHA384,
        TLS1_RFC_ECDHE_RSA_WITH_AES_256_SHA384,
        TLS1_CK_ECDHE_RSA_WITH_AES_256_SHA384,
        SSL_kECDHE,
        SSL_aRSA,
        SSL_AES256,
        SSL_SHA384,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
        TLS1_RFC_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
        TLS1_CK_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
        SSL_kECDHE,
        SSL_aECDSA,
        SSL_AES128GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
        TLS1_RFC_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
        TLS1_CK_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
        SSL_kECDHE,
        SSL_aECDSA,
        SSL_AES256GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
        TLS1_RFC_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
        TLS1_CK_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
        SSL_kECDHE,
        SSL_aRSA,
        SSL_AES128GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
        TLS1_RFC_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
        TLS1_CK_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
        SSL_kECDHE,
        SSL_aRSA,
        SSL_AES256GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_PSK_WITH_NULL_SHA,
        TLS1_RFC_PSK_WITH_NULL_SHA,
        TLS1_CK_PSK_WITH_NULL_SHA,
        SSL_kPSK,
        SSL_aPSK,
        SSL_eNULL,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_STRONG_NONE | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        0,
        0,
    },
    {
        1,
        TLS1_TXT_DHE_PSK_WITH_NULL_SHA,
        TLS1_RFC_DHE_PSK_WITH_NULL_SHA,
        TLS1_CK_DHE_PSK_WITH_NULL_SHA,
        SSL_kDHEPSK,
        SSL_aPSK,
        SSL_eNULL,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_STRONG_NONE | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        0,
        0,
    },
    {
        1,
        TLS1_TXT_RSA_PSK_WITH_NULL_SHA,
        TLS1_RFC_RSA_PSK_WITH_NULL_SHA,
        TLS1_CK_RSA_PSK_WITH_NULL_SHA,
        SSL_kRSAPSK,
        SSL_aRSA,
        SSL_eNULL,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_STRONG_NONE | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        0,
        0,
    },
#ifndef OPENSSL_NO_WEAK_SSL_CIPHERS
    {
        1,
        TLS1_TXT_PSK_WITH_3DES_EDE_CBC_SHA,
        TLS1_RFC_PSK_WITH_3DES_EDE_CBC_SHA,
        TLS1_CK_PSK_WITH_3DES_EDE_CBC_SHA,
        SSL_kPSK,
        SSL_aPSK,
        SSL_3DES,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_MEDIUM | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        112,
        168,
    },
#endif
    {
        1,
        TLS1_TXT_PSK_WITH_AES_128_CBC_SHA,
        TLS1_RFC_PSK_WITH_AES_128_CBC_SHA,
        TLS1_CK_PSK_WITH_AES_128_CBC_SHA,
        SSL_kPSK,
        SSL_aPSK,
        SSL_AES128,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_PSK_WITH_AES_256_CBC_SHA,
        TLS1_RFC_PSK_WITH_AES_256_CBC_SHA,
        TLS1_CK_PSK_WITH_AES_256_CBC_SHA,
        SSL_kPSK,
        SSL_aPSK,
        SSL_AES256,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        256,
        256,
    },
#ifndef OPENSSL_NO_WEAK_SSL_CIPHERS
    {
        1,
        TLS1_TXT_DHE_PSK_WITH_3DES_EDE_CBC_SHA,
        TLS1_RFC_DHE_PSK_WITH_3DES_EDE_CBC_SHA,
        TLS1_CK_DHE_PSK_WITH_3DES_EDE_CBC_SHA,
        SSL_kDHEPSK,
        SSL_aPSK,
        SSL_3DES,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_MEDIUM | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        112,
        168,
    },
#endif
    {
        1,
        TLS1_TXT_DHE_PSK_WITH_AES_128_CBC_SHA,
        TLS1_RFC_DHE_PSK_WITH_AES_128_CBC_SHA,
        TLS1_CK_DHE_PSK_WITH_AES_128_CBC_SHA,
        SSL_kDHEPSK,
        SSL_aPSK,
        SSL_AES128,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_DHE_PSK_WITH_AES_256_CBC_SHA,
        TLS1_RFC_DHE_PSK_WITH_AES_256_CBC_SHA,
        TLS1_CK_DHE_PSK_WITH_AES_256_CBC_SHA,
        SSL_kDHEPSK,
        SSL_aPSK,
        SSL_AES256,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        256,
        256,
    },
#ifndef OPENSSL_NO_WEAK_SSL_CIPHERS
    {
        1,
        TLS1_TXT_RSA_PSK_WITH_3DES_EDE_CBC_SHA,
        TLS1_RFC_RSA_PSK_WITH_3DES_EDE_CBC_SHA,
        TLS1_CK_RSA_PSK_WITH_3DES_EDE_CBC_SHA,
        SSL_kRSAPSK,
        SSL_aRSA,
        SSL_3DES,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_MEDIUM | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        112,
        168,
    },
#endif
    {
        1,
        TLS1_TXT_RSA_PSK_WITH_AES_128_CBC_SHA,
        TLS1_RFC_RSA_PSK_WITH_AES_128_CBC_SHA,
        TLS1_CK_RSA_PSK_WITH_AES_128_CBC_SHA,
        SSL_kRSAPSK,
        SSL_aRSA,
        SSL_AES128,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_RSA_PSK_WITH_AES_256_CBC_SHA,
        TLS1_RFC_RSA_PSK_WITH_AES_256_CBC_SHA,
        TLS1_CK_RSA_PSK_WITH_AES_256_CBC_SHA,
        SSL_kRSAPSK,
        SSL_aRSA,
        SSL_AES256,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_PSK_WITH_AES_128_GCM_SHA256,
        TLS1_RFC_PSK_WITH_AES_128_GCM_SHA256,
        TLS1_CK_PSK_WITH_AES_128_GCM_SHA256,
        SSL_kPSK,
        SSL_aPSK,
        SSL_AES128GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_PSK_WITH_AES_256_GCM_SHA384,
        TLS1_RFC_PSK_WITH_AES_256_GCM_SHA384,
        TLS1_CK_PSK_WITH_AES_256_GCM_SHA384,
        SSL_kPSK,
        SSL_aPSK,
        SSL_AES256GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_DHE_PSK_WITH_AES_128_GCM_SHA256,
        TLS1_RFC_DHE_PSK_WITH_AES_128_GCM_SHA256,
        TLS1_CK_DHE_PSK_WITH_AES_128_GCM_SHA256,
        SSL_kDHEPSK,
        SSL_aPSK,
        SSL_AES128GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_DHE_PSK_WITH_AES_256_GCM_SHA384,
        TLS1_RFC_DHE_PSK_WITH_AES_256_GCM_SHA384,
        TLS1_CK_DHE_PSK_WITH_AES_256_GCM_SHA384,
        SSL_kDHEPSK,
        SSL_aPSK,
        SSL_AES256GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_RSA_PSK_WITH_AES_128_GCM_SHA256,
        TLS1_RFC_RSA_PSK_WITH_AES_128_GCM_SHA256,
        TLS1_CK_RSA_PSK_WITH_AES_128_GCM_SHA256,
        SSL_kRSAPSK,
        SSL_aRSA,
        SSL_AES128GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_RSA_PSK_WITH_AES_256_GCM_SHA384,
        TLS1_RFC_RSA_PSK_WITH_AES_256_GCM_SHA384,
        TLS1_CK_RSA_PSK_WITH_AES_256_GCM_SHA384,
        SSL_kRSAPSK,
        SSL_aRSA,
        SSL_AES256GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_PSK_WITH_AES_128_CBC_SHA256,
        TLS1_RFC_PSK_WITH_AES_128_CBC_SHA256,
        TLS1_CK_PSK_WITH_AES_128_CBC_SHA256,
        SSL_kPSK,
        SSL_aPSK,
        SSL_AES128,
        SSL_SHA256,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_PSK_WITH_AES_256_CBC_SHA384,
        TLS1_RFC_PSK_WITH_AES_256_CBC_SHA384,
        TLS1_CK_PSK_WITH_AES_256_CBC_SHA384,
        SSL_kPSK,
        SSL_aPSK,
        SSL_AES256,
        SSL_SHA384,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_PSK_WITH_NULL_SHA256,
        TLS1_RFC_PSK_WITH_NULL_SHA256,
        TLS1_CK_PSK_WITH_NULL_SHA256,
        SSL_kPSK,
        SSL_aPSK,
        SSL_eNULL,
        SSL_SHA256,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_STRONG_NONE | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        0,
        0,
    },
    {
        1,
        TLS1_TXT_PSK_WITH_NULL_SHA384,
        TLS1_RFC_PSK_WITH_NULL_SHA384,
        TLS1_CK_PSK_WITH_NULL_SHA384,
        SSL_kPSK,
        SSL_aPSK,
        SSL_eNULL,
        SSL_SHA384,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_STRONG_NONE | SSL_FIPS,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        0,
        0,
    },
    {
        1,
        TLS1_TXT_DHE_PSK_WITH_AES_128_CBC_SHA256,
        TLS1_RFC_DHE_PSK_WITH_AES_128_CBC_SHA256,
        TLS1_CK_DHE_PSK_WITH_AES_128_CBC_SHA256,
        SSL_kDHEPSK,
        SSL_aPSK,
        SSL_AES128,
        SSL_SHA256,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_DHE_PSK_WITH_AES_256_CBC_SHA384,
        TLS1_RFC_DHE_PSK_WITH_AES_256_CBC_SHA384,
        TLS1_CK_DHE_PSK_WITH_AES_256_CBC_SHA384,
        SSL_kDHEPSK,
        SSL_aPSK,
        SSL_AES256,
        SSL_SHA384,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_DHE_PSK_WITH_NULL_SHA256,
        TLS1_RFC_DHE_PSK_WITH_NULL_SHA256,
        TLS1_CK_DHE_PSK_WITH_NULL_SHA256,
        SSL_kDHEPSK,
        SSL_aPSK,
        SSL_eNULL,
        SSL_SHA256,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_STRONG_NONE | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        0,
        0,
    },
    {
        1,
        TLS1_TXT_DHE_PSK_WITH_NULL_SHA384,
        TLS1_RFC_DHE_PSK_WITH_NULL_SHA384,
        TLS1_CK_DHE_PSK_WITH_NULL_SHA384,
        SSL_kDHEPSK,
        SSL_aPSK,
        SSL_eNULL,
        SSL_SHA384,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_STRONG_NONE | SSL_FIPS,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        0,
        0,
    },
    {
        1,
        TLS1_TXT_RSA_PSK_WITH_AES_128_CBC_SHA256,
        TLS1_RFC_RSA_PSK_WITH_AES_128_CBC_SHA256,
        TLS1_CK_RSA_PSK_WITH_AES_128_CBC_SHA256,
        SSL_kRSAPSK,
        SSL_aRSA,
        SSL_AES128,
        SSL_SHA256,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_RSA_PSK_WITH_AES_256_CBC_SHA384,
        TLS1_RFC_RSA_PSK_WITH_AES_256_CBC_SHA384,
        TLS1_CK_RSA_PSK_WITH_AES_256_CBC_SHA384,
        SSL_kRSAPSK,
        SSL_aRSA,
        SSL_AES256,
        SSL_SHA384,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_RSA_PSK_WITH_NULL_SHA256,
        TLS1_RFC_RSA_PSK_WITH_NULL_SHA256,
        TLS1_CK_RSA_PSK_WITH_NULL_SHA256,
        SSL_kRSAPSK,
        SSL_aRSA,
        SSL_eNULL,
        SSL_SHA256,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_STRONG_NONE | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        0,
        0,
    },
    {
        1,
        TLS1_TXT_RSA_PSK_WITH_NULL_SHA384,
        TLS1_RFC_RSA_PSK_WITH_NULL_SHA384,
        TLS1_CK_RSA_PSK_WITH_NULL_SHA384,
        SSL_kRSAPSK,
        SSL_aRSA,
        SSL_eNULL,
        SSL_SHA384,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_STRONG_NONE | SSL_FIPS,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        0,
        0,
    },
#ifndef OPENSSL_NO_WEAK_SSL_CIPHERS
    {
        1,
        TLS1_TXT_ECDHE_PSK_WITH_3DES_EDE_CBC_SHA,
        TLS1_RFC_ECDHE_PSK_WITH_3DES_EDE_CBC_SHA,
        TLS1_CK_ECDHE_PSK_WITH_3DES_EDE_CBC_SHA,
        SSL_kECDHEPSK,
        SSL_aPSK,
        SSL_3DES,
        SSL_SHA1,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_MEDIUM | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        112,
        168,
    },
#endif
    {
        1,
        TLS1_TXT_ECDHE_PSK_WITH_AES_128_CBC_SHA,
        TLS1_RFC_ECDHE_PSK_WITH_AES_128_CBC_SHA,
        TLS1_CK_ECDHE_PSK_WITH_AES_128_CBC_SHA,
        SSL_kECDHEPSK,
        SSL_aPSK,
        SSL_AES128,
        SSL_SHA1,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_ECDHE_PSK_WITH_AES_256_CBC_SHA,
        TLS1_RFC_ECDHE_PSK_WITH_AES_256_CBC_SHA,
        TLS1_CK_ECDHE_PSK_WITH_AES_256_CBC_SHA,
        SSL_kECDHEPSK,
        SSL_aPSK,
        SSL_AES256,
        SSL_SHA1,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_ECDHE_PSK_WITH_AES_128_CBC_SHA256,
        TLS1_RFC_ECDHE_PSK_WITH_AES_128_CBC_SHA256,
        TLS1_CK_ECDHE_PSK_WITH_AES_128_CBC_SHA256,
        SSL_kECDHEPSK,
        SSL_aPSK,
        SSL_AES128,
        SSL_SHA256,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_ECDHE_PSK_WITH_AES_256_CBC_SHA384,
        TLS1_RFC_ECDHE_PSK_WITH_AES_256_CBC_SHA384,
        TLS1_CK_ECDHE_PSK_WITH_AES_256_CBC_SHA384,
        SSL_kECDHEPSK,
        SSL_aPSK,
        SSL_AES256,
        SSL_SHA384,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_HIGH | SSL_FIPS,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_ECDHE_PSK_WITH_NULL_SHA,
        TLS1_RFC_ECDHE_PSK_WITH_NULL_SHA,
        TLS1_CK_ECDHE_PSK_WITH_NULL_SHA,
        SSL_kECDHEPSK,
        SSL_aPSK,
        SSL_eNULL,
        SSL_SHA1,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_STRONG_NONE | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        0,
        0,
    },
    {
        1,
        TLS1_TXT_ECDHE_PSK_WITH_NULL_SHA256,
        TLS1_RFC_ECDHE_PSK_WITH_NULL_SHA256,
        TLS1_CK_ECDHE_PSK_WITH_NULL_SHA256,
        SSL_kECDHEPSK,
        SSL_aPSK,
        SSL_eNULL,
        SSL_SHA256,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_STRONG_NONE | SSL_FIPS,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        0,
        0,
    },
    {
        1,
        TLS1_TXT_ECDHE_PSK_WITH_NULL_SHA384,
        TLS1_RFC_ECDHE_PSK_WITH_NULL_SHA384,
        TLS1_CK_ECDHE_PSK_WITH_NULL_SHA384,
        SSL_kECDHEPSK,
        SSL_aPSK,
        SSL_eNULL,
        SSL_SHA384,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_STRONG_NONE | SSL_FIPS,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        0,
        0,
    },

#ifndef OPENSSL_NO_WEAK_SSL_CIPHERS
    {
        1,
        TLS1_TXT_SRP_SHA_WITH_3DES_EDE_CBC_SHA,
        TLS1_RFC_SRP_SHA_WITH_3DES_EDE_CBC_SHA,
        TLS1_CK_SRP_SHA_WITH_3DES_EDE_CBC_SHA,
        SSL_kSRP,
        SSL_aSRP,
        SSL_3DES,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_MEDIUM,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        112,
        168,
    },
    {
        1,
        TLS1_TXT_SRP_SHA_RSA_WITH_3DES_EDE_CBC_SHA,
        TLS1_RFC_SRP_SHA_RSA_WITH_3DES_EDE_CBC_SHA,
        TLS1_CK_SRP_SHA_RSA_WITH_3DES_EDE_CBC_SHA,
        SSL_kSRP,
        SSL_aRSA,
        SSL_3DES,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_MEDIUM,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        112,
        168,
    },
    {
        1,
        TLS1_TXT_SRP_SHA_DSS_WITH_3DES_EDE_CBC_SHA,
        TLS1_RFC_SRP_SHA_DSS_WITH_3DES_EDE_CBC_SHA,
        TLS1_CK_SRP_SHA_DSS_WITH_3DES_EDE_CBC_SHA,
        SSL_kSRP,
        SSL_aDSS,
        SSL_3DES,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_MEDIUM,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        112,
        168,
    },
#endif
    {
        1,
        TLS1_TXT_SRP_SHA_WITH_AES_128_CBC_SHA,
        TLS1_RFC_SRP_SHA_WITH_AES_128_CBC_SHA,
        TLS1_CK_SRP_SHA_WITH_AES_128_CBC_SHA,
        SSL_kSRP,
        SSL_aSRP,
        SSL_AES128,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_HIGH,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_SRP_SHA_RSA_WITH_AES_128_CBC_SHA,
        TLS1_RFC_SRP_SHA_RSA_WITH_AES_128_CBC_SHA,
        TLS1_CK_SRP_SHA_RSA_WITH_AES_128_CBC_SHA,
        SSL_kSRP,
        SSL_aRSA,
        SSL_AES128,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_HIGH,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_SRP_SHA_DSS_WITH_AES_128_CBC_SHA,
        TLS1_RFC_SRP_SHA_DSS_WITH_AES_128_CBC_SHA,
        TLS1_CK_SRP_SHA_DSS_WITH_AES_128_CBC_SHA,
        SSL_kSRP,
        SSL_aDSS,
        SSL_AES128,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_SRP_SHA_WITH_AES_256_CBC_SHA,
        TLS1_RFC_SRP_SHA_WITH_AES_256_CBC_SHA,
        TLS1_CK_SRP_SHA_WITH_AES_256_CBC_SHA,
        SSL_kSRP,
        SSL_aSRP,
        SSL_AES256,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_HIGH,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_SRP_SHA_RSA_WITH_AES_256_CBC_SHA,
        TLS1_RFC_SRP_SHA_RSA_WITH_AES_256_CBC_SHA,
        TLS1_CK_SRP_SHA_RSA_WITH_AES_256_CBC_SHA,
        SSL_kSRP,
        SSL_aRSA,
        SSL_AES256,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_HIGH,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_SRP_SHA_DSS_WITH_AES_256_CBC_SHA,
        TLS1_RFC_SRP_SHA_DSS_WITH_AES_256_CBC_SHA,
        TLS1_CK_SRP_SHA_DSS_WITH_AES_256_CBC_SHA,
        SSL_kSRP,
        SSL_aDSS,
        SSL_AES256,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        256,
        256,
    },

    {
        1,
        TLS1_TXT_DHE_RSA_WITH_CHACHA20_POLY1305,
        TLS1_RFC_DHE_RSA_WITH_CHACHA20_POLY1305,
        TLS1_CK_DHE_RSA_WITH_CHACHA20_POLY1305,
        SSL_kDHE,
        SSL_aRSA,
        SSL_CHACHA20POLY1305,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_ECDHE_RSA_WITH_CHACHA20_POLY1305,
        TLS1_RFC_ECDHE_RSA_WITH_CHACHA20_POLY1305,
        TLS1_CK_ECDHE_RSA_WITH_CHACHA20_POLY1305,
        SSL_kECDHE,
        SSL_aRSA,
        SSL_CHACHA20POLY1305,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_ECDHE_ECDSA_WITH_CHACHA20_POLY1305,
        TLS1_RFC_ECDHE_ECDSA_WITH_CHACHA20_POLY1305,
        TLS1_CK_ECDHE_ECDSA_WITH_CHACHA20_POLY1305,
        SSL_kECDHE,
        SSL_aECDSA,
        SSL_CHACHA20POLY1305,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_PSK_WITH_CHACHA20_POLY1305,
        TLS1_RFC_PSK_WITH_CHACHA20_POLY1305,
        TLS1_CK_PSK_WITH_CHACHA20_POLY1305,
        SSL_kPSK,
        SSL_aPSK,
        SSL_CHACHA20POLY1305,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_ECDHE_PSK_WITH_CHACHA20_POLY1305,
        TLS1_RFC_ECDHE_PSK_WITH_CHACHA20_POLY1305,
        TLS1_CK_ECDHE_PSK_WITH_CHACHA20_POLY1305,
        SSL_kECDHEPSK,
        SSL_aPSK,
        SSL_CHACHA20POLY1305,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_DHE_PSK_WITH_CHACHA20_POLY1305,
        TLS1_RFC_DHE_PSK_WITH_CHACHA20_POLY1305,
        TLS1_CK_DHE_PSK_WITH_CHACHA20_POLY1305,
        SSL_kDHEPSK,
        SSL_aPSK,
        SSL_CHACHA20POLY1305,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_RSA_PSK_WITH_CHACHA20_POLY1305,
        TLS1_RFC_RSA_PSK_WITH_CHACHA20_POLY1305,
        TLS1_CK_RSA_PSK_WITH_CHACHA20_POLY1305,
        SSL_kRSAPSK,
        SSL_aRSA,
        SSL_CHACHA20POLY1305,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        256,
        256,
    },

    {
        1,
        TLS1_TXT_RSA_WITH_CAMELLIA_128_CBC_SHA256,
        TLS1_RFC_RSA_WITH_CAMELLIA_128_CBC_SHA256,
        TLS1_CK_RSA_WITH_CAMELLIA_128_CBC_SHA256,
        SSL_kRSA,
        SSL_aRSA,
        SSL_CAMELLIA128,
        SSL_SHA256,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA256,
        TLS1_RFC_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA256,
        TLS1_CK_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA256,
        SSL_kDHE,
        SSL_aDSS,
        SSL_CAMELLIA128,
        SSL_SHA256,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256,
        TLS1_RFC_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256,
        TLS1_CK_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256,
        SSL_kDHE,
        SSL_aRSA,
        SSL_CAMELLIA128,
        SSL_SHA256,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_ADH_WITH_CAMELLIA_128_CBC_SHA256,
        TLS1_RFC_ADH_WITH_CAMELLIA_128_CBC_SHA256,
        TLS1_CK_ADH_WITH_CAMELLIA_128_CBC_SHA256,
        SSL_kDHE,
        SSL_aNULL,
        SSL_CAMELLIA128,
        SSL_SHA256,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_RSA_WITH_CAMELLIA_256_CBC_SHA256,
        TLS1_RFC_RSA_WITH_CAMELLIA_256_CBC_SHA256,
        TLS1_CK_RSA_WITH_CAMELLIA_256_CBC_SHA256,
        SSL_kRSA,
        SSL_aRSA,
        SSL_CAMELLIA256,
        SSL_SHA256,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA256,
        TLS1_RFC_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA256,
        TLS1_CK_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA256,
        SSL_kDHE,
        SSL_aDSS,
        SSL_CAMELLIA256,
        SSL_SHA256,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256,
        TLS1_RFC_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256,
        TLS1_CK_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256,
        SSL_kDHE,
        SSL_aRSA,
        SSL_CAMELLIA256,
        SSL_SHA256,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_ADH_WITH_CAMELLIA_256_CBC_SHA256,
        TLS1_RFC_ADH_WITH_CAMELLIA_256_CBC_SHA256,
        TLS1_CK_ADH_WITH_CAMELLIA_256_CBC_SHA256,
        SSL_kDHE,
        SSL_aNULL,
        SSL_CAMELLIA256,
        SSL_SHA256,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_RSA_WITH_CAMELLIA_256_CBC_SHA,
        TLS1_RFC_RSA_WITH_CAMELLIA_256_CBC_SHA,
        TLS1_CK_RSA_WITH_CAMELLIA_256_CBC_SHA,
        SSL_kRSA,
        SSL_aRSA,
        SSL_CAMELLIA256,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA,
        TLS1_RFC_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA,
        TLS1_CK_DHE_DSS_WITH_CAMELLIA_256_CBC_SHA,
        SSL_kDHE,
        SSL_aDSS,
        SSL_CAMELLIA256,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA,
        TLS1_RFC_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA,
        TLS1_CK_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA,
        SSL_kDHE,
        SSL_aRSA,
        SSL_CAMELLIA256,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_ADH_WITH_CAMELLIA_256_CBC_SHA,
        TLS1_RFC_ADH_WITH_CAMELLIA_256_CBC_SHA,
        TLS1_CK_ADH_WITH_CAMELLIA_256_CBC_SHA,
        SSL_kDHE,
        SSL_aNULL,
        SSL_CAMELLIA256,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_RSA_WITH_CAMELLIA_128_CBC_SHA,
        TLS1_RFC_RSA_WITH_CAMELLIA_128_CBC_SHA,
        TLS1_CK_RSA_WITH_CAMELLIA_128_CBC_SHA,
        SSL_kRSA,
        SSL_aRSA,
        SSL_CAMELLIA128,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA,
        TLS1_RFC_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA,
        TLS1_CK_DHE_DSS_WITH_CAMELLIA_128_CBC_SHA,
        SSL_kDHE,
        SSL_aDSS,
        SSL_CAMELLIA128,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA,
        TLS1_RFC_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA,
        TLS1_CK_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA,
        SSL_kDHE,
        SSL_aRSA,
        SSL_CAMELLIA128,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_ADH_WITH_CAMELLIA_128_CBC_SHA,
        TLS1_RFC_ADH_WITH_CAMELLIA_128_CBC_SHA,
        TLS1_CK_ADH_WITH_CAMELLIA_128_CBC_SHA,
        SSL_kDHE,
        SSL_aNULL,
        SSL_CAMELLIA128,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_ECDHE_ECDSA_WITH_CAMELLIA_128_CBC_SHA256,
        TLS1_RFC_ECDHE_ECDSA_WITH_CAMELLIA_128_CBC_SHA256,
        TLS1_CK_ECDHE_ECDSA_WITH_CAMELLIA_128_CBC_SHA256,
        SSL_kECDHE,
        SSL_aECDSA,
        SSL_CAMELLIA128,
        SSL_SHA256,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_ECDHE_ECDSA_WITH_CAMELLIA_256_CBC_SHA384,
        TLS1_RFC_ECDHE_ECDSA_WITH_CAMELLIA_256_CBC_SHA384,
        TLS1_CK_ECDHE_ECDSA_WITH_CAMELLIA_256_CBC_SHA384,
        SSL_kECDHE,
        SSL_aECDSA,
        SSL_CAMELLIA256,
        SSL_SHA384,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_ECDHE_RSA_WITH_CAMELLIA_128_CBC_SHA256,
        TLS1_RFC_ECDHE_RSA_WITH_CAMELLIA_128_CBC_SHA256,
        TLS1_CK_ECDHE_RSA_WITH_CAMELLIA_128_CBC_SHA256,
        SSL_kECDHE,
        SSL_aRSA,
        SSL_CAMELLIA128,
        SSL_SHA256,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_ECDHE_RSA_WITH_CAMELLIA_256_CBC_SHA384,
        TLS1_RFC_ECDHE_RSA_WITH_CAMELLIA_256_CBC_SHA384,
        TLS1_CK_ECDHE_RSA_WITH_CAMELLIA_256_CBC_SHA384,
        SSL_kECDHE,
        SSL_aRSA,
        SSL_CAMELLIA256,
        SSL_SHA384,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_PSK_WITH_CAMELLIA_128_CBC_SHA256,
        TLS1_RFC_PSK_WITH_CAMELLIA_128_CBC_SHA256,
        TLS1_CK_PSK_WITH_CAMELLIA_128_CBC_SHA256,
        SSL_kPSK,
        SSL_aPSK,
        SSL_CAMELLIA128,
        SSL_SHA256,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_PSK_WITH_CAMELLIA_256_CBC_SHA384,
        TLS1_RFC_PSK_WITH_CAMELLIA_256_CBC_SHA384,
        TLS1_CK_PSK_WITH_CAMELLIA_256_CBC_SHA384,
        SSL_kPSK,
        SSL_aPSK,
        SSL_CAMELLIA256,
        SSL_SHA384,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_DHE_PSK_WITH_CAMELLIA_128_CBC_SHA256,
        TLS1_RFC_DHE_PSK_WITH_CAMELLIA_128_CBC_SHA256,
        TLS1_CK_DHE_PSK_WITH_CAMELLIA_128_CBC_SHA256,
        SSL_kDHEPSK,
        SSL_aPSK,
        SSL_CAMELLIA128,
        SSL_SHA256,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_DHE_PSK_WITH_CAMELLIA_256_CBC_SHA384,
        TLS1_RFC_DHE_PSK_WITH_CAMELLIA_256_CBC_SHA384,
        TLS1_CK_DHE_PSK_WITH_CAMELLIA_256_CBC_SHA384,
        SSL_kDHEPSK,
        SSL_aPSK,
        SSL_CAMELLIA256,
        SSL_SHA384,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_RSA_PSK_WITH_CAMELLIA_128_CBC_SHA256,
        TLS1_RFC_RSA_PSK_WITH_CAMELLIA_128_CBC_SHA256,
        TLS1_CK_RSA_PSK_WITH_CAMELLIA_128_CBC_SHA256,
        SSL_kRSAPSK,
        SSL_aRSA,
        SSL_CAMELLIA128,
        SSL_SHA256,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_RSA_PSK_WITH_CAMELLIA_256_CBC_SHA384,
        TLS1_RFC_RSA_PSK_WITH_CAMELLIA_256_CBC_SHA384,
        TLS1_CK_RSA_PSK_WITH_CAMELLIA_256_CBC_SHA384,
        SSL_kRSAPSK,
        SSL_aRSA,
        SSL_CAMELLIA256,
        SSL_SHA384,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_ECDHE_PSK_WITH_CAMELLIA_128_CBC_SHA256,
        TLS1_RFC_ECDHE_PSK_WITH_CAMELLIA_128_CBC_SHA256,
        TLS1_CK_ECDHE_PSK_WITH_CAMELLIA_128_CBC_SHA256,
        SSL_kECDHEPSK,
        SSL_aPSK,
        SSL_CAMELLIA128,
        SSL_SHA256,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_ECDHE_PSK_WITH_CAMELLIA_256_CBC_SHA384,
        TLS1_RFC_ECDHE_PSK_WITH_CAMELLIA_256_CBC_SHA384,
        TLS1_CK_ECDHE_PSK_WITH_CAMELLIA_256_CBC_SHA384,
        SSL_kECDHEPSK,
        SSL_aPSK,
        SSL_CAMELLIA256,
        SSL_SHA384,
        TLS1_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        256,
        256,
    },

#ifndef OPENSSL_NO_GOST
    {
        1,
        "GOST2001-GOST89-GOST89",
        "TLS_GOSTR341001_WITH_28147_CNT_IMIT",
        0x3000081,
        SSL_kGOST,
        SSL_aGOST01,
        SSL_eGOST2814789CNT,
        SSL_GOST89MAC,
        TLS1_VERSION,
        TLS1_2_VERSION,
        0,
        0,
        SSL_HIGH,
        SSL_HANDSHAKE_MAC_GOST94 | TLS1_PRF_GOST94 | TLS1_STREAM_MAC,
        256,
        256,
    },
    {
        1,
        "GOST2001-NULL-GOST94",
        "TLS_GOSTR341001_WITH_NULL_GOSTR3411",
        0x3000083,
        SSL_kGOST,
        SSL_aGOST01,
        SSL_eNULL,
        SSL_GOST94,
        TLS1_VERSION,
        TLS1_2_VERSION,
        0,
        0,
        SSL_STRONG_NONE,
        SSL_HANDSHAKE_MAC_GOST94 | TLS1_PRF_GOST94,
        0,
        0,
    },
    {
        1,
        "IANA-GOST2012-GOST8912-GOST8912",
        NULL,
        0x0300c102,
        SSL_kGOST,
        SSL_aGOST12 | SSL_aGOST01,
        SSL_eGOST2814789CNT12,
        SSL_GOST89MAC12,
        TLS1_VERSION,
        TLS1_2_VERSION,
        0,
        0,
        SSL_HIGH,
        SSL_HANDSHAKE_MAC_GOST12_256 | TLS1_PRF_GOST12_256 | TLS1_STREAM_MAC,
        256,
        256,
    },
    {
        1,
        "LEGACY-GOST2012-GOST8912-GOST8912",
        NULL,
        0x0300ff85,
        SSL_kGOST,
        SSL_aGOST12 | SSL_aGOST01,
        SSL_eGOST2814789CNT12,
        SSL_GOST89MAC12,
        TLS1_VERSION,
        TLS1_2_VERSION,
        0,
        0,
        SSL_HIGH,
        SSL_HANDSHAKE_MAC_GOST12_256 | TLS1_PRF_GOST12_256 | TLS1_STREAM_MAC,
        256,
        256,
    },
    {
        1,
        "GOST2012-NULL-GOST12",
        NULL,
        0x0300ff87,
        SSL_kGOST,
        SSL_aGOST12 | SSL_aGOST01,
        SSL_eNULL,
        SSL_GOST12_256,
        TLS1_VERSION,
        TLS1_2_VERSION,
        0,
        0,
        SSL_STRONG_NONE,
        SSL_HANDSHAKE_MAC_GOST12_256 | TLS1_PRF_GOST12_256 | TLS1_STREAM_MAC,
        0,
        0,
    },
    {
        1,
        "GOST2012-KUZNYECHIK-KUZNYECHIKOMAC",
        NULL,
        0x0300C100,
        SSL_kGOST18,
        SSL_aGOST12,
        SSL_KUZNYECHIK,
        SSL_KUZNYECHIKOMAC,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        0,
        0,
        SSL_HIGH,
        SSL_HANDSHAKE_MAC_GOST12_256 | TLS1_PRF_GOST12_256 | TLS1_TLSTREE,
        256,
        256,
    },
    {
        1,
        "GOST2012-MAGMA-MAGMAOMAC",
        NULL,
        0x0300C101,
        SSL_kGOST18,
        SSL_aGOST12,
        SSL_MAGMA,
        SSL_MAGMAOMAC,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        0,
        0,
        SSL_HIGH,
        SSL_HANDSHAKE_MAC_GOST12_256 | TLS1_PRF_GOST12_256 | TLS1_TLSTREE,
        256,
        256,
    },
#endif /* OPENSSL_NO_GOST */

    {
        1,
        SSL3_TXT_RSA_IDEA_128_SHA,
        SSL3_RFC_RSA_IDEA_128_SHA,
        SSL3_CK_RSA_IDEA_128_SHA,
        SSL_kRSA,
        SSL_aRSA,
        SSL_IDEA,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_1_VERSION,
        DTLS1_BAD_VER,
        DTLS1_VERSION,
        SSL_NOT_DEFAULT | SSL_MEDIUM,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },

    {
        1,
        TLS1_TXT_RSA_WITH_SEED_SHA,
        TLS1_RFC_RSA_WITH_SEED_SHA,
        TLS1_CK_RSA_WITH_SEED_SHA,
        SSL_kRSA,
        SSL_aRSA,
        SSL_SEED,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_MEDIUM,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_DHE_DSS_WITH_SEED_SHA,
        TLS1_RFC_DHE_DSS_WITH_SEED_SHA,
        TLS1_CK_DHE_DSS_WITH_SEED_SHA,
        SSL_kDHE,
        SSL_aDSS,
        SSL_SEED,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_MEDIUM,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_DHE_RSA_WITH_SEED_SHA,
        TLS1_RFC_DHE_RSA_WITH_SEED_SHA,
        TLS1_CK_DHE_RSA_WITH_SEED_SHA,
        SSL_kDHE,
        SSL_aRSA,
        SSL_SEED,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_MEDIUM,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_ADH_WITH_SEED_SHA,
        TLS1_RFC_ADH_WITH_SEED_SHA,
        TLS1_CK_ADH_WITH_SEED_SHA,
        SSL_kDHE,
        SSL_aNULL,
        SSL_SEED,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        DTLS1_BAD_VER,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_MEDIUM,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },

#ifndef OPENSSL_NO_WEAK_SSL_CIPHERS
    {
        1,
        SSL3_TXT_RSA_RC4_128_MD5,
        SSL3_RFC_RSA_RC4_128_MD5,
        SSL3_CK_RSA_RC4_128_MD5,
        SSL_kRSA,
        SSL_aRSA,
        SSL_RC4,
        SSL_MD5,
        SSL3_VERSION,
        TLS1_2_VERSION,
        0,
        0,
        SSL_NOT_DEFAULT | SSL_MEDIUM,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        SSL3_TXT_RSA_RC4_128_SHA,
        SSL3_RFC_RSA_RC4_128_SHA,
        SSL3_CK_RSA_RC4_128_SHA,
        SSL_kRSA,
        SSL_aRSA,
        SSL_RC4,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        0,
        0,
        SSL_NOT_DEFAULT | SSL_MEDIUM,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        SSL3_TXT_ADH_RC4_128_MD5,
        SSL3_RFC_ADH_RC4_128_MD5,
        SSL3_CK_ADH_RC4_128_MD5,
        SSL_kDHE,
        SSL_aNULL,
        SSL_RC4,
        SSL_MD5,
        SSL3_VERSION,
        TLS1_2_VERSION,
        0,
        0,
        SSL_NOT_DEFAULT | SSL_MEDIUM,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_ECDHE_PSK_WITH_RC4_128_SHA,
        TLS1_RFC_ECDHE_PSK_WITH_RC4_128_SHA,
        TLS1_CK_ECDHE_PSK_WITH_RC4_128_SHA,
        SSL_kECDHEPSK,
        SSL_aPSK,
        SSL_RC4,
        SSL_SHA1,
        TLS1_VERSION,
        TLS1_2_VERSION,
        0,
        0,
        SSL_NOT_DEFAULT | SSL_MEDIUM,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_ECDH_anon_WITH_RC4_128_SHA,
        TLS1_RFC_ECDH_anon_WITH_RC4_128_SHA,
        TLS1_CK_ECDH_anon_WITH_RC4_128_SHA,
        SSL_kECDHE,
        SSL_aNULL,
        SSL_RC4,
        SSL_SHA1,
        TLS1_VERSION,
        TLS1_2_VERSION,
        0,
        0,
        SSL_NOT_DEFAULT | SSL_MEDIUM,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_ECDHE_ECDSA_WITH_RC4_128_SHA,
        TLS1_RFC_ECDHE_ECDSA_WITH_RC4_128_SHA,
        TLS1_CK_ECDHE_ECDSA_WITH_RC4_128_SHA,
        SSL_kECDHE,
        SSL_aECDSA,
        SSL_RC4,
        SSL_SHA1,
        TLS1_VERSION,
        TLS1_2_VERSION,
        0,
        0,
        SSL_NOT_DEFAULT | SSL_MEDIUM,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_ECDHE_RSA_WITH_RC4_128_SHA,
        TLS1_RFC_ECDHE_RSA_WITH_RC4_128_SHA,
        TLS1_CK_ECDHE_RSA_WITH_RC4_128_SHA,
        SSL_kECDHE,
        SSL_aRSA,
        SSL_RC4,
        SSL_SHA1,
        TLS1_VERSION,
        TLS1_2_VERSION,
        0,
        0,
        SSL_NOT_DEFAULT | SSL_MEDIUM,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_PSK_WITH_RC4_128_SHA,
        TLS1_RFC_PSK_WITH_RC4_128_SHA,
        TLS1_CK_PSK_WITH_RC4_128_SHA,
        SSL_kPSK,
        SSL_aPSK,
        SSL_RC4,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        0,
        0,
        SSL_NOT_DEFAULT | SSL_MEDIUM,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_RSA_PSK_WITH_RC4_128_SHA,
        TLS1_RFC_RSA_PSK_WITH_RC4_128_SHA,
        TLS1_CK_RSA_PSK_WITH_RC4_128_SHA,
        SSL_kRSAPSK,
        SSL_aRSA,
        SSL_RC4,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        0,
        0,
        SSL_NOT_DEFAULT | SSL_MEDIUM,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_DHE_PSK_WITH_RC4_128_SHA,
        TLS1_RFC_DHE_PSK_WITH_RC4_128_SHA,
        TLS1_CK_DHE_PSK_WITH_RC4_128_SHA,
        SSL_kDHEPSK,
        SSL_aPSK,
        SSL_RC4,
        SSL_SHA1,
        SSL3_VERSION,
        TLS1_2_VERSION,
        0,
        0,
        SSL_NOT_DEFAULT | SSL_MEDIUM,
        SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF,
        128,
        128,
    },
#endif /* OPENSSL_NO_WEAK_SSL_CIPHERS */

    {
        1,
        TLS1_TXT_RSA_WITH_ARIA_128_GCM_SHA256,
        TLS1_RFC_RSA_WITH_ARIA_128_GCM_SHA256,
        TLS1_CK_RSA_WITH_ARIA_128_GCM_SHA256,
        SSL_kRSA,
        SSL_aRSA,
        SSL_ARIA128GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_RSA_WITH_ARIA_256_GCM_SHA384,
        TLS1_RFC_RSA_WITH_ARIA_256_GCM_SHA384,
        TLS1_CK_RSA_WITH_ARIA_256_GCM_SHA384,
        SSL_kRSA,
        SSL_aRSA,
        SSL_ARIA256GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_DHE_RSA_WITH_ARIA_128_GCM_SHA256,
        TLS1_RFC_DHE_RSA_WITH_ARIA_128_GCM_SHA256,
        TLS1_CK_DHE_RSA_WITH_ARIA_128_GCM_SHA256,
        SSL_kDHE,
        SSL_aRSA,
        SSL_ARIA128GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_DHE_RSA_WITH_ARIA_256_GCM_SHA384,
        TLS1_RFC_DHE_RSA_WITH_ARIA_256_GCM_SHA384,
        TLS1_CK_DHE_RSA_WITH_ARIA_256_GCM_SHA384,
        SSL_kDHE,
        SSL_aRSA,
        SSL_ARIA256GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_DHE_DSS_WITH_ARIA_128_GCM_SHA256,
        TLS1_RFC_DHE_DSS_WITH_ARIA_128_GCM_SHA256,
        TLS1_CK_DHE_DSS_WITH_ARIA_128_GCM_SHA256,
        SSL_kDHE,
        SSL_aDSS,
        SSL_ARIA128GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_DHE_DSS_WITH_ARIA_256_GCM_SHA384,
        TLS1_RFC_DHE_DSS_WITH_ARIA_256_GCM_SHA384,
        TLS1_CK_DHE_DSS_WITH_ARIA_256_GCM_SHA384,
        SSL_kDHE,
        SSL_aDSS,
        SSL_ARIA256GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_ECDHE_ECDSA_WITH_ARIA_128_GCM_SHA256,
        TLS1_RFC_ECDHE_ECDSA_WITH_ARIA_128_GCM_SHA256,
        TLS1_CK_ECDHE_ECDSA_WITH_ARIA_128_GCM_SHA256,
        SSL_kECDHE,
        SSL_aECDSA,
        SSL_ARIA128GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_ECDHE_ECDSA_WITH_ARIA_256_GCM_SHA384,
        TLS1_RFC_ECDHE_ECDSA_WITH_ARIA_256_GCM_SHA384,
        TLS1_CK_ECDHE_ECDSA_WITH_ARIA_256_GCM_SHA384,
        SSL_kECDHE,
        SSL_aECDSA,
        SSL_ARIA256GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_ECDHE_RSA_WITH_ARIA_128_GCM_SHA256,
        TLS1_RFC_ECDHE_RSA_WITH_ARIA_128_GCM_SHA256,
        TLS1_CK_ECDHE_RSA_WITH_ARIA_128_GCM_SHA256,
        SSL_kECDHE,
        SSL_aRSA,
        SSL_ARIA128GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_ECDHE_RSA_WITH_ARIA_256_GCM_SHA384,
        TLS1_RFC_ECDHE_RSA_WITH_ARIA_256_GCM_SHA384,
        TLS1_CK_ECDHE_RSA_WITH_ARIA_256_GCM_SHA384,
        SSL_kECDHE,
        SSL_aRSA,
        SSL_ARIA256GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_PSK_WITH_ARIA_128_GCM_SHA256,
        TLS1_RFC_PSK_WITH_ARIA_128_GCM_SHA256,
        TLS1_CK_PSK_WITH_ARIA_128_GCM_SHA256,
        SSL_kPSK,
        SSL_aPSK,
        SSL_ARIA128GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_PSK_WITH_ARIA_256_GCM_SHA384,
        TLS1_RFC_PSK_WITH_ARIA_256_GCM_SHA384,
        TLS1_CK_PSK_WITH_ARIA_256_GCM_SHA384,
        SSL_kPSK,
        SSL_aPSK,
        SSL_ARIA256GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_DHE_PSK_WITH_ARIA_128_GCM_SHA256,
        TLS1_RFC_DHE_PSK_WITH_ARIA_128_GCM_SHA256,
        TLS1_CK_DHE_PSK_WITH_ARIA_128_GCM_SHA256,
        SSL_kDHEPSK,
        SSL_aPSK,
        SSL_ARIA128GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_DHE_PSK_WITH_ARIA_256_GCM_SHA384,
        TLS1_RFC_DHE_PSK_WITH_ARIA_256_GCM_SHA384,
        TLS1_CK_DHE_PSK_WITH_ARIA_256_GCM_SHA384,
        SSL_kDHEPSK,
        SSL_aPSK,
        SSL_ARIA256GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        256,
        256,
    },
    {
        1,
        TLS1_TXT_RSA_PSK_WITH_ARIA_128_GCM_SHA256,
        TLS1_RFC_RSA_PSK_WITH_ARIA_128_GCM_SHA256,
        TLS1_CK_RSA_PSK_WITH_ARIA_128_GCM_SHA256,
        SSL_kRSAPSK,
        SSL_aRSA,
        SSL_ARIA128GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256,
        128,
        128,
    },
    {
        1,
        TLS1_TXT_RSA_PSK_WITH_ARIA_256_GCM_SHA384,
        TLS1_RFC_RSA_PSK_WITH_ARIA_256_GCM_SHA384,
        TLS1_CK_RSA_PSK_WITH_ARIA_256_GCM_SHA384,
        SSL_kRSAPSK,
        SSL_aRSA,
        SSL_ARIA256GCM,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        DTLS1_2_VERSION,
        DTLS1_2_VERSION,
        SSL_NOT_DEFAULT | SSL_HIGH,
        SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384,
        256,
        256,
    },
    {
        1,
        BTLS1_TXT_DHE_BIGN_WITH_BELT_CTR_MAC_HBELT,
        BTLS1_RFC_DHE_BIGN_WITH_BELT_CTR_MAC_HBELT,
        0x0300ff15,
        SSL_kBDHE,
        SSL_aBIGN,
        SSL_BELTCTR,
        SSL_BELTMAC,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        0,
        0,
        SSL_HIGH,
        SSL_HANDSHAKE_MAC_HBELT | TLS1_PRF_HBELT,
        256,
        256,
    },
    {
        1,
        BTLS1_TXT_DHE_BIGN_WITH_BELT_DWP_HBELT,
        BTLS1_RFC_DHE_BIGN_WITH_BELT_DWP_HBELT,
        0x0300ff16,
        SSL_kBDHE,
        SSL_aBIGN,
        SSL_BELTDWP,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        0,
        0,
        SSL_HIGH,
        SSL_HANDSHAKE_MAC_HBELT | TLS1_PRF_HBELT,
        256,
        256,
    },
    {
        1,
        BTLS1_TXT_DHT_BIGN_WITH_BELT_CTR_MAC_HBELT,
        BTLS1_RFC_DHT_BIGN_WITH_BELT_CTR_MAC_HBELT,
        0x0300ff17,
        SSL_kBDHT,
        SSL_aBIGN,
        SSL_BELTCTR,
        SSL_BELTMAC,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        0,
        0,
        SSL_HIGH,
        SSL_HANDSHAKE_MAC_HBELT | TLS1_PRF_HBELT,
        256,
        256,
    },
    {
        1,
        BTLS1_TXT_DHT_BIGN_WITH_BELT_DWP_HBELT,
        BTLS1_RFC_DHT_BIGN_WITH_BELT_DWP_HBELT,
        0x0300ff18,
        SSL_kBDHT,
        SSL_aBIGN,
        SSL_BELTDWP,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        0,
        0,
        SSL_HIGH,
        SSL_HANDSHAKE_MAC_HBELT | TLS1_PRF_HBELT,
        256,
        256,
    },

    {
        1,
        BTLS1_TXT_DHE_PSK_BIGN_WITH_BELT_CTR_MAC_HBELT,
        BTLS1_RFC_DHE_PSK_BIGN_WITH_BELT_CTR_MAC_HBELT,
        0x0300ff19,
        SSL_kBDHEPSK,
        SSL_aPSK,
        SSL_BELTCTR,
        SSL_BELTMAC,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        0,
        0,
        SSL_HIGH,
        SSL_HANDSHAKE_MAC_HBELT | TLS1_PRF_HBELT,
        256,
        256,
    },
    {
        1,
        BTLS1_TXT_DHE_PSK_BIGN_WITH_BELT_DWP_HBELT,
        BTLS1_RFC_DHE_PSK_BIGN_WITH_BELT_DWP_HBELT,
        0x0300ff1a,
        SSL_kBDHEPSK,
        SSL_aPSK,
        SSL_BELTDWP,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        0,
        0,
        SSL_HIGH,
        SSL_HANDSHAKE_MAC_HBELT | TLS1_PRF_HBELT,
        256,
        256,
    },
    {
        1,
        BTLS1_TXT_DHT_PSK_BIGN_WITH_BELT_CTR_MAC_HBELT,
        BTLS1_RFC_DHT_PSK_BIGN_WITH_BELT_CTR_MAC_HBELT,
        0x0300ff1b,
        SSL_kBDHTPSK,
        SSL_aBIGN,
        SSL_BELTCTR,
        SSL_BELTMAC,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        0,
        0,
        SSL_HIGH,
        SSL_HANDSHAKE_MAC_HBELT | TLS1_PRF_HBELT,
        256,
        256,
    },
    {
        1,
        BTLS1_TXT_DHT_PSK_BIGN_WITH_BELT_DWP_HBELT,
        BTLS1_RFC_DHT_PSK_BIGN_WITH_BELT_DWP_HBELT,
        0x0300ff1c,
        SSL_kBDHTPSK,
        SSL_aBIGN,
        SSL_BELTDWP,
        SSL_AEAD,
        TLS1_2_VERSION,
        TLS1_2_VERSION,
        0,
        0,
        SSL_HIGH,
        SSL_HANDSHAKE_MAC_HBELT | TLS1_PRF_HBELT,
        256,
        256,
    },
};

/*
 * The list of known Signalling Cipher-Suite Value "ciphers", non-valid
 * values stuffed into the ciphers field of the wire protocol for signalling
 * purposes.
 */
static SSL_CIPHER ssl3_scsvs[] = {
    {
        0,
        "TLS_EMPTY_RENEGOTIATION_INFO_SCSV",
        "TLS_EMPTY_RENEGOTIATION_INFO_SCSV",
        SSL3_CK_SCSV,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
    },
    {
        0,
        "TLS_FALLBACK_SCSV",
        "TLS_FALLBACK_SCSV",
        SSL3_CK_FALLBACK_SCSV,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
    },
};

static int cipher_compare(const void *a, const void *b)
{
    const SSL_CIPHER *ap = (const SSL_CIPHER *)a;
    const SSL_CIPHER *bp = (const SSL_CIPHER *)b;

    if (ap->id == bp->id)
        return 0;
    return ap->id < bp->id ? -1 : 1;
}

void ssl_sort_cipher_list(void)
{
    qsort(tls13_ciphers, TLS13_NUM_CIPHERS, sizeof(tls13_ciphers[0]),
          cipher_compare);
    qsort(ssl3_ciphers, SSL3_NUM_CIPHERS, sizeof(ssl3_ciphers[0]),
          cipher_compare);
    qsort(ssl3_scsvs, SSL3_NUM_SCSVS, sizeof(ssl3_scsvs[0]), cipher_compare);
}

static int ssl_undefined_function_1(SSL *ssl, unsigned char *r, size_t s,
                                    const char *t, size_t u,
                                    const unsigned char *v, size_t w, int x)
{
    (void)r;
    (void)s;
    (void)t;
    (void)u;
    (void)v;
    (void)w;
    (void)x;
    return ssl_undefined_function(ssl);
}

const SSL3_ENC_METHOD SSLv3_enc_data = {
    ssl3_enc,
    n_ssl3_mac,
    ssl3_setup_key_block,
    ssl3_generate_master_secret,
    ssl3_change_cipher_state,
    ssl3_final_finish_mac,
    SSL3_MD_CLIENT_FINISHED_CONST, 4,
    SSL3_MD_SERVER_FINISHED_CONST, 4,
    ssl3_alert_code,
    ssl_undefined_function_1,
    0,
    ssl3_set_handshake_header,
    tls_close_construct_packet,
    ssl3_handshake_write};

long ssl3_default_timeout(void)
{
    /*
     * 2 hours, the 24 hours mentioned in the SSLv3 spec is way too long for
     * http, the cache would over fill
     */
    return (60 * 60 * 2);
}

int ssl3_num_ciphers(void)
{
    return SSL3_NUM_CIPHERS;
}

const SSL_CIPHER *ssl3_get_cipher(unsigned int u)
{
    if (u < SSL3_NUM_CIPHERS)
        return &(ssl3_ciphers[SSL3_NUM_CIPHERS - 1 - u]);
    else
        return NULL;
}

int ssl3_set_handshake_header(SSL *s, WPACKET *pkt, int htype)
{
    /* No header in the event of a CCS */
    if (htype == SSL3_MT_CHANGE_CIPHER_SPEC)
        return 1;

    /* Set the content type and 3 bytes for the message len */
    if (!WPACKET_put_bytes_u8(pkt, htype) || !WPACKET_start_sub_packet_u24(pkt))
        return 0;

    return 1;
}

int ssl3_handshake_write(SSL *s)
{
    return ssl3_do_write(s, SSL3_RT_HANDSHAKE);
}

int ssl3_new(SSL *s)
{
#ifndef OPENSSL_NO_SRP
    if (!ssl_srp_ctx_init_intern(s))
        return 0;
#endif

    if (!s->method->ssl_clear(s))
        return 0;

    return 1;
}

void ssl3_free(SSL *s)
{
    if (s == NULL)
        return;

    ssl3_cleanup_key_block(s);

    EVP_PKEY_free(s->s3.peer_tmp);
    s->s3.peer_tmp = NULL;
    EVP_PKEY_free(s->s3.tmp.pkey);
    s->s3.tmp.pkey = NULL;

    ssl_evp_cipher_free(s->s3.tmp.new_sym_enc);
    ssl_evp_md_free(s->s3.tmp.new_hash);

    OPENSSL_free(s->s3.tmp.ctype);
    sk_X509_NAME_pop_free(s->s3.tmp.peer_ca_names, X509_NAME_free);
    OPENSSL_free(s->s3.tmp.ciphers_raw);
    OPENSSL_clear_free(s->s3.tmp.pms, s->s3.tmp.pmslen);
    OPENSSL_free(s->s3.tmp.peer_sigalgs);
    OPENSSL_free(s->s3.tmp.peer_cert_sigalgs);
    ssl3_free_digest_list(s);
    OPENSSL_free(s->s3.alpn_selected);
    OPENSSL_free(s->s3.alpn_proposed);

#ifndef OPENSSL_NO_PSK
    OPENSSL_free(s->s3.tmp.psk);
#endif

#ifndef OPENSSL_NO_SRP
    ssl_srp_ctx_free_intern(s);
#endif
    memset(&s->s3, 0, sizeof(s->s3));
}

int ssl3_clear(SSL *s)
{
    ssl3_cleanup_key_block(s);
    OPENSSL_free(s->s3.tmp.ctype);
    sk_X509_NAME_pop_free(s->s3.tmp.peer_ca_names, X509_NAME_free);
    OPENSSL_free(s->s3.tmp.ciphers_raw);
    OPENSSL_clear_free(s->s3.tmp.pms, s->s3.tmp.pmslen);
    OPENSSL_free(s->s3.tmp.peer_sigalgs);
    OPENSSL_free(s->s3.tmp.peer_cert_sigalgs);

    EVP_PKEY_free(s->s3.tmp.pkey);
    EVP_PKEY_free(s->s3.peer_tmp);

    ssl3_free_digest_list(s);

    OPENSSL_free(s->s3.alpn_selected);
    OPENSSL_free(s->s3.alpn_proposed);

    /* NULL/zero-out everything in the s3 struct */
    memset(&s->s3, 0, sizeof(s->s3));

    if (!ssl_free_wbio_buffer(s))
        return 0;

    s->version = SSL3_VERSION;

#if !defined(OPENSSL_NO_NEXTPROTONEG)
    OPENSSL_free(s->ext.npn);
    s->ext.npn = NULL;
    s->ext.npn_len = 0;
#endif

    return 1;
}

#ifndef OPENSSL_NO_SRP
static char *srp_password_from_info_cb(SSL *s, void *arg)
{
    return OPENSSL_strdup(s->srp_ctx.info);
}
#endif

static int ssl3_set_req_cert_type(CERT *c, const unsigned char *p, size_t len);

long ssl3_ctrl(SSL *s, int cmd, long larg, void *parg)
{
    int ret = 0;

    switch (cmd)
    {
    case SSL_CTRL_GET_CLIENT_CERT_REQUEST:
        break;
    case SSL_CTRL_GET_NUM_RENEGOTIATIONS:
        ret = s->s3.num_renegotiations;
        break;
    case SSL_CTRL_CLEAR_NUM_RENEGOTIATIONS:
        ret = s->s3.num_renegotiations;
        s->s3.num_renegotiations = 0;
        break;
    case SSL_CTRL_GET_TOTAL_RENEGOTIATIONS:
        ret = s->s3.total_renegotiations;
        break;
    case SSL_CTRL_GET_FLAGS:
        ret = (int)(s->s3.flags);
        break;
#if !defined(OPENSSL_NO_DEPRECATED_3_0)
    case SSL_CTRL_SET_TMP_DH:
    {
        EVP_PKEY *pkdh = NULL;
        if (parg == NULL)
        {
            ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_NULL_PARAMETER);
            return 0;
        }
        pkdh = ssl_dh_to_pkey(parg);
        if (pkdh == NULL)
        {
            ERR_raise(ERR_LIB_SSL, ERR_R_MALLOC_FAILURE);
            return 0;
        }
        if (!SSL_set0_tmp_dh_pkey(s, pkdh))
        {
            EVP_PKEY_free(pkdh);
            return 0;
        }
        return 1;
    }
    break;
    case SSL_CTRL_SET_TMP_DH_CB:
    {
        ERR_raise(ERR_LIB_SSL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
        return ret;
    }
#endif
    case SSL_CTRL_SET_DH_AUTO:
        s->cert->dh_tmp_auto = larg;
        return 1;
#if !defined(OPENSSL_NO_DEPRECATED_3_0)
    case SSL_CTRL_SET_TMP_ECDH:
    {
        if (parg == NULL)
        {
            ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_NULL_PARAMETER);
            return 0;
        }
        return ssl_set_tmp_ecdh_groups(&s->ext.supportedgroups,
                                       &s->ext.supportedgroups_len,
                                       parg);
    }
#endif /* !OPENSSL_NO_DEPRECATED_3_0 */
    case SSL_CTRL_SET_TLSEXT_HOSTNAME:
        /*
         * This API is only used for a client to set what SNI it will request
         * from the server, but we currently allow it to be used on servers
         * as well, which is a programming error.  Currently we just clear
         * the field in SSL_do_handshake() for server SSLs, but when we can
         * make ABI-breaking changes, we may want to make use of this API
         * an error on server SSLs.
         */
        if (larg == TLSEXT_NAMETYPE_host_name)
        {
            size_t len;

            OPENSSL_free(s->ext.hostname);
            s->ext.hostname = NULL;

            ret = 1;
            if (parg == NULL)
                break;
            len = strlen((char *)parg);
            if (len == 0 || len > TLSEXT_MAXLEN_host_name)
            {
                ERR_raise(ERR_LIB_SSL, SSL_R_SSL3_EXT_INVALID_SERVERNAME);
                return 0;
            }
            if ((s->ext.hostname = OPENSSL_strdup((char *)parg)) == NULL)
            {
                ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
                return 0;
            }
        }
        else
        {
            ERR_raise(ERR_LIB_SSL, SSL_R_SSL3_EXT_INVALID_SERVERNAME_TYPE);
            return 0;
        }
        break;
    case SSL_CTRL_SET_TLSEXT_DEBUG_ARG:
        s->ext.debug_arg = parg;
        ret = 1;
        break;

    case SSL_CTRL_GET_TLSEXT_STATUS_REQ_TYPE:
        ret = s->ext.status_type;
        break;

    case SSL_CTRL_SET_TLSEXT_STATUS_REQ_TYPE:
        s->ext.status_type = larg;
        ret = 1;
        break;

    case SSL_CTRL_GET_TLSEXT_STATUS_REQ_EXTS:
        *(STACK_OF(X509_EXTENSION) **)parg = s->ext.ocsp.exts;
        ret = 1;
        break;

    case SSL_CTRL_SET_TLSEXT_STATUS_REQ_EXTS:
        s->ext.ocsp.exts = parg;
        ret = 1;
        break;

    case SSL_CTRL_GET_TLSEXT_STATUS_REQ_IDS:
        *(STACK_OF(OCSP_RESPID) **)parg = s->ext.ocsp.ids;
        ret = 1;
        break;

    case SSL_CTRL_SET_TLSEXT_STATUS_REQ_IDS:
        s->ext.ocsp.ids = parg;
        ret = 1;
        break;

    case SSL_CTRL_GET_TLSEXT_STATUS_REQ_OCSP_RESP:
        *(unsigned char **)parg = s->ext.ocsp.resp;
        if (s->ext.ocsp.resp_len == 0 || s->ext.ocsp.resp_len > LONG_MAX)
            return -1;
        return (long)s->ext.ocsp.resp_len;

    case SSL_CTRL_SET_TLSEXT_STATUS_REQ_OCSP_RESP:
        OPENSSL_free(s->ext.ocsp.resp);
        s->ext.ocsp.resp = parg;
        s->ext.ocsp.resp_len = larg;
        ret = 1;
        break;

    case SSL_CTRL_CHAIN:
        if (larg)
            return ssl_cert_set1_chain(s, NULL, (STACK_OF(X509) *)parg);
        else
            return ssl_cert_set0_chain(s, NULL, (STACK_OF(X509) *)parg);

    case SSL_CTRL_CHAIN_CERT:
        if (larg)
            return ssl_cert_add1_chain_cert(s, NULL, (X509 *)parg);
        else
            return ssl_cert_add0_chain_cert(s, NULL, (X509 *)parg);

    case SSL_CTRL_GET_CHAIN_CERTS:
        *(STACK_OF(X509) **)parg = s->cert->key->chain;
        ret = 1;
        break;

    case SSL_CTRL_SELECT_CURRENT_CERT:
        return ssl_cert_select_current(s->cert, (X509 *)parg);

    case SSL_CTRL_SET_CURRENT_CERT:
        if (larg == SSL_CERT_SET_SERVER)
        {
            const SSL_CIPHER *cipher;
            if (!s->server)
                return 0;
            cipher = s->s3.tmp.new_cipher;
            if (cipher == NULL)
                return 0;
            /*
             * No certificate for unauthenticated ciphersuites or using SRP
             * authentication
             */
            if (cipher->algorithm_auth & (SSL_aNULL | SSL_aSRP))
                return 2;
            if (s->s3.tmp.cert == NULL)
                return 0;
            s->cert->key = s->s3.tmp.cert;
            return 1;
        }
        return ssl_cert_set_current(s->cert, larg);

    case SSL_CTRL_GET_GROUPS:
    {
        uint16_t *clist;
        size_t clistlen;

        if (!s->session)
            return 0;
        clist = s->ext.peer_supportedgroups;
        clistlen = s->ext.peer_supportedgroups_len;
        if (parg)
        {
            size_t i;
            int *cptr = parg;

            for (i = 0; i < clistlen; i++)
            {
                const TLS_GROUP_INFO *cinf = tls1_group_id_lookup(s->ctx, clist[i]);

                if (cinf != NULL)
                    cptr[i] = tls1_group_id2nid(cinf->group_id, 1);
                else
                    cptr[i] = TLSEXT_nid_unknown | clist[i];
            }
        }
        return (int)clistlen;
    }

    case SSL_CTRL_SET_GROUPS:
        return tls1_set_groups(&s->ext.supportedgroups,
                               &s->ext.supportedgroups_len, parg, larg);

    case SSL_CTRL_SET_GROUPS_LIST:
        return tls1_set_groups_list(s->ctx, &s->ext.supportedgroups,
                                    &s->ext.supportedgroups_len, parg);

    case SSL_CTRL_GET_SHARED_GROUP:
    {
        uint16_t id = tls1_shared_group(s, larg);

        if (larg != -1)
            return tls1_group_id2nid(id, 1);
        return id;
    }
    case SSL_CTRL_GET_NEGOTIATED_GROUP:
    {
        unsigned int id;

        if (SSL_IS_TLS13(s) && s->s3.did_kex)
            id = s->s3.group_id;
        else
            id = s->session->kex_group;
        ret = tls1_group_id2nid(id, 1);
        break;
    }
    case SSL_CTRL_SET_SIGALGS:
        return tls1_set_sigalgs(s->cert, parg, larg, 0);

    case SSL_CTRL_SET_SIGALGS_LIST:
        return tls1_set_sigalgs_list(s->cert, parg, 0);

    case SSL_CTRL_SET_CLIENT_SIGALGS:
        return tls1_set_sigalgs(s->cert, parg, larg, 1);

    case SSL_CTRL_SET_CLIENT_SIGALGS_LIST:
        return tls1_set_sigalgs_list(s->cert, parg, 1);

    case SSL_CTRL_GET_CLIENT_CERT_TYPES:
    {
        const unsigned char **pctype = parg;
        if (s->server || !s->s3.tmp.cert_req)
            return 0;
        if (pctype)
            *pctype = s->s3.tmp.ctype;
        return s->s3.tmp.ctype_len;
    }

    case SSL_CTRL_SET_CLIENT_CERT_TYPES:
        if (!s->server)
            return 0;
        return ssl3_set_req_cert_type(s->cert, parg, larg);

    case SSL_CTRL_BUILD_CERT_CHAIN:
        return ssl_build_cert_chain(s, NULL, larg);

    case SSL_CTRL_SET_VERIFY_CERT_STORE:
        return ssl_cert_set_cert_store(s->cert, parg, 0, larg);

    case SSL_CTRL_SET_CHAIN_CERT_STORE:
        return ssl_cert_set_cert_store(s->cert, parg, 1, larg);

    case SSL_CTRL_GET_VERIFY_CERT_STORE:
        return ssl_cert_get_cert_store(s->cert, parg, 0);

    case SSL_CTRL_GET_CHAIN_CERT_STORE:
        return ssl_cert_get_cert_store(s->cert, parg, 1);

    case SSL_CTRL_GET_PEER_SIGNATURE_NID:
        if (s->s3.tmp.peer_sigalg == NULL)
            return 0;
        *(int *)parg = s->s3.tmp.peer_sigalg->hash;
        return 1;

    case SSL_CTRL_GET_SIGNATURE_NID:
        if (s->s3.tmp.sigalg == NULL)
            return 0;
        *(int *)parg = s->s3.tmp.sigalg->hash;
        return 1;

    case SSL_CTRL_GET_PEER_TMP_KEY:
        if (s->session == NULL || s->s3.peer_tmp == NULL)
        {
            return 0;
        }
        else
        {
            EVP_PKEY_up_ref(s->s3.peer_tmp);
            *(EVP_PKEY **)parg = s->s3.peer_tmp;
            return 1;
        }

    case SSL_CTRL_GET_TMP_KEY:
        if (s->session == NULL || s->s3.tmp.pkey == NULL)
        {
            return 0;
        }
        else
        {
            EVP_PKEY_up_ref(s->s3.tmp.pkey);
            *(EVP_PKEY **)parg = s->s3.tmp.pkey;
            return 1;
        }

    case SSL_CTRL_GET_EC_POINT_FORMATS:
    {
        const unsigned char **pformat = parg;

        if (s->ext.peer_ecpointformats == NULL)
            return 0;
        *pformat = s->ext.peer_ecpointformats;
        return (int)s->ext.peer_ecpointformats_len;
    }

    default:
        break;
    }
    return ret;
}

long ssl3_callback_ctrl(SSL *s, int cmd, void (*fp)(void))
{
    int ret = 0;

    switch (cmd)
    {
#if !defined(OPENSSL_NO_DEPRECATED_3_0)
    case SSL_CTRL_SET_TMP_DH_CB:
        s->cert->dh_tmp_cb = (DH * (*)(SSL *, int, int)) fp;
        ret = 1;
        break;
#endif
    case SSL_CTRL_SET_TLSEXT_DEBUG_CB:
        s->ext.debug_cb = (void (*)(SSL *, int, int,
                                    const unsigned char *, int, void *))fp;
        ret = 1;
        break;

    case SSL_CTRL_SET_NOT_RESUMABLE_SESS_CB:
        s->not_resumable_session_cb = (int (*)(SSL *, int))fp;
        ret = 1;
        break;
    default:
        break;
    }
    return ret;
}

long ssl3_ctx_ctrl(SSL_CTX *ctx, int cmd, long larg, void *parg)
{
    switch (cmd)
    {
#if !defined(OPENSSL_NO_DEPRECATED_3_0)
    case SSL_CTRL_SET_TMP_DH:
    {
        EVP_PKEY *pkdh = NULL;
        if (parg == NULL)
        {
            ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_NULL_PARAMETER);
            return 0;
        }
        pkdh = ssl_dh_to_pkey(parg);
        if (pkdh == NULL)
        {
            ERR_raise(ERR_LIB_SSL, ERR_R_MALLOC_FAILURE);
            return 0;
        }
        if (!SSL_CTX_set0_tmp_dh_pkey(ctx, pkdh))
        {
            EVP_PKEY_free(pkdh);
            return 0;
        }
        return 1;
    }
    case SSL_CTRL_SET_TMP_DH_CB:
    {
        ERR_raise(ERR_LIB_SSL, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
        return 0;
    }
#endif
    case SSL_CTRL_SET_DH_AUTO:
        ctx->cert->dh_tmp_auto = larg;
        return 1;
#if !defined(OPENSSL_NO_DEPRECATED_3_0)
    case SSL_CTRL_SET_TMP_ECDH:
    {
        if (parg == NULL)
        {
            ERR_raise(ERR_LIB_SSL, ERR_R_PASSED_NULL_PARAMETER);
            return 0;
        }
        return ssl_set_tmp_ecdh_groups(&ctx->ext.supportedgroups,
                                       &ctx->ext.supportedgroups_len,
                                       parg);
    }
#endif /* !OPENSSL_NO_DEPRECATED_3_0 */
    case SSL_CTRL_SET_TLSEXT_SERVERNAME_ARG:
        ctx->ext.servername_arg = parg;
        break;
    case SSL_CTRL_SET_TLSEXT_TICKET_KEYS:
    case SSL_CTRL_GET_TLSEXT_TICKET_KEYS:
    {
        unsigned char *keys = parg;
        long tick_keylen = (sizeof(ctx->ext.tick_key_name) +
                            sizeof(ctx->ext.secure->tick_hmac_key) +
                            sizeof(ctx->ext.secure->tick_aes_key));
        if (keys == NULL)
            return tick_keylen;
        if (larg != tick_keylen)
        {
            ERR_raise(ERR_LIB_SSL, SSL_R_INVALID_TICKET_KEYS_LENGTH);
            return 0;
        }
        if (cmd == SSL_CTRL_SET_TLSEXT_TICKET_KEYS)
        {
            memcpy(ctx->ext.tick_key_name, keys,
                   sizeof(ctx->ext.tick_key_name));
            memcpy(ctx->ext.secure->tick_hmac_key,
                   keys + sizeof(ctx->ext.tick_key_name),
                   sizeof(ctx->ext.secure->tick_hmac_key));
            memcpy(ctx->ext.secure->tick_aes_key,
                   keys + sizeof(ctx->ext.tick_key_name) +
                       sizeof(ctx->ext.secure->tick_hmac_key),
                   sizeof(ctx->ext.secure->tick_aes_key));
        }
        else
        {
            memcpy(keys, ctx->ext.tick_key_name,
                   sizeof(ctx->ext.tick_key_name));
            memcpy(keys + sizeof(ctx->ext.tick_key_name),
                   ctx->ext.secure->tick_hmac_key,
                   sizeof(ctx->ext.secure->tick_hmac_key));
            memcpy(keys + sizeof(ctx->ext.tick_key_name) +
                       sizeof(ctx->ext.secure->tick_hmac_key),
                   ctx->ext.secure->tick_aes_key,
                   sizeof(ctx->ext.secure->tick_aes_key));
        }
        return 1;
    }

    case SSL_CTRL_GET_TLSEXT_STATUS_REQ_TYPE:
        return ctx->ext.status_type;

    case SSL_CTRL_SET_TLSEXT_STATUS_REQ_TYPE:
        ctx->ext.status_type = larg;
        break;

    case SSL_CTRL_SET_TLSEXT_STATUS_REQ_CB_ARG:
        ctx->ext.status_arg = parg;
        return 1;

    case SSL_CTRL_GET_TLSEXT_STATUS_REQ_CB_ARG:
        *(void **)parg = ctx->ext.status_arg;
        break;

    case SSL_CTRL_GET_TLSEXT_STATUS_REQ_CB:
        *(int (**)(SSL *, void *))parg = ctx->ext.status_cb;
        break;

#ifndef OPENSSL_NO_SRP
    case SSL_CTRL_SET_TLS_EXT_SRP_USERNAME:
        ctx->srp_ctx.srp_Mask |= SSL_kSRP;
        OPENSSL_free(ctx->srp_ctx.login);
        ctx->srp_ctx.login = NULL;
        if (parg == NULL)
            break;
        if (strlen((const char *)parg) > 255 || strlen((const char *)parg) < 1)
        {
            ERR_raise(ERR_LIB_SSL, SSL_R_INVALID_SRP_USERNAME);
            return 0;
        }
        if ((ctx->srp_ctx.login = OPENSSL_strdup((char *)parg)) == NULL)
        {
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            return 0;
        }
        break;
    case SSL_CTRL_SET_TLS_EXT_SRP_PASSWORD:
        ctx->srp_ctx.SRP_give_srp_client_pwd_callback =
            srp_password_from_info_cb;
        if (ctx->srp_ctx.info != NULL)
            OPENSSL_free(ctx->srp_ctx.info);
        if ((ctx->srp_ctx.info = OPENSSL_strdup((char *)parg)) == NULL)
        {
            ERR_raise(ERR_LIB_SSL, ERR_R_INTERNAL_ERROR);
            return 0;
        }
        break;
    case SSL_CTRL_SET_SRP_ARG:
        ctx->srp_ctx.srp_Mask |= SSL_kSRP;
        ctx->srp_ctx.SRP_cb_arg = parg;
        break;

    case SSL_CTRL_SET_TLS_EXT_SRP_STRENGTH:
        ctx->srp_ctx.strength = larg;
        break;
#endif

    case SSL_CTRL_SET_GROUPS:
        return tls1_set_groups(&ctx->ext.supportedgroups,
                               &ctx->ext.supportedgroups_len,
                               parg, larg);

    case SSL_CTRL_SET_GROUPS_LIST:
        return tls1_set_groups_list(ctx, &ctx->ext.supportedgroups,
                                    &ctx->ext.supportedgroups_len,
                                    parg);

    case SSL_CTRL_SET_SIGALGS:
        return tls1_set_sigalgs(ctx->cert, parg, larg, 0);

    case SSL_CTRL_SET_SIGALGS_LIST:
        return tls1_set_sigalgs_list(ctx->cert, parg, 0);

    case SSL_CTRL_SET_CLIENT_SIGALGS:
        return tls1_set_sigalgs(ctx->cert, parg, larg, 1);

    case SSL_CTRL_SET_CLIENT_SIGALGS_LIST:
        return tls1_set_sigalgs_list(ctx->cert, parg, 1);

    case SSL_CTRL_SET_CLIENT_CERT_TYPES:
        return ssl3_set_req_cert_type(ctx->cert, parg, larg);

    case SSL_CTRL_BUILD_CERT_CHAIN:
        return ssl_build_cert_chain(NULL, ctx, larg);

    case SSL_CTRL_SET_VERIFY_CERT_STORE:
        return ssl_cert_set_cert_store(ctx->cert, parg, 0, larg);

    case SSL_CTRL_SET_CHAIN_CERT_STORE:
        return ssl_cert_set_cert_store(ctx->cert, parg, 1, larg);

    case SSL_CTRL_GET_VERIFY_CERT_STORE:
        return ssl_cert_get_cert_store(ctx->cert, parg, 0);

    case SSL_CTRL_GET_CHAIN_CERT_STORE:
        return ssl_cert_get_cert_store(ctx->cert, parg, 1);

        /* A Thawte special :-) */
    case SSL_CTRL_EXTRA_CHAIN_CERT:
        if (ctx->extra_certs == NULL)
        {
            if ((ctx->extra_certs = sk_X509_new_null()) == NULL)
            {
                ERR_raise(ERR_LIB_SSL, ERR_R_MALLOC_FAILURE);
                return 0;
            }
        }
        if (!sk_X509_push(ctx->extra_certs, (X509 *)parg))
        {
            ERR_raise(ERR_LIB_SSL, ERR_R_MALLOC_FAILURE);
            return 0;
        }
        break;

    case SSL_CTRL_GET_EXTRA_CHAIN_CERTS:
        if (ctx->extra_certs == NULL && larg == 0)
            *(STACK_OF(X509) **)parg = ctx->cert->key->chain;
        else
            *(STACK_OF(X509) **)parg = ctx->extra_certs;
        break;

    case SSL_CTRL_CLEAR_EXTRA_CHAIN_CERTS:
        sk_X509_pop_free(ctx->extra_certs, X509_free);
        ctx->extra_certs = NULL;
        break;

    case SSL_CTRL_CHAIN:
        if (larg)
            return ssl_cert_set1_chain(NULL, ctx, (STACK_OF(X509) *)parg);
        else
            return ssl_cert_set0_chain(NULL, ctx, (STACK_OF(X509) *)parg);

    case SSL_CTRL_CHAIN_CERT:
        if (larg)
            return ssl_cert_add1_chain_cert(NULL, ctx, (X509 *)parg);
        else
            return ssl_cert_add0_chain_cert(NULL, ctx, (X509 *)parg);

    case SSL_CTRL_GET_CHAIN_CERTS:
        *(STACK_OF(X509) **)parg = ctx->cert->key->chain;
        break;

    case SSL_CTRL_SELECT_CURRENT_CERT:
        return ssl_cert_select_current(ctx->cert, (X509 *)parg);

    case SSL_CTRL_SET_CURRENT_CERT:
        return ssl_cert_set_current(ctx->cert, larg);

    default:
        return 0;
    }
    return 1;
}

long ssl3_ctx_callback_ctrl(SSL_CTX *ctx, int cmd, void (*fp)(void))
{
    switch (cmd)
    {
#if !defined(OPENSSL_NO_DEPRECATED_3_0)
    case SSL_CTRL_SET_TMP_DH_CB:
    {
        ctx->cert->dh_tmp_cb = (DH * (*)(SSL *, int, int)) fp;
    }
    break;
#endif
    case SSL_CTRL_SET_TLSEXT_SERVERNAME_CB:
        ctx->ext.servername_cb = (int (*)(SSL *, int *, void *))fp;
        break;

    case SSL_CTRL_SET_TLSEXT_STATUS_REQ_CB:
        ctx->ext.status_cb = (int (*)(SSL *, void *))fp;
        break;

#ifndef OPENSSL_NO_DEPRECATED_3_0
    case SSL_CTRL_SET_TLSEXT_TICKET_KEY_CB:
        ctx->ext.ticket_key_cb = (int (*)(SSL *, unsigned char *,
                                          unsigned char *,
                                          EVP_CIPHER_CTX *,
                                          HMAC_CTX *, int))fp;
        break;
#endif

#ifndef OPENSSL_NO_SRP
    case SSL_CTRL_SET_SRP_VERIFY_PARAM_CB:
        ctx->srp_ctx.srp_Mask |= SSL_kSRP;
        ctx->srp_ctx.SRP_verify_param_callback = (int (*)(SSL *, void *))fp;
        break;
    case SSL_CTRL_SET_TLS_EXT_SRP_USERNAME_CB:
        ctx->srp_ctx.srp_Mask |= SSL_kSRP;
        ctx->srp_ctx.TLS_ext_srp_username_callback =
            (int (*)(SSL *, int *, void *))fp;
        break;
    case SSL_CTRL_SET_SRP_GIVE_CLIENT_PWD_CB:
        ctx->srp_ctx.srp_Mask |= SSL_kSRP;
        ctx->srp_ctx.SRP_give_srp_client_pwd_callback =
            (char *(*)(SSL *, void *))fp;
        break;
#endif
    case SSL_CTRL_SET_NOT_RESUMABLE_SESS_CB:
    {
        ctx->not_resumable_session_cb = (int (*)(SSL *, int))fp;
    }
    break;
    default:
        return 0;
    }
    return 1;
}

int SSL_CTX_set_tlsext_ticket_key_evp_cb(SSL_CTX *ctx, int (*fp)(SSL *, unsigned char *, unsigned char *,
                                                                 EVP_CIPHER_CTX *, EVP_MAC_CTX *, int))
{
    ctx->ext.ticket_key_evp_cb = fp;
    return 1;
}

const SSL_CIPHER *ssl3_get_cipher_by_id(uint32_t id)
{
    SSL_CIPHER c;
    const SSL_CIPHER *cp;

    c.id = id;
    cp = OBJ_bsearch_ssl_cipher_id(&c, tls13_ciphers, TLS13_NUM_CIPHERS);
    if (cp != NULL)
        return cp;
    cp = OBJ_bsearch_ssl_cipher_id(&c, ssl3_ciphers, SSL3_NUM_CIPHERS);
    if (cp != NULL)
        return cp;
    return OBJ_bsearch_ssl_cipher_id(&c, ssl3_scsvs, SSL3_NUM_SCSVS);
}

const SSL_CIPHER *ssl3_get_cipher_by_std_name(const char *stdname)
{
    SSL_CIPHER *tbl;
    SSL_CIPHER *alltabs[] = {tls13_ciphers, ssl3_ciphers, ssl3_scsvs};
    size_t i, j, tblsize[] = {TLS13_NUM_CIPHERS, SSL3_NUM_CIPHERS, SSL3_NUM_SCSVS};

    /* this is not efficient, necessary to optimize this? */
    for (j = 0; j < OSSL_NELEM(alltabs); j++)
    {
        for (i = 0, tbl = alltabs[j]; i < tblsize[j]; i++, tbl++)
        {
            if (tbl->stdname == NULL)
                continue;
            if (strcmp(stdname, tbl->stdname) == 0)
            {
                return tbl;
            }
        }
    }
    return NULL;
}

/*
 * This function needs to check if the ciphers required are actually
 * available
 */
const SSL_CIPHER *ssl3_get_cipher_by_char(const unsigned char *p)
{
    return ssl3_get_cipher_by_id(SSL3_CK_CIPHERSUITE_FLAG | ((uint32_t)p[0] << 8L) | (uint32_t)p[1]);
}

int ssl3_put_cipher_by_char(const SSL_CIPHER *c, WPACKET *pkt, size_t *len)
{
    if ((c->id & 0xff000000) != SSL3_CK_CIPHERSUITE_FLAG)
    {
        *len = 0;
        return 1;
    }

    if (!WPACKET_put_bytes_u16(pkt, c->id & 0xffff))
        return 0;

    *len = 2;
    return 1;
}

/*
 * ssl3_choose_cipher - choose a cipher from those offered by the client
 * @s: SSL connection
 * @clnt: ciphers offered by the client
 * @srvr: ciphers enabled on the server?
 *
 * Returns the selected cipher or NULL when no common ciphers.
 */
const SSL_CIPHER *ssl3_choose_cipher(SSL *s, STACK_OF(SSL_CIPHER) * clnt,
                                     STACK_OF(SSL_CIPHER) * srvr)
{
    const SSL_CIPHER *c, *ret = NULL;
    STACK_OF(SSL_CIPHER) * prio, *allow;
    int i, ii, ok, prefer_sha256 = 0;
    unsigned long alg_k = 0, alg_a = 0, mask_k = 0, mask_a = 0;
    STACK_OF(SSL_CIPHER) *prio_chacha = NULL;

    /* Let's see which ciphers we can support */

    /*
     * Do not set the compare functions, because this may lead to a
     * reordering by "id". We want to keep the original ordering. We may pay
     * a price in performance during sk_SSL_CIPHER_find(), but would have to
     * pay with the price of sk_SSL_CIPHER_dup().
     */

    OSSL_TRACE_BEGIN(TLS_CIPHER)
    {
        BIO_printf(trc_out, "Server has %d from %p:\n",
                   sk_SSL_CIPHER_num(srvr), (void *)srvr);
        for (i = 0; i < sk_SSL_CIPHER_num(srvr); ++i)
        {
            c = sk_SSL_CIPHER_value(srvr, i);
            BIO_printf(trc_out, "%p:%s\n", (void *)c, c->name);
        }
        BIO_printf(trc_out, "Client sent %d from %p:\n",
                   sk_SSL_CIPHER_num(clnt), (void *)clnt);
        for (i = 0; i < sk_SSL_CIPHER_num(clnt); ++i)
        {
            c = sk_SSL_CIPHER_value(clnt, i);
            BIO_printf(trc_out, "%p:%s\n", (void *)c, c->name);
        }
    }
    OSSL_TRACE_END(TLS_CIPHER);

    /* SUITE-B takes precedence over server preference and ChaCha priortiy */
    if (tls1_suiteb(s))
    {
        prio = srvr;
        allow = clnt;
    }
    else if (s->options & SSL_OP_CIPHER_SERVER_PREFERENCE)
    {
        prio = srvr;
        allow = clnt;

        /* If ChaCha20 is at the top of the client preference list,
           and there are ChaCha20 ciphers in the server list, then
           temporarily prioritize all ChaCha20 ciphers in the servers list. */
        if (s->options & SSL_OP_PRIORITIZE_CHACHA && sk_SSL_CIPHER_num(clnt) > 0)
        {
            c = sk_SSL_CIPHER_value(clnt, 0);
            if (c->algorithm_enc == SSL_CHACHA20POLY1305)
            {
                /* ChaCha20 is client preferred, check server... */
                int num = sk_SSL_CIPHER_num(srvr);
                int found = 0;
                for (i = 0; i < num; i++)
                {
                    c = sk_SSL_CIPHER_value(srvr, i);
                    if (c->algorithm_enc == SSL_CHACHA20POLY1305)
                    {
                        found = 1;
                        break;
                    }
                }
                if (found)
                {
                    prio_chacha = sk_SSL_CIPHER_new_reserve(NULL, num);
                    /* if reserve fails, then there's likely a memory issue */
                    if (prio_chacha != NULL)
                    {
                        /* Put all ChaCha20 at the top, starting with the one we just found */
                        sk_SSL_CIPHER_push(prio_chacha, c);
                        for (i++; i < num; i++)
                        {
                            c = sk_SSL_CIPHER_value(srvr, i);
                            if (c->algorithm_enc == SSL_CHACHA20POLY1305)
                                sk_SSL_CIPHER_push(prio_chacha, c);
                        }
                        /* Pull in the rest */
                        for (i = 0; i < num; i++)
                        {
                            c = sk_SSL_CIPHER_value(srvr, i);
                            if (c->algorithm_enc != SSL_CHACHA20POLY1305)
                                sk_SSL_CIPHER_push(prio_chacha, c);
                        }
                        prio = prio_chacha;
                    }
                }
            }
        }
    }
    else
    {
        prio = clnt;
        allow = srvr;
    }

    if (SSL_IS_TLS13(s))
    {
#ifndef OPENSSL_NO_PSK
        int j;

        /*
         * If we allow "old" style PSK callbacks, and we have no certificate (so
         * we're not going to succeed without a PSK anyway), and we're in
         * TLSv1.3 then the default hash for a PSK is SHA-256 (as per the
         * TLSv1.3 spec). Therefore we should prioritise ciphersuites using
         * that.
         */
        if (s->psk_server_callback != NULL)
        {
            for (j = 0; j < SSL_PKEY_NUM && !ssl_has_cert(s, j); j++)
                ;
            if (j == SSL_PKEY_NUM)
            {
                /* There are no certificates */
                prefer_sha256 = 1;
            }
        }
#endif
    }
    else
    {
        tls1_set_cert_validity(s);
        ssl_set_masks(s);
    }

    for (i = 0; i < sk_SSL_CIPHER_num(prio); i++)
    {
        c = sk_SSL_CIPHER_value(prio, i);

        /* Skip ciphers not supported by the protocol version */
        if (!SSL_IS_DTLS(s) &&
            ((s->version < c->min_tls) || (s->version > c->max_tls)))
            continue;
        if (SSL_IS_DTLS(s) &&
            (DTLS_VERSION_LT(s->version, c->min_dtls) ||
             DTLS_VERSION_GT(s->version, c->max_dtls)))
            continue;

        /*
         * Since TLS 1.3 ciphersuites can be used with any auth or
         * key exchange scheme skip tests.
         */
        if (!SSL_IS_TLS13(s))
        {
            mask_k = s->s3.tmp.mask_k;
            mask_a = s->s3.tmp.mask_a;
#ifndef OPENSSL_NO_SRP
            if (s->srp_ctx.srp_Mask & SSL_kSRP)
            {
                mask_k |= SSL_kSRP;
                mask_a |= SSL_aSRP;
            }
#endif

            alg_k = c->algorithm_mkey;
            alg_a = c->algorithm_auth;

#ifndef OPENSSL_NO_PSK
            /* with PSK there must be server callback set */
            if ((alg_k & SSL_PSK) && s->psk_server_callback == NULL)
                continue;
#endif /* OPENSSL_NO_PSK */

            ok = (alg_k & mask_k) && (alg_a & mask_a);
            OSSL_TRACE7(TLS_CIPHER,
                        "%d:[%08lX:%08lX:%08lX:%08lX]%p:%s\n",
                        ok, alg_k, alg_a, mask_k, mask_a, (void *)c, c->name);

            /*
             * if we are considering an ECC cipher suite that uses an ephemeral
             * EC key check it
             */
            if (alg_k & SSL_kECDHE)
                ok = ok && tls1_check_ec_tmp_key(s, c->id);

            if (!ok)
                continue;
        }
        ii = sk_SSL_CIPHER_find(allow, c);
        if (ii >= 0)
        {
            /* Check security callback permits this cipher */
            if (!ssl_security(s, SSL_SECOP_CIPHER_SHARED,
                              c->strength_bits, 0, (void *)c))
                continue;

            if ((alg_k & SSL_kECDHE) && (alg_a & SSL_aECDSA) && s->s3.is_probably_safari)
            {
                if (!ret)
                    ret = sk_SSL_CIPHER_value(allow, ii);
                continue;
            }

            if (prefer_sha256)
            {
                const SSL_CIPHER *tmp = sk_SSL_CIPHER_value(allow, ii);
                const EVP_MD *md = ssl_md(s->ctx, tmp->algorithm2);

                if (md != NULL && EVP_MD_is_a(md, OSSL_DIGEST_NAME_SHA2_256))
                {
                    ret = tmp;
                    break;
                }
                if (ret == NULL)
                    ret = tmp;
                continue;
            }
            ret = sk_SSL_CIPHER_value(allow, ii);
            break;
        }
    }

    sk_SSL_CIPHER_free(prio_chacha);

    return ret;
}

int ssl3_get_req_cert_type(SSL *s, WPACKET *pkt)
{
    uint32_t alg_k, alg_a = 0;

    /* If we have custom certificate types set, use them */
    if (s->cert->ctype)
        return WPACKET_memcpy(pkt, s->cert->ctype, s->cert->ctype_len);
    /* Get mask of algorithms disabled by signature list */
    ssl_set_sig_mask(&alg_a, s, SSL_SECOP_SIGALG_MASK);

    alg_k = s->s3.tmp.new_cipher->algorithm_mkey;

#ifndef OPENSSL_NO_GOST
    if (s->version >= TLS1_VERSION && (alg_k & SSL_kGOST))
        if (!WPACKET_put_bytes_u8(pkt, TLS_CT_GOST01_SIGN) || !WPACKET_put_bytes_u8(pkt, TLS_CT_GOST12_IANA_SIGN) || !WPACKET_put_bytes_u8(pkt, TLS_CT_GOST12_IANA_512_SIGN) || !WPACKET_put_bytes_u8(pkt, TLS_CT_GOST12_LEGACY_SIGN) || !WPACKET_put_bytes_u8(pkt, TLS_CT_GOST12_LEGACY_512_SIGN))
            return 0;

    if (s->version >= TLS1_2_VERSION && (alg_k & SSL_kGOST18))
        if (!WPACKET_put_bytes_u8(pkt, TLS_CT_GOST12_IANA_SIGN) || !WPACKET_put_bytes_u8(pkt, TLS_CT_GOST12_IANA_512_SIGN))
            return 0;
#endif
    
    if (s->version >= TLS1_VERSION && (alg_k & SSL_kBDHE))
            return WPACKET_put_bytes_u8(pkt, TLS_CT_BIGN_SIGN);
    if (s->version >= TLS1_VERSION && (alg_k & SSL_kBDHTPSK))
            return WPACKET_put_bytes_u8(pkt, TLS_CT_BIGN_SIGN);


    if ((s->version == SSL3_VERSION) && (alg_k & SSL_kDHE))
    {
        if (!WPACKET_put_bytes_u8(pkt, SSL3_CT_RSA_EPHEMERAL_DH))
            return 0;
        if (!(alg_a & SSL_aDSS) && !WPACKET_put_bytes_u8(pkt, SSL3_CT_DSS_EPHEMERAL_DH))
            return 0;
    }
    if (!(alg_a & SSL_aRSA) && !WPACKET_put_bytes_u8(pkt, SSL3_CT_RSA_SIGN))
        return 0;
    if (!(alg_a & SSL_aDSS) && !WPACKET_put_bytes_u8(pkt, SSL3_CT_DSS_SIGN))
        return 0;

    /*
     * ECDSA certs can be used with RSA cipher suites too so we don't
     * need to check for SSL_kECDH or SSL_kECDHE
     */
    if (s->version >= TLS1_VERSION && !(alg_a & SSL_aECDSA) && !WPACKET_put_bytes_u8(pkt, TLS_CT_ECDSA_SIGN))
        return 0;

    return 1;
}

static int ssl3_set_req_cert_type(CERT *c, const unsigned char *p, size_t len)
{
    OPENSSL_free(c->ctype);
    c->ctype = NULL;
    c->ctype_len = 0;
    if (p == NULL || len == 0)
        return 1;
    if (len > 0xff)
        return 0;
    c->ctype = OPENSSL_memdup(p, len);
    if (c->ctype == NULL)
        return 0;
    c->ctype_len = len;
    return 1;
}

int ssl3_shutdown(SSL *s)
{
    int ret;

    /*
     * Don't do anything much if we have not done the handshake or we don't
     * want to send messages :-)
     */
    if (s->quiet_shutdown || SSL_in_before(s))
    {
        s->shutdown = (SSL_SENT_SHUTDOWN | SSL_RECEIVED_SHUTDOWN);
        return 1;
    }

    if (!(s->shutdown & SSL_SENT_SHUTDOWN))
    {
        s->shutdown |= SSL_SENT_SHUTDOWN;
        ssl3_send_alert(s, SSL3_AL_WARNING, SSL_AD_CLOSE_NOTIFY);
        /*
         * our shutdown alert has been sent now, and if it still needs to be
         * written, s->s3.alert_dispatch will be true
         */
        if (s->s3.alert_dispatch)
            return -1; /* return WANT_WRITE */
    }
    else if (s->s3.alert_dispatch)
    {
        /* resend it if not sent */
        ret = s->method->ssl_dispatch_alert(s);
        if (ret == -1)
        {
            /*
             * we only get to return -1 here the 2nd/Nth invocation, we must
             * have already signalled return 0 upon a previous invocation,
             * return WANT_WRITE
             */
            return ret;
        }
    }
    else if (!(s->shutdown & SSL_RECEIVED_SHUTDOWN))
    {
        size_t readbytes;
        /*
         * If we are waiting for a close from our peer, we are closed
         */
        s->method->ssl_read_bytes(s, 0, NULL, NULL, 0, 0, &readbytes);
        if (!(s->shutdown & SSL_RECEIVED_SHUTDOWN))
        {
            return -1; /* return WANT_READ */
        }
    }

    if ((s->shutdown == (SSL_SENT_SHUTDOWN | SSL_RECEIVED_SHUTDOWN)) &&
        !s->s3.alert_dispatch)
        return 1;
    else
        return 0;
}

int ssl3_write(SSL *s, const void *buf, size_t len, size_t *written)
{
    clear_sys_error();
    if (s->s3.renegotiate)
        ssl3_renegotiate_check(s, 0);

    return s->method->ssl_write_bytes(s, SSL3_RT_APPLICATION_DATA, buf, len,
                                      written);
}

static int ssl3_read_internal(SSL *s, void *buf, size_t len, int peek,
                              size_t *readbytes)
{
    int ret;

    clear_sys_error();
    if (s->s3.renegotiate)
        ssl3_renegotiate_check(s, 0);
    s->s3.in_read_app_data = 1;
    ret =
        s->method->ssl_read_bytes(s, SSL3_RT_APPLICATION_DATA, NULL, buf, len,
                                  peek, readbytes);
    if ((ret == -1) && (s->s3.in_read_app_data == 2))
    {
        /*
         * ssl3_read_bytes decided to call s->handshake_func, which called
         * ssl3_read_bytes to read handshake data. However, ssl3_read_bytes
         * actually found application data and thinks that application data
         * makes sense here; so disable handshake processing and try to read
         * application data again.
         */
        ossl_statem_set_in_handshake(s, 1);
        ret =
            s->method->ssl_read_bytes(s, SSL3_RT_APPLICATION_DATA, NULL, buf,
                                      len, peek, readbytes);
        ossl_statem_set_in_handshake(s, 0);
    }
    else
        s->s3.in_read_app_data = 0;

    return ret;
}

int ssl3_read(SSL *s, void *buf, size_t len, size_t *readbytes)
{
    return ssl3_read_internal(s, buf, len, 0, readbytes);
}

int ssl3_peek(SSL *s, void *buf, size_t len, size_t *readbytes)
{
    return ssl3_read_internal(s, buf, len, 1, readbytes);
}

int ssl3_renegotiate(SSL *s)
{
    if (s->handshake_func == NULL)
        return 1;

    s->s3.renegotiate = 1;
    return 1;
}

/*
 * Check if we are waiting to do a renegotiation and if so whether now is a
 * good time to do it. If |initok| is true then we are being called from inside
 * the state machine so ignore the result of SSL_in_init(s). Otherwise we
 * should not do a renegotiation if SSL_in_init(s) is true. Returns 1 if we
 * should do a renegotiation now and sets up the state machine for it. Otherwise
 * returns 0.
 */
int ssl3_renegotiate_check(SSL *s, int initok)
{
    int ret = 0;

    if (s->s3.renegotiate)
    {
        if (!RECORD_LAYER_read_pending(&s->rlayer) && !RECORD_LAYER_write_pending(&s->rlayer) && (initok || !SSL_in_init(s)))
        {
            /*
             * if we are the server, and we have sent a 'RENEGOTIATE'
             * message, we need to set the state machine into the renegotiate
             * state.
             */
            ossl_statem_set_renegotiate(s);
            s->s3.renegotiate = 0;
            s->s3.num_renegotiations++;
            s->s3.total_renegotiations++;
            ret = 1;
        }
    }
    return ret;
}

/*
 * If we are using default SHA1+MD5 algorithms switch to new SHA256 PRF and
 * handshake macs if required.
 *
 * If PSK and using SHA384 for TLS < 1.2 switch to default.
 */
long ssl_get_algorithm2(SSL *s)
{
    long alg2;
    if (s->s3.tmp.new_cipher == NULL)
        return -1;
    alg2 = s->s3.tmp.new_cipher->algorithm2;
    if (s->method->ssl3_enc->enc_flags & SSL_ENC_FLAG_SHA256_PRF)
    {
        if (alg2 == (SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF))
            return SSL_HANDSHAKE_MAC_SHA256 | TLS1_PRF_SHA256;
    }
    else if (s->s3.tmp.new_cipher->algorithm_mkey & SSL_PSK)
    {
        if (alg2 == (SSL_HANDSHAKE_MAC_SHA384 | TLS1_PRF_SHA384))
            return SSL_HANDSHAKE_MAC_DEFAULT | TLS1_PRF;
    }
    return alg2;
}

/*
 * Fill a ClientRandom or ServerRandom field of length len. Returns <= 0 on
 * failure, 1 on success.
 */
int ssl_fill_hello_random(SSL *s, int server, unsigned char *result, size_t len,
                          DOWNGRADE dgrd)
{
    int send_time = 0, ret;

    if (len < 4)
        return 0;
    if (server)
        send_time = (s->mode & SSL_MODE_SEND_SERVERHELLO_TIME) != 0;
    else
        send_time = (s->mode & SSL_MODE_SEND_CLIENTHELLO_TIME) != 0;
    if (send_time)
    {
        unsigned long Time = (unsigned long)time(NULL);
        unsigned char *p = result;

        l2n(Time, p);
        ret = RAND_bytes_ex(s->ctx->libctx, p, len - 4, 0);
    }
    else
    {
        ret = RAND_bytes_ex(s->ctx->libctx, result, len, 0);
    }

    if (ret > 0)
    {
        if (!ossl_assert(sizeof(tls11downgrade) < len) || !ossl_assert(sizeof(tls12downgrade) < len))
            return 0;
        if (dgrd == DOWNGRADE_TO_1_2)
            memcpy(result + len - sizeof(tls12downgrade), tls12downgrade,
                   sizeof(tls12downgrade));
        else if (dgrd == DOWNGRADE_TO_1_1)
            memcpy(result + len - sizeof(tls11downgrade), tls11downgrade,
                   sizeof(tls11downgrade));
    }

    return ret;
}

int ssl_generate_master_secret(SSL *s, unsigned char *pms, size_t pmslen,
                               int free_pms)
{
    unsigned long alg_k = s->s3.tmp.new_cipher->algorithm_mkey;
    int ret = 0;

    if (alg_k & SSL_PSK)
    {
#ifndef OPENSSL_NO_PSK
        unsigned char *pskpms, *t;
        size_t psklen = s->s3.tmp.psklen;
        size_t pskpmslen;

        /* create PSK premaster_secret */

        /* For plain PSK "other_secret" is psklen zeroes */
        if (alg_k & SSL_kPSK)
            pmslen = psklen;

        pskpmslen = 4 + pmslen + psklen;
        pskpms = OPENSSL_malloc(pskpmslen);
        if (pskpms == NULL)
            goto err;
        t = pskpms;
        s2n(pmslen, t);
        if (alg_k & SSL_kPSK)
            memset(t, 0, pmslen);
        else
            memcpy(t, pms, pmslen);
        t += pmslen;
        s2n(psklen, t);
        memcpy(t, s->s3.tmp.psk, psklen);

        OPENSSL_clear_free(s->s3.tmp.psk, psklen);
        s->s3.tmp.psk = NULL;
        s->s3.tmp.psklen = 0;
        if (!s->method->ssl3_enc->generate_master_secret(s,
                                                         s->session->master_key, pskpms, pskpmslen,
                                                         &s->session->master_key_length))
        {
            OPENSSL_clear_free(pskpms, pskpmslen);
            /* SSLfatal() already called */
            goto err;
        }
        OPENSSL_clear_free(pskpms, pskpmslen);
#else
        /* Should never happen */
        goto err;
#endif
    }
    else
    {
        if (!s->method->ssl3_enc->generate_master_secret(s,
                                                         s->session->master_key, pms, pmslen,
                                                         &s->session->master_key_length))
        {
            /* SSLfatal() already called */
            goto err;
        }
    }

    ret = 1;
err:
    if (pms)
    {
        if (free_pms)
            OPENSSL_clear_free(pms, pmslen);
        else
            OPENSSL_cleanse(pms, pmslen);
    }
    if (s->server == 0)
    {
        s->s3.tmp.pms = NULL;
        s->s3.tmp.pmslen = 0;
    }
    return ret;
}

/* Generate a private key from parameters */
EVP_PKEY *ssl_generate_pkey(SSL *s, EVP_PKEY *pm)
{
    EVP_PKEY_CTX *pctx = NULL;
    EVP_PKEY *pkey = NULL;

    if (pm == NULL)
        return NULL;
    pctx = EVP_PKEY_CTX_new_from_pkey(s->ctx->libctx, pm, s->ctx->propq);
    if (pctx == NULL)
        goto err;
    if (EVP_PKEY_keygen_init(pctx) <= 0)
        goto err;
    if (EVP_PKEY_keygen(pctx, &pkey) <= 0)
    {
        EVP_PKEY_free(pkey);
        pkey = NULL;
    }

err:
    EVP_PKEY_CTX_free(pctx);
    return pkey;
}

/* Generate a private key from a group ID */
EVP_PKEY *ssl_generate_pkey_group(SSL *s, uint16_t id)
{
    const TLS_GROUP_INFO *ginf = tls1_group_id_lookup(s->ctx, id);
    EVP_PKEY_CTX *pctx = NULL;
    EVP_PKEY *pkey = NULL;

    if (ginf == NULL)
    {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }

    pctx = EVP_PKEY_CTX_new_from_name(s->ctx->libctx, ginf->algorithm,
                                      s->ctx->propq);

    if (pctx == NULL)
    {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_MALLOC_FAILURE);
        goto err;
    }
    if (EVP_PKEY_keygen_init(pctx) <= 0)
    {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_EVP_LIB);
        goto err;
    }
    if (EVP_PKEY_CTX_set_group_name(pctx, ginf->realname) <= 0)
    {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_EVP_LIB);
        goto err;
    }
    if (EVP_PKEY_keygen(pctx, &pkey) <= 0)
    {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_EVP_LIB);
        EVP_PKEY_free(pkey);
        pkey = NULL;
    }

err:
    EVP_PKEY_CTX_free(pctx);
    return pkey;
}

/*
 * Generate parameters from a group ID
 */
EVP_PKEY *ssl_generate_param_group(SSL *s, uint16_t id)
{
    EVP_PKEY_CTX *pctx = NULL;
    EVP_PKEY *pkey = NULL;
    const TLS_GROUP_INFO *ginf = tls1_group_id_lookup(s->ctx, id);

    if (ginf == NULL)
        goto err;

    pctx = EVP_PKEY_CTX_new_from_name(s->ctx->libctx, ginf->algorithm,
                                      s->ctx->propq);

    if (pctx == NULL)
        goto err;
    if (EVP_PKEY_paramgen_init(pctx) <= 0)
        goto err;
    if (EVP_PKEY_CTX_set_group_name(pctx, ginf->realname) <= 0)
    {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_EVP_LIB);
        goto err;
    }
    if (EVP_PKEY_paramgen(pctx, &pkey) <= 0)
    {
        EVP_PKEY_free(pkey);
        pkey = NULL;
    }

err:
    EVP_PKEY_CTX_free(pctx);
    return pkey;
}

/* Generate secrets from pms */
int ssl_gensecret(SSL *s, unsigned char *pms, size_t pmslen)
{
    int rv = 0;

    /* SSLfatal() called as appropriate in the below functions */
    if (SSL_IS_TLS13(s))
    {
        /*
         * If we are resuming then we already generated the early secret
         * when we created the ClientHello, so don't recreate it.
         */
        if (!s->hit)
            rv = tls13_generate_secret(s, ssl_handshake_md(s), NULL, NULL,
                                       0,
                                       (unsigned char *)&s->early_secret);
        else
            rv = 1;

        rv = rv && tls13_generate_handshake_secret(s, pms, pmslen);
    }
    else
    {
        rv = ssl_generate_master_secret(s, pms, pmslen, 0);
    }

    return rv;
}

/* Derive secrets for ECDH/DH */
int ssl_derive(SSL *s, EVP_PKEY *privkey, EVP_PKEY *pubkey, int gensecret)
{
    int rv = 0;
    unsigned char *pms = NULL;
    size_t pmslen = 0;
    EVP_PKEY_CTX *pctx;

    if (privkey == NULL || pubkey == NULL)
    {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    pctx = EVP_PKEY_CTX_new_from_pkey(s->ctx->libctx, privkey, s->ctx->propq);

    if (EVP_PKEY_derive_init(pctx) <= 0 || EVP_PKEY_derive_set_peer(pctx, pubkey) <= 0 || EVP_PKEY_derive(pctx, NULL, &pmslen) <= 0)
    {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }

    if (SSL_IS_TLS13(s) && EVP_PKEY_is_a(privkey, "DH"))
        EVP_PKEY_CTX_set_dh_pad(pctx, 1);

    pms = OPENSSL_malloc(pmslen);
    if (pms == NULL)
    {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    if (EVP_PKEY_derive(pctx, pms, &pmslen) <= 0)
    {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }

    if (gensecret)
    {
        /* SSLfatal() called as appropriate in the below functions */
        rv = ssl_gensecret(s, pms, pmslen);
    }
    else
    {
        /* Save premaster secret */
        s->s3.tmp.pms = pms;
        s->s3.tmp.pmslen = pmslen;
        pms = NULL;
        rv = 1;
    }

err:
    OPENSSL_clear_free(pms, pmslen);
    EVP_PKEY_CTX_free(pctx);
    return rv;
}

/* Decapsulate secrets for KEM */
int ssl_decapsulate(SSL *s, EVP_PKEY *privkey,
                    const unsigned char *ct, size_t ctlen,
                    int gensecret)
{
    int rv = 0;
    unsigned char *pms = NULL;
    size_t pmslen = 0;
    EVP_PKEY_CTX *pctx;

    if (privkey == NULL)
    {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    pctx = EVP_PKEY_CTX_new_from_pkey(s->ctx->libctx, privkey, s->ctx->propq);

    if (EVP_PKEY_decapsulate_init(pctx, NULL) <= 0 || EVP_PKEY_decapsulate(pctx, NULL, &pmslen, ct, ctlen) <= 0)
    {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }

    pms = OPENSSL_malloc(pmslen);
    if (pms == NULL)
    {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    if (EVP_PKEY_decapsulate(pctx, pms, &pmslen, ct, ctlen) <= 0)
    {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }

    if (gensecret)
    {
        /* SSLfatal() called as appropriate in the below functions */
        rv = ssl_gensecret(s, pms, pmslen);

    }
    else
    {
        /* Save premaster secret */
        s->s3.tmp.pms = pms;
        s->s3.tmp.pmslen = pmslen;
        pms = NULL;
        rv = 1;
    }

err:
    OPENSSL_clear_free(pms, pmslen);
    EVP_PKEY_CTX_free(pctx);
    return rv;
}

int ssl_encapsulate(SSL *s, EVP_PKEY *pubkey,
                    unsigned char **ctp, size_t *ctlenp,
                    int gensecret)
{
    int rv = 0;
    unsigned char *pms = NULL, *ct = NULL;
    size_t pmslen = 0, ctlen = 0;
    EVP_PKEY_CTX *pctx;

    if (pubkey == NULL)
    {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        return 0;
    }

    pctx = EVP_PKEY_CTX_new_from_pkey(s->ctx->libctx, pubkey, s->ctx->propq);

    if (EVP_PKEY_encapsulate_init(pctx, NULL) <= 0 || EVP_PKEY_encapsulate(pctx, NULL, &ctlen, NULL, &pmslen) <= 0 || pmslen == 0 || ctlen == 0)
    {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }

    pms = OPENSSL_malloc(pmslen);
    ct = OPENSSL_malloc(ctlen);
    if (pms == NULL || ct == NULL)
    {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_MALLOC_FAILURE);
        goto err;
    }

    if (EVP_PKEY_encapsulate(pctx, ct, &ctlen, pms, &pmslen) <= 0)
    {
        SSLfatal(s, SSL_AD_INTERNAL_ERROR, ERR_R_INTERNAL_ERROR);
        goto err;
    }

    if (gensecret)
    {
        /* SSLfatal() called as appropriate in the below functions */
        rv = ssl_gensecret(s, pms, pmslen);
    }
    else
    {
        /* Save premaster secret */
        s->s3.tmp.pms = pms;
        s->s3.tmp.pmslen = pmslen;
        pms = NULL;
        rv = 1;
    }

    if (rv > 0)
    {
        /* Pass ownership of ct to caller */
        *ctp = ct;
        *ctlenp = ctlen;
        ct = NULL;
    }

err:
    OPENSSL_clear_free(pms, pmslen);
    OPENSSL_free(ct);
    EVP_PKEY_CTX_free(pctx);
    return rv;
}

const char *SSL_group_to_name(SSL *s, int nid)
{
    int group_id = 0;
    const TLS_GROUP_INFO *cinf = NULL;

    /* first convert to real group id for internal and external IDs */
    if (nid & TLSEXT_nid_unknown)
        group_id = nid & 0xFFFF;
    else
        group_id = tls1_nid2group_id(nid);

    /* then look up */
    cinf = tls1_group_id_lookup(s->ctx, group_id);

    if (cinf != NULL)
        return cinf->tlsname;
    return NULL;
}
