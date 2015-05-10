/*
 * Embethis EST Library Header
 */

#include "me.h"

#if ME_COM_EST

#include "osdep.h"




/********* Start of file src/est.h ************/


/**
    est.h - Embedded Security Transport
 */

#ifndef _h_EST
#define _h_EST 1

/********************************** Includes **********************************/




/*********************************** Forwards *********************************/

#if ME_CPU_ARCH == ME_CPU_X86 || ME_CPU_ARCH == ME_CPU_X64
    #define EST_HAVE_ASM 1
#endif

/* Enable if using Intel CPU with SSE2 */
#define ME_EST_SSE2 0
/*
    Default configuration, optionally overridden by me.h
 */
#ifndef ME_EST_AES
    #define ME_EST_AES 1
#endif
#ifndef ME_EST_BIGNUM
    #define ME_EST_BIGNUM 1
#endif
#ifndef ME_EST_BASE64
    #define ME_EST_BASE64 1
#endif
#ifndef ME_EST_CAMELLIA
    #define ME_EST_CAMELLIA 0
#endif
#ifndef ME_EST_DES
    #define ME_EST_DES 0
#endif
#ifndef ME_EST_DHM
    #define ME_EST_DHM 1
#endif
#ifndef ME_EST_GEN_PRIME
    #define ME_EST_GEN_PRIME 1
#endif
#ifndef ME_EST_HAVEGE
    #define ME_EST_HAVEGE 1
#endif
#ifndef ME_EST_LOGGING
    #define ME_EST_LOGGING 0
#endif
#ifndef ME_EST_MD2
    #define ME_EST_MD2 0
#endif
#ifndef ME_EST_MD4
    #define ME_EST_MD4 0
#endif
#ifndef ME_EST_MD5
    #define ME_EST_MD5 1
#endif
#ifndef ME_EST_NET
    #define ME_EST_NET 1
#endif
#ifndef ME_EST_PADLOCK
    #define ME_EST_PADLOCK 1
#endif
#ifndef ME_EST_RC4
    #define ME_EST_RC4 1
#endif
#ifndef ME_EST_ROM_TABLES
    #define ME_EST_ROM_TABLES 1
#endif
#ifndef ME_EST_RSA
    #define ME_EST_RSA 1
#endif
#ifndef ME_EST_SELF_TEST
    #define ME_EST_SELF_TEST 0
#endif
#ifndef ME_EST_SHA1
    #define ME_EST_SHA1 1
#endif
#ifndef ME_EST_SHA2
    #define ME_EST_SHA2 1
#endif
#ifndef ME_EST_SHA4
    #define ME_EST_SHA4 1
#endif
#ifndef ME_EST_CLIENT
    #define ME_EST_CLIENT 1
    #undef ME_EST_MD5
    #define ME_EST_MD5 1
#endif
#ifndef ME_EST_SERVER
    #undef ME_EST_MD5
    #define ME_EST_MD5 1
    #define ME_EST_SERVER 1
#endif
#ifndef ME_EST_TEST_CERTS
    #define ME_EST_TEST_CERTS 1
#endif
#ifndef ME_EST_X509
    #define ME_EST_X509 1
#endif
#ifndef ME_EST_X509_WRITE
    #define ME_EST_X509_WRITE 1
#endif
#ifndef ME_EST_XTEA
    #define ME_EST_XTEA 1
#endif

/*
    Required settings
 */
#define ME_EST_SSL 1
#define ME_EST_TIMING 1

#if UNUSED
#define EST_CA_CERT "ca.crt"
#endif

/*
    Include all EST headers
 */













#if UNUSED

#endif











#ifdef __cplusplus
}
#endif

#endif /* _h_EST */

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/bignum.h ************/


/*
    bignum.h -- Bit number support

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#ifndef EST_BIGNUM_H
#define EST_BIGNUM_H

#define EST_ERR_MPI_FILE_IO_ERROR                     -0x0002
#define EST_ERR_MPI_BAD_INPUT_DATA                    -0x0004
#define EST_ERR_MPI_INVALID_CHARACTER                 -0x0006
#define EST_ERR_MPI_BUFFER_TOO_SMALL                  -0x0008
#define EST_ERR_MPI_NEGATIVE_VALUE                    -0x000A
#define EST_ERR_MPI_DIVISION_BY_ZERO                  -0x000C
#define EST_ERR_MPI_NOT_ACCEPTABLE                    -0x000E

#define MPI_CHK(f) if( ( ret = f ) != 0 ) goto cleanup

/*
   Define the base integer type, architecture-wise
 */
#if ME_WORDSIZE == 8
    typedef uchar t_int;
    typedef ushort t_dbl;
#elif ME_WORDSIZE == 16
    typedef ushort t_int;
    typedef ulong t_dbl;
#else
    typedef ulong t_int;
    #if defined(_MSC_VER) && defined(_M_IX86)
        typedef unsigned __int64 t_dbl;
    #else
        #if defined(__amd64__) || defined(__x86_64__) || defined(__ppc64__) || defined(__powerpc64__) || defined(__mips64) || \
                defined(__ia64__)  || defined(__alpha__)
            typedef uint t_dbl __attribute__ ((mode(TI)));
        #else
            typedef unsigned long long t_dbl;
            #define ME_USE_LONG_LONG 1
        #endif
    #endif
#endif

/**
   @brief MPI structure
 */
typedef struct {
    int     s;          /**< integer sign */
    int     n;          /**< total # of limbs  */
    t_int   *p;         /**< pointer to limbs  */
} mpi;

#ifdef __cplusplus
extern "C" {
#endif

    /**
       @brief Initialize one or more mpi
     */
    PUBLIC void mpi_init(mpi *X, ...);

    /**
       @brief Unallocate one or more mpi
     */
    PUBLIC void mpi_free(mpi *X, ...);

    /**
       @brief          Enlarge to the specified number of limbs
       @return         0 if successful, 1 if memory allocation failed
     */
    PUBLIC int mpi_grow(mpi *X, int nblimbs);

    /**
       @brief          Copy the contents of Y into X
       @return         0 if successful, 1 if memory allocation failed
     */
    PUBLIC int mpi_copy(mpi *X, mpi *Y);

    /**
       @brief          Swap the contents of X and Y
     */
    PUBLIC void mpi_swap(mpi *X, mpi *Y);

    /**
       @brief          Set value from integer
       @return         0 if successful, 1 if memory allocation failed
     */
    PUBLIC int mpi_lset(mpi *X, int z);

    /**
       @brief          Return the number of least significant bits
     */
    PUBLIC int mpi_lsb(mpi *X);

    /**
       @brief          Return the number of most significant bits
     */
    PUBLIC int mpi_msb(mpi *X);

    /**
       @brief          Return the total size in bytes
     */
    PUBLIC int mpi_size(mpi *X);

    /**
       @brief          Import from an ASCII string
       @param X        destination mpi
       @param radix    input numeric base
       @param s        null-terminated string buffer
       @return         0 if successful, or an EST_ERR_MPI_XXX error code
     */
    PUBLIC int mpi_read_string(mpi *X, int radix, char *s);

    /**
       @brief          Export into an ASCII string
       @param X        source mpi
       @param radix    output numeric base
       @param s        string buffer
       @param slen     string buffer size
       @return         0 if successful, or an EST_ERR_MPI_XXX error code
       @note           Call this function with *slen = 0 to obtain the minimum required buffer size in *slen.
     */
    PUBLIC int mpi_write_string(mpi *X, int radix, char *s, int *slen);

    /**
       @brief          Read X from an opened file
       @param X        destination mpi
       @param radix    input numeric base
       @param fin      input file handle
       @return         0 if successful, or an EST_ERR_MPI_XXX error code
     */
    PUBLIC int mpi_read_file(mpi *X, int radix, FILE * fin);

    /**
       @brief          Write X into an opened file, or stdout
       @param p        prefix, can be NULL
       @param X        source mpi
       @param radix    output numeric base
       @param fout     output file handle
       @return         0 if successful, or an EST_ERR_MPI_XXX error code
       @note           Set fout == NULL to print X on the console.
     */
    PUBLIC int mpi_write_file(char *p, mpi *X, int radix, FILE * fout);

    /**
       @brief          Import X from unsigned binary data, big endian
       @param X        destination mpi
       @param buf      input buffer
       @param buflen   input buffer size
       @return         0 if successful, 1 if memory allocation failed
     */
    PUBLIC int mpi_read_binary(mpi *X, uchar *buf, int buflen);

    /**
       @brief          Export X into unsigned binary data, big endian
       @param X        source mpi
       @param buf      output buffer
       @param buflen   output buffer size
       @return         0 if successful, EST_ERR_MPI_BUFFER_TOO_SMALL if buf isn't large enough
       @note           Call this function with *buflen = 0 to obtain the minimum required buffer size in *buflen.
     */
    PUBLIC int mpi_write_binary(mpi *X, uchar *buf, int buflen);

    /**
       @brief          Left-shift: X <<= count
       @return         0 if successful, 1 if memory allocation failed
     */
    PUBLIC int mpi_shift_l(mpi *X, int count);

    /**
       @brief          Right-shift: X >>= count
       @return         0 if successful, 1 if memory allocation failed
     */
    PUBLIC int mpi_shift_r(mpi *X, int count);

    /**
       @brief          Compare unsigned values
       @return         1 if |X| is greater than |Y|,
                      -1 if |X| is lesser  than |Y| or
                       0 if |X| is equal to |Y|
     */
    PUBLIC int mpi_cmp_abs(mpi *X, mpi *Y);

    /**
       @brief          Compare signed values
       @return         1 if X is greater than Y,
                      -1 if X is lesser  than Y or
                       0 if X is equal to Y
     */
    PUBLIC int mpi_cmp_mpi(mpi *X, mpi *Y);

    /**
       @brief          Compare signed values
       @return         1 if X is greater than z,
                      -1 if X is lesser  than z or
                       0 if X is equal to z
     */
    PUBLIC int mpi_cmp_int(mpi *X, int z);

    /**
       @brief          Unsigned addition: X = |A| + |B|
       @return         0 if successful, 1 if memory allocation failed
     */
    PUBLIC int mpi_add_abs(mpi *X, mpi *A, mpi *B);

    /**
       @brief          Unsigned substraction: X = |A| - |B|
       @return         0 if successful, EST_ERR_MPI_NEGATIVE_VALUE if B is greater than A
     */
    PUBLIC int mpi_sub_abs(mpi *X, mpi *A, mpi *B);

    /**
       @brief          Signed addition: X = A + B
       @return         0 if successful, 1 if memory allocation failed
     */
    PUBLIC int mpi_add_mpi(mpi *X, mpi *A, mpi *B);

    /**
       @brief          Signed substraction: X = A - B
       @return         0 if successful, 1 if memory allocation failed
     */
    PUBLIC int mpi_sub_mpi(mpi *X, mpi *A, mpi *B);

    /**
       @brief          Signed addition: X = A + b
       @return         0 if successful, 1 if memory allocation failed
     */
    PUBLIC int mpi_add_int(mpi *X, mpi *A, int b);

    /**
       @brief          Signed substraction: X = A - b
       @return         0 if successful, 1 if memory allocation failed
     */
    PUBLIC int mpi_sub_int(mpi *X, mpi *A, int b);

    /**
       @brief          Baseline multiplication: X = A * B
       @return         0 if successful, 1 if memory allocation failed
     */
    PUBLIC int mpi_mul_mpi(mpi *X, mpi *A, mpi *B);

    /**
       @brief          Baseline multiplication: X = A * b
       @return         0 if successful, 1 if memory allocation failed
     */
    PUBLIC int mpi_mul_int(mpi *X, mpi *A, t_int b);

    /**
       @brief          Division by mpi: A = Q * B + R
       @return         0 if successful, 1 if memory allocation failed, EST_ERR_MPI_DIVISION_BY_ZERO if B == 0
       @note           Either Q or R can be NULL.
     */
    PUBLIC int mpi_div_mpi(mpi *Q, mpi *R, mpi *A, mpi *B);

    /**
       @brief          Division by int: A = Q * b + R
       @return         0 if successful, 1 if memory allocation failed, EST_ERR_MPI_DIVISION_BY_ZERO if b == 0
       @note           Either Q or R can be NULL.
     */
    PUBLIC int mpi_div_int(mpi *Q, mpi *R, mpi *A, int b);

    /**
       @brief          Modulo: R = A mod B
       @return         0 if successful, 1 if memory allocation failed, EST_ERR_MPI_DIVISION_BY_ZERO if B == 0
     */
    PUBLIC int mpi_mod_mpi(mpi *R, mpi *A, mpi *B);

    /**
       @brief          Modulo: r = A mod b
       @return         0 if successful, 1 if memory allocation failed, EST_ERR_MPI_DIVISION_BY_ZERO if b == 0
     */
    PUBLIC int mpi_mod_int(t_int * r, mpi *A, int b);

    /**
       @brief          Sliding-window exponentiation: X = A^E mod N
       @return         0 if successful, 1 if memory allocation failed, EST_ERR_MPI_BAD_INPUT_DATA if N is negative or even
       @note           _RR is used to avoid re-computing R*R mod N across multiple calls, which speeds up things a bit. It
                       can be set to NULL if the extra performance is unneeded.  
     */
    PUBLIC int mpi_exp_mod(mpi *X, mpi *A, mpi *E, mpi *N, mpi *_RR);

    /**
       @brief          Greatest common divisor: G = gcd(A, B)
       @return         0 if successful, 1 if memory allocation failed
     */
    PUBLIC int mpi_gcd(mpi *G, mpi *A, mpi *B);

    /**
       @brief          Modular inverse: X = A^-1 mod N
       @return         0 if successful, 1 if memory allocation failed,
                       EST_ERR_MPI_BAD_INPUT_DATA if N is negative or nil
                       EST_ERR_MPI_NOT_ACCEPTABLE if A has no inverse mod N
     */
    PUBLIC int mpi_inv_mod(mpi *X, mpi *A, mpi *N);

    /**
       @brief          Miller-Rabin primality test
       @return         0 if successful (probably prime), 1 if memory allocation failed,
                       EST_ERR_MPI_NOT_ACCEPTABLE if X is not prime
     */
    PUBLIC int mpi_is_prime(mpi *X, int (*f_rng) (void *), void *p_rng);

    /**
       @brief          Prime number generation
       @param X        destination mpi
       @param nbits    required size of X in bits
       @param dh_flag  if 1, then (X-1)/2 will be prime too
       @param f_rng    RNG function
       @param p_rng    RNG parameter
       @return         0 if successful (probably prime), 1 if memory allocation failed,
                       EST_ERR_MPI_BAD_INPUT_DATA if nbits is < 3
     */
    PUBLIC int mpi_gen_prime(mpi *X, int nbits, int dh_flag, int (*f_rng) (void *), void *p_rng);

#if UNUSED
    /**
       @brief          Checkup routine
       @return         0 if successful, or 1 if the test failed
     */
    PUBLIC int mpi_self_test(int verbose);
#endif

#ifdef __cplusplus
}
#endif
#endif              /* bignum.h */

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/net.h ************/


/*
    net.h -- Networking I/O

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#ifndef EST_NET_H
#define EST_NET_H

#define EST_ERR_NET_UNKNOWN_HOST                      -0x0F00
#define EST_ERR_NET_SOCKET_FAILED                     -0x0F10
#define EST_ERR_NET_CONNECT_FAILED                    -0x0F20
#define EST_ERR_NET_BIND_FAILED                       -0x0F30
#define EST_ERR_NET_LISTEN_FAILED                     -0x0F40
#define EST_ERR_NET_ACCEPT_FAILED                     -0x0F50
#define EST_ERR_NET_RECV_FAILED                       -0x0F60
#define EST_ERR_NET_SEND_FAILED                       -0x0F70
#define EST_ERR_NET_CONN_RESET                        -0x0F80
#define EST_ERR_NET_TRY_AGAIN                         -0x0F90

#ifdef __cplusplus
extern "C" {
#endif

    /**
       @brief          Initiate a TCP connection with host:port
       @return         0 if successful, or one of:
                            EST_ERR_NET_SOCKET_FAILED,
                            EST_ERR_NET_UNKNOWN_HOST,
                            EST_ERR_NET_CONNECT_FAILED
     */
    PUBLIC int net_connect(int *fd, char *host, int port);

    /**
       @brief          Create a listening socket on bind_ip:port.
                       If bind_ip == NULL, all interfaces are binded.
       @return         0 if successful, or one of:
                            EST_ERR_NET_SOCKET_FAILED,
                            EST_ERR_NET_BIND_FAILED,
                            EST_ERR_NET_LISTEN_FAILED
     */
    PUBLIC int net_bind(int *fd, char *bind_ip, int port);

    /**
       @brief          Accept a connection from a remote client
       @return         0 if successful, EST_ERR_NET_ACCEPT_FAILED, or EST_ERR_NET_WOULD_BLOCK is bind_fd was set to
                       non-blocking and accept() is blocking.
     */
    PUBLIC int net_accept(int bind_fd, int *client_fd, void *client_ip);

    /**
       @brief          Set the socket blocking
       @return         0 if successful, or a non-zero error code
     */
    PUBLIC int net_set_block(int fd);

    /**
       @brief          Set the socket non-blocking
       @return         0 if successful, or a non-zero error code
     */
    PUBLIC int net_set_nonblock(int fd);

    /**
       @brief          Portable usleep helper
       @note           Real amount of time slept will not be less than select()'s timeout granularity (typically, 10ms).
     */
    PUBLIC void net_usleep(ulong usec);

    /**
       @brief          Read at most 'len' characters. len is updated to
                       reflect the actual number of characters read.
       @return         This function returns the number of bytes received, or a negative error code; EST_ERR_NET_TRY_AGAIN
                       indicates read() is blocking.
     */
    PUBLIC int net_recv(void *ctx, uchar *buf, int len);

    /**
       @brief          Write at most 'len' characters. len is updated to
                       reflect the number of characters _not_ written.
       @return         This function returns the number of bytes sent, or a negative error code; EST_ERR_NET_TRY_AGAIN
                       indicates write() is blocking.
     */
    PUBLIC int net_send(void *ctx, uchar *buf, int len);

    /**
       @brief          Gracefully shutdown the connection
     */
    PUBLIC void net_close(int fd);

#ifdef __cplusplus
}
#endif
#endif              /* net.h */

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/dhm.h ************/


/*
    dhm.h -- Diffie-Helman Support

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#ifndef EST_DHM_H
#define EST_DHM_H

#define EST_ERR_DHM_BAD_INPUT_DATA                    -0x0480
#define EST_ERR_DHM_READ_PARAMS_FAILED                -0x0490
#define EST_ERR_DHM_MAKE_PARAMS_FAILED                -0x04A0
#define EST_ERR_DHM_READ_PUBLIC_FAILED                -0x04B0
#define EST_ERR_DHM_MAKE_PUBLIC_FAILED                -0x04C0
#define EST_ERR_DHM_CALC_SECRET_FAILED                -0x04D0

typedef struct {
    int len;        /**<  size(P) in chars  */
    mpi P;          /**<  prime modulus     */
    mpi G;          /**<  generator         */
    mpi X;          /**<  secret value      */
    mpi GX;         /**<  self = G^X mod P  */
    mpi GY;         /**<  peer = G^Y mod P  */
    mpi K;          /**<  key = GY^X mod P  */
    mpi RP;         /**<  cached R^2 mod P  */
} dhm_context;

#ifdef __cplusplus
extern "C" {
#endif

    /**
       @brief          Parse the ServerKeyExchange parameters
       @param ctx      DHM context
       @param p        &(start of input buffer)
       @param end      end of buffer
       @return         0 if successful, or an EST_ERR_DHM_XXX error code
     */
    PUBLIC int dhm_read_params(dhm_context *ctx, uchar **p, uchar *end);

    /**
       @brief          Setup and write the ServerKeyExchange parameters
       @param ctx      DHM context
       @param x_size   private value size in bits
       @param output   destination buffer
       @param olen     number of chars written
       @param f_rng    RNG function
       @param p_rng    RNG parameter
       @note           This function assumes that ctx->P and ctx->G have already been properly set (for example
                       using mpi_read_string or mpi_read_binary).
       @return         0 if successful, or an EST_ERR_DHM_XXX error code
     */
    PUBLIC int dhm_make_params(dhm_context *ctx, int s_size, uchar *output, int *olen, int (*f_rng) (void *), void *p_rng);

    /**
       @brief          Import the peer's public value G^Y
       @param ctx      DHM context
       @param input    input buffer
       @param ilen     size of buffer
       @return         0 if successful, or an EST_ERR_DHM_XXX error code
     */
    PUBLIC int dhm_read_public(dhm_context *ctx, uchar *input, int ilen);

    /**
       @brief          Create own private value X and export G^X
       @param ctx      DHM context
       @param x_size   private value size in bits
       @param output   destination buffer
       @param olen     must be equal to ctx->P.len
       @param f_rng    RNG function
       @param p_rng    RNG parameter
       @return         0 if successful, or an EST_ERR_DHM_XXX error code
     */
    PUBLIC int dhm_make_public(dhm_context *ctx, int s_size, uchar *output, int olen, int (*f_rng) (void *), void *p_rng);

    /**
       @brief          Derive and export the shared secret (G^Y)^X mod P
       @param ctx      DHM context
       @param output   destination buffer
       @param olen     number of chars written
       @return         0 if successful, or an EST_ERR_DHM_XXX error code
     */
    PUBLIC int dhm_calc_secret(dhm_context *ctx, uchar *output, int *olen);

    /*
       @brief          Free the components of a DHM key
     */
    PUBLIC void dhm_free(dhm_context *ctx);

#if UNUSED
    /**
       @brief          Checkup routine
       @return         0 if successful, or 1 if the test failed
     */
    PUBLIC int dhm_self_test(int verbose);
#endif

#ifdef __cplusplus
}
#endif
#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/rsa.h ************/


/*
    rsa.h -- RSA The RSA public-key cryptosystem


    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#ifndef EST_RSA_H
#define EST_RSA_H

#define EST_ERR_RSA_BAD_INPUT_DATA                    -0x0400
#define EST_ERR_RSA_INVALID_PADDING                   -0x0410
#define EST_ERR_RSA_KEY_GEN_FAILED                    -0x0420
#define EST_ERR_RSA_KEY_CHECK_FAILED                  -0x0430
#define EST_ERR_RSA_PUBLIC_FAILED                     -0x0440
#define EST_ERR_RSA_PRIVATE_FAILED                    -0x0450
#define EST_ERR_RSA_VERIFY_FAILED                     -0x0460
#define EST_ERR_RSA_OUTPUT_TO_LARGE                   -0x0470

/*
   PKCS#1 constants
 */
#define RSA_RAW         0
#define RSA_MD2         2
#define RSA_MD4         3
#define RSA_MD5         4
#define RSA_SHA1        5
#define RSA_SHA256      6

#define RSA_PUBLIC      0
#define RSA_PRIVATE     1

#define RSA_PKCS_V15    0
#define RSA_PKCS_V21    1

#define RSA_SIGN        1
#define RSA_CRYPT       2

/*
   DigestInfo ::= SEQUENCE {
     digestAlgorithm DigestAlgorithmIdentifier,
     digest Digest }
  
   DigestAlgorithmIdentifier ::= AlgorithmIdentifier
  
   Digest ::= OCTET STRING
 */
#define EST_ASN1_HASH_MDX                   \
    "\x30\x20\x30\x0C\x06\x08\x2A\x86\x48"  \
    "\x86\xF7\x0D\x02\x00\x05\x00\x04\x10"

#define EST_ASN1_HASH_SHA1                  \
    "\x30\x21\x30\x09\x06\x05\x2B\x0E\x03"  \
    "\x02\x1A\x05\x00\x04\x14"

/**
   @brief          RSA context structure
 */
typedef struct {
    int ver;            /**< always 0          */
    int len;            /**< size(N) in chars  */
    mpi N;              /**< public modulus    */
    mpi E;              /**< public exponent   */

    mpi D;              /**< private exponent  */
    mpi P;              /**< 1st prime factor  */
    mpi Q;              /**< 2nd prime factor  */
    mpi DP;             /**< D % (P - 1)       */
    mpi DQ;             /**< D % (Q - 1)       */
    mpi QP;             /**< 1 / (Q % P)       */

    mpi RN;             /**< cached R^2 mod N  */
    mpi RP;             /**< cached R^2 mod P  */
    mpi RQ;             /**< cached R^2 mod Q  */

    int padding;        /**< 1.5 or OAEP/PSS   */
    int hash_id;        /**< hash identifier   */
    int (*f_rng)(void*);/**< RNG function      */
    void *p_rng;        /**< RNG parameter     */
} rsa_context;

#ifdef __cplusplus
extern "C" {
#endif

    /**
       @brief          Initialize an RSA context
       @param ctx      RSA context to be initialized
       @param padding  RSA_PKCS_V15 or RSA_PKCS_V21
       @param hash_id  RSA_PKCS_V21 hash identifier
       @param f_rng    RNG function
       @param p_rng    RNG parameter
       @note           The hash_id parameter is actually ignored when using RSA_PKCS_V15 padding.
       @note           Currently (xyssl-0.8), RSA_PKCS_V21 padding is not supported.
     */
    PUBLIC void rsa_init(rsa_context * ctx, int padding, int hash_id, int (*f_rng) (void *), void *p_rng);

    /**
       @brief          Generate an RSA keypair
       @param ctx      RSA context that will hold the key
       @param nbits    size of the public key in bits
       @param exponent public exponent (e.g., 65537)
       @note           rsa_init() must be called beforehand to setup the RSA context (especially f_rng and p_rng).
       @return         0 if successful, or an EST_ERR_RSA_XXX error code
     */
    PUBLIC int rsa_gen_key(rsa_context * ctx, int nbits, int exponent);

    /**
       @brief          Check a public RSA key
       @param ctx      RSA context to be checked
       @return         0 if successful, or an EST_ERR_RSA_XXX error code
     */
    PUBLIC int rsa_check_pubkey(rsa_context * ctx);

    /**
       @brief          Check a private RSA key
       @param ctx      RSA context to be checked
       @return         0 if successful, or an EST_ERR_RSA_XXX error code
     */
    PUBLIC int rsa_check_privkey(rsa_context * ctx);

    /**
       @brief          Do an RSA public key operation
       @param ctx      RSA context
       @param input    input buffer
       @param output   output buffer
       @return         0 if successful, or an EST_ERR_RSA_XXX error code
       @note           This function does NOT take care of message padding. Also, be sure to set input[0] = 0.
       @note           The input and output buffers must be large enough (eg. 128 bytes if RSA-1024 is used).
     */
    PUBLIC int rsa_public(rsa_context * ctx, uchar *input, uchar *output);

    /**
       @brief          Do an RSA private key operation
       @param ctx      RSA context
       @param input    input buffer
       @param output   output buffer
       @return         0 if successful, or an EST_ERR_RSA_XXX error code
       @note           The input and output buffers must be large enough (eg. 128 bytes if RSA-1024 is used).
     */
    PUBLIC int rsa_private(rsa_context * ctx, uchar *input, uchar *output);

    /**
       @brief          Add the message padding, then do an RSA operation
       @param ctx      RSA context
       @param mode     RSA_PUBLIC or RSA_PRIVATE
       @param ilen     contains the the plaintext length
       @param input    buffer holding the data to be encrypted
       @param output   buffer that will hold the ciphertext
       @return         0 if successful, or an EST_ERR_RSA_XXX error code
       @note           The output buffer must be as large as the size of ctx->N (eg. 128 bytes if RSA-1024 is used).
     */
    PUBLIC int rsa_pkcs1_encrypt(rsa_context * ctx, int mode, int ilen, uchar *input, uchar *output);

    /**
       @brief          Do an RSA operation, then remove the message padding
       @param ctx      RSA context
       @param mode     RSA_PUBLIC or RSA_PRIVATE
       @param input    buffer holding the encrypted data
       @param output   buffer that will hold the plaintext
       @param olen     will contain the plaintext length
       @param output_max_len    maximum length of the output buffer
       @return         0 if successful, or an EST_ERR_RSA_XXX error code
       @note           The output buffer must be as large as the size of ctx->N (eg. 128 bytes if RSA-1024 is used) otherwise
                       an error is thrown.
     */
    PUBLIC int rsa_pkcs1_decrypt(rsa_context * ctx, int mode, int *olen, uchar *input, uchar *output, int output_max_len);

    /**
       @brief          Do a private RSA to sign a message digest
       @param ctx      RSA context
       @param mode     RSA_PUBLIC or RSA_PRIVATE
       @param hash_id  RSA_RAW, RSA_MD{2,4,5} or RSA_SHA{1,256}
       @param hashlen  message digest length (for RSA_RAW only)
       @param hash     buffer holding the message digest
       @param sig      buffer that will hold the ciphertext
       @return         0 if the signing operation was successful, or an EST_ERR_RSA_XXX error code
       @note           The "sig" buffer must be as large as the size of ctx->N (eg. 128 bytes if RSA-1024 is used).
     */
    PUBLIC int rsa_pkcs1_sign(rsa_context * ctx, int mode, int hash_id, int hashlen, uchar *hash, uchar *sig);

    /**
       @brief          Do a public RSA and check the message digest
       @param ctx      points to an RSA public key
       @param mode     RSA_PUBLIC or RSA_PRIVATE
       @param hash_id  RSA_RAW, RSA_MD{2,4,5} or RSA_SHA{1,256}
       @param hashlen  message digest length (for RSA_RAW only)
       @param hash     buffer holding the message digest
       @param sig      buffer holding the ciphertext
       @return         0 if the verify operation was successful, or an EST_ERR_RSA_XXX error code
       @note           The "sig" buffer must be as large as the size of ctx->N (eg. 128 bytes if RSA-1024 is used).
     */
    PUBLIC int rsa_pkcs1_verify(rsa_context * ctx, int mode, int hash_id, int hashlen, uchar *hash, uchar *sig);

    /**
       @brief          Free the components of an RSA key
     */
    PUBLIC void rsa_free(rsa_context * ctx);

    /**
       @brief          Checkup routine
       @return         0 if successful, or 1 if the test failed
     */
    PUBLIC int rsa_self_test(int verbose);

#ifdef __cplusplus
}
#endif
#endif              /* rsa.h */

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/md5.h ************/


/*
    md5.h c -- RFC 1321 compliant MD5 implementation

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#ifndef EST_MD5_H
#define EST_MD5_H

/**
   @brief          MD5 context structure
 */
typedef struct {
    ulong total[2];     /**< number of bytes processed  */
    ulong state[4];     /**< intermediate digest state  */
    uchar buffer[64];   /**< data block being processed */
    uchar ipad[64];     /**< HMAC: inner padding        */
    uchar opad[64];     /**< HMAC: outer padding        */
} md5_context;

#ifdef __cplusplus
extern "C" {
#endif

    /**
       @brief          MD5 context setup
       @param ctx      context to be initialized
     */
    PUBLIC void md5_starts(md5_context * ctx);

    /**
       @brief          MD5 process buffer
       @param ctx      MD5 context
       @param input    buffer holding the  data
       @param ilen     length of the input data
     */
    PUBLIC void md5_update(md5_context * ctx, uchar *input, int ilen);

    /**
       @brief          MD5 final digest
       @param ctx      MD5 context
       @param output   MD5 checksum result
     */
    PUBLIC void md5_finish(md5_context * ctx, uchar output[16]);

    /**
       @brief          Output = MD5( input buffer )
       @param input    buffer holding the  data
       @param ilen     length of the input data
       @param output   MD5 checksum result
     */
    PUBLIC void md5(uchar *input, int ilen, uchar output[16]);

    /**
       @brief          Output = MD5( file contents )
       @param path     input file name
       @param output   MD5 checksum result
      
       @return         0 if successful, 1 if fopen failed, or 2 if fread failed
     */
    PUBLIC int md5_file(char *path, uchar output[16]);

    /**
       @brief          MD5 HMAC context setup
       @param ctx      HMAC context to be initialized
       @param key      HMAC secret key
       @param keylen   length of the HMAC key
     */
    PUBLIC void md5_hmac_starts(md5_context * ctx, uchar *key, int keylen);

    /**
       @brief          MD5 HMAC process buffer
       @param ctx      HMAC context
       @param input    buffer holding the  data
       @param ilen     length of the input data
     */
    PUBLIC void md5_hmac_update(md5_context * ctx, uchar *input, int ilen);

    /**
       @brief          MD5 HMAC final digest
       @param ctx      HMAC context
       @param output   MD5 HMAC checksum result
     */
    PUBLIC void md5_hmac_finish(md5_context * ctx, uchar output[16]);

    /**
       @brief          Output = HMAC-MD5( hmac key, input buffer )
       @param key      HMAC secret key
       @param keylen   length of the HMAC key
       @param input    buffer holding the  data
       @param ilen     length of the input data
       @param output   HMAC-MD5 result
     */
    PUBLIC void md5_hmac(uchar *key, int keylen, uchar *input, int ilen, uchar output[16]);

    /**
       @brief          Checkup routine
       @return         0 if successful, or 1 if the test failed
     */
    PUBLIC int md5_self_test(int verbose);

#ifdef __cplusplus
}
#endif
#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/sha1.h ************/


/*
    sha1.h -- 

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#ifndef EST_SHA1_H
#define EST_SHA1_H

/**
   @brief SHA-1 context structure
 */
typedef struct {
    ulong total[2];     /**< number of bytes processed  */
    ulong state[5];     /**< intermediate digest state  */
    uchar buffer[64];   /**< data block being processed */
    uchar ipad[64];     /**< HMAC: inner padding        */
    uchar opad[64];     /**< HMAC: outer padding        */
} sha1_context;

#ifdef __cplusplus
extern "C" {
#endif

    /**
       @brief          SHA-1 context setup
       @param ctx      context to be initialized
     */
    PUBLIC void sha1_starts(sha1_context * ctx);

    /**
       @brief          SHA-1 process buffer
       @param ctx      SHA-1 context
       @param input    buffer holding the  data
       @param ilen     length of the input data
     */
    PUBLIC void sha1_update(sha1_context * ctx, uchar *input, int ilen);

    /**
       @brief          SHA-1 final digest
       @param ctx      SHA-1 context
       @param output   SHA-1 checksum result
     */
    PUBLIC void sha1_finish(sha1_context * ctx, uchar output[20]);

    /**
       @brief          Output = SHA-1( input buffer )
       @param input    buffer holding the  data
       @param ilen     length of the input data
       @param output   SHA-1 checksum result
     */
    PUBLIC void sha1(uchar *input, int ilen, uchar output[20]);

    /**
       @brief          Output = SHA-1( file contents )
       @param path     input file name
       @param output   SHA-1 checksum result
       @return         0 if successful, 1 if fopen failed, or 2 if fread failed
     */
    int sha1_file(char *path, uchar output[20]);

    /**
       @brief          SHA-1 HMAC context setup
       @param ctx      HMAC context to be initialized
       @param key      HMAC secret key
       @param keylen   length of the HMAC key
     */
    PUBLIC void sha1_hmac_starts(sha1_context * ctx, uchar *key, int keylen);

    /**
       @brief          SHA-1 HMAC process buffer
       @param ctx      HMAC context
       @param input    buffer holding the  data
       @param ilen     length of the input data
     */
    PUBLIC void sha1_hmac_update(sha1_context * ctx, uchar *input, int ilen);

    /**
       @brief          SHA-1 HMAC final digest
       @param ctx      HMAC context
       @param output   SHA-1 HMAC checksum result
     */
    PUBLIC void sha1_hmac_finish(sha1_context * ctx, uchar output[20]);

    /**
       @brief          Output = HMAC-SHA-1( hmac key, input buffer )
       @param key      HMAC secret key
       @param keylen   length of the HMAC key
       @param input    buffer holding the  data
       @param ilen     length of the input data
       @param output   HMAC-SHA-1 result
     */
    PUBLIC void sha1_hmac(uchar *key, int keylen, uchar *input, int ilen, uchar output[20]);

    /**
       @brief          Checkup routine
       @return         0 if successful, or 1 if the test failed
     */
    PUBLIC int sha1_self_test(int verbose);

#ifdef __cplusplus
}
#endif
#endif              /* sha1.h */

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/x509.h ************/


/*
    x509.h -- Header for the Multithreaded Portable Runtime (MPR).

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#ifndef EST_X509_H
#define EST_X509_H

#define EST_ERR_ASN1_OUT_OF_DATA                      -0x0014
#define EST_ERR_ASN1_UNEXPECTED_TAG                   -0x0016
#define EST_ERR_ASN1_INVALID_LENGTH                   -0x0018
#define EST_ERR_ASN1_LENGTH_MISMATCH                  -0x001A
#define EST_ERR_ASN1_INVALID_DATA                     -0x001C

#define EST_ERR_X509_FEATURE_UNAVAILABLE              -0x0020
#define EST_ERR_X509_CERT_INVALID_PEM                 -0x0040
#define EST_ERR_X509_CERT_INVALID_FORMAT              -0x0060
#define EST_ERR_X509_CERT_INVALID_VERSION             -0x0080
#define EST_ERR_X509_CERT_INVALID_SERIAL              -0x00A0
#define EST_ERR_X509_CERT_INVALID_ALG                 -0x00C0
#define EST_ERR_X509_CERT_INVALID_NAME                -0x00E0
#define EST_ERR_X509_CERT_INVALID_DATE                -0x0100
#define EST_ERR_X509_CERT_INVALID_PUBKEY              -0x0120
#define EST_ERR_X509_CERT_INVALID_SIGNATURE           -0x0140
#define EST_ERR_X509_CERT_INVALID_EXTENSIONS          -0x0160
#define EST_ERR_X509_CERT_UNKNOWN_VERSION             -0x0180
#define EST_ERR_X509_CERT_UNKNOWN_SIG_ALG             -0x01A0
#define EST_ERR_X509_CERT_UNKNOWN_PK_ALG              -0x01C0
#define EST_ERR_X509_CERT_SIG_MISMATCH                -0x01E0
#define EST_ERR_X509_CERT_VERIFY_FAILED               -0x0200
#define EST_ERR_X509_KEY_INVALID_PEM                  -0x0220
#define EST_ERR_X509_KEY_INVALID_VERSION              -0x0240
#define EST_ERR_X509_KEY_INVALID_FORMAT               -0x0260
#define EST_ERR_X509_KEY_INVALID_ENC_IV               -0x0280
#define EST_ERR_X509_KEY_UNKNOWN_ENC_ALG              -0x02A0
#define EST_ERR_X509_KEY_PASSWORD_REQUIRED            -0x02C0
#define EST_ERR_X509_KEY_PASSWORD_MISMATCH            -0x02E0
#define EST_ERR_X509_POINT_ERROR                      -0x0300
#define EST_ERR_X509_VALUE_TO_LENGTH                  -0x0320

#define BADCERT_EXPIRED                 1
#define BADCERT_REVOKED                 2
#define BADCERT_CN_MISMATCH             4
#define BADCERT_NOT_TRUSTED             8
#define BADCERT_SELF_SIGNED             0x10

/*
   DER constants
 */
#define EST_ASN1_BOOLEAN                 0x01
#define EST_ASN1_INTEGER                 0x02
#define EST_ASN1_ME_STRING              0x03
#define EST_ASN1_OCTET_STRING            0x04
#define EST_ASN1_NULL                    0x05
#define EST_ASN1_OID                     0x06
#define EST_ASN1_UTF8_STRING             0x0C
#define EST_ASN1_SEQUENCE                0x10
#define EST_ASN1_SET                     0x11
#define EST_ASN1_PRINTABLE_STRING        0x13
#define EST_ASN1_T61_STRING              0x14
#define EST_ASN1_IA5_STRING              0x16
#define EST_ASN1_UTC_TIME                0x17
#define EST_ASN1_UNIVERSAL_STRING        0x1C
#define EST_ASN1_BMP_STRING              0x1E
#define EST_ASN1_PRIMITIVE               0x00
#define EST_ASN1_CONSTRUCTED             0x20
#define EST_ASN1_CONTEXT_SPECIFIC        0x80

/*
   various object identifiers
 */
#define X520_COMMON_NAME                3
#define X520_COUNTRY                    6
#define X520_LOCALITY                   7
#define X520_STATE                      8
#define X520_ORGANIZATION              10
#define X520_ORG_UNIT                  11
#define PKCS9_EMAIL                     1

/*
    FUTURE
   Street   9
   Surname  4
   Serial   5
   Title    12
   Desc     13
   BusinessCategory 15
   PostalAddress    16
   PostCode         17
   PostOfficeBox         18
   PhysicalDeliveryOfficeName   19
   Telephone   20
   Telex   21
   TelexID 22
   Fax  23
 */

#define X509_OUTPUT_DER              0x01
#define X509_OUTPUT_PEM              0x02
#define PEM_LINE_LENGTH                72
#define X509_ISSUER                  0x01
#define X509_SUBJECT                 0x02

#define OID_X520                "\x55\x04"
#define OID_CN                  "\x55\x04\x03"
#define OID_PKCS1               "\x2A\x86\x48\x86\xF7\x0D\x01\x01"
#define OID_PKCS1_RSA           "\x2A\x86\x48\x86\xF7\x0D\x01\x01\x01"
#define OID_PKCS1_RSA_SHA       "\x2A\x86\x48\x86\xF7\x0D\x01\x01\x05"
#define OID_PKCS9               "\x2A\x86\x48\x86\xF7\x0D\x01\x09"
#define OID_PKCS9_EMAIL         "\x2A\x86\x48\x86\xF7\x0D\x01\x09\x01"

/*
   Structures for parsing X.509 certificates
 */
typedef struct _x509_buf {
    int tag;
    int len;
    uchar *p;
} x509_buf;

typedef struct _x509_name {
    x509_buf oid;
    x509_buf val;
    struct _x509_name *next;
} x509_name;

typedef struct _x509_time {
    int year, mon, day;
    int hour, min, sec;
} x509_time;

typedef struct _x509_cert {
    x509_buf raw;
    x509_buf tbs;

    int version;
    x509_buf serial;
    x509_buf sig_oid1;

    x509_buf issuer_raw;
    x509_buf subject_raw;

    x509_name issuer;
    x509_name subject;

    x509_time valid_from;
    x509_time valid_to;

    x509_buf pk_oid;
    rsa_context rsa;

    x509_buf issuer_id;
    x509_buf subject_id;
    x509_buf v3_ext;

    int ca_istrue;
    int max_pathlen;

    x509_buf sig_oid2;
    x509_buf sig;

    struct _x509_cert *next;
} x509_cert;


/*
   Structures for writing X.509 certificates
 */
typedef struct _x509_node {
    uchar *data;
    uchar *p;
    uchar *end;
    size_t len;
} x509_node;

typedef struct _x509_raw {
    x509_node raw;
    x509_node tbs;
    x509_node version;
    x509_node serial;
    x509_node tbs_signalg;
    x509_node issuer;
    x509_node validity;
    x509_node subject;
    x509_node subpubkey;
    x509_node signalg;
    x509_node sign;
} x509_raw;

#ifdef __cplusplus
extern "C" {
#endif

    /**
       @brief          Parse one or more certificates and add them to the chained list
       @param chain    points to the start of the chain
       @param buf      buffer holding the certificate data
       @param buflen   size of the buffer
       @return         0 if successful, or a specific X509 error code
     */
    PUBLIC int x509parse_crt(x509_cert * crt, uchar *buf, int buflen);

    /**
       @brief          Load one or more certificates and add them to the chained list
       @param chain    points to the start of the chain
       @param path     filename to read the certificates from
       @return         0 if successful, or a specific X509 error code
     */
    PUBLIC int x509parse_crtfile(x509_cert * crt, char *path);

    /**
       @brief          Parse a private RSA key
       @param rsa      RSA context to be initialized
       @param buf      input buffer
       @param buflen   size of the buffer
       @param pwd      password for decryption (optional)
       @param pwdlen   size of the password
       @return         0 if successful, or a specific X509 error code
     */
    PUBLIC int x509parse_key(rsa_context * rsa, uchar *buf, int buflen, uchar *pwd, int pwdlen);

    /**
       @brief          Load and parse a private RSA key
       @param rsa      RSA context to be initialized
       @param path     filename to read the private key from
       @param pwd      password to decrypt the file (can be NULL)
       @return         0 if successful, or a specific X509 error code
     */
    PUBLIC int x509parse_keyfile(rsa_context * rsa, char *path, char *password);

    /**
       @brief          Store the certificate DN in printable form into buf;
                       no more than (bufsize) characters will be written.
     */
    PUBLIC int x509parse_dn_gets(char *prefix, char *buf, int bufsize, x509_name *dn);

    /**
     * @brief          Returns an informational string about the
     *                 certificate. 
     */
    PUBLIC char *x509parse_cert_info(char *prefix, char *buf, int bufsize, x509_cert *crt);

    /**
     * @brief          Return 0 if the certificate is still valid,
     *                 or BADCERT_EXPIRED
     */
    PUBLIC int x509parse_expired(x509_cert * crt);

    /**
       @brief          Verify the certificate signature
       @param crt      a certificate to be verified
       @param trust_ca the trusted CA chain
       @param cn       expected Common Name (can be set to NULL if the CN must not be verified)
       @param flags    result of the verification
       @return         0 if successful or EST_ERR_X509_SIG_VERIFY_FAILED,
                       in which case *flags will have one or more of
                       the following values set: BADCERT_EXPIRED BADCERT_REVOKED BADCERT_CN_MISMATCH BADCERT_NOT_TRUSTED 
                        BADCERT_SELF_SIGNED
       @note           add two arguments, depth and crl
     */
    PUBLIC int x509parse_verify(x509_cert * crt, x509_cert * trust_ca, char *cn, int *flags);

    /**
        @brief          Unallocate all certificate data
     */
    PUBLIC void x509_free(x509_cert * crt);

    /**
        @brief          Checkup routine
        @return         0 if successful, or 1 if the test failed
     */
    PUBLIC int x509_self_test(int verbose);

#ifdef __cplusplus
}
#endif
#endif              /* x509.h */

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/ssl.h ************/


/*
    ssl.h -- SSL/TLS function

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#ifndef EST_SSL_H
#define EST_SSL_H

#define EST_ERR_SSL_FEATURE_UNAVAILABLE               -0x1000
#define EST_ERR_SSL_BAD_INPUT_DATA                    -0x1800
#define EST_ERR_SSL_INVALID_MAC                       -0x2000
#define EST_ERR_SSL_INVALID_RECORD                    -0x2800
#define EST_ERR_SSL_INVALID_MODULUS_SIZE              -0x3000
#define EST_ERR_SSL_UNKNOWN_CIPHER                    -0x3800
#define EST_ERR_SSL_NO_CIPHER_CHOSEN                  -0x4000
#define EST_ERR_SSL_NO_SESSION_FOUND                  -0x4800
#define EST_ERR_SSL_NO_CLIENT_CERTIFICATE             -0x5000
#define EST_ERR_SSL_CERTIFICATE_TOO_LARGE             -0x5800
#define EST_ERR_SSL_CERTIFICATE_REQUIRED              -0x6000
#define EST_ERR_SSL_PRIVATE_KEY_REQUIRED              -0x6800
#define EST_ERR_SSL_CA_CHAIN_REQUIRED                 -0x7000
#define EST_ERR_SSL_UNEXPECTED_MESSAGE                -0x7800
#define EST_ERR_SSL_FATAL_ALERT_MESSAGE               -0x8000
#define EST_ERR_SSL_PEER_VERIFY_FAILED                -0x8800
#define EST_ERR_SSL_PEER_CLOSE_NOTIFY                 -0x9000
#define EST_ERR_SSL_BAD_HS_CLIENT_HELLO               -0x9800
#define EST_ERR_SSL_BAD_HS_SERVER_HELLO               -0xA000
#define EST_ERR_SSL_BAD_HS_CERTIFICATE                -0xA800
#define EST_ERR_SSL_BAD_HS_CERTIFICATE_REQUEST        -0xB000
#define EST_ERR_SSL_BAD_HS_SERVER_KEY_EXCHANGE        -0xB800
#define EST_ERR_SSL_BAD_HS_SERVER_HELLO_DONE          -0xC000
#define EST_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE        -0xC800
#define EST_ERR_SSL_BAD_HS_CERTIFICATE_VERIFY         -0xD000
#define EST_ERR_SSL_BAD_HS_CHANGE_CIPHER_SPEC         -0xD800
#define EST_ERR_SSL_BAD_HS_FINISHED                   -0xE000

/*
    Various constants
    TODO - change to correspond to TLS version numbers
 */
#define SSL_MAJOR_VERSION_3             3
#define SSL_MINOR_VERSION_0             0   /**< SSL v3.0 */
#define SSL_MINOR_VERSION_1             1   /**< TLS v1.0 */
#define SSL_MINOR_VERSION_2             2   /**< TLS v1.1 */

#define SSL_IS_CLIENT                   0
#define SSL_IS_SERVER                   1
#define SSL_COMPRESS_NULL               0

#define SSL_VERIFY_NO_CHECK             0
#define SSL_VERIFY_OPTIONAL             1
#define SSL_VERIFY_REQUIRED             2

#define SSL_MAX_CONTENT_LEN         16384

/*
    Allow an extra 512 bytes for the record header and encryption overhead (counter + MAC + padding).
 */
#define SSL_BUFFER_LEN (SSL_MAX_CONTENT_LEN + 512)

typedef struct EstCipher {
    char    *name;
    int     code;
} EstCipher;

/*
   Supported ciphersuites
 */
#define TLS_RSA_WITH_RC4_128_MD5                0x4
#define TLS_RSA_WITH_RC4_128_SHA                0x5
#define TLS_RSA_WITH_3DES_EDE_CBC_SHA           0xA
#define TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA       0x16
#define TLS_RSA_WITH_AES_128_CBC_SHA            0x2F
#define TLS_RSA_WITH_AES_256_CBC_SHA            0x35
#define TLS_DHE_RSA_WITH_AES_256_CBC_SHA        0x39
#define TLS_RSA_WITH_CAMELLIA_128_CBC_SHA       0x41
#define TLS_RSA_WITH_CAMELLIA_256_CBC_SHA       0x84
#define TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA   0x88

/*
   Message, alert and handshake types
 */
#define SSL_MSG_CHANGE_CIPHER_SPEC     20
#define SSL_MSG_ALERT                  21
#define SSL_MSG_HANDSHAKE              22
#define SSL_MSG_APPLICATION_DATA       23

#define SSL_ALERT_CLOSE_NOTIFY          0
#define SSL_ALERT_WARNING               1
#define SSL_ALERT_FATAL                 2
#define SSL_ALERT_NO_CERTIFICATE       41

#define SSL_HS_HELLO_REQUEST            0
#define SSL_HS_CLIENT_HELLO             1
#define SSL_HS_SERVER_HELLO             2
#define SSL_HS_CERTIFICATE             11
#define SSL_HS_SERVER_KEY_EXCHANGE     12
#define SSL_HS_CERTIFICATE_REQUEST     13
#define SSL_HS_SERVER_HELLO_DONE       14
#define SSL_HS_CERTIFICATE_VERIFY      15
#define SSL_HS_CLIENT_KEY_EXCHANGE     16
#define SSL_HS_FINISHED                20

/*
    TLS extensions
 */
#define TLS_EXT_SERVERNAME              0
#define TLS_EXT_SERVERNAME_HOSTNAME     0

/*
    SSL state machine
 */
typedef enum {
    SSL_HELLO_REQUEST,
    SSL_CLIENT_HELLO,
    SSL_SERVER_HELLO,
    SSL_SERVER_CERTIFICATE,
    SSL_SERVER_KEY_EXCHANGE,
    SSL_CERTIFICATE_REQUEST,
    SSL_SERVER_HELLO_DONE,
    SSL_CLIENT_CERTIFICATE,
    SSL_CLIENT_KEY_EXCHANGE,
    SSL_CERTIFICATE_VERIFY,
    SSL_CLIENT_CHANGE_CIPHER_SPEC,
    SSL_CLIENT_FINISHED,
    SSL_SERVER_CHANGE_CIPHER_SPEC,
    SSL_SERVER_FINISHED,
    SSL_FLUSH_BUFFERS,
    SSL_HANDSHAKE_OVER
} ssl_states;

typedef struct _ssl_session ssl_session;
typedef struct _ssl_context ssl_context;

/*
    This structure is used for session resuming.
 */
struct _ssl_session {
    time_t start;       /**< starting time      */
    int cipher;         /**< chosen cipher      */
    int length;         /**< session id length  */
    uchar id[32];       /**< session identifier */
    uchar master[48];   /**< the master secret  */
    ssl_session *next;  /**< next session entry */
};

struct _ssl_context {
    /*
        Miscellaneous
     */
    int state;          /**< SSL handshake: current state     */

    int major_ver;      /**< equal to  SSL_MAJOR_VERSION_3    */
    int minor_ver;      /**< either 0 (SSL3) or 1 (TLS1.0)    */

    int max_major_ver;  /**< max. major version from client   */
    int max_minor_ver;  /**< max. minor version from client   */

    /*
        Callbacks (RNG, debug, I/O)
     */
    int (*f_rng) (void *);
    void (*f_dbg) (void *, int, char *);
    int (*f_recv) (void *, uchar *, int);
    int (*f_send) (void *, uchar *, int);

    void *p_rng;        /**< context for the RNG function     */
    void *p_dbg;        /**< context for the debug function   */
    void *p_recv;       /**< context for reading operations   */
    void *p_send;       /**< context for writing operations   */

    /*
        Session layer
     */
    int resume;                     /**< session resuming flag   */
    int timeout;                    /**< sess. expiration time   */
    ssl_session *session;           /**< current session data    */
    int (*s_get) (ssl_context *);   /**< (server) get callback   */
    int (*s_set) (ssl_context *);   /**< (server) set callback   */

    /*
        Record layer (incoming data)
     */
    uchar *in_ctr;      /**< 64-bit incoming message counter  */
    uchar *in_hdr;      /**< 5-byte record header (in_ctr+8)  */
    uchar *in_msg;      /**< the message contents (in_hdr+5)  */
    uchar *in_offt;     /**< read offset in application data  */

    int in_msgtype;     /**< record header: message type      */
    int in_msglen;      /**< record header: message length    */
    int in_left;        /**< amount of data read so far       */

    int in_hslen;       /**< current handshake message length */
    int nb_zero;        /**< # of 0-length encrypted messages */

    /*
        Record layer (outgoing data)
     */
    uchar *out_ctr; /**< 64-bit outgoing message counter  */
    uchar *out_hdr; /**< 5-byte record header (out_ctr+8) */
    uchar *out_msg; /**< the message contents (out_hdr+5) */

    int out_msgtype;    /**< record header: message type      */
    int out_msglen;     /**< record header: message length    */
    int out_left;       /**< amount of data not yet written   */

    /*
        PKI layer
     */
    rsa_context *rsa_key;   /**< own RSA private key     */
    x509_cert *own_cert;    /**< own X.509 certificate   */
    x509_cert *ca_chain;    /**< own trusted CA chain    */
    x509_cert *peer_cert;   /**< peer X.509 cert chain   */
    char *peer_cn;          /**< expected peer CN        */

    int endpoint;           /**< 0: client, 1: server    */
    int authmode;           /**< verification mode       */
    int client_auth;        /**< flag for client auth.   */
    int verify_result;      /**< verification result     */

    /*
        Crypto layer
     */
    dhm_context dhm_ctx;    /**< DHM key exchange        */
    md5_context fin_md5;    /**< Finished MD5 checksum   */
    sha1_context fin_sha1;  /**< Finished SHA-1 checksum */

    int do_crypt;           /**< en(de)cryption flag     */
    int *ciphers;           /**< allowed ciphersuites    */
    int pmslen;             /**< premaster length        */
    int keylen;             /**< symmetric key length    */
    int minlen;             /**< min. ciphertext length  */
    int ivlen;              /**< IV length               */
    int maclen;             /**< MAC length              */

    uchar randbytes[64];    /**< random bytes            */
    uchar premaster[256];   /**< premaster secret        */

    uchar iv_enc[16];   /**< IV (encryption)         */
    uchar iv_dec[16];   /**< IV (decryption)         */

    uchar mac_enc[32];  /**< MAC (encryption)        */
    uchar mac_dec[32];  /**< MAC (decryption)        */

    ulong ctx_enc[128]; /**< encryption context      */
    ulong ctx_dec[128]; /**< decryption context      */

    /*
        TLS extensions
     */
    uchar *hostname;
    ulong hostname_len;
};

#ifdef __cplusplus
extern "C" {
#endif

    extern int ssl_default_ciphers[];

    /**
       @brief          Initialize an SSL context
       @param ssl      SSL context
       @return         0 if successful, or 1 if memory allocation failed
     */
    PUBLIC int ssl_init(ssl_context *ssl);

    /**
       @brief          Set the current endpoint type
       @param ssl      SSL context
       @param endpoint must be SSL_IS_CLIENT or SSL_IS_SERVER
     */
    PUBLIC void ssl_set_endpoint(ssl_context *ssl, int endpoint);

    /**
       @brief          Set the certificate verification mode
       @param ssl      SSL context
       @param mode     can be:
        SSL_VERIFY_NO_CHECK:  peer certificate is not checked (default),
                              this is insecure and SHOULD be avoided.
        SSL_VERIFY_OPTIONAL:  peer certificate is checked, however the
                              handshake continues even if verification failed;
                              ssl_get_verify_result() can be called after the
                              handshake is complete.
        SSL_VERIFY_REQUIRED:  peer *must* present a valid certificate,
                              handshake is aborted if verification failed.
     */
    PUBLIC void ssl_set_authmode(ssl_context *ssl, int authmode);

    /**
       @brief          Set the random number generator callback
       @param ssl      SSL context
       @param f_rng    RNG function
       @param p_rng    RNG parameter
     */
    PUBLIC void ssl_set_rng(ssl_context *ssl, int (*f_rng) (void *), void *p_rng);

    /**
       @brief          Set the debug callback
       @param ssl      SSL context
       @param f_dbg    debug function
       @param p_dbg    debug parameter
     */
    PUBLIC void ssl_set_dbg(ssl_context *ssl, void (*f_dbg) (void *, int, char *), void *p_dbg);

    /**
       @brief          Set the underlying BIO read and write callbacks
       @param ssl      SSL context
       @param f_recv   read callback
       @param p_recv   read parameter
       @param f_send   write callback
       @param p_send   write parameter
     */
    PUBLIC void ssl_set_bio(ssl_context *ssl, int (*f_recv)(void*, uchar*, int), void *p_recv, 
        int (*f_send)(void*, uchar*, int), void *p_send);

    /**
       @brief          Set the session callbacks (server-side only)
       @param ssl      SSL context
       @param s_get    session get callback
       @param s_set    session set callback
     */
    PUBLIC void ssl_set_scb(ssl_context *ssl, int (*s_get)(ssl_context*), int (*s_set)(ssl_context*));

    /**
       @brief          Set the session resuming flag, timeout and data
       @param ssl      SSL context
       @param resume   if 0 (default), the session will not be resumed
       @param timeout  session timeout in seconds, or 0 (no timeout)
       @param session  session context
     */
    PUBLIC void ssl_set_session(ssl_context *ssl, int resume, int timeout, ssl_session *session);

    /**
       @brief          Set the list of allowed ciphersuites
       @param ssl      SSL context
       @param ciphers  0-terminated list of allowed ciphers
     */
    PUBLIC void ssl_set_ciphers(ssl_context *ssl, int *ciphers);

    /**
       @brief          Set the data required to verify peer certificate
       @param ssl      SSL context
       @param ca_chain trusted CA chain
       @param peer_cn  expected peer CommonName (or NULL)
       @note           add two more parameters: depth and crl
     */
    PUBLIC void ssl_set_ca_chain(ssl_context *ssl, x509_cert *ca_chain, char *peer_cn);

    /**
       @brief          Set own certificate and private key
       @param ssl      SSL context
       @param own_cert own public certificate
       @param rsa_key  own private RSA key
     */
    PUBLIC void ssl_set_own_cert(ssl_context *ssl, x509_cert *own_cert, rsa_context *rsa_key);

    /**
       @brief          Set the Diffie-Hellman public P and G values, read as hexadecimal strings (server-side only)
       @param ssl      SSL context
       @param dhm_P    Diffie-Hellman-Merkle modulus
       @param dhm_G    Diffie-Hellman-Merkle generator
       @return         0 if successful
     */
    PUBLIC int ssl_set_dh_param(ssl_context *ssl, char *dhm_P, char *dhm_G);

    /**
       @brief          Set hostname for ServerName TLS Extension
       @param ssl      SSL context
       @param hostname the server hostname
       @return         0 if successful
     */
    PUBLIC int ssl_set_hostname(ssl_context *ssl, char *hostname);

    /**
       @brief          Return the number of data bytes available to read
       @param ssl      SSL context
       @return         how many bytes are available in the read buffer
     */
    PUBLIC int ssl_get_bytes_avail(ssl_context *ssl);

    /**
       @brief          Return the result of the certificate verification
       @param ssl      SSL context
       @return         0 if successful, or a combination of: BADCERT_EXPIRED BADCERT_REVOKED BADCERT_CN_MISMATCH
                            BADCERT_NOT_TRUSTED BADCERT_SELF_SIGNED
     */
    PUBLIC int ssl_get_verify_result(ssl_context *ssl);

    /**
       @brief          Return the name of the current cipher
       @param ssl      SSL context
       @return         a string containing the cipher name
     */
    PUBLIC char *ssl_get_cipher(ssl_context *ssl);

    /**
       @brief          Perform the SSL handshake
       @param ssl      SSL context
       @return         0 if successful, EST_ERR_NET_TRY_AGAIN, or a specific SSL error code.
     */
    PUBLIC int ssl_handshake(ssl_context *ssl);

    /**
       @brief          Read at most 'len' application data bytes
       @param ssl      SSL context
       @param buf      buffer that will hold the data
       @param len      how many bytes must be read
       @return         This function returns the number of bytes read, or a negative error code.
     */
    PUBLIC int ssl_read(ssl_context *ssl, uchar *buf, int len);

    /**
       @brief          Write exactly 'len' application data bytes
       @param ssl      SSL context
       @param buf      buffer holding the data
       @param len      how many bytes must be written
       @return         This function returns the number of bytes written, or a negative error code.
       @note           When this function returns EST_ERR_NET_TRY_AGAIN, it must be called later with the *same* arguments,
                       until it returns a positive value.
     */
    PUBLIC int ssl_write(ssl_context *ssl, uchar *buf, int len);

    /**
       @brief          Notify the peer that the connection is being closed
     */
    PUBLIC int ssl_close_notify(ssl_context *ssl);

    /**
       @brief          Free an SSL context
     */
    PUBLIC void ssl_free(ssl_context *ssl);

    /*
       Internal functions (do not call directly)
     */
    PUBLIC int ssl_handshake_client(ssl_context *ssl);
    PUBLIC int ssl_handshake_server(ssl_context *ssl);

    PUBLIC int ssl_derive_keys(ssl_context *ssl);
    PUBLIC void ssl_calc_verify(ssl_context *ssl, uchar hash[36]);

    PUBLIC int ssl_read_record(ssl_context *ssl);
    PUBLIC int ssl_fetch_input(ssl_context *ssl, int nb_want);

    PUBLIC int ssl_write_record(ssl_context *ssl);
    PUBLIC int ssl_flush_output(ssl_context *ssl);

    PUBLIC int ssl_parse_certificate(ssl_context *ssl);
    PUBLIC int ssl_write_certificate(ssl_context *ssl);

    PUBLIC int ssl_parse_change_cipher_spec(ssl_context *ssl);
    PUBLIC int ssl_write_change_cipher_spec(ssl_context *ssl);

    PUBLIC int ssl_parse_finished(ssl_context *ssl);
    PUBLIC int ssl_write_finished(ssl_context *ssl);

#if EMBEDTHIS || 1
    PUBLIC int *ssl_create_ciphers(cchar *cipherSuite);
#endif

#ifdef __cplusplus
}
#endif
#endif              /* ssl.h */

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/aes.h ************/


/*
    aes.h -- 

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#ifndef EST_AES_H
#define EST_AES_H

#define AES_ENCRYPT     1
#define AES_DECRYPT     0

/**
    @brief AES context structure
 */
typedef struct {
    int     nr;         /**< number of rounds */
    ulong   *rk;        /**< AES round keys */
    ulong   buf[68];    /**<  unaligned data */
} aes_context;

#ifdef __cplusplus
extern "C" {
#endif

    /**
       @brief          AES key schedule (encryption)
       @param ctx      AES context to be initialized
       @param key      encryption key
       @param keysize  must be 128, 192 or 256
     */
    PUBLIC void aes_setkey_enc(aes_context *ctx, uchar *key, int keysize);

    /**
       @brief          AES key schedule (decryption)
       @param ctx      AES context to be initialized
       @param key      decryption key
       @param keysize  must be 128, 192 or 256
     */
    PUBLIC void aes_setkey_dec(aes_context *ctx, uchar *key, int keysize);

    /**
       @brief          AES-ECB block encryption/decryption
       @param ctx      AES context
       @param mode     AES_ENCRYPT or AES_DECRYPT
       @param input    16-byte input block
       @param output   16-byte output block
     */
    PUBLIC void aes_crypt_ecb(aes_context *ctx, int mode, uchar input[16], uchar output[16]);

    /**
       @brief          AES-CBC buffer encryption/decryption
       @param ctx      AES context
       @param mode     AES_ENCRYPT or AES_DECRYPT
       @param length   length of the input data
       @param iv       initialization vector (updated after use)
       @param input    buffer holding the input data
       @param output   buffer holding the output data
     */
    PUBLIC void aes_crypt_cbc(aes_context *ctx, int mode, int length, uchar iv[16], uchar *input, uchar *output);

    /**
       @brief          AES-CFB128 buffer encryption/decryption
       @param ctx      AES context
       @param mode     AES_ENCRYPT or AES_DECRYPT
       @param length   length of the input data
       @param iv_off   offset in IV (updated after use)
       @param iv       initialization vector (updated after use)
       @param input    buffer holding the input data
       @param output   buffer holding the output data
     */
    PUBLIC void aes_crypt_cfb128(aes_context *ctx, int mode, int length, int *iv_off, uchar iv[16], 
            uchar *input, uchar *output);

#if UNUSED
    /**
       @brief          Checkup routine
       @return         0 if successful, or 1 if the test failed
     */
    PUBLIC int aes_self_test(int verbose);
#endif

#ifdef __cplusplus
}
#endif
#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/arc4.h ************/


/*
    arc4.h -- ARCFOUR algorithm

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#ifndef EST_ARC4_H
#define EST_ARC4_H

/**
   @brief          ARC4 context structure
 */
typedef struct {
    int     x;          /**< permutation index */
    int     y;          /**< permutation index */
    uchar   m[256];     /**< permutation table */
} arc4_context;

#ifdef __cplusplus
extern "C" {
#endif

    /**
       @brief          ARC4 key schedule
       @param ctx      ARC4 context to be initialized
       @param key      the secret key
       @param keylen   length of the key
     */
    PUBLIC void arc4_setup(arc4_context *ctx, uchar *key, int keylen);

    /**
       @brief          ARC4 cipher function
       @param ctx      ARC4 context
       @param buf      buffer to be processed
       @param buflen   amount of data in buf
     */
    PUBLIC void arc4_crypt(arc4_context *ctx, uchar *buf, int buflen);

#if UNUSED
    /*
       @brief          Checkup routine
       @return         0 if successful, or 1 if the test failed
     */
    PUBLIC int arc4_self_test(int verbose);
#endif

#ifdef __cplusplus
}
#endif
#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/base64.h ************/


/*
    base64.h -- RFC 1521 base64 encoding/decoding

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#ifndef EST_BASE64_H
#define EST_BASE64_H

#define EST_ERR_BASE64_BUFFER_TOO_SMALL   -0x0010
#define EST_ERR_BASE64_INVALID_CHARACTER  -0x0012

#ifdef __cplusplus
extern "C" {
#endif

    /**
       @brief          Encode a buffer into base64 format
       @param dst      destination buffer
       @param dlen     size of the buffer
       @param src      source buffer
       @param slen     amount of data to be encoded
       @return         0 if successful, or EST_ERR_BASE64_BUFFER_TOO_SMALL. dlen is always updated to reflect the amount
                       of data that has (or would have) been written.
       @note           Call this function with *dlen = 0 to obtain the required buffer size in *dlen
     */
    PUBLIC int base64_encode(uchar *dst, int *dlen, uchar *src, int slen);

    /**
       @brief          Decode a base64-formatted buffer
       @param dst      destination buffer
       @param dlen     size of the buffer
       @param src      source buffer
       @param slen     amount of data to be decoded
       @return         0 if successful, EST_ERR_BASE64_BUFFER_TOO_SMALL, or EST_ERR_BASE64_INVALID_DATA if the input data is
                       not correct. *dlen is always updated to reflect the amount of data that has (or would have) been
                       written.  
       @note           Call this function with *dlen = 0 to obtain the required buffer size in *dlen
     */
    PUBLIC int base64_decode(uchar *dst, int *dlen, uchar *src, int slen);

#if UNUSED
    /**
       @brief          Checkup routine
       @return         0 if successful, or 1 if the test failed
     */
    PUBLIC int base64_self_test(int verbose);
#endif

#ifdef __cplusplus
}
#endif
#endif              /* base64.h */



/********* Start of file src/bn_mul.h ************/


/*
    bn_mul.h -- Header for the Multithreaded Portable Runtime (MPR).

    Copyright (c) All Rights Reserved. See details at the end of the file.

    Multiply source vector [s] with b, add result to destination vector [d] and set carry c.
  
    Currently supports:
       . IA-32 (386+)         . AMD64 / EM64T
       . IA-32 (SSE2)         . Motorola 68000
       . PowerPC, 32-bit      . MicroBlaze
       . PowerPC, 64-bit      . TriCore
       . SPARC v8             . ARM v3+
       . Alpha                . MIPS32
       . C, longlong          . C, generic
 */
#ifndef EST_BN_MUL_H
#define EST_BN_MUL_H

#if defined(EST_HAVE_ASM)

#if defined(__GNUC__)
#if defined(__i386__)

#define MULADDC_INIT                            \
    asm( "movl   %%ebx, %0      " : "=m" (t));  \
    asm( "movl   %0, %%esi      " :: "m" (s));  \
    asm( "movl   %0, %%edi      " :: "m" (d));  \
    asm( "movl   %0, %%ecx      " :: "m" (c));  \
    asm( "movl   %0, %%ebx      " :: "m" (b));

#define MULADDC_CORE                            \
    asm( "lodsl                 " );            \
    asm( "mull   %ebx           " );            \
    asm( "addl   %ecx,   %eax   " );            \
    asm( "adcl   $0,     %edx   " );            \
    asm( "addl   (%edi), %eax   " );            \
    asm( "adcl   $0,     %edx   " );            \
    asm( "movl   %edx,   %ecx   " );            \
    asm( "stosl                 " );

#if ME_EST_SSE2

#define MULADDC_HUIT                            \
    asm( "movd     %ecx,     %mm1     " );      \
    asm( "movd     %ebx,     %mm0     " );      \
    asm( "movd     (%edi),   %mm3     " );      \
    asm( "paddq    %mm3,     %mm1     " );      \
    asm( "movd     (%esi),   %mm2     " );      \
    asm( "pmuludq  %mm0,     %mm2     " );      \
    asm( "movd     4(%esi),  %mm4     " );      \
    asm( "pmuludq  %mm0,     %mm4     " );      \
    asm( "movd     8(%esi),  %mm6     " );      \
    asm( "pmuludq  %mm0,     %mm6     " );      \
    asm( "movd     12(%esi), %mm7     " );      \
    asm( "pmuludq  %mm0,     %mm7     " );      \
    asm( "paddq    %mm2,     %mm1     " );      \
    asm( "movd     4(%edi),  %mm3     " );      \
    asm( "paddq    %mm4,     %mm3     " );      \
    asm( "movd     8(%edi),  %mm5     " );      \
    asm( "paddq    %mm6,     %mm5     " );      \
    asm( "movd     12(%edi), %mm4     " );      \
    asm( "paddq    %mm4,     %mm7     " );      \
    asm( "movd     %mm1,     (%edi)   " );      \
    asm( "movd     16(%esi), %mm2     " );      \
    asm( "pmuludq  %mm0,     %mm2     " );      \
    asm( "psrlq    $32,      %mm1     " );      \
    asm( "movd     20(%esi), %mm4     " );      \
    asm( "pmuludq  %mm0,     %mm4     " );      \
    asm( "paddq    %mm3,     %mm1     " );      \
    asm( "movd     24(%esi), %mm6     " );      \
    asm( "pmuludq  %mm0,     %mm6     " );      \
    asm( "movd     %mm1,     4(%edi)  " );      \
    asm( "psrlq    $32,      %mm1     " );      \
    asm( "movd     28(%esi), %mm3     " );      \
    asm( "pmuludq  %mm0,     %mm3     " );      \
    asm( "paddq    %mm5,     %mm1     " );      \
    asm( "movd     16(%edi), %mm5     " );      \
    asm( "paddq    %mm5,     %mm2     " );      \
    asm( "movd     %mm1,     8(%edi)  " );      \
    asm( "psrlq    $32,      %mm1     " );      \
    asm( "paddq    %mm7,     %mm1     " );      \
    asm( "movd     20(%edi), %mm5     " );      \
    asm( "paddq    %mm5,     %mm4     " );      \
    asm( "movd     %mm1,     12(%edi) " );      \
    asm( "psrlq    $32,      %mm1     " );      \
    asm( "paddq    %mm2,     %mm1     " );      \
    asm( "movd     24(%edi), %mm5     " );      \
    asm( "paddq    %mm5,     %mm6     " );      \
    asm( "movd     %mm1,     16(%edi) " );      \
    asm( "psrlq    $32,      %mm1     " );      \
    asm( "paddq    %mm4,     %mm1     " );      \
    asm( "movd     28(%edi), %mm5     " );      \
    asm( "paddq    %mm5,     %mm3     " );      \
    asm( "movd     %mm1,     20(%edi) " );      \
    asm( "psrlq    $32,      %mm1     " );      \
    asm( "paddq    %mm6,     %mm1     " );      \
    asm( "movd     %mm1,     24(%edi) " );      \
    asm( "psrlq    $32,      %mm1     " );      \
    asm( "paddq    %mm3,     %mm1     " );      \
    asm( "movd     %mm1,     28(%edi) " );      \
    asm( "addl     $32,      %edi     " );      \
    asm( "addl     $32,      %esi     " );      \
    asm( "psrlq    $32,      %mm1     " );      \
    asm( "movd     %mm1,     %ecx     " );

#define MULADDC_STOP                            \
    asm( "emms                        " );      \
    asm( "movl   %0, %%ebx      " :: "m" (t));  \
    asm( "movl   %%ecx, %0      " : "=m" (c));  \
    asm( "movl   %%edi, %0      " : "=m" (d));  \
    asm( "movl   %%esi, %0      " : "=m" (s) :: \
    "eax", "ecx", "edx", "esi", "edi" );

#else

#define MULADDC_STOP                            \
    asm( "movl   %0, %%ebx      " :: "m" (t));  \
    asm( "movl   %%ecx, %0      " : "=m" (c));  \
    asm( "movl   %%edi, %0      " : "=m" (d));  \
    asm( "movl   %%esi, %0      " : "=m" (s) :: \
    "eax", "ecx", "edx", "esi", "edi" );

#endif /* SSE2 */
#endif /* i386 */

#if defined(__amd64__) || defined (__x86_64__)

#define MULADDC_INIT                            \
    asm( "movq   %0, %%rsi      " :: "m" (s));  \
    asm( "movq   %0, %%rdi      " :: "m" (d));  \
    asm( "movq   %0, %%rcx      " :: "m" (c));  \
    asm( "movq   %0, %%rbx      " :: "m" (b));  \
    asm( "xorq   %r8, %r8       " );

#define MULADDC_CORE                            \
    asm( "movq  (%rsi),%rax     " );            \
    asm( "mulq   %rbx           " );            \
    asm( "addq   $8,   %rsi     " );            \
    asm( "addq   %rcx, %rax     " );            \
    asm( "movq   %r8,  %rcx     " );            \
    asm( "adcq   $0,   %rdx     " );            \
    asm( "nop                   " );            \
    asm( "addq   %rax, (%rdi)   " );            \
    asm( "adcq   %rdx, %rcx     " );            \
    asm( "addq   $8,   %rdi     " );

#define MULADDC_STOP                            \
    asm( "movq   %%rcx, %0      " : "=m" (c));  \
    asm( "movq   %%rdi, %0      " : "=m" (d));  \
    asm( "movq   %%rsi, %0      " : "=m" (s) :: \
    "rax", "rcx", "rdx", "rbx", "rsi", "rdi", "r8" );

#endif /* AMD64 */

#if defined(__mc68020__) || defined(__mcpu32__)

#define MULADDC_INIT                            \
    asm( "movl   %0, %%a2       " :: "m" (s));  \
    asm( "movl   %0, %%a3       " :: "m" (d));  \
    asm( "movl   %0, %%d3       " :: "m" (c));  \
    asm( "movl   %0, %%d2       " :: "m" (b));  \
    asm( "moveq  #0, %d0        " );

#define MULADDC_CORE                            \
    asm( "movel  %a2@+, %d1     " );            \
    asm( "mulul  %d2, %d4:%d1   " );            \
    asm( "addl   %d3, %d1       " );            \
    asm( "addxl  %d0, %d4       " );            \
    asm( "moveq  #0,  %d3       " );            \
    asm( "addl   %d1, %a3@+     " );            \
    asm( "addxl  %d4, %d3       " );

#define MULADDC_STOP                            \
    asm( "movl   %%d3, %0       " : "=m" (c));  \
    asm( "movl   %%a3, %0       " : "=m" (d));  \
    asm( "movl   %%a2, %0       " : "=m" (s) :: \
    "d0", "d1", "d2", "d3", "d4", "a2", "a3" );

#define MULADDC_HUIT                            \
    asm( "movel  %a2@+, %d1     " );            \
    asm( "mulul  %d2, %d4:%d1   " );            \
    asm( "addxl  %d3, %d1       " );            \
    asm( "addxl  %d0, %d4       " );            \
    asm( "addl   %d1, %a3@+     " );            \
    asm( "movel  %a2@+, %d1     " );            \
    asm( "mulul  %d2, %d3:%d1   " );            \
    asm( "addxl  %d4, %d1       " );            \
    asm( "addxl  %d0, %d3       " );            \
    asm( "addl   %d1, %a3@+     " );            \
    asm( "movel  %a2@+, %d1     " );            \
    asm( "mulul  %d2, %d4:%d1   " );            \
    asm( "addxl  %d3, %d1       " );            \
    asm( "addxl  %d0, %d4       " );            \
    asm( "addl   %d1, %a3@+     " );            \
    asm( "movel  %a2@+, %d1     " );            \
    asm( "mulul  %d2, %d3:%d1   " );            \
    asm( "addxl  %d4, %d1       " );            \
    asm( "addxl  %d0, %d3       " );            \
    asm( "addl   %d1, %a3@+     " );            \
    asm( "movel  %a2@+, %d1     " );            \
    asm( "mulul  %d2, %d4:%d1   " );            \
    asm( "addxl  %d3, %d1       " );            \
    asm( "addxl  %d0, %d4       " );            \
    asm( "addl   %d1, %a3@+     " );            \
    asm( "movel  %a2@+, %d1     " );            \
    asm( "mulul  %d2, %d3:%d1   " );            \
    asm( "addxl  %d4, %d1       " );            \
    asm( "addxl  %d0, %d3       " );            \
    asm( "addl   %d1, %a3@+     " );            \
    asm( "movel  %a2@+, %d1     " );            \
    asm( "mulul  %d2, %d4:%d1   " );            \
    asm( "addxl  %d3, %d1       " );            \
    asm( "addxl  %d0, %d4       " );            \
    asm( "addl   %d1, %a3@+     " );            \
    asm( "movel  %a2@+, %d1     " );            \
    asm( "mulul  %d2, %d3:%d1   " );            \
    asm( "addxl  %d4, %d1       " );            \
    asm( "addxl  %d0, %d3       " );            \
    asm( "addl   %d1, %a3@+     " );            \
    asm( "addxl  %d0, %d3       " );

#endif /* MC68000 */

#if defined(__powerpc__)   || defined(__ppc__)
#if defined(__powerpc64__) || defined(__ppc64__)

#if defined(__MACH__) && defined(__APPLE__)

#define MULADDC_INIT                            \
    asm( "ld     r3, %0         " :: "m" (s));  \
    asm( "ld     r4, %0         " :: "m" (d));  \
    asm( "ld     r5, %0         " :: "m" (c));  \
    asm( "ld     r6, %0         " :: "m" (b));  \
    asm( "addi   r3, r3, -8     " );            \
    asm( "addi   r4, r4, -8     " );            \
    asm( "addic  r5, r5,  0     " );

#define MULADDC_CORE                            \
    asm( "ldu    r7, 8(r3)      " );            \
    asm( "mulld  r8, r7, r6     " );            \
    asm( "mulhdu r9, r7, r6     " );            \
    asm( "adde   r8, r8, r5     " );            \
    asm( "ld     r7, 8(r4)      " );            \
    asm( "addze  r5, r9         " );            \
    asm( "addc   r8, r8, r7     " );            \
    asm( "stdu   r8, 8(r4)      " );

#define MULADDC_STOP                            \
    asm( "addze  r5, r5         " );            \
    asm( "addi   r4, r4, 8      " );            \
    asm( "addi   r3, r3, 8      " );            \
    asm( "std    r5, %0         " : "=m" (c));  \
    asm( "std    r4, %0         " : "=m" (d));  \
    asm( "std    r3, %0         " : "=m" (s) :: \
    "r3", "r4", "r5", "r6", "r7", "r8", "r9" );

#else

#define MULADDC_INIT                            \
    asm( "ld     %%r3, %0       " :: "m" (s));  \
    asm( "ld     %%r4, %0       " :: "m" (d));  \
    asm( "ld     %%r5, %0       " :: "m" (c));  \
    asm( "ld     %%r6, %0       " :: "m" (b));  \
    asm( "addi   %r3, %r3, -8   " );            \
    asm( "addi   %r4, %r4, -8   " );            \
    asm( "addic  %r5, %r5,  0   " );

#define MULADDC_CORE                            \
    asm( "ldu    %r7, 8(%r3)    " );            \
    asm( "mulld  %r8, %r7, %r6  " );            \
    asm( "mulhdu %r9, %r7, %r6  " );            \
    asm( "adde   %r8, %r8, %r5  " );            \
    asm( "ld     %r7, 8(%r4)    " );            \
    asm( "addze  %r5, %r9       " );            \
    asm( "addc   %r8, %r8, %r7  " );            \
    asm( "stdu   %r8, 8(%r4)    " );

#define MULADDC_STOP                            \
    asm( "addze  %r5, %r5       " );            \
    asm( "addi   %r4, %r4, 8    " );            \
    asm( "addi   %r3, %r3, 8    " );            \
    asm( "std    %%r5, %0       " : "=m" (c));  \
    asm( "std    %%r4, %0       " : "=m" (d));  \
    asm( "std    %%r3, %0       " : "=m" (s) :: \
    "r3", "r4", "r5", "r6", "r7", "r8", "r9" );

#endif

#else /* PPC32 */

#if defined(__MACH__) && defined(__APPLE__)

#define MULADDC_INIT                            \
    asm( "lwz    r3, %0         " :: "m" (s));  \
    asm( "lwz    r4, %0         " :: "m" (d));  \
    asm( "lwz    r5, %0         " :: "m" (c));  \
    asm( "lwz    r6, %0         " :: "m" (b));  \
    asm( "addi   r3, r3, -4     " );            \
    asm( "addi   r4, r4, -4     " );            \
    asm( "addic  r5, r5,  0     " );

#define MULADDC_CORE                            \
    asm( "lwzu   r7, 4(r3)      " );            \
    asm( "mullw  r8, r7, r6     " );            \
    asm( "mulhwu r9, r7, r6     " );            \
    asm( "adde   r8, r8, r5     " );            \
    asm( "lwz    r7, 4(r4)      " );            \
    asm( "addze  r5, r9         " );            \
    asm( "addc   r8, r8, r7     " );            \
    asm( "stwu   r8, 4(r4)      " );

#define MULADDC_STOP                            \
    asm( "addze  r5, r5         " );            \
    asm( "addi   r4, r4, 4      " );            \
    asm( "addi   r3, r3, 4      " );            \
    asm( "stw    r5, %0         " : "=m" (c));  \
    asm( "stw    r4, %0         " : "=m" (d));  \
    asm( "stw    r3, %0         " : "=m" (s) :: \
    "r3", "r4", "r5", "r6", "r7", "r8", "r9" );

#else

#define MULADDC_INIT                            \
    asm( "lwz    %%r3, %0       " :: "m" (s));  \
    asm( "lwz    %%r4, %0       " :: "m" (d));  \
    asm( "lwz    %%r5, %0       " :: "m" (c));  \
    asm( "lwz    %%r6, %0       " :: "m" (b));  \
    asm( "addi   %r3, %r3, -4   " );            \
    asm( "addi   %r4, %r4, -4   " );            \
    asm( "addic  %r5, %r5,  0   " );

#define MULADDC_CORE                            \
    asm( "lwzu   %r7, 4(%r3)    " );            \
    asm( "mullw  %r8, %r7, %r6  " );            \
    asm( "mulhwu %r9, %r7, %r6  " );            \
    asm( "adde   %r8, %r8, %r5  " );            \
    asm( "lwz    %r7, 4(%r4)    " );            \
    asm( "addze  %r5, %r9       " );            \
    asm( "addc   %r8, %r8, %r7  " );            \
    asm( "stwu   %r8, 4(%r4)    " );

#define MULADDC_STOP                            \
    asm( "addze  %r5, %r5       " );            \
    asm( "addi   %r4, %r4, 4    " );            \
    asm( "addi   %r3, %r3, 4    " );            \
    asm( "stw    %%r5, %0       " : "=m" (c));  \
    asm( "stw    %%r4, %0       " : "=m" (d));  \
    asm( "stw    %%r3, %0       " : "=m" (s) :: \
    "r3", "r4", "r5", "r6", "r7", "r8", "r9" );

#endif

#endif /* PPC32 */
#endif /* PPC64 */

#if defined(__sparc__)

#define MULADDC_INIT                            \
    asm( "ld     %0, %%o0       " :: "m" (s));  \
    asm( "ld     %0, %%o1       " :: "m" (d));  \
    asm( "ld     %0, %%o2       " :: "m" (c));  \
    asm( "ld     %0, %%o3       " :: "m" (b));

#define MULADDC_CORE                            \
    asm( "ld    [%o0], %o4      " );            \
    asm( "inc      4,  %o0      " );            \
    asm( "ld    [%o1], %o5      " );            \
    asm( "umul   %o3,  %o4, %o4 " );            \
    asm( "addcc  %o4,  %o2, %o4 " );            \
    asm( "rd      %y,  %g1      " );            \
    asm( "addx   %g1,    0, %g1 " );            \
    asm( "addcc  %o4,  %o5, %o4 " );            \
    asm( "st     %o4, [%o1]     " );            \
    asm( "addx   %g1,    0, %o2 " );            \
    asm( "inc      4,  %o1      " );

#define MULADDC_STOP                            \
    asm( "st     %%o2, %0       " : "=m" (c));  \
    asm( "st     %%o1, %0       " : "=m" (d));  \
    asm( "st     %%o0, %0       " : "=m" (s) :: \
    "g1", "o0", "o1", "o2", "o3", "o4", "o5" );

#endif /* SPARCv8 */

#if defined(__microblaze__) || defined(microblaze)

#define MULADDC_INIT                            \
    asm( "lwi   r3,   %0        " :: "m" (s));  \
    asm( "lwi   r4,   %0        " :: "m" (d));  \
    asm( "lwi   r5,   %0        " :: "m" (c));  \
    asm( "lwi   r6,   %0        " :: "m" (b));  \
    asm( "andi  r7,   r6, 0xffff" );            \
    asm( "bsrli r6,   r6, 16    " );

#define MULADDC_CORE                            \
    asm( "lhui  r8,   r3,   0   " );            \
    asm( "addi  r3,   r3,   2   " );            \
    asm( "lhui  r9,   r3,   0   " );            \
    asm( "addi  r3,   r3,   2   " );            \
    asm( "mul   r10,  r9,  r6   " );            \
    asm( "mul   r11,  r8,  r7   " );            \
    asm( "mul   r12,  r9,  r7   " );            \
    asm( "mul   r13,  r8,  r6   " );            \
    asm( "bsrli  r8, r10,  16   " );            \
    asm( "bsrli  r9, r11,  16   " );            \
    asm( "add   r13, r13,  r8   " );            \
    asm( "add   r13, r13,  r9   " );            \
    asm( "bslli r10, r10,  16   " );            \
    asm( "bslli r11, r11,  16   " );            \
    asm( "add   r12, r12, r10   " );            \
    asm( "addc  r13, r13,  r0   " );            \
    asm( "add   r12, r12, r11   " );            \
    asm( "addc  r13, r13,  r0   " );            \
    asm( "lwi   r10,  r4,   0   " );            \
    asm( "add   r12, r12, r10   " );            \
    asm( "addc  r13, r13,  r0   " );            \
    asm( "add   r12, r12,  r5   " );            \
    asm( "addc   r5, r13,  r0   " );            \
    asm( "swi   r12,  r4,   0   " );            \
    asm( "addi   r4,  r4,   4   " );

#define MULADDC_STOP                            \
    asm( "swi   r5,   %0        " : "=m" (c));  \
    asm( "swi   r4,   %0        " : "=m" (d));  \
    asm( "swi   r3,   %0        " : "=m" (s) :: \
     "r3", "r4" , "r5" , "r6" , "r7" , "r8" ,   \
     "r9", "r10", "r11", "r12", "r13" );

#endif /* MicroBlaze */

#if defined(__tricore__)

#define MULADDC_INIT                            \
    asm( "ld.a   %%a2, %0       " :: "m" (s));  \
    asm( "ld.a   %%a3, %0       " :: "m" (d));  \
    asm( "ld.w   %%d4, %0       " :: "m" (c));  \
    asm( "ld.w   %%d1, %0       " :: "m" (b));  \
    asm( "xor    %d5, %d5       " );

#define MULADDC_CORE                            \
    asm( "ld.w   %d0,   [%a2+]      " );        \
    asm( "madd.u %e2, %e4, %d0, %d1 " );        \
    asm( "ld.w   %d0,   [%a3]       " );        \
    asm( "addx   %d2,    %d2,  %d0  " );        \
    asm( "addc   %d3,    %d3,    0  " );        \
    asm( "mov    %d4,    %d3        " );        \
    asm( "st.w  [%a3+],  %d2        " );

#define MULADDC_STOP                            \
    asm( "st.w   %0, %%d4       " : "=m" (c));  \
    asm( "st.a   %0, %%a3       " : "=m" (d));  \
    asm( "st.a   %0, %%a2       " : "=m" (s) :: \
    "d0", "d1", "e2", "d4", "a2", "a3" );

#endif /* TriCore */

#if defined(__arm__)

#define MULADDC_INIT                            \
    asm( "ldr    r0, %0         " :: "m" (s));  \
    asm( "ldr    r1, %0         " :: "m" (d));  \
    asm( "ldr    r2, %0         " :: "m" (c));  \
    asm( "ldr    r3, %0         " :: "m" (b));

#define MULADDC_CORE                            \
    asm( "ldr    r4, [r0], #4   " );            \
    asm( "mov    r5, #0         " );            \
    asm( "ldr    r6, [r1]       " );            \
    asm( "umlal  r2, r5, r3, r4 " );            \
    asm( "adds   r7, r6, r2     " );            \
    asm( "adc    r2, r5, #0     " );            \
    asm( "str    r7, [r1], #4   " );

#define MULADDC_STOP                            \
    asm( "str    r2, %0         " : "=m" (c));  \
    asm( "str    r1, %0         " : "=m" (d));  \
    asm( "str    r0, %0         " : "=m" (s) :: \
    "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7" );

#endif /* ARMv3 */

#if defined(__alpha__)

#define MULADDC_INIT                            \
    asm( "ldq    $1, %0         " :: "m" (s));  \
    asm( "ldq    $2, %0         " :: "m" (d));  \
    asm( "ldq    $3, %0         " :: "m" (c));  \
    asm( "ldq    $4, %0         " :: "m" (b));

#define MULADDC_CORE                            \
    asm( "ldq    $6,  0($1)     " );            \
    asm( "addq   $1,  8, $1     " );            \
    asm( "mulq   $6, $4, $7     " );            \
    asm( "umulh  $6, $4, $6     " );            \
    asm( "addq   $7, $3, $7     " );            \
    asm( "cmpult $7, $3, $3     " );            \
    asm( "ldq    $5,  0($2)     " );            \
    asm( "addq   $7, $5, $7     " );            \
    asm( "cmpult $7, $5, $5     " );            \
    asm( "stq    $7,  0($2)     " );            \
    asm( "addq   $2,  8, $2     " );            \
    asm( "addq   $6, $3, $3     " );            \
    asm( "addq   $5, $3, $3     " );

#define MULADDC_STOP                            \
    asm( "stq    $3, %0         " : "=m" (c));  \
    asm( "stq    $2, %0         " : "=m" (d));  \
    asm( "stq    $1, %0         " : "=m" (s) :: \
    "$1", "$2", "$3", "$4", "$5", "$6", "$7" );

#endif /* Alpha */

#if defined(__mips__)

#define MULADDC_INIT                            \
    asm( "lw     $10, %0        " :: "m" (s));  \
    asm( "lw     $11, %0        " :: "m" (d));  \
    asm( "lw     $12, %0        " :: "m" (c));  \
    asm( "lw     $13, %0        " :: "m" (b));

#define MULADDC_CORE                            \
    asm( "lw     $14, 0($10)    " );            \
    asm( "multu  $13, $14       " );            \
    asm( "addi   $10, $10, 4    " );            \
    asm( "mflo   $14            " );            \
    asm( "mfhi   $9             " );            \
    asm( "addu   $14, $12, $14  " );            \
    asm( "lw     $15, 0($11)    " );            \
    asm( "sltu   $12, $14, $12  " );            \
    asm( "addu   $15, $14, $15  " );            \
    asm( "sltu   $14, $15, $14  " );            \
    asm( "addu   $12, $12, $9   " );            \
    asm( "sw     $15, 0($11)    " );            \
    asm( "addu   $12, $12, $14  " );            \
    asm( "addi   $11, $11, 4    " );

#define MULADDC_STOP                            \
    asm( "sw     $12, %0        " : "=m" (c));  \
    asm( "sw     $11, %0        " : "=m" (d));  \
    asm( "sw     $10, %0        " : "=m" (s) :: \
    "$9", "$10", "$11", "$12", "$13", "$14", "$15" );

#endif /* MIPS */
#endif /* GNUC */

#if (defined(_MSC_VER) && defined(_M_IX86)) || defined(__WATCOMC__)

#define MULADDC_INIT                            \
    __asm   mov     esi, s                      \
    __asm   mov     edi, d                      \
    __asm   mov     ecx, c                      \
    __asm   mov     ebx, b

#define MULADDC_CORE                            \
    __asm   lodsd                               \
    __asm   mul     ebx                         \
    __asm   add     eax, ecx                    \
    __asm   adc     edx, 0                      \
    __asm   add     eax, [edi]                  \
    __asm   adc     edx, 0                      \
    __asm   mov     ecx, edx                    \
    __asm   stosd

#if ME_EST_SSE2

#define EMIT __asm _emit

#define MULADDC_HUIT                            \
    EMIT 0x0F  EMIT 0x6E  EMIT 0xC9             \
    EMIT 0x0F  EMIT 0x6E  EMIT 0xC3             \
    EMIT 0x0F  EMIT 0x6E  EMIT 0x1F             \
    EMIT 0x0F  EMIT 0xD4  EMIT 0xCB             \
    EMIT 0x0F  EMIT 0x6E  EMIT 0x16             \
    EMIT 0x0F  EMIT 0xF4  EMIT 0xD0             \
    EMIT 0x0F  EMIT 0x6E  EMIT 0x66  EMIT 0x04  \
    EMIT 0x0F  EMIT 0xF4  EMIT 0xE0             \
    EMIT 0x0F  EMIT 0x6E  EMIT 0x76  EMIT 0x08  \
    EMIT 0x0F  EMIT 0xF4  EMIT 0xF0             \
    EMIT 0x0F  EMIT 0x6E  EMIT 0x7E  EMIT 0x0C  \
    EMIT 0x0F  EMIT 0xF4  EMIT 0xF8             \
    EMIT 0x0F  EMIT 0xD4  EMIT 0xCA             \
    EMIT 0x0F  EMIT 0x6E  EMIT 0x5F  EMIT 0x04  \
    EMIT 0x0F  EMIT 0xD4  EMIT 0xDC             \
    EMIT 0x0F  EMIT 0x6E  EMIT 0x6F  EMIT 0x08  \
    EMIT 0x0F  EMIT 0xD4  EMIT 0xEE             \
    EMIT 0x0F  EMIT 0x6E  EMIT 0x67  EMIT 0x0C  \
    EMIT 0x0F  EMIT 0xD4  EMIT 0xFC             \
    EMIT 0x0F  EMIT 0x7E  EMIT 0x0F             \
    EMIT 0x0F  EMIT 0x6E  EMIT 0x56  EMIT 0x10  \
    EMIT 0x0F  EMIT 0xF4  EMIT 0xD0             \
    EMIT 0x0F  EMIT 0x73  EMIT 0xD1  EMIT 0x20  \
    EMIT 0x0F  EMIT 0x6E  EMIT 0x66  EMIT 0x14  \
    EMIT 0x0F  EMIT 0xF4  EMIT 0xE0             \
    EMIT 0x0F  EMIT 0xD4  EMIT 0xCB             \
    EMIT 0x0F  EMIT 0x6E  EMIT 0x76  EMIT 0x18  \
    EMIT 0x0F  EMIT 0xF4  EMIT 0xF0             \
    EMIT 0x0F  EMIT 0x7E  EMIT 0x4F  EMIT 0x04  \
    EMIT 0x0F  EMIT 0x73  EMIT 0xD1  EMIT 0x20  \
    EMIT 0x0F  EMIT 0x6E  EMIT 0x5E  EMIT 0x1C  \
    EMIT 0x0F  EMIT 0xF4  EMIT 0xD8             \
    EMIT 0x0F  EMIT 0xD4  EMIT 0xCD             \
    EMIT 0x0F  EMIT 0x6E  EMIT 0x6F  EMIT 0x10  \
    EMIT 0x0F  EMIT 0xD4  EMIT 0xD5             \
    EMIT 0x0F  EMIT 0x7E  EMIT 0x4F  EMIT 0x08  \
    EMIT 0x0F  EMIT 0x73  EMIT 0xD1  EMIT 0x20  \
    EMIT 0x0F  EMIT 0xD4  EMIT 0xCF             \
    EMIT 0x0F  EMIT 0x6E  EMIT 0x6F  EMIT 0x14  \
    EMIT 0x0F  EMIT 0xD4  EMIT 0xE5             \
    EMIT 0x0F  EMIT 0x7E  EMIT 0x4F  EMIT 0x0C  \
    EMIT 0x0F  EMIT 0x73  EMIT 0xD1  EMIT 0x20  \
    EMIT 0x0F  EMIT 0xD4  EMIT 0xCA             \
    EMIT 0x0F  EMIT 0x6E  EMIT 0x6F  EMIT 0x18  \
    EMIT 0x0F  EMIT 0xD4  EMIT 0xF5             \
    EMIT 0x0F  EMIT 0x7E  EMIT 0x4F  EMIT 0x10  \
    EMIT 0x0F  EMIT 0x73  EMIT 0xD1  EMIT 0x20  \
    EMIT 0x0F  EMIT 0xD4  EMIT 0xCC             \
    EMIT 0x0F  EMIT 0x6E  EMIT 0x6F  EMIT 0x1C  \
    EMIT 0x0F  EMIT 0xD4  EMIT 0xDD             \
    EMIT 0x0F  EMIT 0x7E  EMIT 0x4F  EMIT 0x14  \
    EMIT 0x0F  EMIT 0x73  EMIT 0xD1  EMIT 0x20  \
    EMIT 0x0F  EMIT 0xD4  EMIT 0xCE             \
    EMIT 0x0F  EMIT 0x7E  EMIT 0x4F  EMIT 0x18  \
    EMIT 0x0F  EMIT 0x73  EMIT 0xD1  EMIT 0x20  \
    EMIT 0x0F  EMIT 0xD4  EMIT 0xCB             \
    EMIT 0x0F  EMIT 0x7E  EMIT 0x4F  EMIT 0x1C  \
    EMIT 0x83  EMIT 0xC7  EMIT 0x20             \
    EMIT 0x83  EMIT 0xC6  EMIT 0x20             \
    EMIT 0x0F  EMIT 0x73  EMIT 0xD1  EMIT 0x20  \
    EMIT 0x0F  EMIT 0x7E  EMIT 0xC9

#define MULADDC_STOP                            \
    EMIT 0x0F  EMIT 0x77                        \
    __asm   mov     c, ecx                      \
    __asm   mov     d, edi                      \
    __asm   mov     s, esi                      \

#else

#define MULADDC_STOP                            \
    __asm   mov     c, ecx                      \
    __asm   mov     d, edi                      \
    __asm   mov     s, esi                      \

#endif /* SSE2 */
#endif /* MSVC */

#endif /* EST_HAVE_ASM */

#if !defined(MULADDC_CORE)
#if ME_USE_LONG_LONG

#define MULADDC_INIT                    \
{                                       \
    t_dbl r;                            \
    t_int r0, r1;

#define MULADDC_CORE                    \
    r   = *(s++) * (t_dbl) b;           \
    r0  = r;                            \
    r1  = r >> biL;                     \
    r0 += c;  r1 += (r0 <  c);          \
    r0 += *d; r1 += (r0 < *d);          \
    c = r1; *(d++) = r0;

#define MULADDC_STOP                    \
}

#else
#define MULADDC_INIT                    \
{                                       \
    t_int s0, s1, b0, b1;               \
    t_int r0, r1, rx, ry;               \
    b0 = ( b << biH ) >> biH;           \
    b1 = ( b >> biH );

#define MULADDC_CORE                    \
    s0 = ( *s << biH ) >> biH;          \
    s1 = ( *s >> biH ); s++;            \
    rx = s0 * b1; r0 = s0 * b0;         \
    ry = s1 * b0; r1 = s1 * b1;         \
    r1 += ( rx >> biH );                \
    r1 += ( ry >> biH );                \
    rx <<= biH; ry <<= biH;             \
    r0 += rx; r1 += (r0 < rx);          \
    r0 += ry; r1 += (r0 < ry);          \
    r0 +=  c; r1 += (r0 <  c);          \
    r0 += *d; r1 += (r0 < *d);          \
    c = r1; *(d++) = r0;

#define MULADDC_STOP                    \
}

#endif /* C (generic)  */
#endif /* C (longlong) */

#endif /* bn_mul.h */

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/camellia.h ************/


/*
    camellia.h -- Camellia Cipher

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#ifndef EST_CAMELLIA_H
#define EST_CAMELLIA_H

#define CAMELLIA_ENCRYPT     1
#define CAMELLIA_DECRYPT     0

/**
   @brief CAMELLIA context structure
 */
typedef struct {
    int     nr;         /**< number of rounds */
    ulong   rk[68];     /**< CAMELLIA round keys */
} camellia_context;

#ifdef __cplusplus
extern "C" {
#endif

    /**
       @brief          CAMELLIA key schedule (encryption)
       @param ctx      CAMELLIA context to be initialized
       @param key      encryption key
       @param keysize  must be 128, 192 or 256
     */
    PUBLIC void camellia_setkey_enc(camellia_context *ctx, uchar *key, int keysize);

    /**
       @brief          CAMELLIA key schedule (decryption)
       @param ctx      CAMELLIA context to be initialized
       @param key      decryption key
       @param keysize  must be 128, 192 or 256
     */
    PUBLIC void camellia_setkey_dec(camellia_context *ctx, uchar *key, int keysize);

    /**
       @brief          CAMELLIA-ECB block encryption/decryption
       @param ctx      CAMELLIA context
       @param mode     CAMELLIA_ENCRYPT or CAMELLIA_DECRYPT
       @param input    16-byte input block
       @param output   16-byte output block
     */
    PUBLIC void camellia_crypt_ecb(camellia_context *ctx, int mode, uchar input[16], uchar output[16]);

    /**
       @brief          CAMELLIA-CBC buffer encryption/decryption
       @param ctx      CAMELLIA context
       @param mode     CAMELLIA_ENCRYPT or CAMELLIA_DECRYPT
       @param length   length of the input data
       @param iv       initialization vector (updated after use)
       @param input    buffer holding the input data
       @param output   buffer holding the output data
     */
    PUBLIC void camellia_crypt_cbc(camellia_context *ctx, int mode, int length, uchar iv[16], uchar *input, uchar *output);

    /**
       @brief          CAMELLIA-CFB128 buffer encryption/decryption
       @param ctx      CAMELLIA context
       @param mode     CAMELLIA_ENCRYPT or CAMELLIA_DECRYPT
       @param length   length of the input data
       @param iv_off   offset in IV (updated after use)
       @param iv       initialization vector (updated after use)
       @param input    buffer holding the input data
       @param output   buffer holding the output data
     */
    PUBLIC void camellia_crypt_cfb128(camellia_context *ctx, int mode, int length, int *iv_off, uchar iv[16], 
            uchar *input, uchar *output);

#if UNUSED
    /**
       @brief          Checkup routine
      
       @return         0 if successful, or 1 if the test failed
     */
    PUBLIC int camellia_self_test(int verbose);
#endif

#ifdef __cplusplus
}
#endif
#endif

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/certs.h ************/


/*
    certs.h -- Test certificates

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#ifndef EST_CERTS_H
#define EST_CERTS_H

#ifdef __cplusplus
extern "C" {
#endif

#if UNUSED
    extern char test_ca_crt[];
    extern char test_ca_key[];
    extern char test_ca_pwd[];
    extern char test_srv_crt[];
    extern char test_srv_key[];
    extern char test_cli_crt[];
    extern char test_cli_key[];
    extern char xyssl_ca_crt[];
#endif

#ifdef __cplusplus
}
#endif
#endif              /* certs.h */



/********* Start of file src/debug.h ************/


/*
    debug.h -- Debugging support

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#ifndef SSL_DEBUG_H
#define SSL_DEBUG_H

#if FUTURE && ME_EST_LOGGING
#if ME_DEBUG
    #define LOG(l, ...) if (l <= logLevel) estLog(l, __VA_ARGS__) ; else
    #define RET(l, ...) if (l <= logLevel) estLog(l, __VA_ARGS__) ; else
#else
    #define LOG(l, ...) if (1) ; else
    #define RET(l, ...) if (1) ; else
#endif
    #define SSL_DEBUG_MSG(level, args)              debug_print_msg(ssl, level, debug_fmt args);
    #define SSL_DEBUG_RET(level, text, ret)         debug_print_ret(ssl, level, text, ret);
    #define SSL_DEBUG_BUF(level, text, buf, len)    debug_print_buf(ssl, level, text, buf, len);
    #define SSL_DEBUG_MPI(level, text, X)           debug_print_mpi(ssl, level, text, X);
    #define SSL_DEBUG_CRT(level, text, crt)         debug_print_crt(ssl, level, text, crt);
#endif

#if ME_EST_LOGGING
    #define SSL_DEBUG_MSG(level, args)              debug_print_msg(ssl, level, debug_fmt args);
    #define SSL_DEBUG_RET(level, text, ret)         debug_print_ret(ssl, level, text, ret);
    #define SSL_DEBUG_BUF(level, text, buf, len)    debug_print_buf(ssl, level, text, buf, len);
    #define SSL_DEBUG_MPI(level, text, X)           debug_print_mpi(ssl, level, text, X);
    #define SSL_DEBUG_CRT(level, text, crt)         debug_print_crt(ssl, level, text, crt);
#else
    #define SSL_DEBUG_MSG(level, args)            do {} while(0)
    #define SSL_DEBUG_RET(level, text, ret)       do {} while(0)
    #define SSL_DEBUG_BUF(level, text, buf, len)  do {} while(0)
    #define SSL_DEBUG_MPI(level, text, X)         do {} while(0)
    #define SSL_DEBUG_CRT(level, text, crt)       do {} while(0)
#endif

#ifdef __cplusplus
extern "C" {
#endif
    PUBLIC int snfmt(char *buf, ssize bufsize, cchar *fmt, ...);
    PUBLIC char *debug_fmt(const char *format, ...);
    PUBLIC void debug_print_msg(ssl_context *ssl, int level, char *text);
    PUBLIC void debug_print_ret(ssl_context *ssl, int level, char *text, int ret);
    PUBLIC void debug_print_buf(ssl_context *ssl, int level, char *text, uchar *buf, int len);
    PUBLIC void debug_print_mpi(ssl_context *ssl, int level, char *text, mpi * X);
    PUBLIC void debug_print_crt(ssl_context *ssl, int level, char *text, x509_cert * crt);

#ifdef __cplusplus
}
#endif
#endif  /* debug.h */

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/des.h ************/


/*
    des.h -- Des Cipher

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#ifndef EST_DES_H
#define EST_DES_H

#define DES_ENCRYPT     1
#define DES_DECRYPT     0

/**
   @brief DES context structure
 */
typedef struct {
    int     mode;       /**< encrypt/decrypt */
    ulong   sk[32];     /**< DES subkeys */
} des_context;


/**
   @brief Triple-DES context structure
 */
typedef struct {
    int     mode;       /**< encrypt/decrypt */
    ulong   sk[96];     /**< 3DES subkeys */
} des3_context;

#ifdef __cplusplus
extern "C" {
#endif

    /**
       @brief          DES key schedule (56-bit, encryption)
       @param ctx      DES context to be initialized
       @param key      8-byte secret key
     */
    PUBLIC void des_setkey_enc(des_context *ctx, uchar key[8]);

    /**
       @brief          DES key schedule (56-bit, decryption)
       @param ctx      DES context to be initialized
       @param key      8-byte secret key
     */
    PUBLIC void des_setkey_dec(des_context *ctx, uchar key[8]);

    /**
       @brief          Triple-DES key schedule (112-bit, encryption)
       @param ctx      3DES context to be initialized
       @param key      16-byte secret key
     */
    PUBLIC void des3_set2key_enc(des3_context *ctx, uchar key[16]);

    /**
       @brief          Triple-DES key schedule (112-bit, decryption)
       @param ctx      3DES context to be initialized
       @param key      16-byte secret key
     */
    PUBLIC void des3_set2key_dec(des3_context *ctx, uchar key[16]);

    /**
       @brief          Triple-DES key schedule (168-bit, encryption)
       @param ctx      3DES context to be initialized
       @param key      24-byte secret key
     */
    PUBLIC void des3_set3key_enc(des3_context *ctx, uchar key[24]);

    /**
       @brief          Triple-DES key schedule (168-bit, decryption)
       @param ctx      3DES context to be initialized
       @param key      24-byte secret key
     */
    PUBLIC void des3_set3key_dec(des3_context *ctx, uchar key[24]);

    /**
       @brief          DES-ECB block encryption/decryption
       @param ctx      DES context
       @param input    64-bit input block
       @param output   64-bit output block
     */
    PUBLIC void des_crypt_ecb(des_context *ctx, uchar input[8], uchar output[8]);

    /**
       @brief          DES-CBC buffer encryption/decryption
       @param ctx      DES context
       @param mode     DES_ENCRYPT or DES_DECRYPT
       @param length   length of the input data
       @param iv       initialization vector (updated after use)
       @param input    buffer holding the input data
       @param output   buffer holding the output data
     */
    PUBLIC void des_crypt_cbc(des_context *ctx, int mode, int length, uchar iv[8], uchar *input, uchar *output);

    /**
       @brief          3DES-ECB block encryption/decryption
       @param ctx      3DES context
       @param input    64-bit input block
       @param output   64-bit output block
     */
    PUBLIC void des3_crypt_ecb(des3_context *ctx, uchar input[8], uchar output[8]);

    /**
       @brief          3DES-CBC buffer encryption/decryption
       @param ctx      3DES context
       @param mode     DES_ENCRYPT or DES_DECRYPT
       @param length   length of the input data
       @param iv       initialization vector (updated after use)
       @param input    buffer holding the input data
       @param output   buffer holding the output data
     */
    PUBLIC void des3_crypt_cbc(des3_context *ctx, int mode, int length, uchar iv[8], uchar *input, uchar *output);

#if UNUSED
    /*
       @brief          Checkup routine
       @return         0 if successful, or 1 if the test failed
     */
    PUBLIC int des_self_test(int verbose);
#endif

#ifdef __cplusplus
}
#endif
#endif              /* des.h */

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/havege.h ************/


/*
    havege.h -- Havege Random Support

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#ifndef EST_HAVEGE_H
#define EST_HAVEGE_H

#define COLLECT_SIZE 1024

/**
   @brief          HAVEGE state structure
 */
typedef struct {
    int PT1, PT2, offset[2];
    int pool[COLLECT_SIZE];
    int WALK[8192];
} havege_state;

#ifdef __cplusplus
extern "C" {
#endif

    /**
       @brief          HAVEGE initialization
       @param hs       HAVEGE state to be initialized
     */
    PUBLIC void havege_init(havege_state *hs);

    /**
       @brief          HAVEGE rand function
       @param rng_st   points to an HAVEGE state
       @return         A random int
     */
    PUBLIC int havege_rand(void *p_rng);

#ifdef __cplusplus
}
#endif
#endif 

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/md2.h ************/


/*
    md2.h -- RFC 1115/1319 compliant MD2 implementation

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#ifndef EST_MD2_H
#define EST_MD2_H

/**
   @brief MD2 context structure
 */
typedef struct {
    uchar   cksum[16];      /**< checksum of the data block */
    uchar   state[48];      /**< intermediate digest state  */
    uchar   buffer[16];     /**< data block being processed */

    uchar   ipad[64];       /**< HMAC: inner padding        */
    uchar   opad[64];       /**< HMAC: outer padding        */
    int     left;           /**< amount of data in buffer   */
} md2_context;

#ifdef __cplusplus
extern "C" {
#endif

    /**
       @brief          MD2 context setup
       @param ctx      context to be initialized
     */
    PUBLIC void md2_starts(md2_context * ctx);

    /**
       @brief          MD2 process buffer
       @param ctx      MD2 context
       @param input    buffer holding the  data
       @param ilen     length of the input data
     */
    PUBLIC void md2_update(md2_context * ctx, uchar *input, int ilen);

    /**
       @brief          MD2 final digest
       @param ctx      MD2 context
     * @param output   MD2 checksum result
     */
    PUBLIC void md2_finish(md2_context * ctx, uchar output[16]);

    /**
       @brief          Output = MD2( input buffer )
       @param input    buffer holding the  data
       @param ilen     length of the input data
       @param output   MD2 checksum result
     */
    PUBLIC void md2(uchar *input, int ilen, uchar output[16]);

    /**
       @brief          Output = MD2( file contents )
       @param path     input file name
       @param output   MD2 checksum result
       @return         0 if successful, 1 if fopen failed, or 2 if fread failed
     */
    PUBLIC int md2_file(char *path, uchar output[16]);

    /**
       @brief          MD2 HMAC context setup
       @param ctx      HMAC context to be initialized
       @param key      HMAC secret key
       @param keylen   length of the HMAC key
     */
    PUBLIC void md2_hmac_starts(md2_context * ctx, uchar *key, int keylen);

    /**
       @brief          MD2 HMAC process buffer
       @param ctx      HMAC context
       @param input    buffer holding the  data
       @param ilen     length of the input data
     */
    PUBLIC void md2_hmac_update(md2_context * ctx, uchar *input, int ilen);

    /**
       @brief          MD2 HMAC final digest
       @param ctx      HMAC context
       @param output   MD2 HMAC checksum result
     */
    PUBLIC void md2_hmac_finish(md2_context * ctx, uchar output[16]);

    /**
       @brief          Output = HMAC-MD2( hmac key, input buffer )
       @param key      HMAC secret key
       @param keylen   length of the HMAC key
       @param input    buffer holding the  data
       @param ilen     length of the input data
       @param output   HMAC-MD2 result
     */
    PUBLIC void md2_hmac(uchar *key, int keylen, uchar *input, int ilen, uchar output[16]);

    /**
       @brief          Checkup routine
       @return         0 if successful, or 1 if the test failed
     */
    PUBLIC int md2_self_test(int verbose);

#ifdef __cplusplus
}
#endif
#endif              /* md2.h */

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/md4.h ************/


/*
    md4.h -- RFC 1186/1320 compliant MD4 implementation

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#ifndef EST_MD4_H
#define EST_MD4_H

/**
    @brief          MD4 context structure
 */
typedef struct {
    ulong   total[2];       /**< number of bytes processed  */
    ulong   state[4];       /**< intermediate digest state  */
    uchar   buffer[64];     /**< data block being processed */
    uchar   ipad[64];       /**< HMAC: inner padding        */
    uchar   opad[64];       /**< HMAC: outer padding        */
} md4_context;

#ifdef __cplusplus
extern "C" {
#endif

    /**
       @brief          MD4 context setup
       @param ctx      context to be initialized
     */
    PUBLIC void md4_starts(md4_context * ctx);

    /**
       @brief          MD4 process buffer
       @param ctx      MD4 context
       @param input    buffer holding the  data
       @param ilen     length of the input data
     */
    PUBLIC void md4_update(md4_context * ctx, uchar *input, int ilen);

    /**
       @brief          MD4 final digest
       @param ctx      MD4 context
       @param output   MD4 checksum result
     */
    PUBLIC void md4_finish(md4_context * ctx, uchar output[16]);

    /**
       @brief          Output = MD4( input buffer )
       @param input    buffer holding the  data
       @param ilen     length of the input data
       @param output   MD4 checksum result
     */
    PUBLIC void md4(uchar *input, int ilen, uchar output[16]);

    /**
       @brief          Output = MD4( file contents )
       @param path     input file name
       @param output   MD4 checksum result
       @return         0 if successful, 1 if fopen failed, or 2 if fread failed
     */
    PUBLIC int md4_file(char *path, uchar output[16]);

    /**
       @brief          MD4 HMAC context setup
       @param ctx      HMAC context to be initialized
       @param key      HMAC secret key
       @param keylen   length of the HMAC key
     */
    PUBLIC void md4_hmac_starts(md4_context * ctx, uchar *key, int keylen);

    /**
       @brief          MD4 HMAC process buffer
       @param ctx      HMAC context
       @param input    buffer holding the  data
       @param ilen     length of the input data
     */
    PUBLIC void md4_hmac_update(md4_context * ctx, uchar *input, int ilen);

    /**
       @brief          MD4 HMAC final digest
       @param ctx      HMAC context
       @param output   MD4 HMAC checksum result
     */
    PUBLIC void md4_hmac_finish(md4_context * ctx, uchar output[16]);

    /**
       @brief          Output = HMAC-MD4( hmac key, input buffer )
       @param key      HMAC secret key
       @param keylen   length of the HMAC key
       @param input    buffer holding the  data
       @param ilen     length of the input data
       @param output   HMAC-MD4 result
     */
    PUBLIC void md4_hmac(uchar *key, int keylen, uchar *input, int ilen, uchar output[16]);

    /**
       @brief          Checkup routine
       @return         0 if successful, or 1 if the test failed
     */
    PUBLIC int md4_self_test(int verbose);

#ifdef __cplusplus
}
#endif
#endif 

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/padlock.h ************/


/*
    padlock.h -- Via padlock support

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#ifndef EST_PADLOCK_H
#define EST_PADLOCK_H

#if (defined(__GNUC__) && defined(__i386__))
#ifndef EST_HAVE_X86
#define EST_HAVE_X86
#endif

#define PADLOCK_RNG 0x000C
#define PADLOCK_ACE 0x00C0
#define PADLOCK_PHE 0x0C00
#define PADLOCK_PMM 0x3000

#define PADLOCK_ALIGN16(x) (ulong *) (16 + ((long) x & ~15))

#ifdef __cplusplus
extern "C" {
#endif

    /**
       @brief          PadLock detection routine
       @return         1 if CPU has support for the feature, 0 otherwise
     */
    PUBLIC int padlock_supports(int feature);

    /**
       @brief          PadLock AES-ECB block en(de)cryption
       @param ctx      AES context
       @param mode     AES_ENCRYPT or AES_DECRYPT
       @param input    16-byte input block
       @param output   16-byte output block
       @return         0 if success, 1 if operation failed
     */
    PUBLIC int padlock_xcryptecb(aes_context *ctx, int mode, uchar input[16], uchar output[16]);

    /**
       @brief          PadLock AES-CBC buffer en(de)cryption
       @param ctx      AES context
       @param mode     AES_ENCRYPT or AES_DECRYPT
       @param length   length of the input data
       @param iv       initialization vector (updated after use)
       @param input    buffer holding the input data
       @param output   buffer holding the output data
       @return         0 if success, 1 if operation failed
     */
    PUBLIC int padlock_xcryptcbc(aes_context *ctx, int mode, int length, uchar iv[16], uchar *input, uchar *output);

#ifdef __cplusplus
}
#endif
#endif              /* HAVE_X86  */
#endif              /* padlock.h */

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/sha2.h ************/


/*
    sha2.h -- FIPS-180-2 compliant SHA-256 implementation


    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#ifndef EST_SHA2_H
#define EST_SHA2_H

/**
   @brief SHA-256 context structure
 */
typedef struct {
    ulong total[2];     /**< number of bytes processed  */
    ulong state[8];     /**< intermediate digest state  */
    uchar buffer[64];   /**< data block being processed */
    uchar ipad[64];     /**< HMAC: inner padding        */
    uchar opad[64];     /**< HMAC: outer padding        */
    int is224;          /**< 0 => SHA-256, else SHA-224 */
} sha2_context;

#ifdef __cplusplus
extern "C" {
#endif

    /**
       @brief          SHA-256 context setup
       @param ctx      context to be initialized
       @param is224    0 = use SHA256, 1 = use SHA224
     */
    PUBLIC void sha2_starts(sha2_context *ctx, int is224);

    /**
       @brief          SHA-256 process buffer
       @param ctx      SHA-256 context
       @param input    buffer holding the  data
       @param ilen     length of the input data
     */
    PUBLIC void sha2_update(sha2_context *ctx, uchar *input, int ilen);

    /**
       @brief          SHA-256 final digest
       @param ctx      SHA-256 context
       @param output   SHA-224/256 checksum result
     */
    PUBLIC void sha2_finish(sha2_context *ctx, uchar output[32]);

    /**
       @brief          Output = SHA-256(input buffer)
       @param input    buffer holding the  data
       @param ilen     length of the input data
       @param output   SHA-224/256 checksum result
       @param is224    0 = use SHA256, 1 = use SHA224
     */
    PUBLIC void sha2(uchar *input, int ilen, uchar output[32], int is224);

    /**
       @brief          Output = SHA-256(file contents)
       @param path     input file name
       @param output   SHA-224/256 checksum result
       @param is224    0 = use SHA256, 1 = use SHA224
       @return         0 if successful, 1 if fopen failed, or 2 if fread failed
     */
    PUBLIC int sha2_file(char *path, uchar output[32], int is224);

    /**
       @brief          SHA-256 HMAC context setup
       @param ctx      HMAC context to be initialized
       @param key      HMAC secret key
       @param keylen   length of the HMAC key
       @param is224    0 = use SHA256, 1 = use SHA224
     */
    PUBLIC void sha2_hmac_starts(sha2_context *ctx, uchar *key, int keylen, int is224);

    /**
       @brief          SHA-256 HMAC process buffer
       @param ctx      HMAC context
       @param input    buffer holding the  data
       @param ilen     length of the input data
     */
    PUBLIC void sha2_hmac_update(sha2_context *ctx, uchar *input, int ilen);

    /**
       @brief          SHA-256 HMAC final digest
       @param ctx      HMAC context
       @param output   SHA-224/256 HMAC checksum result
     */
    PUBLIC void sha2_hmac_finish(sha2_context *ctx, uchar output[32]);

    /**
       @brief          Output = HMAC-SHA-256(hmac key, input buffer)
       @param key      HMAC secret key
       @param keylen   length of the HMAC key
       @param input    buffer holding the  data
       @param ilen     length of the input data
       @param output   HMAC-SHA-224/256 result
       @param is224    0 = use SHA256, 1 = use SHA224
     */
    PUBLIC void sha2_hmac(uchar *key, int keylen, uchar *input, int ilen, uchar output[32], int is224);

    /**
       @brief          Checkup routine
       @return         0 if successful, or 1 if the test failed
     */
    PUBLIC int sha2_self_test(int verbose);

#ifdef __cplusplus
}
#endif
#endif              /* sha2.h */

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/sha4.h ************/


/*
    sha4.h -- FIPS-180-2 compliant SHA-384/512 implementation

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#ifndef EST_SHA4_H
#define EST_SHA4_H

#if defined(_MSC_VER) || defined(__WATCOMC__)
    #define UL64(x) x##ui64
    #define int64 __int64
#else
    #define UL64(x) x##ULL
    #define int64 long long
#endif

/**
 * @brief          SHA-512 context structure
 */
typedef struct {
    uint64 total[2];    /**< number of bytes processed  */
    uint64 state[8];    /**< intermediate digest state  */
    uchar buffer[128];  /**< data block being processed */
    uchar ipad[128];    /**< HMAC: inner padding        */
    uchar opad[128];    /**< HMAC: outer padding        */
    int is384;          /**< 0 => SHA-512, else SHA-384 */
} sha4_context;

#ifdef __cplusplus
extern "C" {
#endif

    /**
       @brief          SHA-512 context setup
       @param ctx      context to be initialized
       @param is384    0 = use SHA512, 1 = use SHA384
     */
    PUBLIC void sha4_starts(sha4_context * ctx, int is384);

    /**
       @brief          SHA-512 process buffer
       @param ctx      SHA-512 context
       @param input    buffer holding the  data
       @param ilen     length of the input data
     */
    PUBLIC void sha4_update(sha4_context * ctx, uchar *input, int ilen);

    /**
       @brief          SHA-512 final digest
       @param ctx      SHA-512 context
       @param output   SHA-384/512 checksum result
     */
    PUBLIC void sha4_finish(sha4_context * ctx, uchar output[64]);

    /**
       @brief          Output = SHA-512(input buffer)
       @param input    buffer holding the  data
       @param ilen     length of the input data
       @param output   SHA-384/512 checksum result
       @param is384    0 = use SHA512, 1 = use SHA384
     */
    PUBLIC void sha4(uchar *input, int ilen, uchar output[64], int is384);

    /**
       @brief          Output = SHA-512(file contents)
       @param path     input file name
       @param output   SHA-384/512 checksum result
       @param is384    0 = use SHA512, 1 = use SHA384
       @return         0 if successful, 1 if fopen failed, or 2 if fread failed
     */
    PUBLIC int sha4_file(char *path, uchar output[64], int is384);

    /**
       @brief          SHA-512 HMAC context setup
       @param ctx      HMAC context to be initialized
       @param is384    0 = use SHA512, 1 = use SHA384
       @param key      HMAC secret key
       @param keylen   length of the HMAC key
     */
    PUBLIC void sha4_hmac_starts(sha4_context * ctx, uchar *key, int keylen, int is384);

    /**
       @brief          SHA-512 HMAC process buffer
       @param ctx      HMAC context
       @param input    buffer holding the  data
       @param ilen     length of the input data
     */
    PUBLIC void sha4_hmac_update(sha4_context * ctx, uchar *input, int ilen);

    /**
       @brief          SHA-512 HMAC final digest
       @param ctx      HMAC context
       @param output   SHA-384/512 HMAC checksum result
     */
    PUBLIC void sha4_hmac_finish(sha4_context * ctx, uchar output[64]);

    /**
       @brief          Output = HMAC-SHA-512(hmac key, input buffer)
       @param key      HMAC secret key
       @param keylen   length of the HMAC key
       @param input    buffer holding the  data
       @param ilen     length of the input data
       @param output   HMAC-SHA-384/512 result
       @param is384    0 = use SHA512, 1 = use SHA384
     */
    PUBLIC void sha4_hmac(uchar *key, int keylen, uchar *input, int ilen, uchar output[64], int is384);

    /**
       @brief          Checkup routine
       @return         0 if successful, or 1 if the test failed
     */
    PUBLIC int sha4_self_test(int verbose);

#ifdef __cplusplus
}
#endif
#endif              /* sha4.h */

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/timing.h ************/


/*
    timing.h -- Portable interface to the CPU cycle counter

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#ifndef EST_TIMING_H
#define EST_TIMING_H

/**
   @brief          timer structure
 */
struct hr_time {
    uchar opaque[32];
};

#ifdef __cplusplus
extern "C" {
#endif

    extern int alarmed;

    /**
       @brief          Return the CPU cycle counter value
     */
    PUBLIC ulong hardclock(void);

    /**
       @brief          Return the elapsed time in milliseconds
       @param val      points to a timer structure
       @param reset    if set to 1, the timer is restarted
     */
    PUBLIC ulong get_timer(struct hr_time *val, int reset);

#if UNUSED
    /**
       @brief          Setup an alarm clock
       @param seconds  delay before the "alarmed" flag is set
     */
    PUBLIC void set_alarm(int seconds);
#endif

    /**
       @brief          Sleep for a certain amount of time
     */
    PUBLIC void m_sleep(int milliseconds);

#ifdef __cplusplus
}
#endif
#endif              /* timing.h */

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */



/********* Start of file src/xtea.h ************/


/*
    xtea.h -- An 32-bit implementation of the XTEA algorithm


    Copyright (c) All Rights Reserved. See details at the end of the file.
 */
#ifndef EST_XTEA_H
#define EST_XTEA_H

#define XTEA_ENCRYPT     1
#define XTEA_DECRYPT     0

/**
    @brief XTEA context structure
 */
typedef struct {
    ulong k[4];     /**< key */
} xtea_context;

#ifdef __cplusplus
extern "C" {
#endif

    /**
       @brief          XTEA key schedule
       @param ctx      XTEA context to be initialized
       @param key      the secret key
     */
    PUBLIC void xtea_setup(xtea_context * ctx, uchar key[16]);

    /**
       @brief          XTEA cipher function
       @param ctx      XTEA context
       @param mode     XTEA_ENCRYPT or XTEA_DECRYPT
       @param input    8-byte input block
       @param output   8-byte output block
     */
    PUBLIC void xtea_crypt(xtea_context * ctx, int mode, uchar input[8], uchar output[8]);

    /*
       @brief          Checkup routine
       @return         0 if successful, or 1 if the test failed
     */
    PUBLIC int xtea_self_test(int verbose);

#ifdef __cplusplus
}
#endif
#endif              /* xtea.h */

/*
    @copy   default

    Copyright (c) Embedthis Software. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis Open Source license or you may acquire a 
    commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */

#endif /* ME_COM_EST */
