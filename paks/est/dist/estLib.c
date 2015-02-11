/*
 * Embethis EST Library Source
 */

#include "est.h"

#if ME_COM_EST



/********* Start of file src/aes.c ************/


/*
    aes.c -- FIPS-197 compliant AES implementation

    The AES block cipher was designed by Vincent Rijmen and Joan Daemen.

    http://csrc.nist.gov/encryption/aes/rijndael/Rijndael.pdf
    http://csrc.nist.gov/publications/fips/fips197/fips-197.pdf

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_AES

/*
    32-bit integer manipulation macros (little endian)
 */
#ifndef GET_ULONG_LE
#define GET_ULONG_LE(n,b,i)                     \
{                                               \
    (n) = ( (ulong) (b)[(i)    ]       )        \
        | ( (ulong) (b)[(i) + 1] <<  8 )        \
        | ( (ulong) (b)[(i) + 2] << 16 )        \
        | ( (ulong) (b)[(i) + 3] << 24 );       \
}
#endif

#ifndef PUT_ULONG_LE
#define PUT_ULONG_LE(n,b,i)                     \
{                                               \
    (b)[(i)    ] = (uchar) ( (n)       );       \
    (b)[(i) + 1] = (uchar) ( (n) >>  8 );       \
    (b)[(i) + 2] = (uchar) ( (n) >> 16 );       \
    (b)[(i) + 3] = (uchar) ( (n) >> 24 );       \
}
#endif

#define FSb AESFSb

#if ME_EST_ROM_TABLES
/*
   Forward S-box
 */
static const uchar FSb[256] = {
    0x63, 0x7C, 0x77, 0x7B, 0xF2, 0x6B, 0x6F, 0xC5,
    0x30, 0x01, 0x67, 0x2B, 0xFE, 0xD7, 0xAB, 0x76,
    0xCA, 0x82, 0xC9, 0x7D, 0xFA, 0x59, 0x47, 0xF0,
    0xAD, 0xD4, 0xA2, 0xAF, 0x9C, 0xA4, 0x72, 0xC0,
    0xB7, 0xFD, 0x93, 0x26, 0x36, 0x3F, 0xF7, 0xCC,
    0x34, 0xA5, 0xE5, 0xF1, 0x71, 0xD8, 0x31, 0x15,
    0x04, 0xC7, 0x23, 0xC3, 0x18, 0x96, 0x05, 0x9A,
    0x07, 0x12, 0x80, 0xE2, 0xEB, 0x27, 0xB2, 0x75,
    0x09, 0x83, 0x2C, 0x1A, 0x1B, 0x6E, 0x5A, 0xA0,
    0x52, 0x3B, 0xD6, 0xB3, 0x29, 0xE3, 0x2F, 0x84,
    0x53, 0xD1, 0x00, 0xED, 0x20, 0xFC, 0xB1, 0x5B,
    0x6A, 0xCB, 0xBE, 0x39, 0x4A, 0x4C, 0x58, 0xCF,
    0xD0, 0xEF, 0xAA, 0xFB, 0x43, 0x4D, 0x33, 0x85,
    0x45, 0xF9, 0x02, 0x7F, 0x50, 0x3C, 0x9F, 0xA8,
    0x51, 0xA3, 0x40, 0x8F, 0x92, 0x9D, 0x38, 0xF5,
    0xBC, 0xB6, 0xDA, 0x21, 0x10, 0xFF, 0xF3, 0xD2,
    0xCD, 0x0C, 0x13, 0xEC, 0x5F, 0x97, 0x44, 0x17,
    0xC4, 0xA7, 0x7E, 0x3D, 0x64, 0x5D, 0x19, 0x73,
    0x60, 0x81, 0x4F, 0xDC, 0x22, 0x2A, 0x90, 0x88,
    0x46, 0xEE, 0xB8, 0x14, 0xDE, 0x5E, 0x0B, 0xDB,
    0xE0, 0x32, 0x3A, 0x0A, 0x49, 0x06, 0x24, 0x5C,
    0xC2, 0xD3, 0xAC, 0x62, 0x91, 0x95, 0xE4, 0x79,
    0xE7, 0xC8, 0x37, 0x6D, 0x8D, 0xD5, 0x4E, 0xA9,
    0x6C, 0x56, 0xF4, 0xEA, 0x65, 0x7A, 0xAE, 0x08,
    0xBA, 0x78, 0x25, 0x2E, 0x1C, 0xA6, 0xB4, 0xC6,
    0xE8, 0xDD, 0x74, 0x1F, 0x4B, 0xBD, 0x8B, 0x8A,
    0x70, 0x3E, 0xB5, 0x66, 0x48, 0x03, 0xF6, 0x0E,
    0x61, 0x35, 0x57, 0xB9, 0x86, 0xC1, 0x1D, 0x9E,
    0xE1, 0xF8, 0x98, 0x11, 0x69, 0xD9, 0x8E, 0x94,
    0x9B, 0x1E, 0x87, 0xE9, 0xCE, 0x55, 0x28, 0xDF,
    0x8C, 0xA1, 0x89, 0x0D, 0xBF, 0xE6, 0x42, 0x68,
    0x41, 0x99, 0x2D, 0x0F, 0xB0, 0x54, 0xBB, 0x16
};


/*
   Forward tables
 */
#define FT \
    V(A5,63,63,C6), V(84,7C,7C,F8), V(99,77,77,EE), V(8D,7B,7B,F6), \
    V(0D,F2,F2,FF), V(BD,6B,6B,D6), V(B1,6F,6F,DE), V(54,C5,C5,91), \
    V(50,30,30,60), V(03,01,01,02), V(A9,67,67,CE), V(7D,2B,2B,56), \
    V(19,FE,FE,E7), V(62,D7,D7,B5), V(E6,AB,AB,4D), V(9A,76,76,EC), \
    V(45,CA,CA,8F), V(9D,82,82,1F), V(40,C9,C9,89), V(87,7D,7D,FA), \
    V(15,FA,FA,EF), V(EB,59,59,B2), V(C9,47,47,8E), V(0B,F0,F0,FB), \
    V(EC,AD,AD,41), V(67,D4,D4,B3), V(FD,A2,A2,5F), V(EA,AF,AF,45), \
    V(BF,9C,9C,23), V(F7,A4,A4,53), V(96,72,72,E4), V(5B,C0,C0,9B), \
    V(C2,B7,B7,75), V(1C,FD,FD,E1), V(AE,93,93,3D), V(6A,26,26,4C), \
    V(5A,36,36,6C), V(41,3F,3F,7E), V(02,F7,F7,F5), V(4F,CC,CC,83), \
    V(5C,34,34,68), V(F4,A5,A5,51), V(34,E5,E5,D1), V(08,F1,F1,F9), \
    V(93,71,71,E2), V(73,D8,D8,AB), V(53,31,31,62), V(3F,15,15,2A), \
    V(0C,04,04,08), V(52,C7,C7,95), V(65,23,23,46), V(5E,C3,C3,9D), \
    V(28,18,18,30), V(A1,96,96,37), V(0F,05,05,0A), V(B5,9A,9A,2F), \
    V(09,07,07,0E), V(36,12,12,24), V(9B,80,80,1B), V(3D,E2,E2,DF), \
    V(26,EB,EB,CD), V(69,27,27,4E), V(CD,B2,B2,7F), V(9F,75,75,EA), \
    V(1B,09,09,12), V(9E,83,83,1D), V(74,2C,2C,58), V(2E,1A,1A,34), \
    V(2D,1B,1B,36), V(B2,6E,6E,DC), V(EE,5A,5A,B4), V(FB,A0,A0,5B), \
    V(F6,52,52,A4), V(4D,3B,3B,76), V(61,D6,D6,B7), V(CE,B3,B3,7D), \
    V(7B,29,29,52), V(3E,E3,E3,DD), V(71,2F,2F,5E), V(97,84,84,13), \
    V(F5,53,53,A6), V(68,D1,D1,B9), V(00,00,00,00), V(2C,ED,ED,C1), \
    V(60,20,20,40), V(1F,FC,FC,E3), V(C8,B1,B1,79), V(ED,5B,5B,B6), \
    V(BE,6A,6A,D4), V(46,CB,CB,8D), V(D9,BE,BE,67), V(4B,39,39,72), \
    V(DE,4A,4A,94), V(D4,4C,4C,98), V(E8,58,58,B0), V(4A,CF,CF,85), \
    V(6B,D0,D0,BB), V(2A,EF,EF,C5), V(E5,AA,AA,4F), V(16,FB,FB,ED), \
    V(C5,43,43,86), V(D7,4D,4D,9A), V(55,33,33,66), V(94,85,85,11), \
    V(CF,45,45,8A), V(10,F9,F9,E9), V(06,02,02,04), V(81,7F,7F,FE), \
    V(F0,50,50,A0), V(44,3C,3C,78), V(BA,9F,9F,25), V(E3,A8,A8,4B), \
    V(F3,51,51,A2), V(FE,A3,A3,5D), V(C0,40,40,80), V(8A,8F,8F,05), \
    V(AD,92,92,3F), V(BC,9D,9D,21), V(48,38,38,70), V(04,F5,F5,F1), \
    V(DF,BC,BC,63), V(C1,B6,B6,77), V(75,DA,DA,AF), V(63,21,21,42), \
    V(30,10,10,20), V(1A,FF,FF,E5), V(0E,F3,F3,FD), V(6D,D2,D2,BF), \
    V(4C,CD,CD,81), V(14,0C,0C,18), V(35,13,13,26), V(2F,EC,EC,C3), \
    V(E1,5F,5F,BE), V(A2,97,97,35), V(CC,44,44,88), V(39,17,17,2E), \
    V(57,C4,C4,93), V(F2,A7,A7,55), V(82,7E,7E,FC), V(47,3D,3D,7A), \
    V(AC,64,64,C8), V(E7,5D,5D,BA), V(2B,19,19,32), V(95,73,73,E6), \
    V(A0,60,60,C0), V(98,81,81,19), V(D1,4F,4F,9E), V(7F,DC,DC,A3), \
    V(66,22,22,44), V(7E,2A,2A,54), V(AB,90,90,3B), V(83,88,88,0B), \
    V(CA,46,46,8C), V(29,EE,EE,C7), V(D3,B8,B8,6B), V(3C,14,14,28), \
    V(79,DE,DE,A7), V(E2,5E,5E,BC), V(1D,0B,0B,16), V(76,DB,DB,AD), \
    V(3B,E0,E0,DB), V(56,32,32,64), V(4E,3A,3A,74), V(1E,0A,0A,14), \
    V(DB,49,49,92), V(0A,06,06,0C), V(6C,24,24,48), V(E4,5C,5C,B8), \
    V(5D,C2,C2,9F), V(6E,D3,D3,BD), V(EF,AC,AC,43), V(A6,62,62,C4), \
    V(A8,91,91,39), V(A4,95,95,31), V(37,E4,E4,D3), V(8B,79,79,F2), \
    V(32,E7,E7,D5), V(43,C8,C8,8B), V(59,37,37,6E), V(B7,6D,6D,DA), \
    V(8C,8D,8D,01), V(64,D5,D5,B1), V(D2,4E,4E,9C), V(E0,A9,A9,49), \
    V(B4,6C,6C,D8), V(FA,56,56,AC), V(07,F4,F4,F3), V(25,EA,EA,CF), \
    V(AF,65,65,CA), V(8E,7A,7A,F4), V(E9,AE,AE,47), V(18,08,08,10), \
    V(D5,BA,BA,6F), V(88,78,78,F0), V(6F,25,25,4A), V(72,2E,2E,5C), \
    V(24,1C,1C,38), V(F1,A6,A6,57), V(C7,B4,B4,73), V(51,C6,C6,97), \
    V(23,E8,E8,CB), V(7C,DD,DD,A1), V(9C,74,74,E8), V(21,1F,1F,3E), \
    V(DD,4B,4B,96), V(DC,BD,BD,61), V(86,8B,8B,0D), V(85,8A,8A,0F), \
    V(90,70,70,E0), V(42,3E,3E,7C), V(C4,B5,B5,71), V(AA,66,66,CC), \
    V(D8,48,48,90), V(05,03,03,06), V(01,F6,F6,F7), V(12,0E,0E,1C), \
    V(A3,61,61,C2), V(5F,35,35,6A), V(F9,57,57,AE), V(D0,B9,B9,69), \
    V(91,86,86,17), V(58,C1,C1,99), V(27,1D,1D,3A), V(B9,9E,9E,27), \
    V(38,E1,E1,D9), V(13,F8,F8,EB), V(B3,98,98,2B), V(33,11,11,22), \
    V(BB,69,69,D2), V(70,D9,D9,A9), V(89,8E,8E,07), V(A7,94,94,33), \
    V(B6,9B,9B,2D), V(22,1E,1E,3C), V(92,87,87,15), V(20,E9,E9,C9), \
    V(49,CE,CE,87), V(FF,55,55,AA), V(78,28,28,50), V(7A,DF,DF,A5), \
    V(8F,8C,8C,03), V(F8,A1,A1,59), V(80,89,89,09), V(17,0D,0D,1A), \
    V(DA,BF,BF,65), V(31,E6,E6,D7), V(C6,42,42,84), V(B8,68,68,D0), \
    V(C3,41,41,82), V(B0,99,99,29), V(77,2D,2D,5A), V(11,0F,0F,1E), \
    V(CB,B0,B0,7B), V(FC,54,54,A8), V(D6,BB,BB,6D), V(3A,16,16,2C)

#define V(a,b,c,d) 0x##a##b##c##d
static const ulong FT0[256] = { FT };

#undef V
#define V(a,b,c,d) 0x##b##c##d##a
static const ulong FT1[256] = { FT };

#undef V
#define V(a,b,c,d) 0x##c##d##a##b
static const ulong FT2[256] = { FT };

#undef V
#define V(a,b,c,d) 0x##d##a##b##c
static const ulong FT3[256] = { FT };

#undef V
#undef FT

/*
   Reverse S-box
 */
static const uchar RSb[256] = {
    0x52, 0x09, 0x6A, 0xD5, 0x30, 0x36, 0xA5, 0x38,
    0xBF, 0x40, 0xA3, 0x9E, 0x81, 0xF3, 0xD7, 0xFB,
    0x7C, 0xE3, 0x39, 0x82, 0x9B, 0x2F, 0xFF, 0x87,
    0x34, 0x8E, 0x43, 0x44, 0xC4, 0xDE, 0xE9, 0xCB,
    0x54, 0x7B, 0x94, 0x32, 0xA6, 0xC2, 0x23, 0x3D,
    0xEE, 0x4C, 0x95, 0x0B, 0x42, 0xFA, 0xC3, 0x4E,
    0x08, 0x2E, 0xA1, 0x66, 0x28, 0xD9, 0x24, 0xB2,
    0x76, 0x5B, 0xA2, 0x49, 0x6D, 0x8B, 0xD1, 0x25,
    0x72, 0xF8, 0xF6, 0x64, 0x86, 0x68, 0x98, 0x16,
    0xD4, 0xA4, 0x5C, 0xCC, 0x5D, 0x65, 0xB6, 0x92,
    0x6C, 0x70, 0x48, 0x50, 0xFD, 0xED, 0xB9, 0xDA,
    0x5E, 0x15, 0x46, 0x57, 0xA7, 0x8D, 0x9D, 0x84,
    0x90, 0xD8, 0xAB, 0x00, 0x8C, 0xBC, 0xD3, 0x0A,
    0xF7, 0xE4, 0x58, 0x05, 0xB8, 0xB3, 0x45, 0x06,
    0xD0, 0x2C, 0x1E, 0x8F, 0xCA, 0x3F, 0x0F, 0x02,
    0xC1, 0xAF, 0xBD, 0x03, 0x01, 0x13, 0x8A, 0x6B,
    0x3A, 0x91, 0x11, 0x41, 0x4F, 0x67, 0xDC, 0xEA,
    0x97, 0xF2, 0xCF, 0xCE, 0xF0, 0xB4, 0xE6, 0x73,
    0x96, 0xAC, 0x74, 0x22, 0xE7, 0xAD, 0x35, 0x85,
    0xE2, 0xF9, 0x37, 0xE8, 0x1C, 0x75, 0xDF, 0x6E,
    0x47, 0xF1, 0x1A, 0x71, 0x1D, 0x29, 0xC5, 0x89,
    0x6F, 0xB7, 0x62, 0x0E, 0xAA, 0x18, 0xBE, 0x1B,
    0xFC, 0x56, 0x3E, 0x4B, 0xC6, 0xD2, 0x79, 0x20,
    0x9A, 0xDB, 0xC0, 0xFE, 0x78, 0xCD, 0x5A, 0xF4,
    0x1F, 0xDD, 0xA8, 0x33, 0x88, 0x07, 0xC7, 0x31,
    0xB1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xEC, 0x5F,
    0x60, 0x51, 0x7F, 0xA9, 0x19, 0xB5, 0x4A, 0x0D,
    0x2D, 0xE5, 0x7A, 0x9F, 0x93, 0xC9, 0x9C, 0xEF,
    0xA0, 0xE0, 0x3B, 0x4D, 0xAE, 0x2A, 0xF5, 0xB0,
    0xC8, 0xEB, 0xBB, 0x3C, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2B, 0x04, 0x7E, 0xBA, 0x77, 0xD6, 0x26,
    0xE1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0C, 0x7D
};

/*
   Reverse tables
 */
#define RT \
    V(50,A7,F4,51), V(53,65,41,7E), V(C3,A4,17,1A), V(96,5E,27,3A), \
    V(CB,6B,AB,3B), V(F1,45,9D,1F), V(AB,58,FA,AC), V(93,03,E3,4B), \
    V(55,FA,30,20), V(F6,6D,76,AD), V(91,76,CC,88), V(25,4C,02,F5), \
    V(FC,D7,E5,4F), V(D7,CB,2A,C5), V(80,44,35,26), V(8F,A3,62,B5), \
    V(49,5A,B1,DE), V(67,1B,BA,25), V(98,0E,EA,45), V(E1,C0,FE,5D), \
    V(02,75,2F,C3), V(12,F0,4C,81), V(A3,97,46,8D), V(C6,F9,D3,6B), \
    V(E7,5F,8F,03), V(95,9C,92,15), V(EB,7A,6D,BF), V(DA,59,52,95), \
    V(2D,83,BE,D4), V(D3,21,74,58), V(29,69,E0,49), V(44,C8,C9,8E), \
    V(6A,89,C2,75), V(78,79,8E,F4), V(6B,3E,58,99), V(DD,71,B9,27), \
    V(B6,4F,E1,BE), V(17,AD,88,F0), V(66,AC,20,C9), V(B4,3A,CE,7D), \
    V(18,4A,DF,63), V(82,31,1A,E5), V(60,33,51,97), V(45,7F,53,62), \
    V(E0,77,64,B1), V(84,AE,6B,BB), V(1C,A0,81,FE), V(94,2B,08,F9), \
    V(58,68,48,70), V(19,FD,45,8F), V(87,6C,DE,94), V(B7,F8,7B,52), \
    V(23,D3,73,AB), V(E2,02,4B,72), V(57,8F,1F,E3), V(2A,AB,55,66), \
    V(07,28,EB,B2), V(03,C2,B5,2F), V(9A,7B,C5,86), V(A5,08,37,D3), \
    V(F2,87,28,30), V(B2,A5,BF,23), V(BA,6A,03,02), V(5C,82,16,ED), \
    V(2B,1C,CF,8A), V(92,B4,79,A7), V(F0,F2,07,F3), V(A1,E2,69,4E), \
    V(CD,F4,DA,65), V(D5,BE,05,06), V(1F,62,34,D1), V(8A,FE,A6,C4), \
    V(9D,53,2E,34), V(A0,55,F3,A2), V(32,E1,8A,05), V(75,EB,F6,A4), \
    V(39,EC,83,0B), V(AA,EF,60,40), V(06,9F,71,5E), V(51,10,6E,BD), \
    V(F9,8A,21,3E), V(3D,06,DD,96), V(AE,05,3E,DD), V(46,BD,E6,4D), \
    V(B5,8D,54,91), V(05,5D,C4,71), V(6F,D4,06,04), V(FF,15,50,60), \
    V(24,FB,98,19), V(97,E9,BD,D6), V(CC,43,40,89), V(77,9E,D9,67), \
    V(BD,42,E8,B0), V(88,8B,89,07), V(38,5B,19,E7), V(DB,EE,C8,79), \
    V(47,0A,7C,A1), V(E9,0F,42,7C), V(C9,1E,84,F8), V(00,00,00,00), \
    V(83,86,80,09), V(48,ED,2B,32), V(AC,70,11,1E), V(4E,72,5A,6C), \
    V(FB,FF,0E,FD), V(56,38,85,0F), V(1E,D5,AE,3D), V(27,39,2D,36), \
    V(64,D9,0F,0A), V(21,A6,5C,68), V(D1,54,5B,9B), V(3A,2E,36,24), \
    V(B1,67,0A,0C), V(0F,E7,57,93), V(D2,96,EE,B4), V(9E,91,9B,1B), \
    V(4F,C5,C0,80), V(A2,20,DC,61), V(69,4B,77,5A), V(16,1A,12,1C), \
    V(0A,BA,93,E2), V(E5,2A,A0,C0), V(43,E0,22,3C), V(1D,17,1B,12), \
    V(0B,0D,09,0E), V(AD,C7,8B,F2), V(B9,A8,B6,2D), V(C8,A9,1E,14), \
    V(85,19,F1,57), V(4C,07,75,AF), V(BB,DD,99,EE), V(FD,60,7F,A3), \
    V(9F,26,01,F7), V(BC,F5,72,5C), V(C5,3B,66,44), V(34,7E,FB,5B), \
    V(76,29,43,8B), V(DC,C6,23,CB), V(68,FC,ED,B6), V(63,F1,E4,B8), \
    V(CA,DC,31,D7), V(10,85,63,42), V(40,22,97,13), V(20,11,C6,84), \
    V(7D,24,4A,85), V(F8,3D,BB,D2), V(11,32,F9,AE), V(6D,A1,29,C7), \
    V(4B,2F,9E,1D), V(F3,30,B2,DC), V(EC,52,86,0D), V(D0,E3,C1,77), \
    V(6C,16,B3,2B), V(99,B9,70,A9), V(FA,48,94,11), V(22,64,E9,47), \
    V(C4,8C,FC,A8), V(1A,3F,F0,A0), V(D8,2C,7D,56), V(EF,90,33,22), \
    V(C7,4E,49,87), V(C1,D1,38,D9), V(FE,A2,CA,8C), V(36,0B,D4,98), \
    V(CF,81,F5,A6), V(28,DE,7A,A5), V(26,8E,B7,DA), V(A4,BF,AD,3F), \
    V(E4,9D,3A,2C), V(0D,92,78,50), V(9B,CC,5F,6A), V(62,46,7E,54), \
    V(C2,13,8D,F6), V(E8,B8,D8,90), V(5E,F7,39,2E), V(F5,AF,C3,82), \
    V(BE,80,5D,9F), V(7C,93,D0,69), V(A9,2D,D5,6F), V(B3,12,25,CF), \
    V(3B,99,AC,C8), V(A7,7D,18,10), V(6E,63,9C,E8), V(7B,BB,3B,DB), \
    V(09,78,26,CD), V(F4,18,59,6E), V(01,B7,9A,EC), V(A8,9A,4F,83), \
    V(65,6E,95,E6), V(7E,E6,FF,AA), V(08,CF,BC,21), V(E6,E8,15,EF), \
    V(D9,9B,E7,BA), V(CE,36,6F,4A), V(D4,09,9F,EA), V(D6,7C,B0,29), \
    V(AF,B2,A4,31), V(31,23,3F,2A), V(30,94,A5,C6), V(C0,66,A2,35), \
    V(37,BC,4E,74), V(A6,CA,82,FC), V(B0,D0,90,E0), V(15,D8,A7,33), \
    V(4A,98,04,F1), V(F7,DA,EC,41), V(0E,50,CD,7F), V(2F,F6,91,17), \
    V(8D,D6,4D,76), V(4D,B0,EF,43), V(54,4D,AA,CC), V(DF,04,96,E4), \
    V(E3,B5,D1,9E), V(1B,88,6A,4C), V(B8,1F,2C,C1), V(7F,51,65,46), \
    V(04,EA,5E,9D), V(5D,35,8C,01), V(73,74,87,FA), V(2E,41,0B,FB), \
    V(5A,1D,67,B3), V(52,D2,DB,92), V(33,56,10,E9), V(13,47,D6,6D), \
    V(8C,61,D7,9A), V(7A,0C,A1,37), V(8E,14,F8,59), V(89,3C,13,EB), \
    V(EE,27,A9,CE), V(35,C9,61,B7), V(ED,E5,1C,E1), V(3C,B1,47,7A), \
    V(59,DF,D2,9C), V(3F,73,F2,55), V(79,CE,14,18), V(BF,37,C7,73), \
    V(EA,CD,F7,53), V(5B,AA,FD,5F), V(14,6F,3D,DF), V(86,DB,44,78), \
    V(81,F3,AF,CA), V(3E,C4,68,B9), V(2C,34,24,38), V(5F,40,A3,C2), \
    V(72,C3,1D,16), V(0C,25,E2,BC), V(8B,49,3C,28), V(41,95,0D,FF), \
    V(71,01,A8,39), V(DE,B3,0C,08), V(9C,E4,B4,D8), V(90,C1,56,64), \
    V(61,84,CB,7B), V(70,B6,32,D5), V(74,5C,6C,48), V(42,57,B8,D0)

#define V(a,b,c,d) 0x##a##b##c##d
static const ulong RT0[256] = { RT };

#undef V
#define V(a,b,c,d) 0x##b##c##d##a
static const ulong RT1[256] = { RT };

#undef V
#define V(a,b,c,d) 0x##c##d##a##b
static const ulong RT2[256] = { RT };

#undef V
#define V(a,b,c,d) 0x##d##a##b##c
static const ulong RT3[256] = { RT };

#undef V
#undef RT

/*
   Round constants
 */
static const ulong RCON[10] = {
    0x00000001, 0x00000002, 0x00000004, 0x00000008,
    0x00000010, 0x00000020, 0x00000040, 0x00000080,
    0x0000001B, 0x00000036
};

#else

/*
   Forward S-box & tables
 */
static uchar FSb[256];
static ulong FT0[256];
static ulong FT1[256];
static ulong FT2[256];
static ulong FT3[256];

/*
   Reverse S-box & tables
 */
static uchar RSb[256];
static ulong RT0[256];
static ulong RT1[256];
static ulong RT2[256];
static ulong RT3[256];

/*
   Round constants
 */
static ulong RCON[10];

/*
   Tables generation code
 */
#define ROTL8(x) ((x << 8) & 0xFFFFFFFF) | (x >> 24)
#define XTIME(x) ((x << 1) ^ ((x & 0x80) ? 0x1B : 0x00))
#define MUL(x,y) ((x && y) ? pow[(log[x]+log[y]) % 255] : 0)

static int aes_init_done = 0;

static void aes_gen_tables(void)
{
    int i, x, y, z;
    int pow[256];
    int log[256];

    /*
       Compute pow and log tables over GF(2^8)
     */
    for (i = 0, x = 1; i < 256; i++) {
        pow[i] = x;
        log[x] = i;
        x = (x ^ XTIME(x)) & 0xFF;
    }
    /*
        Calculate the round constants
     */
    for (i = 0, x = 1; i < 10; i++) {
        RCON[i] = (ulong)x;
        x = XTIME(x) & 0xFF;
    }
    /*
        Generate the forward and reverse S-boxes
     */
    FSb[0x00] = 0x63;
    RSb[0x63] = 0x00;

    for (i = 1; i < 256; i++) {
        x = pow[255 - log[i]];
        y = x;
        y = ((y << 1) | (y >> 7)) & 0xFF;
        x ^= y;
        y = ((y << 1) | (y >> 7)) & 0xFF;
        x ^= y;
        y = ((y << 1) | (y >> 7)) & 0xFF;
        x ^= y;
        y = ((y << 1) | (y >> 7)) & 0xFF;
        x ^= y ^ 0x63;
        FSb[i] = (uchar)x;
        RSb[x] = (uchar)i;
    }

    /*
       generate the forward and reverse tables
     */
    for (i = 0; i < 256; i++) {
        x = FSb[i];
        y = XTIME(x) & 0xFF;
        z = (y ^ x) & 0xFF;

        FT0[i] = ((ulong)y) ^ ((ulong)x << 8) ^ ((ulong)x << 16) ^ ((ulong)z << 24);
        FT1[i] = ROTL8(FT0[i]);
        FT2[i] = ROTL8(FT1[i]);
        FT3[i] = ROTL8(FT2[i]);

        x = RSb[i];
        RT0[i] = ((ulong)MUL(0x0E, x)) ^ ((ulong)MUL(0x09, x) << 8) ^ ((ulong)MUL(0x0D, x) << 16) ^
            ((ulong)MUL(0x0B, x) << 24);

        RT1[i] = ROTL8(RT0[i]);
        RT2[i] = ROTL8(RT1[i]);
        RT3[i] = ROTL8(RT2[i]);
    }
}

#endif

/*
   AES key schedule (encryption)
 */
void aes_setkey_enc(aes_context * ctx, uchar *key, int keysize)
{
    int i;
    ulong *RK;

#if !ME_EST_ROM_TABLES
    if (aes_init_done == 0) {
        aes_gen_tables();
        aes_init_done = 1;
    }
#endif

    switch (keysize) {
    case 128:
        ctx->nr = 10;
        break;
    case 192:
        ctx->nr = 12;
        break;
    case 256:
        ctx->nr = 14;
        break;
    default:
        return;
    }

#if defined(PADLOCK_ALIGN16)
    ctx->rk = RK = PADLOCK_ALIGN16(ctx->buf);
#else
    ctx->rk = RK = ctx->buf;
#endif

    for (i = 0; i < (keysize >> 5); i++) {
        GET_ULONG_LE(RK[i], key, i << 2);
    }
    switch (ctx->nr) {
    case 10:
        for (i = 0; i < 10; i++, RK += 4) {
            RK[4] = RK[0] ^ RCON[i] ^
                ((ulong)FSb[(RK[3] >> 8) & 0xFF]) ^
                ((ulong)FSb[(RK[3] >> 16) & 0xFF] << 8) ^
                ((ulong)FSb[(RK[3] >> 24) & 0xFF] << 16) ^
                ((ulong)FSb[(RK[3]) & 0xFF] << 24);

            RK[5] = RK[1] ^ RK[4];
            RK[6] = RK[2] ^ RK[5];
            RK[7] = RK[3] ^ RK[6];
        }
        break;

    case 12:
        for (i = 0; i < 8; i++, RK += 6) {
            RK[6] = RK[0] ^ RCON[i] ^
                ((ulong)FSb[(RK[5] >> 8) & 0xFF]) ^
                ((ulong)FSb[(RK[5] >> 16) & 0xFF] << 8) ^
                ((ulong)FSb[(RK[5] >> 24) & 0xFF] << 16) ^
                ((ulong)FSb[(RK[5]) & 0xFF] << 24);

            RK[7] = RK[1] ^ RK[6];
            RK[8] = RK[2] ^ RK[7];
            RK[9] = RK[3] ^ RK[8];
            RK[10] = RK[4] ^ RK[9];
            RK[11] = RK[5] ^ RK[10];
        }
        break;

    case 14:
        for (i = 0; i < 7; i++, RK += 8) {
            RK[8] = RK[0] ^ RCON[i] ^
                ((ulong)FSb[(RK[7] >> 8) & 0xFF]) ^
                ((ulong)FSb[(RK[7] >> 16) & 0xFF] << 8) ^
                ((ulong)FSb[(RK[7] >> 24) & 0xFF] << 16) ^
                ((ulong)FSb[(RK[7]) & 0xFF] << 24);

            RK[9] = RK[1] ^ RK[8];
            RK[10] = RK[2] ^ RK[9];
            RK[11] = RK[3] ^ RK[10];

            RK[12] = RK[4] ^
                ((ulong)FSb[(RK[11]) & 0xFF]) ^
                ((ulong)FSb[(RK[11] >> 8) & 0xFF] << 8) ^
                ((ulong)FSb[(RK[11] >> 16) & 0xFF] << 16) ^
                ((ulong)FSb[(RK[11] >> 24) & 0xFF] << 24);

            RK[13] = RK[5] ^ RK[12];
            RK[14] = RK[6] ^ RK[13];
            RK[15] = RK[7] ^ RK[14];
        }
        break;

    default:
        break;
    }
}


/*
   AES key schedule (decryption)
 */
void aes_setkey_dec(aes_context * ctx, uchar *key, int keysize)
{
    int         i, j;
    aes_context cty;
    ulong       *RK, *SK;

    switch (keysize) {
    case 128:
        ctx->nr = 10;
        break;
    case 192:
        ctx->nr = 12;
        break;
    case 256:
        ctx->nr = 14;
        break;
    default:
        return;
    }

#if defined(PADLOCK_ALIGN16)
    ctx->rk = RK = PADLOCK_ALIGN16(ctx->buf);
#else
    ctx->rk = RK = ctx->buf;
#endif
    aes_setkey_enc(&cty, key, keysize);
    SK = cty.rk + cty.nr * 4;

    *RK++ = *SK++;
    *RK++ = *SK++;
    *RK++ = *SK++;
    *RK++ = *SK++;

    for (i = ctx->nr - 1, SK -= 8; i > 0; i--, SK -= 8) {
        for (j = 0; j < 4; j++, SK++) {
            *RK++ = RT0[FSb[(*SK) & 0xFF]] ^
                RT1[FSb[(*SK >> 8) & 0xFF]] ^
                RT2[FSb[(*SK >> 16) & 0xFF]] ^
                RT3[FSb[(*SK >> 24) & 0xFF]];
        }
    }

    *RK++ = *SK++;
    *RK++ = *SK++;
    *RK++ = *SK++;
    *RK++ = *SK++;

    memset(&cty, 0, sizeof(aes_context));
}


#define AES_FROUND(X0,X1,X2,X3,Y0,Y1,Y2,Y3)     \
{                                               \
    X0 = *RK++ ^ FT0[ ( Y0       ) & 0xFF ] ^   \
                 FT1[ ( Y1 >>  8 ) & 0xFF ] ^   \
                 FT2[ ( Y2 >> 16 ) & 0xFF ] ^   \
                 FT3[ ( Y3 >> 24 ) & 0xFF ];    \
                                                \
    X1 = *RK++ ^ FT0[ ( Y1       ) & 0xFF ] ^   \
                 FT1[ ( Y2 >>  8 ) & 0xFF ] ^   \
                 FT2[ ( Y3 >> 16 ) & 0xFF ] ^   \
                 FT3[ ( Y0 >> 24 ) & 0xFF ];    \
                                                \
    X2 = *RK++ ^ FT0[ ( Y2       ) & 0xFF ] ^   \
                 FT1[ ( Y3 >>  8 ) & 0xFF ] ^   \
                 FT2[ ( Y0 >> 16 ) & 0xFF ] ^   \
                 FT3[ ( Y1 >> 24 ) & 0xFF ];    \
                                                \
    X3 = *RK++ ^ FT0[ ( Y3       ) & 0xFF ] ^   \
                 FT1[ ( Y0 >>  8 ) & 0xFF ] ^   \
                 FT2[ ( Y1 >> 16 ) & 0xFF ] ^   \
                 FT3[ ( Y2 >> 24 ) & 0xFF ];    \
}

#define AES_RROUND(X0,X1,X2,X3,Y0,Y1,Y2,Y3)     \
{                                               \
    X0 = *RK++ ^ RT0[ ( Y0       ) & 0xFF ] ^   \
                 RT1[ ( Y3 >>  8 ) & 0xFF ] ^   \
                 RT2[ ( Y2 >> 16 ) & 0xFF ] ^   \
                 RT3[ ( Y1 >> 24 ) & 0xFF ];    \
                                                \
    X1 = *RK++ ^ RT0[ ( Y1       ) & 0xFF ] ^   \
                 RT1[ ( Y0 >>  8 ) & 0xFF ] ^   \
                 RT2[ ( Y3 >> 16 ) & 0xFF ] ^   \
                 RT3[ ( Y2 >> 24 ) & 0xFF ];    \
                                                \
    X2 = *RK++ ^ RT0[ ( Y2       ) & 0xFF ] ^   \
                 RT1[ ( Y1 >>  8 ) & 0xFF ] ^   \
                 RT2[ ( Y0 >> 16 ) & 0xFF ] ^   \
                 RT3[ ( Y3 >> 24 ) & 0xFF ];    \
                                                \
    X3 = *RK++ ^ RT0[ ( Y3       ) & 0xFF ] ^   \
                 RT1[ ( Y2 >>  8 ) & 0xFF ] ^   \
                 RT2[ ( Y1 >> 16 ) & 0xFF ] ^   \
                 RT3[ ( Y0 >> 24 ) & 0xFF ];    \
}


/*
   AES-ECB block encryption/decryption
 */
void aes_crypt_ecb(aes_context * ctx, int mode, uchar input[16], uchar output[16])
{
    int     i;
    ulong   *RK, X0, X1, X2, X3, Y0, Y1, Y2, Y3;

#if ME_EST_PADLOCK && defined(EST_HAVE_X86)
    if (padlock_supports(PADLOCK_ACE)) {
        if (padlock_xcryptecb(ctx, mode, input, output) == 0) {
            return;
        }
    }
#endif
    RK = ctx->rk;
    GET_ULONG_LE(X0, input, 0);
    X0 ^= *RK++;
    GET_ULONG_LE(X1, input, 4);
    X1 ^= *RK++;
    GET_ULONG_LE(X2, input, 8);
    X2 ^= *RK++;
    GET_ULONG_LE(X3, input, 12);
    X3 ^= *RK++;

    if (mode == AES_DECRYPT) {
        for (i = (ctx->nr >> 1) - 1; i > 0; i--) {
            AES_RROUND(Y0, Y1, Y2, Y3, X0, X1, X2, X3);
            AES_RROUND(X0, X1, X2, X3, Y0, Y1, Y2, Y3);
        }
        AES_RROUND(Y0, Y1, Y2, Y3, X0, X1, X2, X3);
        X0 = *RK++ ^
            ((ulong)RSb[(Y0) & 0xFF]) ^
            ((ulong)RSb[(Y3 >> 8) & 0xFF] << 8) ^
            ((ulong)RSb[(Y2 >> 16) & 0xFF] << 16) ^
            ((ulong)RSb[(Y1 >> 24) & 0xFF] << 24);

        X1 = *RK++ ^
            ((ulong)RSb[(Y1) & 0xFF]) ^
            ((ulong)RSb[(Y0 >> 8) & 0xFF] << 8) ^
            ((ulong)RSb[(Y3 >> 16) & 0xFF] << 16) ^
            ((ulong)RSb[(Y2 >> 24) & 0xFF] << 24);

        X2 = *RK++ ^
            ((ulong)RSb[(Y2) & 0xFF]) ^
            ((ulong)RSb[(Y1 >> 8) & 0xFF] << 8) ^
            ((ulong)RSb[(Y0 >> 16) & 0xFF] << 16) ^
            ((ulong)RSb[(Y3 >> 24) & 0xFF] << 24);

        X3 = *RK++ ^
            ((ulong)RSb[(Y3) & 0xFF]) ^
            ((ulong)RSb[(Y2 >> 8) & 0xFF] << 8) ^
            ((ulong)RSb[(Y1 >> 16) & 0xFF] << 16) ^
            ((ulong)RSb[(Y0 >> 24) & 0xFF] << 24);
    } else {
        /* AES_ENCRYPT */
        for (i = (ctx->nr >> 1) - 1; i > 0; i--) {
            AES_FROUND(Y0, Y1, Y2, Y3, X0, X1, X2, X3);
            AES_FROUND(X0, X1, X2, X3, Y0, Y1, Y2, Y3);
        }
        AES_FROUND(Y0, Y1, Y2, Y3, X0, X1, X2, X3);
        X0 = *RK++ ^
            ((ulong)FSb[(Y0) & 0xFF]) ^
            ((ulong)FSb[(Y1 >> 8) & 0xFF] << 8) ^
            ((ulong)FSb[(Y2 >> 16) & 0xFF] << 16) ^
            ((ulong)FSb[(Y3 >> 24) & 0xFF] << 24);

        X1 = *RK++ ^
            ((ulong)FSb[(Y1) & 0xFF]) ^
            ((ulong)FSb[(Y2 >> 8) & 0xFF] << 8) ^
            ((ulong)FSb[(Y3 >> 16) & 0xFF] << 16) ^
            ((ulong)FSb[(Y0 >> 24) & 0xFF] << 24);

        X2 = *RK++ ^
            ((ulong)FSb[(Y2) & 0xFF]) ^
            ((ulong)FSb[(Y3 >> 8) & 0xFF] << 8) ^
            ((ulong)FSb[(Y0 >> 16) & 0xFF] << 16) ^
            ((ulong)FSb[(Y1 >> 24) & 0xFF] << 24);

        X3 = *RK++ ^
            ((ulong)FSb[(Y3) & 0xFF]) ^
            ((ulong)FSb[(Y0 >> 8) & 0xFF] << 8) ^
            ((ulong)FSb[(Y1 >> 16) & 0xFF] << 16) ^
            ((ulong)FSb[(Y2 >> 24) & 0xFF] << 24);
    }
    PUT_ULONG_LE(X0, output, 0);
    PUT_ULONG_LE(X1, output, 4);
    PUT_ULONG_LE(X2, output, 8);
    PUT_ULONG_LE(X3, output, 12);
}


/*
   AES-CBC buffer encryption/decryption
 */
void aes_crypt_cbc(aes_context *ctx, int mode, int length, uchar iv[16], uchar *input, uchar *output)
{
    int     i;
    uchar   temp[16];

#if ME_EST_PADLOCK && defined(EST_HAVE_X86)
    if (padlock_supports(PADLOCK_ACE)) {
        if (padlock_xcryptcbc(ctx, mode, length, iv, input, output) == 0)
            return;
    }
#endif
    if (mode == AES_DECRYPT) {
        while (length > 0) {
            memcpy(temp, input, 16);
            aes_crypt_ecb(ctx, mode, input, output);

            for (i = 0; i < 16; i++) {
                output[i] = (uchar)(output[i] ^ iv[i]);
            }
            memcpy(iv, temp, 16);
            input += 16;
            output += 16;
            length -= 16;
        }
    } else {
        while (length > 0) {
            for (i = 0; i < 16; i++) {
                output[i] = (uchar)(input[i] ^ iv[i]);
            }
            aes_crypt_ecb(ctx, mode, output, output);
            memcpy(iv, output, 16);
            input += 16;
            output += 16;
            length -= 16;
        }
    }
}


/*
   AES-CFB128 buffer encryption/decryption
 */
void aes_crypt_cfb128(aes_context *ctx, int mode, int length, int *iv_off, uchar iv[16], uchar *input, uchar *output)
{
    int c, n;

    n = *iv_off;
    if (mode == AES_DECRYPT) {
        while (length--) {
            if (n == 0) {
                aes_crypt_ecb(ctx, AES_ENCRYPT, iv, iv);
            }
            c = *input++;
            *output++ = (uchar)(c ^ iv[n]);
            iv[n] = (uchar)c;
            n = (n + 1) & 0x0F;
        }
    } else {
        while (length--) {
            if (n == 0) {
                aes_crypt_ecb(ctx, AES_ENCRYPT, iv, iv);
            }
            iv[n] = *output++ = (uchar)(iv[n] ^ *input++);
            n = (n + 1) & 0x0F;
        }
    }
    *iv_off = n;
}


#undef FSb
#undef SWAP
#undef P
#undef R
#undef S0
#undef S1
#undef S2
#undef S3

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



/********* Start of file src/arc4.c ************/


/*
    arc4.c -- An implementation of the ARCFOUR algorithm

    The ARCFOUR algorithm was publicly disclosed on 94/09.

    http://groups.google.com/group/sci.crypt/msg/10a300c9d21afca0

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_RC4
/*
   ARC4 key schedule
 */
void arc4_setup(arc4_context *ctx, uchar *key, int keylen)
{
    int     i, j, k, a;
    uchar   *m;

    ctx->x = 0;
    ctx->y = 0;
    m = ctx->m;

    for (i = 0; i < 256; i++) {
        m[i] = (uchar) i;
    }
    j = k = 0;
    for (i = 0; i < 256; i++, k++) {
        if (k >= keylen) {
            k = 0;
        }
        a = m[i];
        j = (j + a + key[k]) & 0xFF;
        m[i] = m[j];
        m[j] = (uchar)a;
    }
}


/*
   ARC4 cipher function
 */
void arc4_crypt(arc4_context *ctx, uchar *buf, int buflen)
{
    int     i, x, y, a, b;
    uchar   *m;

    x = ctx->x;
    y = ctx->y;
    m = ctx->m;

    for (i = 0; i < buflen; i++) {
        x = (x + 1) & 0xFF;
        a = m[x];
        y = (y + a) & 0xFF;
        b = m[y];

        m[x] = (uchar)b;
        m[y] = (uchar)a;

        buf[i] = (uchar) (buf[i] ^ m[(uchar)(a + b)]);
    }
    ctx->x = x;
    ctx->y = y;
}

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



/********* Start of file src/base64.c ************/


/*
    base64.c -- RFC 1521 base64 encoding/decoding

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_BASE64
static const uchar base64_enc_map[64] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
    'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
    'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',
    'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x',
    'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', '+', '/'
};

static const uchar base64_dec_map[128] = {
    127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
    127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
    127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
    127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
    127, 127, 127, 62, 127, 127, 127, 63, 52, 53,
    54, 55, 56, 57, 58, 59, 60, 61, 127, 127,
    127, 64, 127, 127, 127, 0, 1, 2, 3, 4,
    5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
    25, 127, 127, 127, 127, 127, 127, 26, 27, 28,
    29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
    39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
    49, 50, 51, 127, 127, 127, 127, 127
};

/*
 * Encode a buffer into base64 format
 */
int base64_encode(uchar *dst, int *dlen, uchar *src, int slen)
{
    int i, n;
    int C1, C2, C3;
    uchar *p;

    if (slen == 0) {
        return 0;
    }
    n = (slen << 3) / 6;

    switch ((slen << 3) - (n * 6)) {
    case 2:
        n += 3;
        break;
    case 4:
        n += 2;
        break;
    default:
        break;
    }

    if (*dlen < n + 1) {
        *dlen = n + 1;
        return EST_ERR_BASE64_BUFFER_TOO_SMALL;
    }

    n = (slen / 3) * 3;

    for (i = 0, p = dst; i < n; i += 3) {
        C1 = *src++;
        C2 = *src++;
        C3 = *src++;
        *p++ = base64_enc_map[(C1 >> 2) & 0x3F];
        *p++ = base64_enc_map[(((C1 & 3) << 4) + (C2 >> 4)) & 0x3F];
        *p++ = base64_enc_map[(((C2 & 15) << 2) + (C3 >> 6)) & 0x3F];
        *p++ = base64_enc_map[C3 & 0x3F];
    }

    if (i < slen) {
        C1 = *src++;
        C2 = ((i + 1) < slen) ? *src++ : 0;

        *p++ = base64_enc_map[(C1 >> 2) & 0x3F];
        *p++ = base64_enc_map[(((C1 & 3) << 4) + (C2 >> 4)) & 0x3F];

        if ((i + 1) < slen) {
            *p++ = base64_enc_map[((C2 & 15) << 2) & 0x3F];
        } else {
            *p++ = '=';
        }
        *p++ = '=';
    }
    *dlen = p - dst;
    *p = 0;
    return 0;
}

/*
    Decode a base64-formatted buffer
 */
int base64_decode(uchar *dst, int *dlen, uchar *src, int slen)
{
    int i, j, n;
    ulong x;
    uchar *p;

    for (i = j = n = 0; i < slen; i++) {
        if ((slen - i) >= 2 && src[i] == '\r' && src[i + 1] == '\n') {
            continue;
        }
        if (src[i] == '\n') {
            continue;
        }
        if (src[i] == '=' && ++j > 2) {
            return EST_ERR_BASE64_INVALID_CHARACTER;
        }
        if (src[i] > 127 || base64_dec_map[src[i]] == 127) {
            return EST_ERR_BASE64_INVALID_CHARACTER;
        }
        if (base64_dec_map[src[i]] < 64 && j != 0) {
            return EST_ERR_BASE64_INVALID_CHARACTER;
        }
        n++;
    }

    if (n == 0) {
        return 0;
    }
    n = ((n * 6) + 7) >> 3;

    if (*dlen < n) {
        *dlen = n;
        return EST_ERR_BASE64_BUFFER_TOO_SMALL;
    }
    for (j = 3, n = x = 0, p = dst; i > 0; i--, src++) {
        if (*src == '\r' || *src == '\n')
            continue;

        j -= (base64_dec_map[*src] == 64);
        x = (x << 6) | (base64_dec_map[*src] & 0x3F);

        if (++n == 4) {
            n = 0;
            if (j > 0) {
                *p++ = (uchar)(x >> 16);
            }
            if (j > 1) {
                *p++ = (uchar)(x >> 8);
            }
            if (j > 2) {
                *p++ = (uchar)(x);
            }
        }
    }
    *dlen = p - dst;
    return 0;
}

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



/********* Start of file src/bignum.c ************/


/*
    bignum.c -- Multi-precision integer library

    This MPI implementation is based on:
  
    http://www.cacr.math.uwaterloo.ca/hac/about/chap14.pdf
    http://www.stillhq.com/extracted/gnupg-api/mpi/
    http://math.libtomcrypt.com/files/tommath.pdf

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */



#if ME_EST_BIGNUM

#define ciL    ((int) sizeof(t_int))    /* chars in limb  */
#define biL    (ciL << 3)               /* bits  in limb  */
#define biH    (ciL << 2)               /* half limb size */

/*
   Convert between bits/chars and number of limbs
 */
#define BITS_TO_LIMBS(i)  (((i) + biL - 1) / biL)
#define CHARS_TO_LIMBS(i) (((i) + ciL - 1) / ciL)

/*
    Initialize one or more mpi
 */
void mpi_init(mpi *X, ...)
{
    va_list args;

    va_start(args, X);

    while (X != NULL) {
        X->s = 1;
        X->n = 0;
        X->p = NULL;
        X = va_arg(args, mpi *);
    }
    va_end(args);
}


/*
    Unallocate one or more mpi
 */
void mpi_free(mpi *X, ...)
{
    va_list args;

    va_start(args, X);

    while (X != NULL) {
        if (X->p != NULL) {
            memset(X->p, 0, X->n * ciL);
            free(X->p);
        }
        X->s = 1;
        X->n = 0;
        X->p = NULL;
        X = va_arg(args, mpi *);
    }
    va_end(args);
}


/*
    Enlarge to the specified number of limbs
 */
int mpi_grow(mpi *X, int nblimbs)
{
    t_int *p;

    if (X->n < nblimbs) {
        if ((p = (t_int *) malloc(nblimbs * ciL)) == NULL) {
            return 1;
        }
        memset(p, 0, nblimbs * ciL);
        if (X->p != NULL) {
            memcpy(p, X->p, X->n * ciL);
            memset(X->p, 0, X->n * ciL);
            free(X->p);
        }
        X->n = nblimbs;
        X->p = p;
    }
    return 0;
}


/*
    Copy the contents of Y into X
 */
int mpi_copy(mpi *X, mpi *Y)
{
    int ret, i;

    if (X == Y) {
        return 0;
    }
    for (i = Y->n - 1; i > 0; i--) {
        if (Y->p[i] != 0) {
            break;
        }
    }
    i++;
    X->s = Y->s;
    MPI_CHK(mpi_grow(X, i));
    memset(X->p, 0, X->n * ciL);
    memcpy(X->p, Y->p, i * ciL);

cleanup:
    return ret;
}


/*
    Swap the contents of X and Y
 */
void mpi_swap(mpi *X, mpi *Y)
{
    mpi     T;

    memcpy(&T, X, sizeof(mpi));
    memcpy(X, Y, sizeof(mpi));
    memcpy(Y, &T, sizeof(mpi));
}


/*
    Set value from integer
 */
int mpi_lset(mpi *X, int z)
{
    int ret;

    MPI_CHK(mpi_grow(X, 1));
    memset(X->p, 0, X->n * ciL);

    X->p[0] = (z < 0) ? -z : z;
    X->s = (z < 0) ? -1 : 1;

cleanup:

    return ret;
}


/*
    Return the number of least significant bits
 */
int mpi_lsb(mpi *X)
{
    int i, j, count = 0;

    for (i = 0; i < X->n; i++)
        for (j = 0; j < (int)biL; j++, count++)
            if (((X->p[i] >> j) & 1) != 0)
                return count;

    return 0;
}

/*
 * Return the number of most significant bits
 */
int mpi_msb(mpi *X)
{
    int i, j;

    for (i = X->n - 1; i > 0; i--)
        if (X->p[i] != 0)
            break;

    for (j = biL - 1; j >= 0; j--)
        if (((X->p[i] >> j) & 1) != 0)
            break;

    return (i * biL) + j + 1;
}


/*
    Return the total size in bytes
 */
int mpi_size(mpi *X)
{
    return (mpi_msb(X) + 7) >> 3;
}


/*
    Convert an ASCII character to digit value
 */
static int mpi_get_digit(t_int * d, int radix, char c)
{
    *d = 255;

    if (c >= 0x30 && c <= 0x39)
        *d = c - 0x30;
    if (c >= 0x41 && c <= 0x46)
        *d = c - 0x37;
    if (c >= 0x61 && c <= 0x66)
        *d = c - 0x57;

    if (*d >= (t_int) radix)
        return EST_ERR_MPI_INVALID_CHARACTER;

    return 0;
}


/*
    Import from an ASCII string
 */
int mpi_read_string(mpi *X, int radix, char *s)
{
    int ret, i, j, n;
    t_int d;
    mpi T;

    if (radix < 2 || radix > 16)
        return EST_ERR_MPI_BAD_INPUT_DATA;

    mpi_init(&T, NULL);

    if (radix == 16) {
        n = BITS_TO_LIMBS(strlen(s) << 2);

        MPI_CHK(mpi_grow(X, n));
        MPI_CHK(mpi_lset(X, 0));

        for (i = strlen(s) - 1, j = 0; i >= 0; i--, j++) {
            if (i == 0 && s[i] == '-') {
                X->s = -1;
                break;
            }

            MPI_CHK(mpi_get_digit(&d, radix, s[i]));
            X->p[j / (2 * ciL)] |= d << ((j % (2 * ciL)) << 2);
        }
    } else {
        MPI_CHK(mpi_lset(X, 0));

        for (i = 0; i < (int)strlen(s); i++) {
            if (i == 0 && s[i] == '-') {
                X->s = -1;
                continue;
            }

            MPI_CHK(mpi_get_digit(&d, radix, s[i]));
            MPI_CHK(mpi_mul_int(&T, X, radix));
            MPI_CHK(mpi_add_int(X, &T, d));
        }
    }

cleanup:
    mpi_free(&T, NULL);
    return ret;
}


/*
   Helper to write the digits high-order first
 */
static int mpi_write_hlp(mpi *X, int radix, char **p)
{
    int ret;
    t_int r;

    if (radix < 2 || radix > 16)
        return EST_ERR_MPI_BAD_INPUT_DATA;

    MPI_CHK(mpi_mod_int(&r, X, radix));
    MPI_CHK(mpi_div_int(X, NULL, X, radix));

    if (mpi_cmp_int(X, 0) != 0)
        MPI_CHK(mpi_write_hlp(X, radix, p));

    if (r < 10)
        *(*p)++ = (char)(r + 0x30);
    else
        *(*p)++ = (char)(r + 0x37);

cleanup:

    return ret;
}


/*
    Export into an ASCII string
 */
int mpi_write_string(mpi *X, int radix, char *s, int *slen)
{
    int ret = 0, n;
    char *p;
    mpi T;

    if (radix < 2 || radix > 16)
        return EST_ERR_MPI_BAD_INPUT_DATA;

    n = mpi_msb(X);
    if (radix >= 4)
        n >>= 1;
    if (radix >= 16)
        n >>= 1;
    n += 3;

    if (*slen < n) {
        *slen = n;
        return EST_ERR_MPI_BUFFER_TOO_SMALL;
    }

    p = s;
    mpi_init(&T, NULL);

    if (X->s == -1) {
        *p++ = '-';
    }
    if (radix == 16) {
        int c, i, j, k;

        for (i = X->n - 1, k = 0; i >= 0; i--) {
            for (j = ciL - 1; j >= 0; j--) {
                c = (X->p[i] >> (j << 3)) & 0xFF;

                if (c == 0 && k == 0 && (i + j) != 0) {
                    continue;
                }
                p += sprintf(p, "%02X", c);
                k = 1;
            }
        }
    } else {
        MPI_CHK(mpi_copy(&T, X));
        MPI_CHK(mpi_write_hlp(&T, radix, &p));
    }

    *p++ = '\0';
    *slen = p - s;

cleanup:
    mpi_free(&T, NULL);
    return ret;
}


/*
    Read X from an opened file
 */
int mpi_read_file(mpi *X, int radix, FILE * fin)
{
    t_int d;
    int slen;
    char *p;
    char s[1024];

    memset(s, 0, sizeof(s));
    if (fgets(s, sizeof(s) - 1, fin) == NULL) {
        return EST_ERR_MPI_FILE_IO_ERROR;
    }
    slen = strlen(s);
    if (s[slen - 1] == '\n') {
        slen--;
        s[slen] = '\0';
    }
    if (s[slen - 1] == '\r') {
        slen--;
        s[slen] = '\0';
    }

    p = s + slen;
    while (--p >= s) {
        if (mpi_get_digit(&d, radix, *p) != 0) {
            break;
        }
    }
    return mpi_read_string(X, radix, p + 1);
}


/*
    Write X into an opened file (or stdout if fout == NULL)
 */
int mpi_write_file(char *p, mpi *X, int radix, FILE * fout)
{
    int n, ret;
    size_t slen;
    size_t plen;
    char s[1024];

    n = sizeof(s);
    memset(s, 0, n);
    n -= 2;

    MPI_CHK(mpi_write_string(X, radix, s, (int *)&n));

    if (p == NULL) {
        p = "";
    }
    plen = strlen(p);
    slen = strlen(s);
    s[slen++] = '\r';
    s[slen++] = '\n';

    if (fout != NULL) {
        if (fwrite(p, 1, plen, fout) != plen || fwrite(s, 1, slen, fout) != slen) {
            return EST_ERR_MPI_FILE_IO_ERROR;
        }
    } else {
        printf("%s%s", p, s);
    }

cleanup:
    return ret;
}


/*
    Import X from unsigned binary data, big endian
 */
int mpi_read_binary(mpi *X, uchar *buf, int buflen)
{
    int ret, i, j, n;

    for (n = 0; n < buflen; n++) {
        if (buf[n] != 0) {
            break;
        }
    }
    MPI_CHK(mpi_grow(X, CHARS_TO_LIMBS(buflen - n)));
    MPI_CHK(mpi_lset(X, 0));

    for (i = buflen - 1, j = 0; i >= n; i--, j++) {
        X->p[j / ciL] |= ((t_int) buf[i]) << ((j % ciL) << 3);
    }

cleanup:
    return ret;
}


/*
    Export X into unsigned binary data, big endian
 */
int mpi_write_binary(mpi *X, uchar *buf, int buflen)
{
    int i, j, n;

    n = mpi_size(X);

    if (buflen < n) {
        return EST_ERR_MPI_BUFFER_TOO_SMALL;
    }
    memset(buf, 0, buflen);

    for (i = buflen - 1, j = 0; n > 0; i--, j++, n--) {
        buf[i] = (uchar)(X->p[j / ciL] >> ((j % ciL) << 3));
    }
    return 0;
}


/*
    Left-shift: X <<= count
 */
int mpi_shift_l(mpi *X, int count)
{
    int ret, i, v0, t1;
    t_int r0 = 0, r1;

    v0 = count / (biL);
    t1 = count & (biL - 1);

    i = mpi_msb(X) + count;

    if (X->n * (int)biL < i)
        MPI_CHK(mpi_grow(X, BITS_TO_LIMBS(i)));

    ret = 0;

    /*
        shift by count / limb_size
     */
    if (v0 > 0) {
        for (i = X->n - 1; i >= v0; i--)
            X->p[i] = X->p[i - v0];

        for (; i >= 0; i--)
            X->p[i] = 0;
    }

    /*
        shift by count % limb_size
     */
    if (t1 > 0) {
        for (i = v0; i < X->n; i++) {
            r1 = X->p[i] >> (biL - t1);
            X->p[i] <<= t1;
            X->p[i] |= r0;
            r0 = r1;
        }
    }
cleanup:
    return ret;
}


/*
    Right-shift: X >>= count
 */
int mpi_shift_r(mpi *X, int count)
{
    int i, v0, v1;
    t_int r0 = 0, r1;

    v0 = count / biL;
    v1 = count & (biL - 1);

    /*
        shift by count / limb_size
     */
    if (v0 > 0) {
        for (i = 0; i < X->n - v0; i++)
            X->p[i] = X->p[i + v0];

        for (; i < X->n; i++)
            X->p[i] = 0;
    }

    /*
        shift by count % limb_size
     */
    if (v1 > 0) {
        for (i = X->n - 1; i >= 0; i--) {
            r1 = X->p[i] << (biL - v1);
            X->p[i] >>= v1;
            X->p[i] |= r0;
            r0 = r1;
        }
    }
    return 0;
}


/*
    Compare unsigned values
 */
int mpi_cmp_abs(mpi *X, mpi *Y)
{
    int i, j;

    for (i = X->n - 1; i >= 0; i--)
        if (X->p[i] != 0)
            break;

    for (j = Y->n - 1; j >= 0; j--)
        if (Y->p[j] != 0)
            break;

    if (i < 0 && j < 0)
        return 0;

    if (i > j)
        return 1;
    if (j > i)
        return -1;

    for (; i >= 0; i--) {
        if (X->p[i] > Y->p[i])
            return 1;
        if (X->p[i] < Y->p[i])
            return -1;
    }
    return 0;
}


/*
    Compare signed values
 */
int mpi_cmp_mpi(mpi *X, mpi *Y)
{
    int i, j;

    for (i = X->n - 1; i >= 0; i--)
        if (X->p[i] != 0)
            break;

    for (j = Y->n - 1; j >= 0; j--)
        if (Y->p[j] != 0)
            break;

    if (i < 0 && j < 0)
        return 0;

    if (i > j)
        return X->s;
    if (j > i)
        return -X->s;

    if (X->s > 0 && Y->s < 0)
        return 1;
    if (Y->s > 0 && X->s < 0)
        return -1;

    for (; i >= 0; i--) {
        if (X->p[i] > Y->p[i])
            return X->s;
        if (X->p[i] < Y->p[i])
            return -X->s;
    }

    return 0;
}


/*
    Compare signed values
 */
int mpi_cmp_int(mpi *X, int z)
{
    mpi Y;
    t_int p[1];

    *p = (z < 0) ? -z : z;
    Y.s = (z < 0) ? -1 : 1;
    Y.n = 1;
    Y.p = p;

    return mpi_cmp_mpi(X, &Y);
}


/*
    Unsigned addition: X = |A| + |B|  (HAC 14.7)
 */
int mpi_add_abs(mpi *X, mpi *A, mpi *B)
{
    int ret, i, j;
    t_int *o, *p, c;

    if (X == B) {
        mpi *T = A;
        A = X;
        B = T;
    }

    if (X != A)
        MPI_CHK(mpi_copy(X, A));

    for (j = B->n - 1; j >= 0; j--)
        if (B->p[j] != 0)
            break;

    MPI_CHK(mpi_grow(X, j + 1));

    o = B->p;
    p = X->p;
    c = 0;

    for (i = 0; i <= j; i++, o++, p++) {
        *p += c;
        c = (*p < c);
        *p += *o;
        c += (*p < *o);
    }
    while (c != 0) {
        if (i >= X->n) {
            MPI_CHK(mpi_grow(X, i + 1));
            p = X->p + i;
        }

        *p += c;
        c = (*p < c);
        i++;
    }
cleanup:
    return ret;
}


/*
    Helper for mpi substraction
 */
static void mpi_sub_hlp(int n, t_int * s, t_int * d)
{
    int     i;
    t_int   c, z;

    for (i = c = 0; i < n; i++, s++, d++) {
        z = (*d < c);
        *d -= c;
        c = (*d < *s) + z;
        *d -= *s;
    }

    while (c != 0) {
        z = (*d < c);
        *d -= c;
        c = z;
        i++;
        d++;
    }
}


/*
    Unsigned substraction: X = |A| - |B|  (HAC 14.9)
 */
int mpi_sub_abs(mpi *X, mpi *A, mpi *B)
{
    mpi TB;
    int ret, n;

    if (mpi_cmp_abs(A, B) < 0)
        return EST_ERR_MPI_NEGATIVE_VALUE;

    mpi_init(&TB, NULL);

    if (X == B) {
        MPI_CHK(mpi_copy(&TB, B));
        B = &TB;
    }
    if (X != A) {
        MPI_CHK(mpi_copy(X, A));
    }
    ret = 0;

    for (n = B->n - 1; n >= 0; n--) {
        if (B->p[n] != 0) {
            break;
        }
    }
    mpi_sub_hlp(n + 1, B->p, X->p);

cleanup:

    mpi_free(&TB, NULL);
    return ret;
}


/*
    Signed addition: X = A + B
 */
int mpi_add_mpi(mpi *X, mpi *A, mpi *B)
{
    int ret, s = A->s;

    if (A->s * B->s < 0) {
        if (mpi_cmp_abs(A, B) >= 0) {
            MPI_CHK(mpi_sub_abs(X, A, B));
            X->s = s;
        } else {
            MPI_CHK(mpi_sub_abs(X, B, A));
            X->s = -s;
        }
    } else {
        MPI_CHK(mpi_add_abs(X, A, B));
        X->s = s;
    }
cleanup:
    return ret;
}


/*
    Signed substraction: X = A - B
 */
int mpi_sub_mpi(mpi *X, mpi *A, mpi *B)
{
    int ret, s = A->s;

    if (A->s * B->s > 0) {
        if (mpi_cmp_abs(A, B) >= 0) {
            MPI_CHK(mpi_sub_abs(X, A, B));
            X->s = s;
        } else {
            MPI_CHK(mpi_sub_abs(X, B, A));
            X->s = -s;
        }
    } else {
        MPI_CHK(mpi_add_abs(X, A, B));
        X->s = s;
    }
cleanup:
    return ret;
}


/*
    Signed addition: X = A + b
 */
int mpi_add_int(mpi *X, mpi *A, int b)
{
    mpi     _B;
    t_int   p[1];

    p[0] = (b < 0) ? -b : b;
    _B.s = (b < 0) ? -1 : 1;
    _B.n = 1;
    _B.p = p;
    return mpi_add_mpi(X, A, &_B);
}


/*
    Signed substraction: X = A - b
 */
int mpi_sub_int(mpi *X, mpi *A, int b)
{
    mpi     _B;
    t_int   p[1];

    p[0] = (b < 0) ? -b : b;
    _B.s = (b < 0) ? -1 : 1;
    _B.n = 1;
    _B.p = p;
    return mpi_sub_mpi(X, A, &_B);
}


/*
    Helper for mpi multiplication
 */
static void mpi_mul_hlp(int i, t_int * s, t_int * d, t_int b)
{
    t_int c = 0, t = 0;

#if defined(MULADDC_HUIT)
    for (; i >= 8; i -= 8) {
    MULADDC_INIT MULADDC_HUIT MULADDC_STOP} for (; i > 0; i--) {
    MULADDC_INIT MULADDC_CORE MULADDC_STOP}
#else
    for (; i >= 16; i -= 16) {
        MULADDC_INIT
        MULADDC_CORE MULADDC_CORE
        MULADDC_CORE MULADDC_CORE
        MULADDC_CORE MULADDC_CORE
        MULADDC_CORE MULADDC_CORE
        MULADDC_CORE MULADDC_CORE
        MULADDC_CORE MULADDC_CORE
        MULADDC_CORE MULADDC_CORE
        MULADDC_CORE MULADDC_CORE 
        MULADDC_STOP
    }
    for (; i >= 8; i -= 8) {
        MULADDC_INIT
        MULADDC_CORE MULADDC_CORE
        MULADDC_CORE MULADDC_CORE
        MULADDC_CORE MULADDC_CORE
        MULADDC_CORE MULADDC_CORE 
        MULADDC_STOP
    }
    for (; i > 0; i--) {
        MULADDC_INIT 
        MULADDC_CORE 
        MULADDC_STOP
    }
#endif
    t++;

    do {
        *d += c;
        c = (*d < c);
        d++;
    } while (c != 0);
}


/*
   Baseline multiplication: X = A * B  (HAC 14.12)
 */
int mpi_mul_mpi(mpi *X, mpi *A, mpi *B)
{
    int     ret, i, j;
    mpi     TA, TB;

    mpi_init(&TA, &TB, NULL);

    if (X == A) {
        MPI_CHK(mpi_copy(&TA, A));
        A = &TA;
    }
    if (X == B) {
        MPI_CHK(mpi_copy(&TB, B));
        B = &TB;
    }
    for (i = A->n - 1; i >= 0; i--)
        if (A->p[i] != 0)
            break;

    for (j = B->n - 1; j >= 0; j--)
        if (B->p[j] != 0)
            break;

    MPI_CHK(mpi_grow(X, i + j + 2));
    MPI_CHK(mpi_lset(X, 0));

    for (i++; j >= 0; j--)
        mpi_mul_hlp(i, A->p, X->p + j, B->p[j]);

    X->s = A->s * B->s;

cleanup:
    mpi_free(&TB, &TA, NULL);
    return ret;
}


/*
    Baseline multiplication: X = A * b
 */
int mpi_mul_int(mpi *X, mpi *A, t_int b)
{
    mpi _B;
    t_int p[1];

    _B.s = 1;
    _B.n = 1;
    _B.p = p;
    p[0] = b;
    return mpi_mul_mpi(X, A, &_B);
}


/*
    Division by mpi: A = Q * B + R  (HAC 14.20)
 */
int mpi_div_mpi(mpi *Q, mpi *R, mpi *A, mpi *B)
{
    int ret, i, n, t, k;
    mpi X, Y, Z, T1, T2;

    if (mpi_cmp_int(B, 0) == 0)
        return EST_ERR_MPI_DIVISION_BY_ZERO;

    mpi_init(&X, &Y, &Z, &T1, &T2, NULL);

    if (mpi_cmp_abs(A, B) < 0) {
        if (Q != NULL)
            MPI_CHK(mpi_lset(Q, 0));
        if (R != NULL)
            MPI_CHK(mpi_copy(R, A));
        return 0;
    }
    MPI_CHK(mpi_copy(&X, A));
    MPI_CHK(mpi_copy(&Y, B));
    X.s = Y.s = 1;

    MPI_CHK(mpi_grow(&Z, A->n + 2));
    MPI_CHK(mpi_lset(&Z, 0));
    MPI_CHK(mpi_grow(&T1, 2));
    MPI_CHK(mpi_grow(&T2, 3));

    k = mpi_msb(&Y) % biL;
    if (k < (int)biL - 1) {
        k = biL - 1 - k;
        MPI_CHK(mpi_shift_l(&X, k));
        MPI_CHK(mpi_shift_l(&Y, k));
    } else
        k = 0;

    n = X.n - 1;
    t = Y.n - 1;
    mpi_shift_l(&Y, biL * (n - t));

    while (mpi_cmp_mpi(&X, &Y) >= 0) {
        Z.p[n - t]++;
        mpi_sub_mpi(&X, &X, &Y);
    }
    mpi_shift_r(&Y, biL * (n - t));

    for (i = n; i > t; i--) {
        if (X.p[i] >= Y.p[t])
            Z.p[i - t - 1] = ~0;
        else {
#if ME_USE_LONG_LONG
            t_dbl r;

            r = (t_dbl) X.p[i] << biL;
            r |= (t_dbl) X.p[i - 1];
            r /= Y.p[t];
            if (r > ((t_dbl) 1 << biL) - 1)
                r = ((t_dbl) 1 << biL) - 1;

            Z.p[i - t - 1] = (t_int) r;
#else
            /*
             * __udiv_qrnnd_c, from gmp/longlong.h
             */
            t_int q0, q1, r0, r1;
            t_int d0, d1, d, m;

            d = Y.p[t];
            d0 = (d << biH) >> biH;
            d1 = (d >> biH);

            q1 = X.p[i] / d1;
            r1 = X.p[i] - d1 * q1;
            r1 <<= biH;
            r1 |= (X.p[i - 1] >> biH);

            m = q1 * d0;
            if (r1 < m) {
                q1--, r1 += d;
                while (r1 >= d && r1 < m)
                    q1--, r1 += d;
            }
            r1 -= m;

            q0 = r1 / d1;
            r0 = r1 - d1 * q0;
            r0 <<= biH;
            r0 |= (X.p[i - 1] << biH) >> biH;

            m = q0 * d0;
            if (r0 < m) {
                q0--, r0 += d;
                while (r0 >= d && r0 < m)
                    q0--, r0 += d;
            }
            r0 -= m;

            Z.p[i - t - 1] = (q1 << biH) | q0;
#endif
        }

        Z.p[i - t - 1]++;
        do {
            Z.p[i - t - 1]--;

            MPI_CHK(mpi_lset(&T1, 0));
            T1.p[0] = (t < 1) ? 0 : Y.p[t - 1];
            T1.p[1] = Y.p[t];
            MPI_CHK(mpi_mul_int(&T1, &T1, Z.p[i - t - 1]));

            MPI_CHK(mpi_lset(&T2, 0));
            T2.p[0] = (i < 2) ? 0 : X.p[i - 2];
            T2.p[1] = (i < 1) ? 0 : X.p[i - 1];
            T2.p[2] = X.p[i];
        } while (mpi_cmp_mpi(&T1, &T2) > 0);

        MPI_CHK(mpi_mul_int(&T1, &Y, Z.p[i - t - 1]));
        MPI_CHK(mpi_shift_l(&T1, biL * (i - t - 1)));
        MPI_CHK(mpi_sub_mpi(&X, &X, &T1));

        if (mpi_cmp_int(&X, 0) < 0) {
            MPI_CHK(mpi_copy(&T1, &Y));
            MPI_CHK(mpi_shift_l(&T1, biL * (i - t - 1)));
            MPI_CHK(mpi_add_mpi(&X, &X, &T1));
            Z.p[i - t - 1]--;
        }
    }

    if (Q != NULL) {
        mpi_copy(Q, &Z);
        Q->s = A->s * B->s;
    }

    if (R != NULL) {
        mpi_shift_r(&X, k);
        mpi_copy(R, &X);

        R->s = A->s;
        if (mpi_cmp_int(R, 0) == 0)
            R->s = 1;
    }

cleanup:
    mpi_free(&X, &Y, &Z, &T1, &T2, NULL);
    return ret;
}


/*
   Division by int: A = Q * b + R
  
   Returns 0 if successful
           1 if memory allocation failed
           EST_ERR_MPI_DIVISION_BY_ZERO if b == 0
 */
int mpi_div_int(mpi *Q, mpi *R, mpi *A, int b)
{
    mpi _B;
    t_int p[1];

    p[0] = (b < 0) ? -b : b;
    _B.s = (b < 0) ? -1 : 1;
    _B.n = 1;
    _B.p = p;

    return mpi_div_mpi(Q, R, A, &_B);
}


/*
    Modulo: R = A mod B
 */
int mpi_mod_mpi(mpi *R, mpi *A, mpi *B)
{
    int ret;

    MPI_CHK(mpi_div_mpi(NULL, R, A, B));

    while (mpi_cmp_int(R, 0) < 0)
        MPI_CHK(mpi_add_mpi(R, R, B));

    while (mpi_cmp_mpi(R, B) >= 0)
        MPI_CHK(mpi_sub_mpi(R, R, B));

cleanup:
    return ret;
}


/*
   Modulo: r = A mod b
 */
int mpi_mod_int(t_int * r, mpi *A, int b)
{
    int i;
    t_int x, y, z;

    if (b == 0)
        return EST_ERR_MPI_DIVISION_BY_ZERO;

    if (b < 0)
        b = -b;

    /*
       handle trivial cases
     */
    if (b == 1) {
        *r = 0;
        return 0;
    }

    if (b == 2) {
        *r = A->p[0] & 1;
        return 0;
    }

    /*
     * general case
     */
    for (i = A->n - 1, y = 0; i >= 0; i--) {
        x = A->p[i];
        y = (y << biH) | (x >> biH);
        z = y / b;
        y -= z * b;

        x <<= biH;
        y = (y << biH) | (x >> biH);
        z = y / b;
        y -= z * b;
    }
    *r = y;
    return 0;
}


/*
    Fast Montgomery initialization (thanks to Tom St Denis)
 */
static void mpi_montg_init(t_int * mm, mpi *N)
{
    t_int x, m0 = N->p[0];

    x = m0;
    x += ((m0 + 2) & 4) << 1;
    x *= (2 - (m0 * x));

    if (biL >= 16)
        x *= (2 - (m0 * x));
    if (biL >= 32)
        x *= (2 - (m0 * x));
    if (biL >= 64)
        x *= (2 - (m0 * x));

    *mm = ~x + 1;
}


/*
    Montgomery multiplication: A = A * B * R^-1 mod N  (HAC 14.36)
 */
static void mpi_montmul(mpi *A, mpi *B, mpi *N, t_int mm, mpi *T)
{
    int i, n, m;
    t_int u0, u1, *d;

    memset(T->p, 0, T->n * ciL);

    d = T->p;
    n = N->n;
    m = (B->n < n) ? B->n : n;

    for (i = 0; i < n; i++) {
        /*
            T = (T + u0*B + u1*N) / 2^biL
         */
        u0 = A->p[i];
        u1 = (d[0] + u0 * B->p[0]) * mm;

        mpi_mul_hlp(m, B->p, d, u0);
        mpi_mul_hlp(n, N->p, d, u1);

        *d++ = u0;
        d[n + 1] = 0;
    }

    memcpy(A->p, d, (n + 1) * ciL);

    if (mpi_cmp_abs(A, N) >= 0)
        mpi_sub_hlp(n, N->p, A->p);
    else
        /* prevent timing attacks */
        mpi_sub_hlp(n, A->p, T->p);
}


/*
    Montgomery reduction: A = A * R^-1 mod N
 */
static void mpi_montred(mpi *A, mpi *N, t_int mm, mpi *T)
{
    t_int z = 1;
    mpi U;

    U.n = U.s = z;
    U.p = &z;

    mpi_montmul(A, &U, N, mm, T);
}


/*
    Sliding-window exponentiation: X = A^E mod N  (HAC 14.85)
 */
int mpi_exp_mod(mpi *X, mpi *A, mpi *E, mpi *N, mpi *_RR)
{
    int ret, i, j, wsize, wbits;
    int bufsize, nblimbs, nbits;
    t_int ei, mm, state;
    mpi RR, T, W[64];

    if (mpi_cmp_int(N, 0) < 0 || (N->p[0] & 1) == 0)
        return EST_ERR_MPI_BAD_INPUT_DATA;

    /*
       Init temps and window size
     */
    mpi_montg_init(&mm, N);
    mpi_init(&RR, &T, NULL);
    memset(W, 0, sizeof(W));

    i = mpi_msb(E);

    wsize = (i > 671) ? 6 : (i > 239) ? 5 : (i > 79) ? 4 : (i > 23) ? 3 : 1;

    j = N->n + 1;
    MPI_CHK(mpi_grow(X, j));
    MPI_CHK(mpi_grow(&W[1], j));
    MPI_CHK(mpi_grow(&T, j * 2));

    /*
        If 1st call, pre-compute R^2 mod N
     */
    if (_RR == NULL || _RR->p == NULL) {
        MPI_CHK(mpi_lset(&RR, 1));
        MPI_CHK(mpi_shift_l(&RR, N->n * 2 * biL));
        MPI_CHK(mpi_mod_mpi(&RR, &RR, N));

        if (_RR != NULL)
            memcpy(_RR, &RR, sizeof(mpi));
    } else
        memcpy(&RR, _RR, sizeof(mpi));

    /*
        W[1] = A * R^2 * R^-1 mod N = A * R mod N
     */
    if (mpi_cmp_mpi(A, N) >= 0)
        mpi_mod_mpi(&W[1], A, N);
    else
        mpi_copy(&W[1], A);

    mpi_montmul(&W[1], &RR, N, mm, &T);

    /*
        X = R^2 * R^-1 mod N = R mod N
     */
    MPI_CHK(mpi_copy(X, &RR));
    mpi_montred(X, N, mm, &T);

    if (wsize > 1) {
        /*
         * W[1 << (wsize - 1)] = W[1] ^ (wsize - 1)
         */
        j = 1 << (wsize - 1);

        MPI_CHK(mpi_grow(&W[j], N->n + 1));
        MPI_CHK(mpi_copy(&W[j], &W[1]));

        for (i = 0; i < wsize - 1; i++)
            mpi_montmul(&W[j], &W[j], N, mm, &T);

        /*
         * W[i] = W[i - 1] * W[1]
         */
        for (i = j + 1; i < (1 << wsize); i++) {
            MPI_CHK(mpi_grow(&W[i], N->n + 1));
            MPI_CHK(mpi_copy(&W[i], &W[i - 1]));

            mpi_montmul(&W[i], &W[1], N, mm, &T);
        }
    }

    nblimbs = E->n;
    bufsize = 0;
    nbits = 0;
    wbits = 0;
    state = 0;

    while (1) {
        if (bufsize == 0) {
            if (nblimbs-- == 0)
                break;

            bufsize = sizeof(t_int) << 3;
        }

        bufsize--;

        ei = (E->p[nblimbs] >> bufsize) & 1;

        /*
            skip leading 0s
         */
        if (ei == 0 && state == 0)
            continue;

        if (ei == 0 && state == 1) {
            /*
             * out of window, square X
             */
            mpi_montmul(X, X, N, mm, &T);
            continue;
        }

        /*
            add ei to current window
         */
        state = 2;

        nbits++;
        wbits |= (ei << (wsize - nbits));

        if (nbits == wsize) {
            /*
                X = X^wsize R^-1 mod N
             */
            for (i = 0; i < wsize; i++)
                mpi_montmul(X, X, N, mm, &T);

            /*
                X = X * W[wbits] R^-1 mod N
             */
            mpi_montmul(X, &W[wbits], N, mm, &T);

            state--;
            nbits = 0;
            wbits = 0;
        }
    }

    /*
        process the remaining bits
     */
    for (i = 0; i < nbits; i++) {
        mpi_montmul(X, X, N, mm, &T);

        wbits <<= 1;

        if ((wbits & (1 << wsize)) != 0)
            mpi_montmul(X, &W[1], N, mm, &T);
    }

    /*
        X = A^E * R * R^-1 mod N = A^E mod N
     */
    mpi_montred(X, N, mm, &T);

cleanup:

    for (i = (1 << (wsize - 1)); i < (1 << wsize); i++)
        mpi_free(&W[i], NULL);

    if (_RR != NULL)
        mpi_free(&W[1], &T, NULL);
    else
        mpi_free(&W[1], &T, &RR, NULL);

    return ret;
}


/*
    Greatest common divisor: G = gcd(A, B)  (HAC 14.54)
 */
int mpi_gcd(mpi *G, mpi *A, mpi *B)
{
    int ret, lz, lzt;
    mpi TG, TA, TB;

    mpi_init(&TG, &TA, &TB, NULL);

    MPI_CHK(mpi_copy(&TA, A));
    MPI_CHK(mpi_copy(&TB, B));

    lz = mpi_lsb(&TA);
    lzt = mpi_lsb(&TB);

    if (lzt < lz)
        lz = lzt;

    MPI_CHK(mpi_shift_r(&TA, lz));
    MPI_CHK(mpi_shift_r(&TB, lz));

    TA.s = TB.s = 1;

    while (mpi_cmp_int(&TA, 0) != 0) {
        MPI_CHK(mpi_shift_r(&TA, mpi_lsb(&TA)));
        MPI_CHK(mpi_shift_r(&TB, mpi_lsb(&TB)));

        if (mpi_cmp_mpi(&TA, &TB) >= 0) {
            MPI_CHK(mpi_sub_abs(&TA, &TA, &TB));
            MPI_CHK(mpi_shift_r(&TA, 1));
        } else {
            MPI_CHK(mpi_sub_abs(&TB, &TB, &TA));
            MPI_CHK(mpi_shift_r(&TB, 1));
        }
    }

    MPI_CHK(mpi_shift_l(&TB, lz));
    MPI_CHK(mpi_copy(G, &TB));

cleanup:
    mpi_free(&TB, &TA, &TG, NULL);
    return ret;
}


#if ME_EST_GEN_PRIME
/*
    Modular inverse: X = A^-1 mod N  (HAC 14.61 / 14.64)
 */
int mpi_inv_mod(mpi *X, mpi *A, mpi *N)
{
    int ret;
    mpi G, TA, TU, U1, U2, TB, TV, V1, V2;

    if (mpi_cmp_int(N, 0) <= 0)
        return EST_ERR_MPI_BAD_INPUT_DATA;

    mpi_init(&TA, &TU, &U1, &U2, &G, &TB, &TV, &V1, &V2, NULL);

    MPI_CHK(mpi_gcd(&G, A, N));

    if (mpi_cmp_int(&G, 1) != 0) {
        ret = EST_ERR_MPI_NOT_ACCEPTABLE;
        goto cleanup;
    }

    MPI_CHK(mpi_mod_mpi(&TA, A, N));
    MPI_CHK(mpi_copy(&TU, &TA));
    MPI_CHK(mpi_copy(&TB, N));
    MPI_CHK(mpi_copy(&TV, N));

    MPI_CHK(mpi_lset(&U1, 1));
    MPI_CHK(mpi_lset(&U2, 0));
    MPI_CHK(mpi_lset(&V1, 0));
    MPI_CHK(mpi_lset(&V2, 1));

    do {
        while ((TU.p[0] & 1) == 0) {
            MPI_CHK(mpi_shift_r(&TU, 1));

            if ((U1.p[0] & 1) != 0 || (U2.p[0] & 1) != 0) {
                MPI_CHK(mpi_add_mpi(&U1, &U1, &TB));
                MPI_CHK(mpi_sub_mpi(&U2, &U2, &TA));
            }

            MPI_CHK(mpi_shift_r(&U1, 1));
            MPI_CHK(mpi_shift_r(&U2, 1));
        }

        while ((TV.p[0] & 1) == 0) {
            MPI_CHK(mpi_shift_r(&TV, 1));

            if ((V1.p[0] & 1) != 0 || (V2.p[0] & 1) != 0) {
                MPI_CHK(mpi_add_mpi(&V1, &V1, &TB));
                MPI_CHK(mpi_sub_mpi(&V2, &V2, &TA));
            }

            MPI_CHK(mpi_shift_r(&V1, 1));
            MPI_CHK(mpi_shift_r(&V2, 1));
        }

        if (mpi_cmp_mpi(&TU, &TV) >= 0) {
            MPI_CHK(mpi_sub_mpi(&TU, &TU, &TV));
            MPI_CHK(mpi_sub_mpi(&U1, &U1, &V1));
            MPI_CHK(mpi_sub_mpi(&U2, &U2, &V2));
        } else {
            MPI_CHK(mpi_sub_mpi(&TV, &TV, &TU));
            MPI_CHK(mpi_sub_mpi(&V1, &V1, &U1));
            MPI_CHK(mpi_sub_mpi(&V2, &V2, &U2));
        }
    } while (mpi_cmp_int(&TU, 0) != 0);

    while (mpi_cmp_int(&V1, 0) < 0)
        MPI_CHK(mpi_add_mpi(&V1, &V1, N));

    while (mpi_cmp_mpi(&V1, N) >= 0)
        MPI_CHK(mpi_sub_mpi(&V1, &V1, N));

    MPI_CHK(mpi_copy(X, &V1));

cleanup:
    mpi_free(&V2, &V1, &TV, &TB, &G, &U2, &U1, &TU, &TA, NULL);
    return ret;
}


static const int small_prime[] = {
    3, 5, 7, 11, 13, 17, 19, 23,
    29, 31, 37, 41, 43, 47, 53, 59,
    61, 67, 71, 73, 79, 83, 89, 97,
    101, 103, 107, 109, 113, 127, 131, 137,
    139, 149, 151, 157, 163, 167, 173, 179,
    181, 191, 193, 197, 199, 211, 223, 227,
    229, 233, 239, 241, 251, 257, 263, 269,
    271, 277, 281, 283, 293, 307, 311, 313,
    317, 331, 337, 347, 349, 353, 359, 367,
    373, 379, 383, 389, 397, 401, 409, 419,
    421, 431, 433, 439, 443, 449, 457, 461,
    463, 467, 479, 487, 491, 499, 503, 509,
    521, 523, 541, 547, 557, 563, 569, 571,
    577, 587, 593, 599, 601, 607, 613, 617,
    619, 631, 641, 643, 647, 653, 659, 661,
    673, 677, 683, 691, 701, 709, 719, 727,
    733, 739, 743, 751, 757, 761, 769, 773,
    787, 797, 809, 811, 821, 823, 827, 829,
    839, 853, 857, 859, 863, 877, 881, 883,
    887, 907, 911, 919, 929, 937, 941, 947,
    953, 967, 971, 977, 983, 991, 997, -103
};


/*
    Miller-Rabin primality test  (HAC 4.24)
 */
int mpi_is_prime(mpi *X, int (*f_rng) (void *), void *p_rng)
{
    int ret, i, j, n, s, xs;
    mpi W, R, T, A, RR;
    uchar *p;

    if (mpi_cmp_int(X, 0) == 0)
        return 0;

    mpi_init(&W, &R, &T, &A, &RR, NULL);

    xs = X->s;
    X->s = 1;

    /*
        test trivial factors first
     */
    if ((X->p[0] & 1) == 0)
        return EST_ERR_MPI_NOT_ACCEPTABLE;

    for (i = 0; small_prime[i] > 0; i++) {
        t_int r;

        if (mpi_cmp_int(X, small_prime[i]) <= 0) {
            return 0;
        }
        MPI_CHK(mpi_mod_int(&r, X, small_prime[i]));
        if (r == 0) {
            return EST_ERR_MPI_NOT_ACCEPTABLE;
        }
    }

    /*
       W = |X| - 1
       R = W >> lsb(W)
     */
    s = mpi_lsb(&W);
    MPI_CHK(mpi_sub_int(&W, X, 1));
    MPI_CHK(mpi_copy(&R, &W));
    MPI_CHK(mpi_shift_r(&R, s));

    i = mpi_msb(X);
    /*
        HAC, table 4.4
     */
    n = ((i >= 1300) ? 2 : (i >= 850) ? 3 :
         (i >= 650) ? 4 : (i >= 350) ? 8 :
         (i >= 250) ? 12 : (i >= 150) ? 18 : 27);

    for (i = 0; i < n; i++) {
        /*
            pick a random A, 1 < A < |X| - 1
         */
        MPI_CHK(mpi_grow(&A, X->n));

        p = (uchar *)A.p;
        for (j = 0; j < A.n * ciL; j++) {
            *p++ = (uchar)f_rng(p_rng);
        }
        j = mpi_msb(&A) - mpi_msb(&W);
        MPI_CHK(mpi_shift_r(&A, j + 1));
        A.p[0] |= 3;

        /*
            A = A^R mod |X|
         */
        MPI_CHK(mpi_exp_mod(&A, &A, &R, X, &RR));

        if (mpi_cmp_mpi(&A, &W) == 0 || mpi_cmp_int(&A, 1) == 0) {
            continue;
        }
        j = 1;
        while (j < s && mpi_cmp_mpi(&A, &W) != 0) {
            /*
                A = A * A mod |X|
             */
            MPI_CHK(mpi_mul_mpi(&T, &A, &A));
            MPI_CHK(mpi_mod_mpi(&A, &T, X));

            if (mpi_cmp_int(&A, 1) == 0) {
                break;
            }
            j++;
        }

        /*
            not prime if A != |X| - 1 or A == 1
         */
        if (mpi_cmp_mpi(&A, &W) != 0 || mpi_cmp_int(&A, 1) == 0) {
            ret = EST_ERR_MPI_NOT_ACCEPTABLE;
            break;
        }
    }
cleanup:
    X->s = xs;
    mpi_free(&RR, &A, &T, &R, &W, NULL);
    return ret;
}


/*
    Prime number generation
 */
int mpi_gen_prime(mpi *X, int nbits, int dh_flag, int (*f_rng) (void *), void *p_rng)
{
    int ret, k, n;
    uchar *p;
    mpi Y;

    if (nbits < 3) {
        return EST_ERR_MPI_BAD_INPUT_DATA;
    }
    mpi_init(&Y, NULL);

    n = BITS_TO_LIMBS(nbits);

    MPI_CHK(mpi_grow(X, n));
    MPI_CHK(mpi_lset(X, 0));

    p = (uchar *)X->p;
    for (k = 0; k < X->n * ciL; k++)
        *p++ = (uchar)f_rng(p_rng);

    k = mpi_msb(X);
    if (k < nbits) {
        MPI_CHK(mpi_shift_l(X, nbits - k));
    }
    if (k > nbits) {
        MPI_CHK(mpi_shift_r(X, k - nbits));
    }
    X->p[0] |= 3;

    if (dh_flag == 0) {
        while ((ret = mpi_is_prime(X, f_rng, p_rng)) != 0) {
            if (ret != EST_ERR_MPI_NOT_ACCEPTABLE)
                goto cleanup;

            MPI_CHK(mpi_add_int(X, X, 2));
        }
    } else {
        MPI_CHK(mpi_sub_int(&Y, X, 1));
        MPI_CHK(mpi_shift_r(&Y, 1));
        while (1) {
            if ((ret = mpi_is_prime(X, f_rng, p_rng)) == 0) {
                if ((ret = mpi_is_prime(&Y, f_rng, p_rng)) == 0) {
                    break;
                }
                if (ret != EST_ERR_MPI_NOT_ACCEPTABLE) {
                    goto cleanup;
                }
            }
            if (ret != EST_ERR_MPI_NOT_ACCEPTABLE) {
                goto cleanup;
            }
            MPI_CHK(mpi_add_int(&Y, X, 1));
            MPI_CHK(mpi_add_int(X, X, 2));
            MPI_CHK(mpi_shift_r(&Y, 1));
        }
    }

cleanup:
    mpi_free(&Y, NULL);
    return ret;
}

#endif /* ME_EST_GEN_PRIME */
#endif /* ME_EST_BIGNUM */

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



/********* Start of file src/camellia.c ************/


/*
    camellia.c -- Header for the Multithreaded Portable Runtime (MPR).

    The Camellia block cipher was designed by NTT and Mitsubishi Electric Corporation.
    http://info.isl.ntt.co.jp/crypt/eng/camellia/dl/01espec.pdf

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_CAMELLIA

/*
    32-bit integer manipulation macros (big endian)
 */
#ifndef GET_ULONG_BE
#define GET_ULONG_BE(n,b,i)                     \
    {                                           \
        (n) = ( (ulong) (b)[(i)    ] << 24 )    \
            | ( (ulong) (b)[(i) + 1] << 16 )    \
            | ( (ulong) (b)[(i) + 2] <<  8 )    \
            | ( (ulong) (b)[(i) + 3]       );   \
    }
#endif

#ifndef PUT_ULONG_BE
#define PUT_ULONG_BE(n,b,i)                     \
    {                                           \
        (b)[(i)    ] = (uchar) ( (n) >> 24 );   \
        (b)[(i) + 1] = (uchar) ( (n) >> 16 );   \
        (b)[(i) + 2] = (uchar) ( (n) >>  8 );   \
        (b)[(i) + 3] = (uchar) ( (n)       );   \
    }
#endif

static const uchar SIGMA_CHARS[6][8] = {
    {0xa0, 0x9e, 0x66, 0x7f, 0x3b, 0xcc, 0x90, 0x8b},
    {0xb6, 0x7a, 0xe8, 0x58, 0x4c, 0xaa, 0x73, 0xb2},
    {0xc6, 0xef, 0x37, 0x2f, 0xe9, 0x4f, 0x82, 0xbe},
    {0x54, 0xff, 0x53, 0xa5, 0xf1, 0xd3, 0x6f, 0x1c},
    {0x10, 0xe5, 0x27, 0xfa, 0xde, 0x68, 0x2d, 0x1d},
    {0xb0, 0x56, 0x88, 0xc2, 0xb3, 0xe6, 0xc1, 0xfd}
};

#define FSb CAMELLIAFSb

#ifdef EST_CAMELLIA_SMALL_MEMORY
static const uchar FSb[256] = {
    112, 130, 44, 236, 179, 39, 192, 229, 228, 133, 87, 53, 234, 12, 174,
    65,
    35, 239, 107, 147, 69, 25, 165, 33, 237, 14, 79, 78, 29, 101, 146, 189,
    134, 184, 175, 143, 124, 235, 31, 206, 62, 48, 220, 95, 94, 197, 11, 26,
    166, 225, 57, 202, 213, 71, 93, 61, 217, 1, 90, 214, 81, 86, 108, 77,
    139, 13, 154, 102, 251, 204, 176, 45, 116, 18, 43, 32, 240, 177, 132,
    153,
    223, 76, 203, 194, 52, 126, 118, 5, 109, 183, 169, 49, 209, 23, 4, 215,
    20, 88, 58, 97, 222, 27, 17, 28, 50, 15, 156, 22, 83, 24, 242, 34,
    254, 68, 207, 178, 195, 181, 122, 145, 36, 8, 232, 168, 96, 252, 105,
    80,
    170, 208, 160, 125, 161, 137, 98, 151, 84, 91, 30, 149, 224, 255, 100,
    210,
    16, 196, 0, 72, 163, 247, 117, 219, 138, 3, 230, 218, 9, 63, 221, 148,
    135, 92, 131, 2, 205, 74, 144, 51, 115, 103, 246, 243, 157, 127, 191,
    226,
    82, 155, 216, 38, 200, 55, 198, 59, 129, 150, 111, 75, 19, 190, 99, 46,
    233, 121, 167, 140, 159, 110, 188, 142, 41, 245, 249, 182, 47, 253, 180,
    89,
    120, 152, 6, 106, 231, 70, 113, 186, 212, 37, 171, 66, 136, 162, 141,
    250,
    114, 7, 185, 85, 248, 238, 172, 10, 54, 73, 42, 104, 60, 56, 241, 164,
    64, 40, 211, 123, 187, 201, 67, 193, 21, 227, 173, 244, 119, 199, 128,
    158
};

#define SBOX1(n) FSb[(n)]
#define SBOX2(n) (uchar)((FSb[(n)] >> 7 ^ FSb[(n)] << 1) & 0xff)
#define SBOX3(n) (uchar)((FSb[(n)] >> 1 ^ FSb[(n)] << 7) & 0xff)
#define SBOX4(n) FSb[((n) << 1 ^ (n) >> 7) &0xff]

#else

static const uchar FSb[256] = {
    112, 130, 44, 236, 179, 39, 192, 229, 228, 133, 87, 53, 234, 12, 174,
    65,
    35, 239, 107, 147, 69, 25, 165, 33, 237, 14, 79, 78, 29, 101, 146, 189,
    134, 184, 175, 143, 124, 235, 31, 206, 62, 48, 220, 95, 94, 197, 11, 26,
    166, 225, 57, 202, 213, 71, 93, 61, 217, 1, 90, 214, 81, 86, 108, 77,
    139, 13, 154, 102, 251, 204, 176, 45, 116, 18, 43, 32, 240, 177, 132,
    153,
    223, 76, 203, 194, 52, 126, 118, 5, 109, 183, 169, 49, 209, 23, 4, 215,
    20, 88, 58, 97, 222, 27, 17, 28, 50, 15, 156, 22, 83, 24, 242, 34,
    254, 68, 207, 178, 195, 181, 122, 145, 36, 8, 232, 168, 96, 252, 105,
    80,
    170, 208, 160, 125, 161, 137, 98, 151, 84, 91, 30, 149, 224, 255, 100,
    210,
    16, 196, 0, 72, 163, 247, 117, 219, 138, 3, 230, 218, 9, 63, 221, 148,
    135, 92, 131, 2, 205, 74, 144, 51, 115, 103, 246, 243, 157, 127, 191,
    226,
    82, 155, 216, 38, 200, 55, 198, 59, 129, 150, 111, 75, 19, 190, 99, 46,
    233, 121, 167, 140, 159, 110, 188, 142, 41, 245, 249, 182, 47, 253, 180,
    89,
    120, 152, 6, 106, 231, 70, 113, 186, 212, 37, 171, 66, 136, 162, 141,
    250,
    114, 7, 185, 85, 248, 238, 172, 10, 54, 73, 42, 104, 60, 56, 241, 164,
    64, 40, 211, 123, 187, 201, 67, 193, 21, 227, 173, 244, 119, 199, 128,
    158
};

static const uchar FSb2[256] = {
    224, 5, 88, 217, 103, 78, 129, 203, 201, 11, 174, 106, 213, 24, 93, 130,
    70, 223, 214, 39, 138, 50, 75, 66, 219, 28, 158, 156, 58, 202, 37, 123,
    13, 113, 95, 31, 248, 215, 62, 157, 124, 96, 185, 190, 188, 139, 22, 52,
    77, 195, 114, 149, 171, 142, 186, 122, 179, 2, 180, 173, 162, 172, 216,
    154,
    23, 26, 53, 204, 247, 153, 97, 90, 232, 36, 86, 64, 225, 99, 9, 51,
    191, 152, 151, 133, 104, 252, 236, 10, 218, 111, 83, 98, 163, 46, 8,
    175,
    40, 176, 116, 194, 189, 54, 34, 56, 100, 30, 57, 44, 166, 48, 229, 68,
    253, 136, 159, 101, 135, 107, 244, 35, 72, 16, 209, 81, 192, 249, 210,
    160,
    85, 161, 65, 250, 67, 19, 196, 47, 168, 182, 60, 43, 193, 255, 200, 165,
    32, 137, 0, 144, 71, 239, 234, 183, 21, 6, 205, 181, 18, 126, 187, 41,
    15, 184, 7, 4, 155, 148, 33, 102, 230, 206, 237, 231, 59, 254, 127, 197,
    164, 55, 177, 76, 145, 110, 141, 118, 3, 45, 222, 150, 38, 125, 198, 92,
    211, 242, 79, 25, 63, 220, 121, 29, 82, 235, 243, 109, 94, 251, 105,
    178,
    240, 49, 12, 212, 207, 140, 226, 117, 169, 74, 87, 132, 17, 69, 27, 245,
    228, 14, 115, 170, 241, 221, 89, 20, 108, 146, 84, 208, 120, 112, 227,
    73,
    128, 80, 167, 246, 119, 147, 134, 131, 42, 199, 91, 233, 238, 143, 1, 61
};

static const uchar FSb3[256] = {
    56, 65, 22, 118, 217, 147, 96, 242, 114, 194, 171, 154, 117, 6, 87, 160,
    145, 247, 181, 201, 162, 140, 210, 144, 246, 7, 167, 39, 142, 178, 73,
    222,
    67, 92, 215, 199, 62, 245, 143, 103, 31, 24, 110, 175, 47, 226, 133, 13,
    83, 240, 156, 101, 234, 163, 174, 158, 236, 128, 45, 107, 168, 43, 54,
    166,
    197, 134, 77, 51, 253, 102, 88, 150, 58, 9, 149, 16, 120, 216, 66, 204,
    239, 38, 229, 97, 26, 63, 59, 130, 182, 219, 212, 152, 232, 139, 2, 235,
    10, 44, 29, 176, 111, 141, 136, 14, 25, 135, 78, 11, 169, 12, 121, 17,
    127, 34, 231, 89, 225, 218, 61, 200, 18, 4, 116, 84, 48, 126, 180, 40,
    85, 104, 80, 190, 208, 196, 49, 203, 42, 173, 15, 202, 112, 255, 50,
    105,
    8, 98, 0, 36, 209, 251, 186, 237, 69, 129, 115, 109, 132, 159, 238, 74,
    195, 46, 193, 1, 230, 37, 72, 153, 185, 179, 123, 249, 206, 191, 223,
    113,
    41, 205, 108, 19, 100, 155, 99, 157, 192, 75, 183, 165, 137, 95, 177,
    23,
    244, 188, 211, 70, 207, 55, 94, 71, 148, 250, 252, 91, 151, 254, 90,
    172,
    60, 76, 3, 53, 243, 35, 184, 93, 106, 146, 213, 33, 68, 81, 198, 125,
    57, 131, 220, 170, 124, 119, 86, 5, 27, 164, 21, 52, 30, 28, 248, 82,
    32, 20, 233, 189, 221, 228, 161, 224, 138, 241, 214, 122, 187, 227, 64,
    79
};

static const uchar FSb4[256] = {
    112, 44, 179, 192, 228, 87, 234, 174, 35, 107, 69, 165, 237, 79, 29,
    146,
    134, 175, 124, 31, 62, 220, 94, 11, 166, 57, 213, 93, 217, 90, 81, 108,
    139, 154, 251, 176, 116, 43, 240, 132, 223, 203, 52, 118, 109, 169, 209,
    4,
    20, 58, 222, 17, 50, 156, 83, 242, 254, 207, 195, 122, 36, 232, 96, 105,
    170, 160, 161, 98, 84, 30, 224, 100, 16, 0, 163, 117, 138, 230, 9, 221,
    135, 131, 205, 144, 115, 246, 157, 191, 82, 216, 200, 198, 129, 111, 19,
    99,
    233, 167, 159, 188, 41, 249, 47, 180, 120, 6, 231, 113, 212, 171, 136,
    141,
    114, 185, 248, 172, 54, 42, 60, 241, 64, 211, 187, 67, 21, 173, 119,
    128,
    130, 236, 39, 229, 133, 53, 12, 65, 239, 147, 25, 33, 14, 78, 101, 189,
    184, 143, 235, 206, 48, 95, 197, 26, 225, 202, 71, 61, 1, 214, 86, 77,
    13, 102, 204, 45, 18, 32, 177, 153, 76, 194, 126, 5, 183, 49, 23, 215,
    88, 97, 27, 28, 15, 22, 24, 34, 68, 178, 181, 145, 8, 168, 252, 80,
    208, 125, 137, 151, 91, 149, 255, 210, 196, 72, 247, 219, 3, 218, 63,
    148,
    92, 2, 74, 51, 103, 243, 127, 226, 155, 38, 55, 59, 150, 75, 190, 46,
    121, 140, 110, 142, 245, 182, 253, 89, 152, 106, 70, 186, 37, 66, 162,
    250,
    7, 85, 238, 10, 73, 104, 56, 164, 40, 123, 201, 193, 227, 244, 199, 158
};

#define SBOX1(n) FSb[(n)]
#define SBOX2(n) FSb2[(n)]
#define SBOX3(n) FSb3[(n)]
#define SBOX4(n) FSb4[(n)]
#endif

static const uchar shifts[2][4][4] = {
    {
     {1, 1, 1, 1},      /* KL */
     {0, 0, 0, 0},      /* KR */
     {1, 1, 1, 1},      /* KA */
     {0, 0, 0, 0}       /* KB */
     },
    {
     {1, 0, 1, 1},      /* KL */
     {1, 1, 0, 1},      /* KR */
     {1, 1, 1, 0},      /* KA */
     {1, 1, 0, 1}       /* KB */
     }
};

static cchar indexes[2][4][20] = {
    {
     {
      0, 1, 2, 3, 8, 9, 10, 11, 38, 39,
      36, 37, 23, 20, 21, 22, 27, -1, -1, 26},  /* KL -> RK */
     {
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},  /* KR -> RK */
     {
      4, 5, 6, 7, 12, 13, 14, 15, 16, 17,
      18, 19, -1, 24, 25, -1, 31, 28, 29, 30},  /* KA -> RK */
     {
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}   /* KB -> RK */
     },
    {
     {
      0, 1, 2, 3, 61, 62, 63, 60, -1, -1,
      -1, -1, 27, 24, 25, 26, 35, 32, 33, 34},  /* KL -> RK */
     {
      -1, -1, -1, -1, 8, 9, 10, 11, 16, 17,
      18, 19, -1, -1, -1, -1, 39, 36, 37, 38},  /* KR -> RK */
     {
      -1, -1, -1, -1, 12, 13, 14, 15, 58, 59,
      56, 57, 31, 28, 29, 30, -1, -1, -1, -1},  /* KA -> RK */
     {
      4, 5, 6, 7, 65, 66, 67, 64, 20, 21,
      22, 23, -1, -1, -1, -1, 43, 40, 41, 42}   /* KB -> RK */
     }
};

static cchar transposes[2][20] = {
    {
     21, 22, 23, 20,
     -1, -1, -1, -1,
     18, 19, 16, 17,
     11, 8, 9, 10,
     15, 12, 13, 14},
    {
     25, 26, 27, 24,
     29, 30, 31, 28,
     18, 19, 16, 17,
     -1, -1, -1, -1,
     -1, -1, -1, -1}
};

/* Shift macro for 128 bit strings with rotation smaller than 32 bits (!) */
#define ROTL(DEST, SRC, SHIFT)                                          \
    {                                                                   \
        (DEST)[0] = (SRC)[0] << (SHIFT) ^ (SRC)[1] >> (32 - (SHIFT));   \
        (DEST)[1] = (SRC)[1] << (SHIFT) ^ (SRC)[2] >> (32 - (SHIFT));   \
        (DEST)[2] = (SRC)[2] << (SHIFT) ^ (SRC)[3] >> (32 - (SHIFT));   \
        (DEST)[3] = (SRC)[3] << (SHIFT) ^ (SRC)[0] >> (32 - (SHIFT));   \
    }

#define FL(XL, XR, KL, KR)                                              \
    {                                                                   \
        (XR) = ((((XL) & (KL)) << 1) | (((XL) & (KL)) >> 31)) ^ (XR);   \
        (XL) = ((XR) | (KR)) ^ (XL);                                    \
    }

#define FLInv(YL, YR, KL, KR)                                           \
    {                                                                   \
        (YL) = ((YR) | (KR)) ^ (YL);                                    \
        (YR) = ((((YL) & (KL)) << 1) | (((YL) & (KL)) >> 31)) ^ (YR);   \
    }

#define SHIFT_AND_PLACE(INDEX, OFFSET)                              \
    {                                                               \
        TK[0] = KC[(OFFSET) * 4 + 0];                               \
        TK[1] = KC[(OFFSET) * 4 + 1];                               \
        TK[2] = KC[(OFFSET) * 4 + 2];                               \
        TK[3] = KC[(OFFSET) * 4 + 3];                               \
                                                                    \
        for ( i = 1; i <= 4; i++ )                                  \
            if (shifts[(INDEX)][(OFFSET)][i -1])                    \
                ROTL(TK + i * 4, TK, (15 * i) % 32);                \
                                                                    \
        for ( i = 0; i < 20; i++ )                                  \
            if (indexes[(INDEX)][(OFFSET)][i] != -1) {              \
                RK[(int)indexes[(INDEX)][(OFFSET)][i]] = TK[ i ];   \
            }                                                       \
    }

void camellia_feistel(ulong x[2], ulong k[2],
              ulong z[2])
{
    ulong I0, I1;
    I0 = x[0] ^ k[0];
    I1 = x[1] ^ k[1];

    I0 = (SBOX1((I0 >> 24) & 0xFF) << 24) |
        (SBOX2((I0 >> 16) & 0xFF) << 16) |
        (SBOX3((I0 >> 8) & 0xFF) << 8) | (SBOX4((I0) & 0xFF));
    I1 = (SBOX2((I1 >> 24) & 0xFF) << 24) |
        (SBOX3((I1 >> 16) & 0xFF) << 16) |
        (SBOX4((I1 >> 8) & 0xFF) << 8) | (SBOX1((I1) & 0xFF));

    I0 ^= (I1 << 8) | (I1 >> 24);
    I1 ^= (I0 << 16) | (I0 >> 16);
    I0 ^= (I1 >> 8) | (I1 << 24);
    I1 ^= (I0 >> 8) | (I0 << 24);

    z[0] ^= I1;
    z[1] ^= I0;
}

/*
 * Camellia key schedule (encryption)
 */
void camellia_setkey_enc(camellia_context *ctx, uchar *key, int keysize)
{
    int i, idx;
    ulong *RK;
    uchar t[64];

    RK = ctx->rk;

    memset(t, 0, 64);
    memset(RK, 0, sizeof(ctx->rk));

    switch (keysize) {
    case 128:
        ctx->nr = 3;
        idx = 0;
        break;
    case 192:
    case 256:
        ctx->nr = 4;
        idx = 1;
        break;
    default:
        return;
    }

    for (i = 0; i < keysize / 8; ++i)
        t[i] = key[i];

    if (keysize == 192) {
        for (i = 0; i < 8; i++)
            t[24 + i] = ~t[16 + i];
    }

    /*
       Prepare SIGMA values
     */
    ulong SIGMA[6][2];
    for (i = 0; i < 6; i++) {
        GET_ULONG_BE(SIGMA[i][0], SIGMA_CHARS[i], 0);
        GET_ULONG_BE(SIGMA[i][1], SIGMA_CHARS[i], 4);
    }

    /*
       Key storage in KC
       Order: KL, KR, KA, KB
     */
    ulong KC[16];
    memset(KC, 0, sizeof(KC));

    /* Store KL, KR */
    for (i = 0; i < 8; i++)
        GET_ULONG_BE(KC[i], t, i * 4);

    /* Generate KA */
    for (i = 0; i < 4; ++i)
        KC[8 + i] = KC[i] ^ KC[4 + i];

    camellia_feistel(KC + 8, SIGMA[0], KC + 10);
    camellia_feistel(KC + 10, SIGMA[1], KC + 8);

    for (i = 0; i < 4; ++i)
        KC[8 + i] ^= KC[i];

    camellia_feistel(KC + 8, SIGMA[2], KC + 10);
    camellia_feistel(KC + 10, SIGMA[3], KC + 8);

    if (keysize > 128) {
        /* Generate KB */
        for (i = 0; i < 4; ++i)
            KC[12 + i] = KC[4 + i] ^ KC[8 + i];

        camellia_feistel(KC + 12, SIGMA[4], KC + 14);
        camellia_feistel(KC + 14, SIGMA[5], KC + 12);
    }

    /*
       Generating subkeys
     */
    ulong TK[20];

    /* Manipulating KL */
    SHIFT_AND_PLACE(idx, 0);

    /* Manipulating KR */
    if (keysize > 128) {
        SHIFT_AND_PLACE(idx, 1);
    }

    /* Manipulating KA */
    SHIFT_AND_PLACE(idx, 2);

    /* Manipulating KB */
    if (keysize > 128) {
        SHIFT_AND_PLACE(idx, 3);
    }

    /* Do transpositions */
    for (i = 0; i < 20; i++) {
        if (transposes[idx][i] != -1) {
            RK[32 + 12 * idx + i] = RK[(int)transposes[idx][i]];
        }
    }
}


/*
   Camellia key schedule (decryption)
 */
void camellia_setkey_dec(camellia_context *ctx, uchar *key, int keysize)
{
    int i, idx;
    camellia_context cty;
    ulong *RK;
    ulong *SK;

    switch (keysize) {
    case 128:
        ctx->nr = 3;
        idx = 0;
        break;
    case 192:
    case 256:
        ctx->nr = 4;
        idx = 1;
        break;
    default:
        return;
    }

    RK = ctx->rk;
    camellia_setkey_enc(&cty, key, keysize);
    SK = cty.rk + 24 * 2 + 8 * idx * 2;

    *RK++ = *SK++;
    *RK++ = *SK++;
    *RK++ = *SK++;
    *RK++ = *SK++;

    for (i = 22 + 8 * idx, SK -= 6; i > 0; i--, SK -= 4) {
        *RK++ = *SK++;
        *RK++ = *SK++;
    }
    SK -= 2;
    *RK++ = *SK++;
    *RK++ = *SK++;
    *RK++ = *SK++;
    *RK++ = *SK++;
    memset(&cty, 0, sizeof(camellia_context));
}


/*
   Camellia-ECB block encryption/decryption
 */
void camellia_crypt_ecb(camellia_context *ctx, int mode, uchar input[16], uchar output[16])
{
    int NR;
    ulong *RK, X[4];

    NR = ctx->nr;
    RK = ctx->rk;

    GET_ULONG_BE(X[0], input, 0);
    GET_ULONG_BE(X[1], input, 4);
    GET_ULONG_BE(X[2], input, 8);
    GET_ULONG_BE(X[3], input, 12);

    X[0] ^= *RK++;
    X[1] ^= *RK++;
    X[2] ^= *RK++;
    X[3] ^= *RK++;

    while (NR) {
        --NR;
        camellia_feistel(X, RK, X + 2);
        RK += 2;
        camellia_feistel(X + 2, RK, X);
        RK += 2;
        camellia_feistel(X, RK, X + 2);
        RK += 2;
        camellia_feistel(X + 2, RK, X);
        RK += 2;
        camellia_feistel(X, RK, X + 2);
        RK += 2;
        camellia_feistel(X + 2, RK, X);
        RK += 2;

        if (NR) {
            FL(X[0], X[1], RK[0], RK[1]);
            RK += 2;
            FLInv(X[2], X[3], RK[0], RK[1]);
            RK += 2;
        }
    }
    X[2] ^= *RK++;
    X[3] ^= *RK++;
    X[0] ^= *RK++;
    X[1] ^= *RK++;

    PUT_ULONG_BE(X[2], output, 0);
    PUT_ULONG_BE(X[3], output, 4);
    PUT_ULONG_BE(X[0], output, 8);
    PUT_ULONG_BE(X[1], output, 12);
}


/*
   Camellia-CBC buffer encryption/decryption
 */
void camellia_crypt_cbc(camellia_context *ctx, int mode, int length, uchar iv[16], uchar *input, uchar *output)
{
    int i;
    uchar temp[16];

    if (mode == CAMELLIA_DECRYPT) {
        while (length > 0) {
            memcpy(temp, input, 16);
            camellia_crypt_ecb(ctx, mode, input, output);

            for (i = 0; i < 16; i++) {
                output[i] = (uchar)(output[i] ^ iv[i]);
            }
            memcpy(iv, temp, 16);
            input += 16;
            output += 16;
            length -= 16;
        }
    } else {
        while (length > 0) {
            for (i = 0; i < 16; i++) {
                output[i] = (uchar)(input[i] ^ iv[i]);
            }
            camellia_crypt_ecb(ctx, mode, output, output);
            memcpy(iv, output, 16);
            input += 16;
            output += 16;
            length -= 16;
        }
    }
}


/*
   Camellia-CFB128 buffer encryption/decryption
 */
void camellia_crypt_cfb128(camellia_context *ctx, int mode, int length, int *iv_off, uchar iv[16], uchar *input, 
        uchar *output)
{
    int c, n = *iv_off;

    if (mode == CAMELLIA_DECRYPT) {
        while (length--) {
            if (n == 0) {
                camellia_crypt_ecb(ctx, CAMELLIA_ENCRYPT, iv, iv);
            }
            c = *input++;
            *output++ = (uchar)(c ^ iv[n]);
            iv[n] = (uchar)c;
            n = (n + 1) & 0x0F;
        }
    } else {
        while (length--) {
            if (n == 0) {
                camellia_crypt_ecb(ctx, CAMELLIA_ENCRYPT, iv, iv);
            }
            iv[n] = *output++ = (uchar)(iv[n] ^ *input++);
            n = (n + 1) & 0x0F;
        }
    }

    *iv_off = n;
}


#undef FSb
#undef SWAP
#undef P
#undef R
#undef S0
#undef S1
#undef S2
#undef S3

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



/********* Start of file src/certs.c ************/


/*
    certs.c -- X.509 test certificates

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_TEST_CERTS

char test_ca_crt[] =
    "-----BEGIN CERTIFICATE-----\r\n"
    "MIIDpTCCAo2gAwIBAgIBADANBgkqhkiG9w0BAQUFADBFMQswCQYDVQQGEwJGUjEO\r\n"
    "MAwGA1UEBxMFUGFyaXMxDjAMBgNVBAoTBVh5U1NMMRYwFAYDVQQDEw1YeVNTTCBU\r\n"
    "ZXN0IENBMB4XDTA3MDcwNzA1MDAxOFoXDTE3MDcwNzA1MDAxOFowRTELMAkGA1UE\r\n"
    "BhMCRlIxDjAMBgNVBAcTBVBhcmlzMQ4wDAYDVQQKEwVYeVNTTDEWMBQGA1UEAxMN\r\n"
    "WHlTU0wgVGVzdCBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAM+k\r\n"
    "gt70fIPiYqmvXr+9uPmWoN405eoSzxdiRLLCHqL4V/Ts0E/H+JNfHS4DHlAgxrJu\r\n"
    "+ZIadvSJuHkI6e1iMkAh5SU1DqaF3jrrFdJCooM6a077M4CRkE1tdAeZDf+BYp0q\r\n"
    "BeMU9Y2+j7ibsQaPAizunbXLf4QdteExCwhYRJ8OVXSEaSNt339gJzTD6kOhES3b\r\n"
    "lEN3qbx6lqFaJ5MLHTix5uNVc2rvbOizV5oLhJNqm52AKOp11tv6WTiI8loagvAc\r\n"
    "jlhEZRNb9el5SL6Jai/uFcqXKzfXNKW3FYPQHFGobmiMfGt1lUSBJ3F2mrqEC7gC\r\n"
    "wHy/FDvAI64/k5LZAFkCAwEAAaOBnzCBnDAMBgNVHRMEBTADAQH/MB0GA1UdDgQW\r\n"
    "BBS87h+Y6Porg+SkfV7DdXKTMdkyZzBtBgNVHSMEZjBkgBS87h+Y6Porg+SkfV7D\r\n"
    "dXKTMdkyZ6FJpEcwRTELMAkGA1UEBhMCRlIxDjAMBgNVBAcTBVBhcmlzMQ4wDAYD\r\n"
    "VQQKEwVYeVNTTDEWMBQGA1UEAxMNWHlTU0wgVGVzdCBDQYIBADANBgkqhkiG9w0B\r\n"
    "AQUFAAOCAQEAIHdoh0NCg6KAAhwDSmfEgSbKUI8/Zr/d56uw42HO0sj/uKPQzUco\r\n"
    "3Mx2BYE1m1itg7q5OhrkB7J4ZB78EtNZM84nV+y6od3YpR0Z9VUxCx7948MozYRy\r\n"
    "TKF5x/lKHRx1PJKfEO4clKdWTFAtWtGhewXrHJQ8C+ENh2Up2wTVh3Z+pEzuZNv3\r\n"
    "u/JYu1H+vkt3l1WCy/9mxUnu+anW1DzxPWnjy4lx6Mi0BD2qfKBWLjVS+7v6ALcj\r\n"
    "S2oRWWr4LUvXT7z9BBAvw2eJQD+a4uAya6EURG7AsAvr5MnWn/r0wLWmBJ6fB1Yp\r\n"
    "F1kOmamOFvstLMf74rLX+LGKeJ/nwuI5FQ==\r\n" "-----END CERTIFICATE-----\r\n";

char test_ca_key[] =
    "-----BEGIN RSA PRIVATE KEY-----\r\n"
    "Proc-Type: 4,ENCRYPTED\r\n"
    "DEK-Info: DES-EDE3-CBC,7BDC280BA4C2F45C\r\n"
    "\r\n"
    "KI6rmwY0ChjF/0/Q7d1vE2jojZQTuF8j0wJUOf02PUUD4en8/FCLj69Pf3/lvOlu\r\n"
    "F8hU4IuBHxN2+feuXp6xTmGd/VyKdWh4+NGtKr2V1gXfk2wqHk3/P/YuF7QhOQjZ\r\n"
    "BlNF7n5o76Nufr3iwBbtIABCGQfNRUwGzrvmFXLIrSqns8ppv83qaD5jHVQQj5MM\r\n"
    "cGIoidwmNORdVmBCVZ17dQ+lMPfqjhN27GBhzXbp9a2EP3w+IqrXOvcl4DqSY+DU\r\n"
    "PIhS6KuQZ4iek4dI93wmw2D8Q67omcKYOX/BjCkZ/v6oq481qOej6MicIY/L9LxH\r\n"
    "r91jqIYLJC+1w20YavB3kIkSe5zys30RzhPTOtG60j8kgiQ4Fh+L/nKBP5noOXGE\r\n"
    "uzDBa051HCEYfufOVkXr6wBCFo9boqM1p/GI0Ze5gCsYY+Vzyu96gu+OGv55Jtyo\r\n"
    "fY2yEwVKhfqNy+xJ+8dwf8dcc5vQLatXebDynuSPI2jbaos6MJT6n0LoY2TpAz47\r\n"
    "vNE7UtEEdgZPPVwE4xO+dVa0kCqJxuG8b+ZZTHZidlQ6MBiebL4vXbeIFieQRzUM\r\n"
    "06IQ7YqsiBVLYxicMlApdFpJ4QM2fFKlZRo+fHg8EN9HEbRpgxIf4IwAAFjOgsiO\r\n"
    "ll7OmyzSF12NUIrRsIo02E4Y8X6xSFeYnOIiqZqdxG4xz/DZKoX6Js8WdYbtzrDv\r\n"
    "7gYYEZy1cuR9PJzMDKXoLz/mjBqDsh11Vgm1nAHbhBqFaWSuueH/LV2rgRijUBKh\r\n"
    "yMTnGXrz56DeJ85bYLjVEOM3xhvjsz6Fq5xLqXRTAzyQn7QuqRg2szLtCnN8olCh\r\n"
    "rdS1Gd7EV5g24WnF9gTzYJ4lwO9oxOsPKKCD1Z77B+lbZLoxTu7+rakTtqLikWMv\r\n"
    "7OILfR4iaIu1nKxhhXpwnze3u+1OWcuk0UnBjSnF519tlrgV3dmA0P2LHd6h6xru\r\n"
    "vIgUFHbigCV0i55peEPxtcy9JGLGJ3WvHA3NGSsqkkB/ymdfEaMmEH7403UKqQqq\r\n"
    "+K9Z1cYmeZlfoXClmdSjsxkYeN5raB0kOOSV/MEOySG+lztyLUGB4n46AZG2PgXN\r\n"
    "A9g9tv2gqc51Vl+55qwTg5429DigjiByRkcmBU3A1aF8yzD1QerwobPmqeNZ0mWv\r\n"
    "SllAvgVs2uy2Wf9/5gEWm+HjnJwS9VPNTocG/r+4BnDK8XG0Jy4oI+jSPxj/Y8Jt\r\n"
    "0ejFPM5A7HGngiaPFYHxcwPwo4Aa4HZnk+keFrA+vF9eDd1IOj195a9TESL+IGt1\r\n"
    "nuNmmuYjyJqM9Uvi1Mutv0UQ6Fl6yv4XxaUtMZKl4LtrAaMdW1T0PEgUG0tjSIxX\r\n"
    "hBd1W+Ob4nmK29aa4iuXaOxeAA3QK7KCZo18CJFgnp1w97qohwlKf+4FuNXHL64Q\r\n"
    "FgmpycV9nfP8G9aUYKAxs1+xutNv/B6opHmfLNL6d0cwi8dvGsrDdGlcyi8Dk/PF\r\n"
    "GKSqlQTF5V7l4UOanefB51tuziEBY+LWcXP4XgqNGPQknPF90NnbH1EglQG2paqb\r\n"
    "5bLyT8G6kfCSY4uHxs9lPuvfOjk9ptjy2FwfyBb3Sl4K+IFEE8XTNuNErh83AKh2\r\n"
    "-----END RSA PRIVATE KEY-----\r\n";

char test_ca_pwd[] = "test";

char test_srv_crt[] =
    "-----BEGIN CERTIFICATE-----\r\n"
    "MIIDPjCCAiagAwIBAgIBAjANBgkqhkiG9w0BAQUFADBFMQswCQYDVQQGEwJGUjEO\r\n"
    "MAwGA1UEBxMFUGFyaXMxDjAMBgNVBAoTBVh5U1NMMRYwFAYDVQQDEw1YeVNTTCBU\r\n"
    "ZXN0IENBMB4XDTA3MDcwNzA1MDEyOVoXDTA4MDcwNjA1MDEyOVowMTELMAkGA1UE\r\n"
    "BhMCRlIxDjAMBgNVBAoTBVh5U1NMMRIwEAYDVQQDEwlsb2NhbGhvc3QwggEiMA0G\r\n"
    "CSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQC40PDcGTgmHkt6noXDfkjVuymjiNYB\r\n"
    "gjtiL7uA1Ke3tXStacEecQek/OJxYqYr7ffcWalS29LL6HbKpi0xLZKBbD9ACkDh\r\n"
    "1Z/SvHlyQPILJdYb9DMw+kzZds5myXUjzn7Aem1YjoxMZUAMyc34i2900X2pL0v2\r\n"
    "SfCeJ9Ym4MOnZxYl217+dX9ZbkgIgrT6uY2IYK4boDwxbTcyT8i/NPsVsiMwtWPM\r\n"
    "rnQMr+XbgS98sUzcZE70Pe1TlV9Iy8j/8d2OiFo+qTyMu/6UpM2s3gdkQkMzx+Sm\r\n"
    "4QitRUjzmEXeUePRUjEgHIv7vz069xuVBzrks36w5BXiVAhLke/OTKVPAgMBAAGj\r\n"
    "TTBLMAkGA1UdEwQCMAAwHQYDVR0OBBYEFNkOyCTx64SDdPySGWl/tzD7/WMSMB8G\r\n"
    "A1UdIwQYMBaAFLzuH5jo+iuD5KR9XsN1cpMx2TJnMA0GCSqGSIb3DQEBBQUAA4IB\r\n"
    "AQBelJv5t+suaqy5Lo5bjNeHjNZfgg8EigDQ7NqaosvlQZAsh2N34Gg5YdkGyVdg\r\n"
    "s32I/K5aaywyUbG9qVXQxCM2T95qBqyK56h9yJoZKWQD9H//+zB8kCK/16WvRfv3\r\n"
    "VA7eSR19qOFWlHe+1qGh2YhxeDUfyi+fm4D36dGxqC2A34tZjo0QPHKtIeqM0kJy\r\n"
    "zzL65TlbJQKkyTuRHofFv0jW9ZFG2wkGysVgCY5fjuLI1do/sWUaXd2987iNFa+K\r\n"
    "FrHsTi6urSfZuGlZNxDXDHEE7Q2snAvvev+KR7DD9X4DJGcPX9gA4CGJj+9ZzyAA\r\n"
    "ZTGpOzk1hIH44RFs2lJMZRlE\r\n" "-----END CERTIFICATE-----\r\n";

char test_srv_key[] =
    "-----BEGIN RSA PRIVATE KEY-----\r\n"
    "MIIEowIBAAKCAQEAuNDw3Bk4Jh5Lep6Fw35I1bspo4jWAYI7Yi+7gNSnt7V0rWnB\r\n"
    "HnEHpPzicWKmK+333FmpUtvSy+h2yqYtMS2SgWw/QApA4dWf0rx5ckDyCyXWG/Qz\r\n"
    "MPpM2XbOZsl1I85+wHptWI6MTGVADMnN+ItvdNF9qS9L9knwnifWJuDDp2cWJdte\r\n"
    "/nV/WW5ICIK0+rmNiGCuG6A8MW03Mk/IvzT7FbIjMLVjzK50DK/l24EvfLFM3GRO\r\n"
    "9D3tU5VfSMvI//HdjohaPqk8jLv+lKTNrN4HZEJDM8fkpuEIrUVI85hF3lHj0VIx\r\n"
    "IByL+789OvcblQc65LN+sOQV4lQIS5HvzkylTwIDAQABAoIBABeah8h0aBlmMRmd\r\n"
    "+VN4Y3D4kF7UcRCMQ21Mz1Oq1Si/QgGLyiBLK0DFE16LzNE7eTZpNRjh/lAQhmtn\r\n"
    "QcpQGa/x1TomlRbCo8DUVWZkKQWHdYroa0lMDliPtdimzhEepE2M1T5EJmLzY3S+\r\n"
    "qVGe7UMsJjJfWgJAezyXteANQK+2YSt+CjPIqIHch1KexUnvdN9++1oEx6AbuZ8T\r\n"
    "4avhFYZQP15tZNGsk2LfQlYS/NfbowkCsd0/TVubJBmDGUML/E5MbxjxLzlaNB2M\r\n"
    "V59cBNgsgA35CODAUF4xOyoSfZGqG1Rb9qQrv1E6Jz56dG8SsKF3HqnDjxiPOVBN\r\n"
    "FBnVJ+ECgYEA29MhAsKMm4XqBUKp6pIMFTgm/s1E5vxig70vqIiL+guvBhhQ7zs1\r\n"
    "8UMTNXZoMELNoB/ev9fN0Cjc1Vr46b/x/yDw7wMb96i+vzENOzu4RHWi3OWpCPbp\r\n"
    "qBKEi3hzN8M+BulPX8CDQx3aLRrfxw51J5EuA0NeybngbItgxTi0u6kCgYEA1zr0\r\n"
    "6P5YdOhYHtSWDlkeD49MApcVuzaHnsHZVAhUqu3Rwiy9LRaJLZfr7fQDb9DYJbZp\r\n"
    "sxTRLG6LSAcsR7mw+m+GvNqGt/9pSqbtW+L/VwVWSyF+YYklxZUD3UAAyrDVcDEC\r\n"
    "a5S+jad4Csi/lVHt5ulWIckWL1fJvadn5ubKNDcCgYA+71xVGPP+lsFgTiytfrC8\r\n"
    "5n2rl4MxinJ9+w0I+EbzCKNMYGvTgiU4dJasSMEdiBKs1FMGo7dF8F0BLHF1IsIa\r\n"
    "5Ah2tXItXn9154o9OiTQXMmK6qmRaneM6fhOoeaCwYAhpGxYIpqx/Xr4TOhiag46\r\n"
    "jMMaphAeOvw4t1K2RDziOQKBgQCyPCCU0gxuw/o1jda2CxbZy9EmU/erEX09+0n+\r\n"
    "TOfQpSEPq/z9WaxAFY9LfsdZ0ZktoeHma1bNdL3i6A3DWAM3YSQzQMRPmzOWnqXx\r\n"
    "cgoCBmlvzkzaeLjO5phMoLQHJmmafvuCG6uxov3F8Hi3LyHUF2c8k0nL6ucmJ3vj\r\n"
    "uzu4AQKBgBSASMAJS63M9UJB1Eazy2v2NWw04CmzNxUfWrHuKpd/C2ik4QKu0sRO\r\n"
    "r9KnkDgxxEhjDm7lXhlW12PU42yORst5I3Eaa1Cfi4KPFn/ozt+iNBYrzd8Tyvnb\r\n"
    "qkdECl0+G2Fo/ER4NRCv7a24WNEsOMGzGRqw5cnSJrjbZLYMaIyK\r\n"
    "-----END RSA PRIVATE KEY-----\r\n";

char test_cli_crt[] =
    "-----BEGIN CERTIFICATE-----\r\n"
    "MIIDPTCCAiWgAwIBAgIBATANBgkqhkiG9w0BAQUFADBFMQswCQYDVQQGEwJGUjEO\r\n"
    "MAwGA1UEBxMFUGFyaXMxDjAMBgNVBAoTBVh5U1NMMRYwFAYDVQQDEw1YeVNTTCBU\r\n"
    "ZXN0IENBMB4XDTA3MDcwNzA1MDEyMFoXDTA4MDcwNjA1MDEyMFowMDELMAkGA1UE\r\n"
    "BhMCRlIxDjAMBgNVBAoTBVh5U1NMMREwDwYDVQQDEwhKb2UgVXNlcjCCASIwDQYJ\r\n"
    "KoZIhvcNAQEBBQADggEPADCCAQoCggEBAKZkg8ANl6kGmLGqKc6KUHb9IsZwb2+K\r\n"
    "jBw83Qb0KuvPVnu3MzEcXFvOZ83g0PL/z8ob5PKr8HP6bVYzhsD65imcCDEEVPk2\r\n"
    "9r0XGTggGjB601Fd8aTShUWE4NLrKw6YNXTXgTNdvhHNxXwqmdNVLkmZjj3ZwYUc\r\n"
    "cEE8eE5jHs8cMDXJLMCwgKIM7Sax22OhSHQHKwifVO4/Fdw5G+Suys8PhMX2jDXM\r\n"
    "ICFwq8ld+bZGoNUtgp48FWhAMfJyTEaHh9LC46KkqGSDRIzx7/4cPB6QqrpzJN0o\r\n"
    "Kr8kH7vdRDTFDmO23D4C5l0Bw/2aC76DhEJpB2bGA4iIszJs+F/PIL8CAwEAAaNN\r\n"
    "MEswCQYDVR0TBAIwADAdBgNVHQ4EFgQUiWX1IvjRdYGt0zz5Sq16x01k0o4wHwYD\r\n"
    "VR0jBBgwFoAUvO4fmOj6K4PkpH1ew3VykzHZMmcwDQYJKoZIhvcNAQEFBQADggEB\r\n"
    "AGdqD7VThJmC+oeeMUHk2TQX2wZNU+GsC+RLjtlenckny95KnljGvMtCznyLkS5D\r\n"
    "fAjLKfR1No8pk5gRdscqgyIuQx5WnHNv4QBZmMsmvDICxzRQaxuPFHbS4aLXldeL\r\n"
    "yOWm5Z4qkMHpCKvA86blYsEkksGDV47fF9ZkOQ8nkh7Z4eY4/5TwqTY72ww5g4NL\r\n"
    "6DZtWpcpGbX99NRaNVzcq9D+ElxkgHnH4YWafOKBclSgqrutbRLi2uZx/QpvuF+i\r\n"
    "sUbe+HFPMWwU5lBv/oOhQkz0VD+HusYtXWS2lG88cT40aNly2CkYUugdTR/b9Uea\r\n"
    "p/i862sL/lO40qlQ0xV5N7U=\r\n" "-----END CERTIFICATE-----\r\n";

char test_cli_key[] =
    "-----BEGIN RSA PRIVATE KEY-----\r\n"
    "MIIEowIBAAKCAQEApmSDwA2XqQaYsaopzopQdv0ixnBvb4qMHDzdBvQq689We7cz\r\n"
    "MRxcW85nzeDQ8v/Pyhvk8qvwc/ptVjOGwPrmKZwIMQRU+Tb2vRcZOCAaMHrTUV3x\r\n"
    "pNKFRYTg0usrDpg1dNeBM12+Ec3FfCqZ01UuSZmOPdnBhRxwQTx4TmMezxwwNcks\r\n"
    "wLCAogztJrHbY6FIdAcrCJ9U7j8V3Dkb5K7Kzw+ExfaMNcwgIXCryV35tkag1S2C\r\n"
    "njwVaEAx8nJMRoeH0sLjoqSoZINEjPHv/hw8HpCqunMk3SgqvyQfu91ENMUOY7bc\r\n"
    "PgLmXQHD/ZoLvoOEQmkHZsYDiIizMmz4X88gvwIDAQABAoIBAE0nBkAjDVN+j4ax\r\n"
    "1DjEwZKqxVkmAUXDBDyDrCjxRoWY2gz7YW1ALUMUbeV0fO5v1zVrwbkUKKZeVBxI\r\n"
    "QA9zRw28H8A6tfvolHgRIcx4dixMh3ePC+DVDJ6zglvKV2ipAwBufKYIrX0r4Io2\r\n"
    "ZqUrNg9CeEYNlkHWceaN12rhYwO82pgHxnB1p5pI42pY7lzyLgSddf5n+M5UBOJI\r\n"
    "gsNCkvbGdv7WQPVFTRDiRgEnCJ3rI8oPSK6MOUWJw3rh2hbkx+ex8NPvEKbzEXiU\r\n"
    "p5j1AlbHIWP5sYBbA1YviFtryAV4fyfLcWPfoqa33Oozofjlwoj0Aixz+6rerLjZ\r\n"
    "cpTSrAECgYEA2oCffUo6HH3Lq9oeWhFCoOyG3YjZmFrJaJwjHnvroX9/pXHqYKog\r\n"
    "TehcjUJBtFZw0klcetYbZCFqT8v9nf0uPlgaiVGCtXf1MSbFXDUFKkYBiFwzdWMT\r\n"
    "Ysmvhff82jMWZ8ecsXTyDRl858SR5WPZ52qEsCc5X2un7QENm6FtVT8CgYEAwvKS\r\n"
    "zQNzuoJETqZX7AalmK3JM8Fdam+Qm5LNMcbvhbKwI8HKMS1VMuqaR0XdAX/iMXAx\r\n"
    "P1VhSsmoSDbsMpxBEZIptpCen/GcqctITxANTakrBHxqb2aQ5EEu7SzgfHZWse3/\r\n"
    "vQEyfcFTBlPIdcZUDzk4/w7WmyivpYtCWoAh1IECgYEA0UYZ+1UJfVpapRj+swMP\r\n"
    "DrQbo7i7t7lUaFYLKNpFX2OPLTWC5txqnlOruTu5VHDqE+5hneDNUUTT3uOg4B2q\r\n"
    "mdmmaNjh2M6wz0e0BVFexhNQynqMaqTe32IOM8DFs3L0xacgg7JfVn6P7CeQGOVe\r\n"
    "wc96kICw6ZxhtJSqpOGipt8CgYBI/0Pw+IXxJK4nNSpe+u4vCYP5mUI9hKEFYCbt\r\n"
    "qKwvyAUknn/zgiIQ+r/iSErFMPmlwXjvWi0gL/qPb+Fp4hCLX8u2zNhY08Px4Gin\r\n"
    "Ej+pANtWxq+kHyfKEI5dyRwV/snfvlqwjy404JsSF3VMhIMdYDPzbb72Qnni5w5l\r\n"
    "jO0eAQKBgBqt9jJMd1JdpemC2dm0BuuDIz2h3/MH+CMjfaDLenVpKykn17B6N92h\r\n"
    "klMesqK3RQzDGwauDw431LQw0R69onn9fCM3wJw2yEC6wC9sF8I8hsNZbt64yZhZ\r\n"
    "4Bi2YRTiHhpEuBqKlhHLDFHneo3SMYh8PU/PDQQcyWGHHUi9z1RE\r\n"
    "-----END RSA PRIVATE KEY-----\r\n";

char xyssl_ca_crt[] =
    "-----BEGIN CERTIFICATE-----\r\n"
    "MIIEADCCAuigAwIBAgIBADANBgkqhkiG9w0BAQUFADBjMQswCQYDVQQGEwJVUzEh\r\n"
    "MB8GA1UEChMYVGhlIEdvIERhZGR5IEdyb3VwLCBJbmMuMTEwLwYDVQQLEyhHbyBE\r\n"
    "YWRkeSBDbGFzcyAyIENlcnRpZmljYXRpb24gQXV0aG9yaXR5MB4XDTA0MDYyOTE3\r\n"
    "MDYyMFoXDTM0MDYyOTE3MDYyMFowYzELMAkGA1UEBhMCVVMxITAfBgNVBAoTGFRo\r\n"
    "ZSBHbyBEYWRkeSBHcm91cCwgSW5jLjExMC8GA1UECxMoR28gRGFkZHkgQ2xhc3Mg\r\n"
    "MiBDZXJ0aWZpY2F0aW9uIEF1dGhvcml0eTCCASAwDQYJKoZIhvcNAQEBBQADggEN\r\n"
    "ADCCAQgCggEBAN6d1+pXGEmhW+vXX0iG6r7d/+TvZxz0ZWizV3GgXne77ZtJ6XCA\r\n"
    "PVYYYwhv2vLM0D9/AlQiVBDYsoHUwHU9S3/Hd8M+eKsaA7Ugay9qK7HFiH7Eux6w\r\n"
    "wdhFJ2+qN1j3hybX2C32qRe3H3I2TqYXP2WYktsqbl2i/ojgC95/5Y0V4evLOtXi\r\n"
    "EqITLdiOr18SPaAIBQi2XKVlOARFmR6jYGB0xUGlcmIbYsUfb18aQr4CUWWoriMY\r\n"
    "avx4A6lNf4DD+qta/KFApMoZFv6yyO9ecw3ud72a9nmYvLEHZ6IVDd2gWMZEewo+\r\n"
    "YihfukEHU1jPEX44dMX4/7VpkI+EdOqXG68CAQOjgcAwgb0wHQYDVR0OBBYEFNLE\r\n"
    "sNKR1EwRcbNhyz2h/t2oatTjMIGNBgNVHSMEgYUwgYKAFNLEsNKR1EwRcbNhyz2h\r\n"
    "/t2oatTjoWekZTBjMQswCQYDVQQGEwJVUzEhMB8GA1UEChMYVGhlIEdvIERhZGR5\r\n"
    "IEdyb3VwLCBJbmMuMTEwLwYDVQQLEyhHbyBEYWRkeSBDbGFzcyAyIENlcnRpZmlj\r\n"
    "YXRpb24gQXV0aG9yaXR5ggEAMAwGA1UdEwQFMAMBAf8wDQYJKoZIhvcNAQEFBQAD\r\n"
    "ggEBADJL87LKPpH8EsahB4yOd6AzBhRckB4Y9wimPQoZ+YeAEW5p5JYXMP80kWNy\r\n"
    "OO7MHAGjHZQopDH2esRU1/blMVgDoszOYtuURXO1v0XJJLXVggKtI3lpjbi2Tc7P\r\n"
    "TMozI+gciKqdi0FuFskg5YmezTvacPd+mSYgFFQlq25zheabIZ0KbIIOqPjCDPoQ\r\n"
    "HmyW74cNxA9hi63ugyuV+I6ShHI56yDqg+2DzZduCLzrTia2cyvk0/ZM/iZx4mER\r\n"
    "dEr/VxqHD3VILs9RaRegAhJhldXRQLIQTO7ErBBDpqWeCtWVYpoNz4iCxTIM5Cuf\r\n"
    "ReYNnyicsbkqWletNw+vHX/bvZ8=\r\n" "-----END CERTIFICATE-----\r\n";

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



/********* Start of file src/debug.c ************/


/*
    debug.c -- Debugging routines

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if defined _MSC_VER && !defined  snprintf
    #define snprintf  _snprintf
#endif

#if defined _MSC_VER && !defined vsnprintf
    #define vsnprintf _vsnprintf
#endif

/*
    Safely format into a buffer. This works around inconsistencies in snprintf which is dangerous.
 */
int snfmt(char *buf, ssize bufsize, cchar *fmt, ...)
{
    va_list     ap;
    int         n;

    if (bufsize <= 0) {
        return 0;
    }
    va_start(ap, fmt);
#if _WIN32
    /* Windows does not guarantee a null will be appended */
    if ((n = vsnprintf(buf, bufsize - 1, fmt, ap)) < 0) {
        n = 0;
    }
    buf[n] = '\0';
#else
    /* Posix will return the number of characters that would fix in an unlimited buffer -- Ugh, dangerous! */
    n = vsnprintf(buf, bufsize, fmt, ap);
    n = min(n, bufsize);
#endif
    va_end(ap);
    return n;
}

#if ME_EST_LOGGING

char *debug_fmt(cchar *format, ...)
{
    va_list argp;
    static char str[512];
    int maxlen = sizeof(str) - 1;

    va_start(argp, format);
    vsnprintf(str, maxlen, format, argp);
    va_end(argp);
    str[maxlen] = '\0';
    return str;
}


void debug_print_msg(ssl_context *ssl, int level, char *text)
{
    char str[512];
    int maxlen = sizeof(str) - 1;

    if (ssl->f_dbg == NULL) {
        return;
    }
    snprintf(str, maxlen, "%s\n", text);
    str[maxlen] = '\0';
    ssl->f_dbg(ssl->p_dbg, level, str);
}


void debug_print_ret(ssl_context *ssl, int level, char *text, int ret)
{
    char str[512];
    int maxlen = sizeof(str) - 1;

    if (ssl->f_dbg == NULL) {
        return;
    }
    snprintf(str, maxlen, "%s() returned %d (0x%x)\n", text, ret, ret);
    str[maxlen] = '\0';
    ssl->f_dbg(ssl->p_dbg, level, str);
}


void debug_print_buf(ssl_context *ssl, int level, char *text, uchar *buf, int len)
{
    char str[512];
    int i, maxlen = sizeof(str) - 1;

    if (ssl->f_dbg == NULL || len < 0) {
        return;
    }
    snfmt(str, maxlen, "dumping '%s' (%d bytes)\n", text, len);
    str[maxlen] = '\0';
    ssl->f_dbg(ssl->p_dbg, level, str);

    for (i = 0; i < len; i++) {
        if (i >= 4096) {
            break;
        }
        if (i % 16 == 0) {
            if (i > 0) {
                ssl->f_dbg(ssl->p_dbg, level, "\n");
            }
            snfmt(str, maxlen, "%04x: ", i);
            str[maxlen] = '\0';
            ssl->f_dbg(ssl->p_dbg, level, str);
        }
        snfmt(str, maxlen, " %02x", (uint)buf[i]);
        str[maxlen] = '\0';
        ssl->f_dbg(ssl->p_dbg, level, str);
    }
    if (len > 0) {
        ssl->f_dbg(ssl->p_dbg, level, "\n");
    }
}


void debug_print_mpi(ssl_context *ssl, int level, char *text, mpi * X)
{
    char str[512];
    int i, j, k, n, maxlen = sizeof(str) - 1;

    if (ssl->f_dbg == NULL || X == NULL) {
        return;
    }
    for (n = X->n - 1; n >= 0; n--) {
        if (X->p[n] != 0) {
            break;
        }
    }
    snfmt(str, maxlen, "value of '%s' (%u bits) is:\n", text, (int) ((n + 1) * sizeof(t_int)) << 3);

    str[maxlen] = '\0';
    ssl->f_dbg(ssl->p_dbg, level, str);

    for (i = n, j = 0; i >= 0; i--, j++) {
        if (j % (16 / sizeof(t_int)) == 0) {
            if (j > 0) {
                ssl->f_dbg(ssl->p_dbg, level, "\n");
            }
            snfmt(str, maxlen, " ");
            str[maxlen] = '\0';
            ssl->f_dbg(ssl->p_dbg, level, str);
        }
        for (k = sizeof(t_int) - 1; k >= 0; k--) {
            snfmt(str, maxlen, " %02x", (uint) (X->p[i] >> (k << 3)) & 0xFF);
            str[maxlen] = '\0';
            ssl->f_dbg(ssl->p_dbg, level, str);
        }
    }
    ssl->f_dbg(ssl->p_dbg, level, "\n");
}


void debug_print_crt(ssl_context *ssl, int level, char *text, x509_cert * crt)
{
    char cbuf[5120], str[512], *p;
    int i, maxlen;

    if (ssl->f_dbg == NULL || crt == NULL) {
        return;
    }
    maxlen = sizeof(str) - 1;

    i = 0;
    while (crt != NULL && crt->next != NULL) {
        p = x509parse_cert_info("", cbuf, sizeof(cbuf), crt);
        snfmt(str, maxlen, "%s #%d:\n%s", text, ++i, p);
        str[maxlen] = '\0';
        ssl->f_dbg(ssl->p_dbg, level, str);
        debug_print_mpi(ssl, level, "crt->rsa.N", &crt->rsa.N);
        debug_print_mpi(ssl, level, "crt->rsa.E", &crt->rsa.E);
        crt = crt->next;
    }
}

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



/********* Start of file src/des.c ************/


/*
    des.c -- FIPS-46-3 compliant Triple-DES implementation

    DES, on which TDES is based, was originally designed by Horst Feistel
    at IBM in 1974, and was adopted as a standard by NIST (formerly NBS).

    http://csrc.nist.gov/publications/fips/fips46-3/fips46-3.pdf

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_DES

/*
    32-bit integer manipulation macros (big endian)
 */
#ifndef GET_ULONG_BE
#define GET_ULONG_BE(n,b,i)                     \
    {                                           \
        (n) = ( (ulong) (b)[(i)    ] << 24 )    \
            | ( (ulong) (b)[(i) + 1] << 16 )    \
            | ( (ulong) (b)[(i) + 2] <<  8 )    \
            | ( (ulong) (b)[(i) + 3]       );   \
    }
#endif

#ifndef PUT_ULONG_BE
#define PUT_ULONG_BE(n,b,i)                     \
    {                                           \
        (b)[(i)    ] = (uchar) ( (n) >> 24 );   \
        (b)[(i) + 1] = (uchar) ( (n) >> 16 );   \
        (b)[(i) + 2] = (uchar) ( (n) >>  8 );   \
        (b)[(i) + 3] = (uchar) ( (n)       );   \
    }
#endif

/*
    Expanded DES S-boxes
 */
static const ulong SB1[64] = {
    0x01010400, 0x00000000, 0x00010000, 0x01010404,
    0x01010004, 0x00010404, 0x00000004, 0x00010000,
    0x00000400, 0x01010400, 0x01010404, 0x00000400,
    0x01000404, 0x01010004, 0x01000000, 0x00000004,
    0x00000404, 0x01000400, 0x01000400, 0x00010400,
    0x00010400, 0x01010000, 0x01010000, 0x01000404,
    0x00010004, 0x01000004, 0x01000004, 0x00010004,
    0x00000000, 0x00000404, 0x00010404, 0x01000000,
    0x00010000, 0x01010404, 0x00000004, 0x01010000,
    0x01010400, 0x01000000, 0x01000000, 0x00000400,
    0x01010004, 0x00010000, 0x00010400, 0x01000004,
    0x00000400, 0x00000004, 0x01000404, 0x00010404,
    0x01010404, 0x00010004, 0x01010000, 0x01000404,
    0x01000004, 0x00000404, 0x00010404, 0x01010400,
    0x00000404, 0x01000400, 0x01000400, 0x00000000,
    0x00010004, 0x00010400, 0x00000000, 0x01010004
};

static const ulong SB2[64] = {
    0x80108020, 0x80008000, 0x00008000, 0x00108020,
    0x00100000, 0x00000020, 0x80100020, 0x80008020,
    0x80000020, 0x80108020, 0x80108000, 0x80000000,
    0x80008000, 0x00100000, 0x00000020, 0x80100020,
    0x00108000, 0x00100020, 0x80008020, 0x00000000,
    0x80000000, 0x00008000, 0x00108020, 0x80100000,
    0x00100020, 0x80000020, 0x00000000, 0x00108000,
    0x00008020, 0x80108000, 0x80100000, 0x00008020,
    0x00000000, 0x00108020, 0x80100020, 0x00100000,
    0x80008020, 0x80100000, 0x80108000, 0x00008000,
    0x80100000, 0x80008000, 0x00000020, 0x80108020,
    0x00108020, 0x00000020, 0x00008000, 0x80000000,
    0x00008020, 0x80108000, 0x00100000, 0x80000020,
    0x00100020, 0x80008020, 0x80000020, 0x00100020,
    0x00108000, 0x00000000, 0x80008000, 0x00008020,
    0x80000000, 0x80100020, 0x80108020, 0x00108000
};

static const ulong SB3[64] = {
    0x00000208, 0x08020200, 0x00000000, 0x08020008,
    0x08000200, 0x00000000, 0x00020208, 0x08000200,
    0x00020008, 0x08000008, 0x08000008, 0x00020000,
    0x08020208, 0x00020008, 0x08020000, 0x00000208,
    0x08000000, 0x00000008, 0x08020200, 0x00000200,
    0x00020200, 0x08020000, 0x08020008, 0x00020208,
    0x08000208, 0x00020200, 0x00020000, 0x08000208,
    0x00000008, 0x08020208, 0x00000200, 0x08000000,
    0x08020200, 0x08000000, 0x00020008, 0x00000208,
    0x00020000, 0x08020200, 0x08000200, 0x00000000,
    0x00000200, 0x00020008, 0x08020208, 0x08000200,
    0x08000008, 0x00000200, 0x00000000, 0x08020008,
    0x08000208, 0x00020000, 0x08000000, 0x08020208,
    0x00000008, 0x00020208, 0x00020200, 0x08000008,
    0x08020000, 0x08000208, 0x00000208, 0x08020000,
    0x00020208, 0x00000008, 0x08020008, 0x00020200
};

static const ulong SB4[64] = {
    0x00802001, 0x00002081, 0x00002081, 0x00000080,
    0x00802080, 0x00800081, 0x00800001, 0x00002001,
    0x00000000, 0x00802000, 0x00802000, 0x00802081,
    0x00000081, 0x00000000, 0x00800080, 0x00800001,
    0x00000001, 0x00002000, 0x00800000, 0x00802001,
    0x00000080, 0x00800000, 0x00002001, 0x00002080,
    0x00800081, 0x00000001, 0x00002080, 0x00800080,
    0x00002000, 0x00802080, 0x00802081, 0x00000081,
    0x00800080, 0x00800001, 0x00802000, 0x00802081,
    0x00000081, 0x00000000, 0x00000000, 0x00802000,
    0x00002080, 0x00800080, 0x00800081, 0x00000001,
    0x00802001, 0x00002081, 0x00002081, 0x00000080,
    0x00802081, 0x00000081, 0x00000001, 0x00002000,
    0x00800001, 0x00002001, 0x00802080, 0x00800081,
    0x00002001, 0x00002080, 0x00800000, 0x00802001,
    0x00000080, 0x00800000, 0x00002000, 0x00802080
};

static const ulong SB5[64] = {
    0x00000100, 0x02080100, 0x02080000, 0x42000100,
    0x00080000, 0x00000100, 0x40000000, 0x02080000,
    0x40080100, 0x00080000, 0x02000100, 0x40080100,
    0x42000100, 0x42080000, 0x00080100, 0x40000000,
    0x02000000, 0x40080000, 0x40080000, 0x00000000,
    0x40000100, 0x42080100, 0x42080100, 0x02000100,
    0x42080000, 0x40000100, 0x00000000, 0x42000000,
    0x02080100, 0x02000000, 0x42000000, 0x00080100,
    0x00080000, 0x42000100, 0x00000100, 0x02000000,
    0x40000000, 0x02080000, 0x42000100, 0x40080100,
    0x02000100, 0x40000000, 0x42080000, 0x02080100,
    0x40080100, 0x00000100, 0x02000000, 0x42080000,
    0x42080100, 0x00080100, 0x42000000, 0x42080100,
    0x02080000, 0x00000000, 0x40080000, 0x42000000,
    0x00080100, 0x02000100, 0x40000100, 0x00080000,
    0x00000000, 0x40080000, 0x02080100, 0x40000100
};

static const ulong SB6[64] = {
    0x20000010, 0x20400000, 0x00004000, 0x20404010,
    0x20400000, 0x00000010, 0x20404010, 0x00400000,
    0x20004000, 0x00404010, 0x00400000, 0x20000010,
    0x00400010, 0x20004000, 0x20000000, 0x00004010,
    0x00000000, 0x00400010, 0x20004010, 0x00004000,
    0x00404000, 0x20004010, 0x00000010, 0x20400010,
    0x20400010, 0x00000000, 0x00404010, 0x20404000,
    0x00004010, 0x00404000, 0x20404000, 0x20000000,
    0x20004000, 0x00000010, 0x20400010, 0x00404000,
    0x20404010, 0x00400000, 0x00004010, 0x20000010,
    0x00400000, 0x20004000, 0x20000000, 0x00004010,
    0x20000010, 0x20404010, 0x00404000, 0x20400000,
    0x00404010, 0x20404000, 0x00000000, 0x20400010,
    0x00000010, 0x00004000, 0x20400000, 0x00404010,
    0x00004000, 0x00400010, 0x20004010, 0x00000000,
    0x20404000, 0x20000000, 0x00400010, 0x20004010
};

static const ulong SB7[64] = {
    0x00200000, 0x04200002, 0x04000802, 0x00000000,
    0x00000800, 0x04000802, 0x00200802, 0x04200800,
    0x04200802, 0x00200000, 0x00000000, 0x04000002,
    0x00000002, 0x04000000, 0x04200002, 0x00000802,
    0x04000800, 0x00200802, 0x00200002, 0x04000800,
    0x04000002, 0x04200000, 0x04200800, 0x00200002,
    0x04200000, 0x00000800, 0x00000802, 0x04200802,
    0x00200800, 0x00000002, 0x04000000, 0x00200800,
    0x04000000, 0x00200800, 0x00200000, 0x04000802,
    0x04000802, 0x04200002, 0x04200002, 0x00000002,
    0x00200002, 0x04000000, 0x04000800, 0x00200000,
    0x04200800, 0x00000802, 0x00200802, 0x04200800,
    0x00000802, 0x04000002, 0x04200802, 0x04200000,
    0x00200800, 0x00000000, 0x00000002, 0x04200802,
    0x00000000, 0x00200802, 0x04200000, 0x00000800,
    0x04000002, 0x04000800, 0x00000800, 0x00200002
};

static const ulong SB8[64] = {
    0x10001040, 0x00001000, 0x00040000, 0x10041040,
    0x10000000, 0x10001040, 0x00000040, 0x10000000,
    0x00040040, 0x10040000, 0x10041040, 0x00041000,
    0x10041000, 0x00041040, 0x00001000, 0x00000040,
    0x10040000, 0x10000040, 0x10001000, 0x00001040,
    0x00041000, 0x00040040, 0x10040040, 0x10041000,
    0x00001040, 0x00000000, 0x00000000, 0x10040040,
    0x10000040, 0x10001000, 0x00041040, 0x00040000,
    0x00041040, 0x00040000, 0x10041000, 0x00001000,
    0x00000040, 0x10040040, 0x00001000, 0x00041040,
    0x10001000, 0x00000040, 0x10000040, 0x10040000,
    0x10040040, 0x10000000, 0x00040000, 0x10001040,
    0x00000000, 0x10041040, 0x00040040, 0x10000040,
    0x10040000, 0x10001000, 0x10001040, 0x00000000,
    0x10041040, 0x00041000, 0x00041000, 0x00001040,
    0x00001040, 0x00040040, 0x10000000, 0x10041000
};

/*
 * PC1: left and right halves bit-swap
 */
static const ulong LHs[16] = {
    0x00000000, 0x00000001, 0x00000100, 0x00000101,
    0x00010000, 0x00010001, 0x00010100, 0x00010101,
    0x01000000, 0x01000001, 0x01000100, 0x01000101,
    0x01010000, 0x01010001, 0x01010100, 0x01010101
};

static const ulong RHs[16] = {
    0x00000000, 0x01000000, 0x00010000, 0x01010000,
    0x00000100, 0x01000100, 0x00010100, 0x01010100,
    0x00000001, 0x01000001, 0x00010001, 0x01010001,
    0x00000101, 0x01000101, 0x00010101, 0x01010101,
};

/*
 * Initial Permutation macro
 */
#define DES_IP(X,Y)                                                 \
    {                                                               \
        T = ((X >>  4) ^ Y) & 0x0F0F0F0F; Y ^= T; X ^= (T <<  4);   \
        T = ((X >> 16) ^ Y) & 0x0000FFFF; Y ^= T; X ^= (T << 16);   \
        T = ((Y >>  2) ^ X) & 0x33333333; X ^= T; Y ^= (T <<  2);   \
        T = ((Y >>  8) ^ X) & 0x00FF00FF; X ^= T; Y ^= (T <<  8);   \
        Y = ((Y << 1) | (Y >> 31)) & 0xFFFFFFFF;                    \
        T = (X ^ Y) & 0xAAAAAAAA; Y ^= T; X ^= T;                   \
        X = ((X << 1) | (X >> 31)) & 0xFFFFFFFF;                    \
    }

/*
 * Final Permutation macro
 */
#define DES_FP(X,Y)                                                 \
    {                                                               \
        X = ((X << 31) | (X >> 1)) & 0xFFFFFFFF;                    \
        T = (X ^ Y) & 0xAAAAAAAA; X ^= T; Y ^= T;                   \
        Y = ((Y << 31) | (Y >> 1)) & 0xFFFFFFFF;                    \
        T = ((Y >>  8) ^ X) & 0x00FF00FF; X ^= T; Y ^= (T <<  8);   \
        T = ((Y >>  2) ^ X) & 0x33333333; X ^= T; Y ^= (T <<  2);   \
        T = ((X >> 16) ^ Y) & 0x0000FFFF; Y ^= T; X ^= (T << 16);   \
        T = ((X >>  4) ^ Y) & 0x0F0F0F0F; Y ^= T; X ^= (T <<  4);   \
    }

/*
 * DES round macro
 */
#define DES_ROUND(X,Y)                          \
    {                                           \
        T = *SK++ ^ X;                          \
        Y ^= SB8[ (T      ) & 0x3F ] ^          \
            SB6[ (T >>   8) & 0x3F ] ^          \
            SB4[ (T >> 16) & 0x3F ] ^           \
            SB2[ (T >> 24) & 0x3F ];            \
                                                \
        T = *SK++ ^ ((X << 28) | (X >> 4));     \
        Y ^= SB7[ (T      ) & 0x3F ] ^          \
            SB5[ (T >>   8) & 0x3F ] ^          \
            SB3[ (T >> 16) & 0x3F ] ^           \
            SB1[ (T >> 24) & 0x3F ];            \
    }

#define SWAP(a,b) { ulong t = a; a = b; b = t; t = 0; }

static void des_setkey(ulong SK[32], uchar key[8])
{
    int i;
    ulong X, Y, T;

    GET_ULONG_BE(X, key, 0);
    GET_ULONG_BE(Y, key, 4);

    /*
     * Permuted Choice 1
     */
    T = ((Y >> 4) ^ X) & 0x0F0F0F0F;
    X ^= T;
    Y ^= (T << 4);
    T = ((Y) ^ X) & 0x10101010;
    X ^= T;
    Y ^= (T);

    X = (LHs[(X) & 0xF] << 3) | (LHs[(X >> 8) & 0xF] << 2)
        | (LHs[(X >> 16) & 0xF] << 1) | (LHs[(X >> 24) & 0xF])
        | (LHs[(X >> 5) & 0xF] << 7) | (LHs[(X >> 13) & 0xF] << 6)
        | (LHs[(X >> 21) & 0xF] << 5) | (LHs[(X >> 29) & 0xF] << 4);

    Y = (RHs[(Y >> 1) & 0xF] << 3) | (RHs[(Y >> 9) & 0xF] << 2)
        | (RHs[(Y >> 17) & 0xF] << 1) | (RHs[(Y >> 25) & 0xF])
        | (RHs[(Y >> 4) & 0xF] << 7) | (RHs[(Y >> 12) & 0xF] << 6)
        | (RHs[(Y >> 20) & 0xF] << 5) | (RHs[(Y >> 28) & 0xF] << 4);

    X &= 0x0FFFFFFF;
    Y &= 0x0FFFFFFF;

    /*
       calculate subkeys
     */
    for (i = 0; i < 16; i++) {
        if (i < 2 || i == 8 || i == 15) {
            X = ((X << 1) | (X >> 27)) & 0x0FFFFFFF;
            Y = ((Y << 1) | (Y >> 27)) & 0x0FFFFFFF;
        } else {
            X = ((X << 2) | (X >> 26)) & 0x0FFFFFFF;
            Y = ((Y << 2) | (Y >> 26)) & 0x0FFFFFFF;
        }
        *SK++ = ((X << 4) & 0x24000000) | ((X << 28) & 0x10000000)
            | ((X << 14) & 0x08000000) | ((X << 18) & 0x02080000)
            | ((X << 6) & 0x01000000) | ((X << 9) & 0x00200000)
            | ((X >> 1) & 0x00100000) | ((X << 10) & 0x00040000)
            | ((X << 2) & 0x00020000) | ((X >> 10) & 0x00010000)
            | ((Y >> 13) & 0x00002000) | ((Y >> 4) & 0x00001000)
            | ((Y << 6) & 0x00000800) | ((Y >> 1) & 0x00000400)
            | ((Y >> 14) & 0x00000200) | ((Y) & 0x00000100)
            | ((Y >> 5) & 0x00000020) | ((Y >> 10) & 0x00000010)
            | ((Y >> 3) & 0x00000008) | ((Y >> 18) & 0x00000004)
            | ((Y >> 26) & 0x00000002) | ((Y >> 24) & 0x00000001);

        *SK++ = ((X << 15) & 0x20000000) | ((X << 17) & 0x10000000)
            | ((X << 10) & 0x08000000) | ((X << 22) & 0x04000000)
            | ((X >> 2) & 0x02000000) | ((X << 1) & 0x01000000)
            | ((X << 16) & 0x00200000) | ((X << 11) & 0x00100000)
            | ((X << 3) & 0x00080000) | ((X >> 6) & 0x00040000)
            | ((X << 15) & 0x00020000) | ((X >> 4) & 0x00010000)
            | ((Y >> 2) & 0x00002000) | ((Y << 8) & 0x00001000)
            | ((Y >> 14) & 0x00000808) | ((Y >> 9) & 0x00000400)
            | ((Y) & 0x00000200) | ((Y << 7) & 0x00000100)
            | ((Y >> 7) & 0x00000020) | ((Y >> 3) & 0x00000011)
            | ((Y << 2) & 0x00000004) | ((Y >> 21) & 0x00000002);
    }
}


/*
    DES key schedule (56-bit, encryption)
 */
void des_setkey_enc(des_context *ctx, uchar key[8])
{
    des_setkey(ctx->sk, key);
}


/*
    DES key schedule (56-bit, decryption)
 */
void des_setkey_dec(des_context *ctx, uchar key[8])
{
    int i;

    des_setkey(ctx->sk, key);

    for (i = 0; i < 16; i += 2) {
        SWAP(ctx->sk[i], ctx->sk[30 - i]);
        SWAP(ctx->sk[i + 1], ctx->sk[31 - i]);
    }
}

static void des3_set2key(ulong esk[96], ulong dsk[96], uchar key[16])
{
    int i;

    des_setkey(esk, key);
    des_setkey(dsk + 32, key + 8);

    for (i = 0; i < 32; i += 2) {
        dsk[i] = esk[30 - i];
        dsk[i + 1] = esk[31 - i];

        esk[i + 32] = dsk[62 - i];
        esk[i + 33] = dsk[63 - i];

        esk[i + 64] = esk[i];
        esk[i + 65] = esk[i + 1];

        dsk[i + 64] = dsk[i];
        dsk[i + 65] = dsk[i + 1];
    }
}


/*
    Triple-DES key schedule (112-bit, encryption)
 */
void des3_set2key_enc(des3_context *ctx, uchar key[16])
{
    ulong   sk[96];

    des3_set2key(ctx->sk, sk, key);
    memset(sk, 0, sizeof(sk));
}


/*
    Triple-DES key schedule (112-bit, decryption)
 */
void des3_set2key_dec(des3_context *ctx, uchar key[16])
{
    ulong sk[96];

    des3_set2key(sk, ctx->sk, key);
    memset(sk, 0, sizeof(sk));
}


static void des3_set3key(ulong esk[96], ulong dsk[96], uchar key[24])
{
    int i;

    des_setkey(esk, key);
    des_setkey(dsk + 32, key + 8);
    des_setkey(esk + 64, key + 16);

    for (i = 0; i < 32; i += 2) {
        dsk[i] = esk[94 - i];
        dsk[i + 1] = esk[95 - i];
        esk[i + 32] = dsk[62 - i];
        esk[i + 33] = dsk[63 - i];
        dsk[i + 64] = esk[30 - i];
        dsk[i + 65] = esk[31 - i];
    }
}


/*
    Triple-DES key schedule (168-bit, encryption)
 */
void des3_set3key_enc(des3_context *ctx, uchar key[24])
{
    ulong   sk[96];

    des3_set3key(ctx->sk, sk, key);
    memset(sk, 0, sizeof(sk));
}


/*
    Triple-DES key schedule (168-bit, decryption)
 */
void des3_set3key_dec(des3_context *ctx, uchar key[24])
{
    ulong sk[96];

    des3_set3key(sk, ctx->sk, key);
    memset(sk, 0, sizeof(sk));
}


/*
    DES-ECB block encryption/decryption
 */
void des_crypt_ecb(des_context *ctx, uchar input[8], uchar output[8])
{
    ulong   X, Y, T, *SK;
    int     i;

    SK = ctx->sk;
    GET_ULONG_BE(X, input, 0);
    GET_ULONG_BE(Y, input, 4);
    DES_IP(X, Y);

    for (i = 0; i < 8; i++) {
        DES_ROUND(Y, X);
        DES_ROUND(X, Y);
    }
    DES_FP(Y, X);
    PUT_ULONG_BE(Y, output, 0);
    PUT_ULONG_BE(X, output, 4);
}


/*
    DES-CBC buffer encryption/decryption
 */
void des_crypt_cbc(des_context *ctx, int mode, int length, uchar iv[8], uchar *input, uchar *output)
{
    int i;
    uchar temp[8];

    if (mode == DES_ENCRYPT) {
        while (length > 0) {
            for (i = 0; i < 8; i++) {
                output[i] = (uchar)(input[i] ^ iv[i]);
            }
            des_crypt_ecb(ctx, output, output);
            memcpy(iv, output, 8);
            input += 8;
            output += 8;
            length -= 8;
        }
    } else {
        /* DES_DECRYPT */
        while (length > 0) {
            memcpy(temp, input, 8);
            des_crypt_ecb(ctx, input, output);
            for (i = 0; i < 8; i++) {
                output[i] = (uchar)(output[i] ^ iv[i]);
            }
            memcpy(iv, temp, 8);
            input += 8;
            output += 8;
            length -= 8;
        }
    }
}

/*
    3DES-ECB block encryption/decryption
 */
void des3_crypt_ecb(des3_context *ctx, uchar input[8], uchar output[8])
{
    int i;
    ulong X, Y, T, *SK;

    SK = ctx->sk;
    GET_ULONG_BE(X, input, 0);
    GET_ULONG_BE(Y, input, 4);
    DES_IP(X, Y);

    for (i = 0; i < 8; i++) {
        DES_ROUND(Y, X);
        DES_ROUND(X, Y);
    }
    for (i = 0; i < 8; i++) {
        DES_ROUND(X, Y);
        DES_ROUND(Y, X);
    }
    for (i = 0; i < 8; i++) {
        DES_ROUND(Y, X);
        DES_ROUND(X, Y);
    }
    DES_FP(Y, X);
    PUT_ULONG_BE(Y, output, 0);
    PUT_ULONG_BE(X, output, 4);
}


/*
    3DES-CBC buffer encryption/decryption
 */
void des3_crypt_cbc(des3_context *ctx, int mode, int length, uchar iv[8], uchar *input, uchar *output)
{
    int i;
    uchar temp[8];

    if (mode == DES_ENCRYPT) {
        while (length > 0) {
            for (i = 0; i < 8; i++) {
                output[i] = (uchar)(input[i] ^ iv[i]);
            }
            des3_crypt_ecb(ctx, output, output);
            memcpy(iv, output, 8);
            input += 8;
            output += 8;
            length -= 8;
        }
    } else {
        /* DES_DECRYPT */
        while (length > 0) {
            memcpy(temp, input, 8);
            des3_crypt_ecb(ctx, input, output);
            for (i = 0; i < 8; i++) {
                output[i] = (uchar)(output[i] ^ iv[i]);
            }
            memcpy(iv, temp, 8);
            input += 8;
            output += 8;
            length -= 8;
        }
    }
}


#undef SWAP
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



/********* Start of file src/dhm.c ************/


/*
    dhm.c -- Diffie-Hellman-Merkle key exchange

    http://www.cacr.math.uwaterloo.ca/hac/ (chapter 12)
 
    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_DHM
/*
   helper to validate the mpi size and import it
 */
static int dhm_read_bignum(mpi * X, uchar **p, uchar *end)
{
    int ret, n;

    if (end - *p < 2) {
        return EST_ERR_DHM_BAD_INPUT_DATA;
    }
    n = ((*p)[0] << 8) | (*p)[1];
    (*p) += 2;

    if ((int)(end - *p) < n) {
        return EST_ERR_DHM_BAD_INPUT_DATA;
    }
    if ((ret = mpi_read_binary(X, *p, n)) != 0) {
        return EST_ERR_DHM_READ_PARAMS_FAILED | ret;
    }
    (*p) += n;
    return 0;
}


/*
    Parse the ServerKeyExchange parameters
 */
int dhm_read_params(dhm_context *ctx, uchar **p, uchar *end)
{
    int ret, n;

    memset(ctx, 0, sizeof(dhm_context));

    if ((ret = dhm_read_bignum(&ctx->P, p, end)) != 0 ||
        (ret = dhm_read_bignum(&ctx->G, p, end)) != 0 ||
        (ret = dhm_read_bignum(&ctx->GY, p, end)) != 0)
        return ret;

    ctx->len = mpi_size(&ctx->P);

    if (end - *p < 2)
        return EST_ERR_DHM_BAD_INPUT_DATA;

    n = ((*p)[0] << 8) | (*p)[1];
    (*p) += 2;

    if (end != *p + n)
        return EST_ERR_DHM_BAD_INPUT_DATA;
    return 0;
}


/*
   Setup and write the ServerKeyExchange parameters
 */
int dhm_make_params(dhm_context *ctx, int x_size, uchar *output, int *olen, int (*f_rng) (void *), void *p_rng)
{
    int i, ret, n, n1, n2, n3;
    uchar *p;

    /*
        Generate X and calculate GX = G^X mod P
     */
    n = x_size / sizeof(t_int);
    MPI_CHK(mpi_grow(&ctx->X, n));
    MPI_CHK(mpi_lset(&ctx->X, 0));

    n = x_size >> 3;
    p = (uchar *)ctx->X.p;
    for (i = 0; i < n; i++)
        *p++ = (uchar)f_rng(p_rng);

    while (mpi_cmp_mpi(&ctx->X, &ctx->P) >= 0)
        mpi_shift_r(&ctx->X, 1);

    MPI_CHK(mpi_exp_mod(&ctx->GX, &ctx->G, &ctx->X, &ctx->P, &ctx->RP));

    /*
        Export P, G, GX
     */
#define DHM_MPI_EXPORT(X,n) \
    MPI_CHK(mpi_write_binary( X, p + 2, n ) ); \
    *p++ = (uchar)( n >> 8 ); \
    *p++ = (uchar)( n      ); p += n;

    n1 = mpi_size(&ctx->P);
    n2 = mpi_size(&ctx->G);
    n3 = mpi_size(&ctx->GX);

    p = output;
    DHM_MPI_EXPORT(&ctx->P, n1);
    DHM_MPI_EXPORT(&ctx->G, n2);
    DHM_MPI_EXPORT(&ctx->GX, n3);

    *olen = p - output;
    ctx->len = n1;

cleanup:
    if (ret != 0)
        return ret | EST_ERR_DHM_MAKE_PARAMS_FAILED;
    return 0;
}


/*
   Import the peer's public value G^Y
 */
int dhm_read_public(dhm_context *ctx, uchar *input, int ilen)
{
    int ret;

    if (ctx == NULL || ilen < 1 || ilen > ctx->len) {
        return EST_ERR_DHM_BAD_INPUT_DATA;
    }
    if ((ret = mpi_read_binary(&ctx->GY, input, ilen)) != 0) {
        return EST_ERR_DHM_READ_PUBLIC_FAILED | ret;
    }
    return 0;
}


/*
   Create own private value X and export G^X
 */
int dhm_make_public(dhm_context *ctx, int x_size, uchar *output, int olen, int (*f_rng) (void *), void *p_rng)
{
    int ret, i, n;
    uchar *p;

    if (ctx == NULL || olen < 1 || olen > ctx->len) {
        return EST_ERR_DHM_BAD_INPUT_DATA;
    }

    /*
        generate X and calculate GX = G^X mod P
     */
    n = x_size / sizeof(t_int);
    MPI_CHK(mpi_grow(&ctx->X, n));
    MPI_CHK(mpi_lset(&ctx->X, 0));

    n = x_size >> 3;
    p = (uchar *)ctx->X.p;
    for (i = 0; i < n; i++) {
        *p++ = (uchar)f_rng(p_rng);
    }
    while (mpi_cmp_mpi(&ctx->X, &ctx->P) >= 0) {
        mpi_shift_r(&ctx->X, 1);
    }
    MPI_CHK(mpi_exp_mod(&ctx->GX, &ctx->G, &ctx->X, &ctx->P, &ctx->RP));
    MPI_CHK(mpi_write_binary(&ctx->GX, output, olen));

cleanup:
    if (ret != 0) {
        return EST_ERR_DHM_MAKE_PUBLIC_FAILED | ret;
    }
    return 0;
}


/*
    Derive and export the shared secret (G^Y)^X mod P
 */
int dhm_calc_secret(dhm_context *ctx, uchar *output, int *olen)
{
    int ret;

    if (ctx == NULL || *olen < ctx->len) {
        return EST_ERR_DHM_BAD_INPUT_DATA;
    }
    MPI_CHK(mpi_exp_mod(&ctx->K, &ctx->GY, &ctx->X, &ctx->P, &ctx->RP));
    *olen = mpi_size(&ctx->K);
    MPI_CHK(mpi_write_binary(&ctx->K, output, *olen));

cleanup:
    if (ret != 0) {
        return EST_ERR_DHM_CALC_SECRET_FAILED | ret;
    }
    return 0;
}


/*
    Free the components of a DHM key
 */
void dhm_free(dhm_context * ctx)
{
    mpi_free(&ctx->RP, &ctx->K, &ctx->GY, &ctx->GX, &ctx->X, &ctx->G, &ctx->P, NULL);
}

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



/********* Start of file src/havege.c ************/


/*
    havege.c -- Hardware Volatile Entropy Gathering and Expansion

    The HAVEGE RNG was designed by Andre Seznec in 2002.
    http://www.irisa.fr/caps/projects/hipsor/publi.php

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_HAVEGE

/* Undefine for windows */
#undef IN

/*
   On average, one iteration accesses two 8-word blocks in the havege WALK table, and generates 16 words in the RES array.
  
   The data read in the WALK table is updated and permuted after each use. The result of the hardware clock counter read is
   used for this update.
  
   25 conditional tests are present. The conditional tests are grouped in two nested groups of 12 conditional tests and 1
   test that controls the permutation; on average, there should be 6 tests executed and 3 of them should be mispredicted.
 */

#define SWAP(X,Y) { int *T = X; X = Y; Y = T; }

#define TST1_ENTER if( PTEST & 1 ) { PTEST ^= 3; PTEST >>= 1;
#define TST2_ENTER if( PTEST & 1 ) { PTEST ^= 3; PTEST >>= 1;

#define TST1_LEAVE U1++; }
#define TST2_LEAVE U2++; }

#define ONE_ITERATION                                   \
                                                        \
    PTEST = PT1 >> 20;                                  \
                                                        \
    TST1_ENTER  TST1_ENTER  TST1_ENTER  TST1_ENTER      \
    TST1_ENTER  TST1_ENTER  TST1_ENTER  TST1_ENTER      \
    TST1_ENTER  TST1_ENTER  TST1_ENTER  TST1_ENTER      \
                                                        \
    TST1_LEAVE  TST1_LEAVE  TST1_LEAVE  TST1_LEAVE      \
    TST1_LEAVE  TST1_LEAVE  TST1_LEAVE  TST1_LEAVE      \
    TST1_LEAVE  TST1_LEAVE  TST1_LEAVE  TST1_LEAVE      \
                                                        \
    PTX = (PT1 >> 18) & 7;                              \
    PT1 &= 0x1FFF;                                      \
    PT2 &= 0x1FFF;                                      \
    CLK = (int) hardclock();                            \
                                                        \
    i = 0;                                              \
    A = &WALK[PT1    ]; RES[i++] ^= *A;                 \
    B = &WALK[PT2    ]; RES[i++] ^= *B;                 \
    C = &WALK[PT1 ^ 1]; RES[i++] ^= *C;                 \
    D = &WALK[PT2 ^ 4]; RES[i++] ^= *D;                 \
                                                        \
    IN = (*A >> (1)) ^ (*A << (31)) ^ CLK;              \
    *A = (*B >> (2)) ^ (*B << (30)) ^ CLK;              \
    *B = IN ^ U1;                                       \
    *C = (*C >> (3)) ^ (*C << (29)) ^ CLK;              \
    *D = (*D >> (4)) ^ (*D << (28)) ^ CLK;              \
                                                        \
    A = &WALK[PT1 ^ 2]; RES[i++] ^= *A;                 \
    B = &WALK[PT2 ^ 2]; RES[i++] ^= *B;                 \
    C = &WALK[PT1 ^ 3]; RES[i++] ^= *C;                 \
    D = &WALK[PT2 ^ 6]; RES[i++] ^= *D;                 \
                                                        \
    if( PTEST & 1 ) SWAP( A, C );                       \
                                                        \
    IN = (*A >> (5)) ^ (*A << (27)) ^ CLK;              \
    *A = (*B >> (6)) ^ (*B << (26)) ^ CLK;              \
    *B = IN; CLK = (int) hardclock();                   \
    *C = (*C >> (7)) ^ (*C << (25)) ^ CLK;              \
    *D = (*D >> (8)) ^ (*D << (24)) ^ CLK;              \
                                                        \
    A = &WALK[PT1 ^ 4];                                 \
    B = &WALK[PT2 ^ 1];                                 \
                                                        \
    PTEST = PT2 >> 1;                                   \
                                                        \
    PT2 = (RES[(i - 8) ^ PTY] ^ WALK[PT2 ^ PTY ^ 7]);   \
    PT2 = ((PT2 & 0x1FFF) & (~8)) ^ ((PT1 ^ 8) & 0x8);  \
    PTY = (PT2 >> 10) & 7;                              \
                                                        \
    TST2_ENTER  TST2_ENTER  TST2_ENTER  TST2_ENTER      \
    TST2_ENTER  TST2_ENTER  TST2_ENTER  TST2_ENTER      \
    TST2_ENTER  TST2_ENTER  TST2_ENTER  TST2_ENTER      \
                                                        \
    TST2_LEAVE  TST2_LEAVE  TST2_LEAVE  TST2_LEAVE      \
    TST2_LEAVE  TST2_LEAVE  TST2_LEAVE  TST2_LEAVE      \
    TST2_LEAVE  TST2_LEAVE  TST2_LEAVE  TST2_LEAVE      \
                                                        \
    C = &WALK[PT1 ^ 5];                                 \
    D = &WALK[PT2 ^ 5];                                 \
                                                        \
    RES[i++] ^= *A;                                     \
    RES[i++] ^= *B;                                     \
    RES[i++] ^= *C;                                     \
    RES[i++] ^= *D;                                     \
                                                        \
    IN = (*A >> ( 9)) ^ (*A << (23)) ^ CLK;             \
    *A = (*B >> (10)) ^ (*B << (22)) ^ CLK;             \
    *B = IN ^ U2;                                       \
    *C = (*C >> (11)) ^ (*C << (21)) ^ CLK;             \
    *D = (*D >> (12)) ^ (*D << (20)) ^ CLK;             \
                                                        \
    A = &WALK[PT1 ^ 6]; RES[i++] ^= *A;                 \
    B = &WALK[PT2 ^ 3]; RES[i++] ^= *B;                 \
    C = &WALK[PT1 ^ 7]; RES[i++] ^= *C;                 \
    D = &WALK[PT2 ^ 7]; RES[i++] ^= *D;                 \
                                                        \
    IN = (*A >> (13)) ^ (*A << (19)) ^ CLK;             \
    *A = (*B >> (14)) ^ (*B << (18)) ^ CLK;             \
    *B = IN;                                            \
    *C = (*C >> (15)) ^ (*C << (17)) ^ CLK;             \
    *D = (*D >> (16)) ^ (*D << (16)) ^ CLK;             \
                                                        \
    PT1 = ( RES[(i - 8) ^ PTX] ^                        \
            WALK[PT1 ^ PTX ^ 7] ) & (~1);               \
    PT1 ^= (PT2 ^ 0x10) & 0x10;                         \
                                                        \
    for (n++, i = 0; i < 16; i++)                       \
        hs->pool[n % COLLECT_SIZE] ^= RES[i];

/*
    Entropy gathering function
 */
static void havege_fill(havege_state * hs)
{
    int i, n = 0;
    int U1, U2, *A, *B, *C, *D;
    int PT1, PT2, *WALK, RES[16];
    int PTX, PTY, CLK, PTEST, IN;

    WALK = hs->WALK;
    PT1 = hs->PT1;
    PT2 = hs->PT2;
    PTX = U1 = 0;
    PTY = U2 = 0;

    memset(RES, 0, sizeof(RES));

    while (n < COLLECT_SIZE * 4) {
        ONE_ITERATION 
        ONE_ITERATION 
        ONE_ITERATION 
        ONE_ITERATION
    } 
    hs->PT1 = PT1;
    hs->PT2 = PT2;
    hs->offset[0] = 0;
    hs->offset[1] = COLLECT_SIZE / 2;
}


/*
    HAVEGE initialization
 */
void havege_init(havege_state * hs)
{
    memset(hs, 0, sizeof(havege_state));
    havege_fill(hs);
}


/*
    HAVEGE rand function
 */
int havege_rand(void *p_rng)
{
    havege_state *hs = (havege_state *) p_rng;
    int     ret;

    if (hs->offset[1] >= COLLECT_SIZE) {
        havege_fill(hs);
    }
    ret = hs->pool[hs->offset[0]++];
    ret ^= hs->pool[hs->offset[1]++];
    return ret;
}

#undef SWAP

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



/********* Start of file src/md2.c ************/


/*
    md2.c -- RFC 1115/1319 compliant MD2 implementation

    The MD2 algorithm was designed by Ron Rivest in 1989.

    http://www.ietf.org/rfc/rfc1115.txt
    http://www.ietf.org/rfc/rfc1319.txt

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_MD2

static const uchar PI_SUBST[256] = {
    0x29, 0x2E, 0x43, 0xC9, 0xA2, 0xD8, 0x7C, 0x01, 0x3D, 0x36,
    0x54, 0xA1, 0xEC, 0xF0, 0x06, 0x13, 0x62, 0xA7, 0x05, 0xF3,
    0xC0, 0xC7, 0x73, 0x8C, 0x98, 0x93, 0x2B, 0xD9, 0xBC, 0x4C,
    0x82, 0xCA, 0x1E, 0x9B, 0x57, 0x3C, 0xFD, 0xD4, 0xE0, 0x16,
    0x67, 0x42, 0x6F, 0x18, 0x8A, 0x17, 0xE5, 0x12, 0xBE, 0x4E,
    0xC4, 0xD6, 0xDA, 0x9E, 0xDE, 0x49, 0xA0, 0xFB, 0xF5, 0x8E,
    0xBB, 0x2F, 0xEE, 0x7A, 0xA9, 0x68, 0x79, 0x91, 0x15, 0xB2,
    0x07, 0x3F, 0x94, 0xC2, 0x10, 0x89, 0x0B, 0x22, 0x5F, 0x21,
    0x80, 0x7F, 0x5D, 0x9A, 0x5A, 0x90, 0x32, 0x27, 0x35, 0x3E,
    0xCC, 0xE7, 0xBF, 0xF7, 0x97, 0x03, 0xFF, 0x19, 0x30, 0xB3,
    0x48, 0xA5, 0xB5, 0xD1, 0xD7, 0x5E, 0x92, 0x2A, 0xAC, 0x56,
    0xAA, 0xC6, 0x4F, 0xB8, 0x38, 0xD2, 0x96, 0xA4, 0x7D, 0xB6,
    0x76, 0xFC, 0x6B, 0xE2, 0x9C, 0x74, 0x04, 0xF1, 0x45, 0x9D,
    0x70, 0x59, 0x64, 0x71, 0x87, 0x20, 0x86, 0x5B, 0xCF, 0x65,
    0xE6, 0x2D, 0xA8, 0x02, 0x1B, 0x60, 0x25, 0xAD, 0xAE, 0xB0,
    0xB9, 0xF6, 0x1C, 0x46, 0x61, 0x69, 0x34, 0x40, 0x7E, 0x0F,
    0x55, 0x47, 0xA3, 0x23, 0xDD, 0x51, 0xAF, 0x3A, 0xC3, 0x5C,
    0xF9, 0xCE, 0xBA, 0xC5, 0xEA, 0x26, 0x2C, 0x53, 0x0D, 0x6E,
    0x85, 0x28, 0x84, 0x09, 0xD3, 0xDF, 0xCD, 0xF4, 0x41, 0x81,
    0x4D, 0x52, 0x6A, 0xDC, 0x37, 0xC8, 0x6C, 0xC1, 0xAB, 0xFA,
    0x24, 0xE1, 0x7B, 0x08, 0x0C, 0xBD, 0xB1, 0x4A, 0x78, 0x88,
    0x95, 0x8B, 0xE3, 0x63, 0xE8, 0x6D, 0xE9, 0xCB, 0xD5, 0xFE,
    0x3B, 0x00, 0x1D, 0x39, 0xF2, 0xEF, 0xB7, 0x0E, 0x66, 0x58,
    0xD0, 0xE4, 0xA6, 0x77, 0x72, 0xF8, 0xEB, 0x75, 0x4B, 0x0A,
    0x31, 0x44, 0x50, 0xB4, 0x8F, 0xED, 0x1F, 0x1A, 0xDB, 0x99,
    0x8D, 0x33, 0x9F, 0x11, 0x83, 0x14
};


/*
    MD2 context setup
 */
void md2_starts(md2_context *ctx)
{
    memset(ctx, 0, sizeof(md2_context));
}


static void md2_process(md2_context *ctx)
{
    int     i, j;
    uchar   t = 0;

    for (i = 0; i < 16; i++) {
        ctx->state[i + 16] = ctx->buffer[i];
        ctx->state[i + 32] = (uchar)(ctx->buffer[i] ^ ctx->state[i]);
    }

    for (i = 0; i < 18; i++) {
        for (j = 0; j < 48; j++) {
            ctx->state[j] = (uchar) (ctx->state[j] ^ PI_SUBST[t]);
            t = ctx->state[j];
        }
        t = (uchar)(t + i);
    }
    t = ctx->cksum[15];

    for (i = 0; i < 16; i++) {
        ctx->cksum[i] = (uchar) (ctx->cksum[i] ^ PI_SUBST[ctx->buffer[i] ^ t]);
        t = ctx->cksum[i];
    }
}


/*
    MD2 process buffer
 */
void md2_update(md2_context *ctx, uchar *input, int ilen)
{
    int fill;

    while (ilen > 0) {
        if (ctx->left + ilen > 16) {
            fill = 16 - ctx->left;
        } else {
            fill = ilen;
        }
        memcpy(ctx->buffer + ctx->left, input, fill);
        ctx->left += fill;
        input += fill;
        ilen -= fill;
        if (ctx->left == 16) {
            ctx->left = 0;
            md2_process(ctx);
        }
    }
}


/*
    MD2 final digest
 */
void md2_finish(md2_context *ctx, uchar output[16])
{
    uchar   x;
    int     i;

    x = (uchar)(16 - ctx->left);
    for (i = ctx->left; i < 16; i++) {
        ctx->buffer[i] = x;
    }
    md2_process(ctx);
    memcpy(ctx->buffer, ctx->cksum, 16);
    md2_process(ctx);
    memcpy(output, ctx->state, 16);
}


/*
    output = MD2( input buffer )
 */
void md2(uchar *input, int ilen, uchar output[16])
{
    md2_context ctx;

    md2_starts(&ctx);
    md2_update(&ctx, input, ilen);
    md2_finish(&ctx, output);
    memset(&ctx, 0, sizeof(md2_context));
}


/*
    output = MD2( file contents )
 */
int md2_file(char *path, uchar output[16])
{
    FILE        *f;
    size_t      n;
    md2_context ctx;
    uchar       buf[1024];

    if ((f = fopen(path, "rb")) == NULL) {
        return 1;
    }
    md2_starts(&ctx);

    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        md2_update(&ctx, buf, (int)n);
    }
    md2_finish(&ctx, output);
    memset(&ctx, 0, sizeof(md2_context));

    if (ferror(f) != 0) {
        fclose(f);
        return 2;
    }
    fclose(f);
    return 0;
}


/*
    MD2 HMAC context setup
 */
void md2_hmac_starts(md2_context *ctx, uchar *key, int keylen)
{
    uchar   sum[16];
    int     i;

    if (keylen > 64) {
        md2(key, keylen, sum);
        keylen = 16;
        key = sum;
    }
    memset(ctx->ipad, 0x36, 64);
    memset(ctx->opad, 0x5C, 64);

    for (i = 0; i < keylen; i++) {
        ctx->ipad[i] = (uchar)(ctx->ipad[i] ^ key[i]);
        ctx->opad[i] = (uchar)(ctx->opad[i] ^ key[i]);
    }
    md2_starts(ctx);
    md2_update(ctx, ctx->ipad, 64);
    memset(sum, 0, sizeof(sum));
}


/*
    MD2 HMAC process buffer
 */
void md2_hmac_update(md2_context *ctx, uchar *input, int ilen)
{
    md2_update(ctx, input, ilen);
}


/*
    MD2 HMAC final digest
 */
void md2_hmac_finish(md2_context *ctx, uchar output[16])
{
    uchar tmpbuf[16];

    md2_finish(ctx, tmpbuf);
    md2_starts(ctx);
    md2_update(ctx, ctx->opad, 64);
    md2_update(ctx, tmpbuf, 16);
    md2_finish(ctx, output);
    memset(tmpbuf, 0, sizeof(tmpbuf));
}


/*
    output = HMAC-MD2( hmac key, input buffer )
 */
void md2_hmac(uchar *key, int keylen, uchar *input, int ilen, uchar output[16])
{
    md2_context ctx;

    md2_hmac_starts(&ctx, key, keylen);
    md2_hmac_update(&ctx, input, ilen);
    md2_hmac_finish(&ctx, output);
    memset(&ctx, 0, sizeof(md2_context));
}


#undef P
#undef R
#undef SHR
#undef ROTR
#undef S0
#undef S1
#undef S2
#undef S3
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



/********* Start of file src/md4.c ************/


/*
    md4.c -- RFC 1186/1320 compliant MD4 implementation

    The MD4 algorithm was designed by Ron Rivest in 1990.
    http://www.ietf.org/rfc/rfc1186.txt
    http://www.ietf.org/rfc/rfc1320.txt

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_MD4

/*
   32-bit integer manipulation macros (little endian)
 */
#ifndef GET_ULONG_LE
#define GET_ULONG_LE(n,b,i)                     \
    {                                           \
        (n) = ( (ulong) (b)[(i)    ]       )    \
            | ( (ulong) (b)[(i) + 1] <<  8 )    \
            | ( (ulong) (b)[(i) + 2] << 16 )    \
            | ( (ulong) (b)[(i) + 3] << 24 );   \
    }
#endif

#ifndef PUT_ULONG_LE
#define PUT_ULONG_LE(n,b,i)                     \
    {                                           \
        (b)[(i)    ] = (uchar) ( (n)       );   \
        (b)[(i) + 1] = (uchar) ( (n) >>  8 );   \
        (b)[(i) + 2] = (uchar) ( (n) >> 16 );   \
        (b)[(i) + 3] = (uchar) ( (n) >> 24 );   \
    }
#endif


/*
   MD4 context setup
 */
void md4_starts(md4_context * ctx)
{
    ctx->total[0] = 0;
    ctx->total[1] = 0;

    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
}


static void md4_process(md4_context * ctx, uchar data[64])
{
    ulong X[16], A, B, C, D;

    GET_ULONG_LE(X[0], data, 0);
    GET_ULONG_LE(X[1], data, 4);
    GET_ULONG_LE(X[2], data, 8);
    GET_ULONG_LE(X[3], data, 12);
    GET_ULONG_LE(X[4], data, 16);
    GET_ULONG_LE(X[5], data, 20);
    GET_ULONG_LE(X[6], data, 24);
    GET_ULONG_LE(X[7], data, 28);
    GET_ULONG_LE(X[8], data, 32);
    GET_ULONG_LE(X[9], data, 36);
    GET_ULONG_LE(X[10], data, 40);
    GET_ULONG_LE(X[11], data, 44);
    GET_ULONG_LE(X[12], data, 48);
    GET_ULONG_LE(X[13], data, 52);
    GET_ULONG_LE(X[14], data, 56);
    GET_ULONG_LE(X[15], data, 60);

#define S(x,n) ((x << n) | ((x & 0xFFFFFFFF) >> (32 - n)))

    A = ctx->state[0];
    B = ctx->state[1];
    C = ctx->state[2];
    D = ctx->state[3];

#define F(x, y, z) ((x & y) | ((~x) & z))
#define P(a,b,c,d,x,s) { a += F(b,c,d) + x; a = S(a,s); }

    P(A, B, C, D, X[0], 3);
    P(D, A, B, C, X[1], 7);
    P(C, D, A, B, X[2], 11);
    P(B, C, D, A, X[3], 19);
    P(A, B, C, D, X[4], 3);
    P(D, A, B, C, X[5], 7);
    P(C, D, A, B, X[6], 11);
    P(B, C, D, A, X[7], 19);
    P(A, B, C, D, X[8], 3);
    P(D, A, B, C, X[9], 7);
    P(C, D, A, B, X[10], 11);
    P(B, C, D, A, X[11], 19);
    P(A, B, C, D, X[12], 3);
    P(D, A, B, C, X[13], 7);
    P(C, D, A, B, X[14], 11);
    P(B, C, D, A, X[15], 19);

#undef P
#undef F

#define F(x,y,z) ((x & y) | (x & z) | (y & z))
#define P(a,b,c,d,x,s) { a += F(b,c,d) + x + 0x5A827999; a = S(a,s); }

    P(A, B, C, D, X[0], 3);
    P(D, A, B, C, X[4], 5);
    P(C, D, A, B, X[8], 9);
    P(B, C, D, A, X[12], 13);
    P(A, B, C, D, X[1], 3);
    P(D, A, B, C, X[5], 5);
    P(C, D, A, B, X[9], 9);
    P(B, C, D, A, X[13], 13);
    P(A, B, C, D, X[2], 3);
    P(D, A, B, C, X[6], 5);
    P(C, D, A, B, X[10], 9);
    P(B, C, D, A, X[14], 13);
    P(A, B, C, D, X[3], 3);
    P(D, A, B, C, X[7], 5);
    P(C, D, A, B, X[11], 9);
    P(B, C, D, A, X[15], 13);

#undef P
#undef F

#define F(x,y,z) (x ^ y ^ z)
#define P(a,b,c,d,x,s) { a += F(b,c,d) + x + 0x6ED9EBA1; a = S(a,s); }

    P(A, B, C, D, X[0], 3);
    P(D, A, B, C, X[8], 9);
    P(C, D, A, B, X[4], 11);
    P(B, C, D, A, X[12], 15);
    P(A, B, C, D, X[2], 3);
    P(D, A, B, C, X[10], 9);
    P(C, D, A, B, X[6], 11);
    P(B, C, D, A, X[14], 15);
    P(A, B, C, D, X[1], 3);
    P(D, A, B, C, X[9], 9);
    P(C, D, A, B, X[5], 11);
    P(B, C, D, A, X[13], 15);
    P(A, B, C, D, X[3], 3);
    P(D, A, B, C, X[11], 9);
    P(C, D, A, B, X[7], 11);
    P(B, C, D, A, X[15], 15);

#undef F
#undef P

    ctx->state[0] += A;
    ctx->state[1] += B;
    ctx->state[2] += C;
    ctx->state[3] += D;
}


/*
   MD4 process buffer
 */
void md4_update(md4_context * ctx, uchar *input, int ilen)
{
    int     fill;
    ulong   left;

    if (ilen <= 0)
        return;

    left = ctx->total[0] & 0x3F;
    fill = 64 - left;

    ctx->total[0] += ilen;
    ctx->total[0] &= 0xFFFFFFFF;

    if (ctx->total[0] < (ulong) ilen) {
        ctx->total[1]++;
    }
    if (left && ilen >= fill) {
        memcpy((void *)(ctx->buffer + left), (void *)input, fill);
        md4_process(ctx, ctx->buffer);
        input += fill;
        ilen -= fill;
        left = 0;
    }
    while (ilen >= 64) {
        md4_process(ctx, input);
        input += 64;
        ilen -= 64;
    }
    if (ilen > 0) {
        memcpy((void *)(ctx->buffer + left), (void *)input, ilen);
    }
}


static const uchar md4_padding[64] = {
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


/*
    MD4 final digest
 */
void md4_finish(md4_context * ctx, uchar output[16])
{
    ulong last, padn;
    ulong high, low;
    uchar msglen[8];

    high = (ctx->total[0] >> 29) | (ctx->total[1] << 3);
    low = (ctx->total[0] << 3);

    PUT_ULONG_LE(low, msglen, 0);
    PUT_ULONG_LE(high, msglen, 4);

    last = ctx->total[0] & 0x3F;
    padn = (last < 56) ? (56 - last) : (120 - last);

    md4_update(ctx, (uchar *)md4_padding, padn);
    md4_update(ctx, msglen, 8);

    PUT_ULONG_LE(ctx->state[0], output, 0);
    PUT_ULONG_LE(ctx->state[1], output, 4);
    PUT_ULONG_LE(ctx->state[2], output, 8);
    PUT_ULONG_LE(ctx->state[3], output, 12);
}


/*
    output = MD4( input buffer )
 */
void md4(uchar *input, int ilen, uchar output[16])
{
    md4_context ctx;

    md4_starts(&ctx);
    md4_update(&ctx, input, ilen);
    md4_finish(&ctx, output);
    memset(&ctx, 0, sizeof(md4_context));
}


/*
    output = MD4( file contents )
 */
int md4_file(char *path, uchar output[16])
{
    FILE *f;
    size_t n;
    md4_context ctx;
    uchar buf[1024];

    if ((f = fopen(path, "rb")) == NULL) {
        return (1);
    }
    md4_starts(&ctx);

    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        md4_update(&ctx, buf, (int)n);
    }
    md4_finish(&ctx, output);
    memset(&ctx, 0, sizeof(md4_context));

    if (ferror(f) != 0) {
        fclose(f);
        return (2);
    }
    fclose(f);
    return 0;
}


/*
    MD4 HMAC context setup
 */
void md4_hmac_starts(md4_context * ctx, uchar *key, int keylen)
{
    int     i;
    uchar   sum[16];

    if (keylen > 64) {
        md4(key, keylen, sum);
        keylen = 16;
        key = sum;
    }
    memset(ctx->ipad, 0x36, 64);
    memset(ctx->opad, 0x5C, 64);

    for (i = 0; i < keylen; i++) {
        ctx->ipad[i] = (uchar)(ctx->ipad[i] ^ key[i]);
        ctx->opad[i] = (uchar)(ctx->opad[i] ^ key[i]);
    }
    md4_starts(ctx);
    md4_update(ctx, ctx->ipad, 64);
    memset(sum, 0, sizeof(sum));
}


/*
    MD4 HMAC process buffer
 */
void md4_hmac_update(md4_context * ctx, uchar *input, int ilen)
{
    md4_update(ctx, input, ilen);
}


/*
    MD4 HMAC final digest
 */
void md4_hmac_finish(md4_context * ctx, uchar output[16])
{
    uchar tmpbuf[16];

    md4_finish(ctx, tmpbuf);
    md4_starts(ctx);
    md4_update(ctx, ctx->opad, 64);
    md4_update(ctx, tmpbuf, 16);
    md4_finish(ctx, output);
    memset(tmpbuf, 0, sizeof(tmpbuf));
}


/*
    output = HMAC-MD4( hmac key, input buffer )
 */
void md4_hmac(uchar *key, int keylen, uchar *input, int ilen, uchar output[16])
{
    md4_context ctx;

    md4_hmac_starts(&ctx, key, keylen);
    md4_hmac_update(&ctx, input, ilen);
    md4_hmac_finish(&ctx, output);
    memset(&ctx, 0, sizeof(md4_context));
}


#undef P
#undef R
#undef SHR
#undef ROTR
#undef S0
#undef S1
#undef S2
#undef S3
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



/********* Start of file src/md5.c ************/


/*
    md5.c -- RFC 1321 compliant MD5 implementation

    The MD5 algorithm was designed by Ron Rivest in 1991.
    http://www.ietf.org/rfc/rfc1321.txt

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_MD5

/*
    32-bit integer manipulation macros (little endian)
 */
#ifndef GET_ULONG_LE
#define GET_ULONG_LE(n,b,i)                     \
    {                                           \
        (n) = ((ulong) (b)[(i)    ]      )      \
            | ((ulong) (b)[(i) + 1] <<  8)      \
            | ((ulong) (b)[(i) + 2] << 16)      \
            | ((ulong) (b)[(i) + 3] << 24);     \
    }
#endif

#ifndef PUT_ULONG_LE
#define PUT_ULONG_LE(n,b,i)                     \
    {                                           \
        (b)[(i)    ] = (uchar) ((n)      );     \
        (b)[(i) + 1] = (uchar) ((n) >>  8);     \
        (b)[(i) + 2] = (uchar) ((n) >> 16);     \
        (b)[(i) + 3] = (uchar) ((n) >> 24);     \
    }
#endif

/*
    MD5 context setup
 */
void md5_starts(md5_context * ctx)
{
    ctx->total[0] = 0;
    ctx->total[1] = 0;

    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
}

static void md5_process(md5_context * ctx, uchar data[64])
{
    ulong X[16], A, B, C, D;

    GET_ULONG_LE(X[0], data, 0);
    GET_ULONG_LE(X[1], data, 4);
    GET_ULONG_LE(X[2], data, 8);
    GET_ULONG_LE(X[3], data, 12);
    GET_ULONG_LE(X[4], data, 16);
    GET_ULONG_LE(X[5], data, 20);
    GET_ULONG_LE(X[6], data, 24);
    GET_ULONG_LE(X[7], data, 28);
    GET_ULONG_LE(X[8], data, 32);
    GET_ULONG_LE(X[9], data, 36);
    GET_ULONG_LE(X[10], data, 40);
    GET_ULONG_LE(X[11], data, 44);
    GET_ULONG_LE(X[12], data, 48);
    GET_ULONG_LE(X[13], data, 52);
    GET_ULONG_LE(X[14], data, 56);
    GET_ULONG_LE(X[15], data, 60);

#define S(x,n) ((x << n) | ((x & 0xFFFFFFFF) >> (32 - n)))

#define P(a,b,c,d,k,s,t)                            \
    {                                               \
        a += F(b,c,d) + X[k] + t; a = S(a,s) + b;   \
    }

    A = ctx->state[0];
    B = ctx->state[1];
    C = ctx->state[2];
    D = ctx->state[3];

#define F(x,y,z) (z ^ (x & (y ^ z)))

    P(A, B, C, D, 0, 7, 0xD76AA478);
    P(D, A, B, C, 1, 12, 0xE8C7B756);
    P(C, D, A, B, 2, 17, 0x242070DB);
    P(B, C, D, A, 3, 22, 0xC1BDCEEE);
    P(A, B, C, D, 4, 7, 0xF57C0FAF);
    P(D, A, B, C, 5, 12, 0x4787C62A);
    P(C, D, A, B, 6, 17, 0xA8304613);
    P(B, C, D, A, 7, 22, 0xFD469501);
    P(A, B, C, D, 8, 7, 0x698098D8);
    P(D, A, B, C, 9, 12, 0x8B44F7AF);
    P(C, D, A, B, 10, 17, 0xFFFF5BB1);
    P(B, C, D, A, 11, 22, 0x895CD7BE);
    P(A, B, C, D, 12, 7, 0x6B901122);
    P(D, A, B, C, 13, 12, 0xFD987193);
    P(C, D, A, B, 14, 17, 0xA679438E);
    P(B, C, D, A, 15, 22, 0x49B40821);

#undef F

#define F(x,y,z) (y ^ (z & (x ^ y)))

    P(A, B, C, D, 1, 5, 0xF61E2562);
    P(D, A, B, C, 6, 9, 0xC040B340);
    P(C, D, A, B, 11, 14, 0x265E5A51);
    P(B, C, D, A, 0, 20, 0xE9B6C7AA);
    P(A, B, C, D, 5, 5, 0xD62F105D);
    P(D, A, B, C, 10, 9, 0x02441453);
    P(C, D, A, B, 15, 14, 0xD8A1E681);
    P(B, C, D, A, 4, 20, 0xE7D3FBC8);
    P(A, B, C, D, 9, 5, 0x21E1CDE6);
    P(D, A, B, C, 14, 9, 0xC33707D6);
    P(C, D, A, B, 3, 14, 0xF4D50D87);
    P(B, C, D, A, 8, 20, 0x455A14ED);
    P(A, B, C, D, 13, 5, 0xA9E3E905);
    P(D, A, B, C, 2, 9, 0xFCEFA3F8);
    P(C, D, A, B, 7, 14, 0x676F02D9);
    P(B, C, D, A, 12, 20, 0x8D2A4C8A);

#undef F

#define F(x,y,z) (x ^ y ^ z)

    P(A, B, C, D, 5, 4, 0xFFFA3942);
    P(D, A, B, C, 8, 11, 0x8771F681);
    P(C, D, A, B, 11, 16, 0x6D9D6122);
    P(B, C, D, A, 14, 23, 0xFDE5380C);
    P(A, B, C, D, 1, 4, 0xA4BEEA44);
    P(D, A, B, C, 4, 11, 0x4BDECFA9);
    P(C, D, A, B, 7, 16, 0xF6BB4B60);
    P(B, C, D, A, 10, 23, 0xBEBFBC70);
    P(A, B, C, D, 13, 4, 0x289B7EC6);
    P(D, A, B, C, 0, 11, 0xEAA127FA);
    P(C, D, A, B, 3, 16, 0xD4EF3085);
    P(B, C, D, A, 6, 23, 0x04881D05);
    P(A, B, C, D, 9, 4, 0xD9D4D039);
    P(D, A, B, C, 12, 11, 0xE6DB99E5);
    P(C, D, A, B, 15, 16, 0x1FA27CF8);
    P(B, C, D, A, 2, 23, 0xC4AC5665);

#undef F

#define F(x,y,z) (y ^ (x | ~z))

    P(A, B, C, D, 0, 6, 0xF4292244);
    P(D, A, B, C, 7, 10, 0x432AFF97);
    P(C, D, A, B, 14, 15, 0xAB9423A7);
    P(B, C, D, A, 5, 21, 0xFC93A039);
    P(A, B, C, D, 12, 6, 0x655B59C3);
    P(D, A, B, C, 3, 10, 0x8F0CCC92);
    P(C, D, A, B, 10, 15, 0xFFEFF47D);
    P(B, C, D, A, 1, 21, 0x85845DD1);
    P(A, B, C, D, 8, 6, 0x6FA87E4F);
    P(D, A, B, C, 15, 10, 0xFE2CE6E0);
    P(C, D, A, B, 6, 15, 0xA3014314);
    P(B, C, D, A, 13, 21, 0x4E0811A1);
    P(A, B, C, D, 4, 6, 0xF7537E82);
    P(D, A, B, C, 11, 10, 0xBD3AF235);
    P(C, D, A, B, 2, 15, 0x2AD7D2BB);
    P(B, C, D, A, 9, 21, 0xEB86D391);

#undef F

    ctx->state[0] += A;
    ctx->state[1] += B;
    ctx->state[2] += C;
    ctx->state[3] += D;
}


/*
   MD5 process buffer
 */
void md5_update(md5_context *ctx, uchar *input, int ilen)
{
    ulong   left;
    int     fill;

    if (ilen <= 0) {
        return;
    }
    left = ctx->total[0] & 0x3F;
    fill = 64 - left;

    ctx->total[0] += ilen;
    ctx->total[0] &= 0xFFFFFFFF;

    if (ctx->total[0] < (ulong)ilen) {
        ctx->total[1]++;
    }
    if (left && ilen >= fill) {
        memcpy((void *)(ctx->buffer + left), (void *)input, fill);
        md5_process(ctx, ctx->buffer);
        input += fill;
        ilen -= fill;
        left = 0;
    }
    while (ilen >= 64) {
        md5_process(ctx, input);
        input += 64;
        ilen -= 64;
    }
    if (ilen > 0) {
        memcpy((void *)(ctx->buffer + left), (void *)input, ilen);
    }
}

static const uchar md5_padding[64] = {
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


/*
    MD5 final digest
 */
void md5_finish(md5_context *ctx, uchar output[16])
{
    ulong last, padn, high, low;
    uchar msglen[8];

    high = (ctx->total[0] >> 29) | (ctx->total[1] << 3);
    low = (ctx->total[0] << 3);

    PUT_ULONG_LE(low, msglen, 0);
    PUT_ULONG_LE(high, msglen, 4);

    last = ctx->total[0] & 0x3F;
    padn = (last < 56) ? (56 - last) : (120 - last);

    md5_update(ctx, (uchar *)md5_padding, padn);
    md5_update(ctx, msglen, 8);

    PUT_ULONG_LE(ctx->state[0], output, 0);
    PUT_ULONG_LE(ctx->state[1], output, 4);
    PUT_ULONG_LE(ctx->state[2], output, 8);
    PUT_ULONG_LE(ctx->state[3], output, 12);
}

/*
    output = MD5(input buffer)
 */
void md5(uchar *input, int ilen, uchar output[16])
{
    md5_context ctx;

    md5_starts(&ctx);
    md5_update(&ctx, input, ilen);
    md5_finish(&ctx, output);

    memset(&ctx, 0, sizeof(md5_context));
}


/*
    output = MD5(file contents)
 */
int md5_file(char *path, uchar output[16])
{
    FILE        *f;
    size_t      n;
    md5_context ctx;
    uchar       buf[1024];

    if ((f = fopen(path, "rb")) == NULL) {
        return 1;
    }
    md5_starts(&ctx);

    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        md5_update(&ctx, buf, (int)n);

    md5_finish(&ctx, output);

    memset(&ctx, 0, sizeof(md5_context));

    if (ferror(f) != 0) {
        fclose(f);
        return 2;
    }
    fclose(f);
    return 0;
}


/*
   MD5 HMAC context setup
 */
void md5_hmac_starts(md5_context *ctx, uchar *key, int keylen)
{
    uchar   sum[16];
    int     i;

    if (keylen > 64) {
        md5(key, keylen, sum);
        keylen = 16;
        key = sum;
    }
    memset(ctx->ipad, 0x36, 64);
    memset(ctx->opad, 0x5C, 64);

    for (i = 0; i < keylen; i++) {
        ctx->ipad[i] = (uchar)(ctx->ipad[i] ^ key[i]);
        ctx->opad[i] = (uchar)(ctx->opad[i] ^ key[i]);
    }
    md5_starts(ctx);
    md5_update(ctx, ctx->ipad, 64);
    memset(sum, 0, sizeof(sum));
}


/*
   MD5 HMAC process buffer
 */
void md5_hmac_update(md5_context *ctx, uchar *input, int ilen)
{
    md5_update(ctx, input, ilen);
}


/*
    MD5 HMAC final digest
 */
void md5_hmac_finish(md5_context *ctx, uchar output[16])
{
    uchar tmpbuf[16];

    md5_finish(ctx, tmpbuf);
    md5_starts(ctx);
    md5_update(ctx, ctx->opad, 64);
    md5_update(ctx, tmpbuf, 16);
    md5_finish(ctx, output);

    memset(tmpbuf, 0, sizeof(tmpbuf));
}


/*
    output = HMAC-MD5(hmac key, input buffer)
 */
void md5_hmac(uchar *key, int keylen, uchar *input, int ilen, uchar output[16])
{
    md5_context ctx;

    md5_hmac_starts(&ctx, key, keylen);
    md5_hmac_update(&ctx, input, ilen);
    md5_hmac_finish(&ctx, output);

    memset(&ctx, 0, sizeof(md5_context));
}


#undef P
#undef R
#undef SHR
#undef ROTR
#undef S0
#undef S1
#undef S2
#undef S3
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



/********* Start of file src/net.c ************/


/*
    net.c -- Network routines

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_NET

#if WINDOWS || WINCE
    #undef read
    #undef write
    #undef close
    #define read(fd,buf,len)        recv(fd,buf,len,0)
    #define write(fd,buf,len)       send(fd,buf,len,0)
    #define close(fd)               closesocket(fd)
    static int wsa_init_done = 0;
#endif

/*
    htons() is not always available
 */
#if defined(__BYTE_ORDER) && defined(__BIG_ENDIAN) && __BYTE_ORDER == __BIG_ENDIAN
    #define SSL_HTONS(n) (n)
#else
    #define SSL_HTONS(n) (((((unsigned short)(n) & 0xFF)) << 8) | (((unsigned short)(n) & 0xFF00) >> 8))
#endif
#define net_htons(n) SSL_HTONS(n)

/*
   Initiate a TCP connection with host:port
 */
int net_connect(int *fd, char *host, int port)
{
    struct sockaddr_in server_addr;
    struct hostent *server_host;

#if WINDOWS || WINCE
    WSADATA wsaData;

    if (!wsa_init_done) {
        if (WSAStartup(MAKEWORD(2, 0), &wsaData) == SOCKET_ERROR) {
            return EST_ERR_NET_SOCKET_FAILED;
        }
        wsa_init_done = 1;
    }
#else
    signal(SIGPIPE, SIG_IGN);
#endif

    if ((server_host = gethostbyname(host)) == NULL) {
        return EST_ERR_NET_UNKNOWN_HOST;
    }
    if ((*fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) < 0) {
        return EST_ERR_NET_SOCKET_FAILED;
    }
    memcpy((void *)&server_addr.sin_addr, (void *)server_host->h_addr, server_host->h_length);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = net_htons(port);

    if (connect(*fd, (struct sockaddr*) &server_addr, sizeof(server_addr)) < 0) {
        closesocket(*fd);
        return EST_ERR_NET_CONNECT_FAILED;
    }
    return 0;
}


/*
   Create a listening socket on bind_ip:port
 */
int net_bind(int *fd, char *bind_ip, int port)
{
    int n, c[4];
    struct sockaddr_in server_addr;

#if defined(WIN32) || defined(_WIN32_WCE)
    WSADATA wsaData;

    if (wsa_init_done == 0) {
        if (WSAStartup(MAKEWORD(2, 0), &wsaData) == SOCKET_ERROR) {
            return EST_ERR_NET_SOCKET_FAILED;
        }
        wsa_init_done = 1;
    }
#else
    signal(SIGPIPE, SIG_IGN);
#endif

    if ((*fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) < 0) {
        return EST_ERR_NET_SOCKET_FAILED;
    }
    n = 1;
    setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR, (char*) &n, sizeof(n));

    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = net_htons(port);

    if (bind_ip != NULL) {
        memset(c, 0, sizeof(c));
        sscanf(bind_ip, "%d.%d.%d.%d", &c[0], &c[1], &c[2], &c[3]);

        for (n = 0; n < 4; n++) {
            if (c[n] < 0 || c[n] > 255) {
                break;
            }
        }
        if (n == 4) {
            server_addr.sin_addr.s_addr = ((ulong)c[0] << 24) | ((ulong)c[1] << 16) | ((ulong)c[2] << 8) | ((ulong)c[3]);
        }
    }
    if (bind(*fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        closesocket(*fd);
        return EST_ERR_NET_BIND_FAILED;
    }
    if (listen(*fd, 10) != 0) {
        closesocket(*fd);
        return EST_ERR_NET_LISTEN_FAILED;
    }
    return 0;
}


/*
    Check if the current operation is blocking
 */
static int net_is_blocking(void)
{
#if WINDOWS || WINCE
    return WSAGetLastError() == WSAEWOULDBLOCK;
#else
    switch (errno) {
#if defined EAGAIN
    case EAGAIN:
#endif
#if defined EWOULDBLOCK && EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:
#endif
        return 1;
    }
    return 0;
#endif
}


/*
    Accept a connection from a remote client
 */
int net_accept(int bind_fd, int *client_fd, void *client_ip)
{
    struct sockaddr_in client_addr;

#if defined(__socklen_t_defined) || 1
    socklen_t n = (socklen_t) sizeof(client_addr);
#else
    int n = (int)sizeof(client_addr);
#endif

    *client_fd = accept(bind_fd, (struct sockaddr *) &client_addr, &n);

    if (*client_fd < 0) {
        if (net_is_blocking() != 0) {
            return EST_ERR_NET_TRY_AGAIN;
        }
        return EST_ERR_NET_ACCEPT_FAILED;
    }
    if (client_ip != NULL) {
        memcpy(client_ip, &client_addr.sin_addr.s_addr, sizeof(client_addr.sin_addr.s_addr));
    }
    return 0;
}


/*
    Set the socket blocking or non-blocking
 */
int net_set_block(int fd)
{
#if ME_WIN_LIKE
    long n = 0;
    return ioctlsocket(fd, FIONBIO, &n);
#else
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);
#endif
}


int net_set_nonblock(int fd)
{
#if ME_WIN_LIKE
    long n = 1;
    return ioctlsocket(fd, FIONBIO, &n);
#else
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
#endif
}


/*
    Portable usleep helper
 */
void net_usleep(ulong usec)
{
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = usec;
    select(0, NULL, NULL, NULL, &tv);
}


/*
    Read at most 'len' characters
 */
int net_recv(void *ctx, uchar *buf, int len)
{
    int ret = read(*((int *)ctx), buf, len);

    if (len > 0 && ret == 0) {
        return EST_ERR_NET_CONN_RESET;
    }
    if (ret < 0) {
        if (net_is_blocking() != 0) {
            return EST_ERR_NET_TRY_AGAIN;
        }
#if defined(WIN32) || defined(_WIN32_WCE)
        if (WSAGetLastError() == WSAECONNRESET) {
            return EST_ERR_NET_CONN_RESET;
        }
#else
        if (errno == EPIPE || errno == ECONNRESET) {
            return EST_ERR_NET_CONN_RESET;
        }
        if (errno == EINTR) {
            return EST_ERR_NET_TRY_AGAIN;
        }
#endif
        return EST_ERR_NET_RECV_FAILED;
    }
    return ret;
}


/*
    Write at most 'len' characters
 */
int net_send(void *ctx, uchar *buf, int len)
{
    int ret = send(*((int*)ctx), buf, len, 0);

    if (ret < 0) {
        if (net_is_blocking() != 0) {
            return EST_ERR_NET_TRY_AGAIN;
        }
#if defined(WIN32) || defined(_WIN32_WCE)
        if (WSAGetLastError() == WSAECONNRESET) {
            return EST_ERR_NET_CONN_RESET;
        }
#else
        if (errno == EPIPE || errno == ECONNRESET) {
            return EST_ERR_NET_CONN_RESET;
        }
        if (errno == EINTR) {
            return EST_ERR_NET_TRY_AGAIN;
        }
#endif
        return EST_ERR_NET_SEND_FAILED;
    }
    return ret;
}


/*
    Gracefully close the connection
 */
void net_close(int fd)
{
    shutdown(fd, 2);
    closesocket(fd);
}

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



/********* Start of file src/padlock.c ************/


/*
    padlock.c -- Header VIA padlock suport

    This implementation is based on the VIA PadLock Programming Guide:
    http://www.via.com.tw/en/downloads/whitepapers/initiatives/padlock/programming_guide.pdf

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_PADLOCK

// #if ME_CPU_ARCH == ME_CPU_X86
#if defined(EST_HAVE_X86)

/*
    PadLock detection routine
 */
int padlock_supports(int feature)
{
    static int flags = -1;
    int ebx, edx;

    if (flags == -1) {
asm("movl  %%ebx, %0           \n" "movl  $0xC0000000, %%eax  \n" "cpuid                     \n" "cmpl  $0xC0000001, %%eax  \n" "movl  $0, %%edx           \n" "jb    unsupported         \n" "movl  $0xC0000001, %%eax  \n" "cpuid                     \n" "unsupported:              \n" "movl  %%edx, %1           \n" "movl  %2, %%ebx           \n":"=m"(ebx),
            "=m"
            (edx)
:           "m"(ebx)
:           "eax", "ecx", "edx");

        flags = edx;
    }

    return flags & feature;
}

/*
    PadLock AES-ECB block en(de)cryption
 */
int padlock_xcryptecb(aes_context * ctx, int mode, uchar input[16], uchar output[16])
{
    ulong   *rk, *blk, *ctrl, buf[256];
    int     ebx;

    rk = ctx->rk;
    blk = PADLOCK_ALIGN16(buf);
    memcpy(blk, input, 16);

    ctrl = blk + 4;
    *ctrl = 0x80 | ctx->nr | ((ctx->nr + (mode ^ 1) - 10) << 9);

asm("pushfl; popfl         \n" "movl    %%ebx, %0     \n" "movl    $1, %%ecx     \n" "movl    %2, %%edx     \n" "movl    %3, %%ebx     \n" "movl    %4, %%esi     \n" "movl    %4, %%edi     \n" ".byte  0xf3,0x0f,0xa7,0xc8\n" "movl    %1, %%ebx     \n":"=m"(ebx)
:       "m"(ebx), "m"(ctrl), "m"(rk), "m"(blk)
:       "ecx", "edx", "esi", "edi");

    memcpy(output, blk, 16);
    return 0;
}


/*
    PadLock AES-CBC buffer en(de)cryption
 */
int padlock_xcryptcbc(aes_context * ctx, int mode, int length, uchar iv[16], uchar *input, uchar *output)
{
    ulong   *rk, *iw, *ctrl, buf[256];
    int     ebx, count;

    if (((long)input & 15) != 0 || ((long)output & 15) != 0) {
        return 1;
    }
    rk = ctx->rk;
    iw = PADLOCK_ALIGN16(buf);
    memcpy(iw, iv, 16);

    ctrl = iw + 4;
    *ctrl = 0x80 | ctx->nr | ((ctx->nr + (mode ^ 1) - 10) << 9);

    count = (length + 15) >> 4;

asm("pushfl; popfl         \n" "movl    %%ebx, %0     \n" "movl    %2, %%ecx     \n" "movl    %3, %%edx     \n" "movl    %4, %%ebx     \n" "movl    %5, %%esi     \n" "movl    %6, %%edi     \n" "movl    %7, %%eax     \n" ".byte  0xf3,0x0f,0xa7,0xd0\n" "movl    %1, %%ebx     \n":"=m"(ebx)
:       "m"(ebx), "m"(count), "m"(ctrl),
        "m"(rk), "m"(input), "m"(output), "m"(iw)
:       "eax", "ecx", "edx", "esi", "edi");

    memcpy(iv, iw, 16);
    return 0;
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



/********* Start of file src/rsa.c ************/


/*
    rsa.c -- The RSA public-key cryptosystem

    RSA was designed by Ron Rivest, Adi Shamir and Len Adleman.

    http://theory.lcs.mit.edu/~rivest/rsapaper.pdf
    http://www.cacr.math.uwaterloo.ca/hac/about/chap8.pdf

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_RSA

/*
    Initialize an RSA context
 */
void rsa_init(rsa_context *ctx, int padding, int hash_id, int (*f_rng) (void *), void *p_rng)
{
    memset(ctx, 0, sizeof(rsa_context));
    ctx->padding = padding;
    ctx->hash_id = hash_id;
    ctx->f_rng = f_rng;
    ctx->p_rng = p_rng;
}


#if ME_EST_GEN_PRIME
/*
    Generate an RSA keypair
 */
int rsa_gen_key(rsa_context *ctx, int nbits, int exponent)
{
    mpi     P1, Q1, H, G;
    int     ret;

    if (ctx->f_rng == NULL || nbits < 128 || exponent < 3) {
        return EST_ERR_RSA_BAD_INPUT_DATA;
    }
    mpi_init(&P1, &Q1, &H, &G, NULL);

    /*
        find primes P and Q with Q < P so that: GCD( E, (P-1)*(Q-1) ) == 1
     */
    MPI_CHK(mpi_lset(&ctx->E, exponent));

    do {
        MPI_CHK(mpi_gen_prime(&ctx->P, (nbits + 1) >> 1, 0, ctx->f_rng, ctx->p_rng));

        MPI_CHK(mpi_gen_prime(&ctx->Q, (nbits + 1) >> 1, 0, ctx->f_rng, ctx->p_rng));

        if (mpi_cmp_mpi(&ctx->P, &ctx->Q) < 0) {
            mpi_swap(&ctx->P, &ctx->Q);
        }
        if (mpi_cmp_mpi(&ctx->P, &ctx->Q) == 0) {
            continue;
        }
        MPI_CHK(mpi_mul_mpi(&ctx->N, &ctx->P, &ctx->Q));
        if (mpi_msb(&ctx->N) != nbits) {
            continue;
        }
        MPI_CHK(mpi_sub_int(&P1, &ctx->P, 1));
        MPI_CHK(mpi_sub_int(&Q1, &ctx->Q, 1));
        MPI_CHK(mpi_mul_mpi(&H, &P1, &Q1));
        MPI_CHK(mpi_gcd(&G, &ctx->E, &H));

    } while (mpi_cmp_int(&G, 1) != 0);

    /*
       D  = E^-1 mod ((P-1)*(Q-1))
       DP = D mod (P - 1)
       DQ = D mod (Q - 1)
       QP = Q^-1 mod P
     */
    MPI_CHK(mpi_inv_mod(&ctx->D, &ctx->E, &H));
    MPI_CHK(mpi_mod_mpi(&ctx->DP, &ctx->D, &P1));
    MPI_CHK(mpi_mod_mpi(&ctx->DQ, &ctx->D, &Q1));
    MPI_CHK(mpi_inv_mod(&ctx->QP, &ctx->Q, &ctx->P));

    ctx->len = (mpi_msb(&ctx->N) + 7) >> 3;

cleanup:
    mpi_free(&G, &H, &Q1, &P1, NULL);
    if (ret != 0) {
        rsa_free(ctx);
        return EST_ERR_RSA_KEY_GEN_FAILED | ret;
    }
    return 0;
}
#endif

/*
    Check a public RSA key
 */
int rsa_check_pubkey(rsa_context *ctx)
{
    if ((ctx->N.p[0] & 1) == 0 || (ctx->E.p[0] & 1) == 0) {
        return EST_ERR_RSA_KEY_CHECK_FAILED;
    }
    if (mpi_msb(&ctx->N) < 128 || mpi_msb(&ctx->N) > 4096) {
        return EST_ERR_RSA_KEY_CHECK_FAILED;
    }
    if (mpi_msb(&ctx->E) < 2 || mpi_msb(&ctx->E) > 64) {
        return EST_ERR_RSA_KEY_CHECK_FAILED;
    }
    return 0;
}


/*
    Check a private RSA key
 */
int rsa_check_privkey(rsa_context *ctx)
{
    int ret;
    mpi PQ, DE, P1, Q1, H, I, G;

    if ((ret = rsa_check_pubkey(ctx)) != 0) {
        return ret;
    }
    mpi_init(&PQ, &DE, &P1, &Q1, &H, &I, &G, NULL);

    MPI_CHK(mpi_mul_mpi(&PQ, &ctx->P, &ctx->Q));
    MPI_CHK(mpi_mul_mpi(&DE, &ctx->D, &ctx->E));
    MPI_CHK(mpi_sub_int(&P1, &ctx->P, 1));
    MPI_CHK(mpi_sub_int(&Q1, &ctx->Q, 1));
    MPI_CHK(mpi_mul_mpi(&H, &P1, &Q1));
    MPI_CHK(mpi_mod_mpi(&I, &DE, &H));
    MPI_CHK(mpi_gcd(&G, &ctx->E, &H));

    if (mpi_cmp_mpi(&PQ, &ctx->N) == 0 && mpi_cmp_int(&I, 1) == 0 && mpi_cmp_int(&G, 1) == 0) {
        mpi_free(&G, &I, &H, &Q1, &P1, &DE, &PQ, NULL);
        return 0;
    }
cleanup:
    mpi_free(&G, &I, &H, &Q1, &P1, &DE, &PQ, NULL);
    return EST_ERR_RSA_KEY_CHECK_FAILED | ret;
}


/*
    Do an RSA public key operation
 */
int rsa_public(rsa_context *ctx, uchar *input, uchar *output)
{
    int ret, olen;
    mpi T;

    mpi_init(&T, NULL);

    MPI_CHK(mpi_read_binary(&T, input, ctx->len));

    if (mpi_cmp_mpi(&T, &ctx->N) >= 0) {
        mpi_free(&T, NULL);
        return EST_ERR_RSA_BAD_INPUT_DATA;
    }
    olen = ctx->len;
    MPI_CHK(mpi_exp_mod(&T, &T, &ctx->E, &ctx->N, &ctx->RN));
    MPI_CHK(mpi_write_binary(&T, output, olen));

cleanup:
    mpi_free(&T, NULL);
    if (ret != 0) {
        return EST_ERR_RSA_PUBLIC_FAILED | ret;
    }
    return 0;
}


/*
    Do an RSA private key operation
 */
int rsa_private(rsa_context *ctx, uchar *input, uchar *output)
{
    int ret, olen;
    mpi T, T1, T2;

    mpi_init(&T, &T1, &T2, NULL);

    MPI_CHK(mpi_read_binary(&T, input, ctx->len));
    if (mpi_cmp_mpi(&T, &ctx->N) >= 0) {
        mpi_free(&T, NULL);
        return EST_ERR_RSA_BAD_INPUT_DATA;
    }
#if 0
    MPI_CHK(mpi_exp_mod(&T, &T, &ctx->D, &ctx->N, &ctx->RN));
#else
    /*
       faster decryption using the CRT
      
       T1 = input ^ dP mod P
       T2 = input ^ dQ mod Q
     */
    MPI_CHK(mpi_exp_mod(&T1, &T, &ctx->DP, &ctx->P, &ctx->RP));
    MPI_CHK(mpi_exp_mod(&T2, &T, &ctx->DQ, &ctx->Q, &ctx->RQ));

    /*
       T = (T1 - T2) * (Q^-1 mod P) mod P
     */
    MPI_CHK(mpi_sub_mpi(&T, &T1, &T2));
    MPI_CHK(mpi_mul_mpi(&T1, &T, &ctx->QP));
    MPI_CHK(mpi_mod_mpi(&T, &T1, &ctx->P));

    /*
       output = T2 + T * Q
     */
    MPI_CHK(mpi_mul_mpi(&T1, &T, &ctx->Q));
    MPI_CHK(mpi_add_mpi(&T, &T2, &T1));
#endif

    olen = ctx->len;
    MPI_CHK(mpi_write_binary(&T, output, olen));

cleanup:
    mpi_free(&T, &T1, &T2, NULL);
    if (ret != 0)
        return EST_ERR_RSA_PRIVATE_FAILED | ret;

    return 0;
}


/*
    Add the message padding, then do an RSA operation
 */
int rsa_pkcs1_encrypt(rsa_context *ctx, int mode, int ilen, uchar *input, uchar *output)
{
    int nb_pad, olen;
    uchar *p = output;

    olen = ctx->len;

    switch (ctx->padding) {
    case RSA_PKCS_V15:

        if (ilen < 0 || olen < ilen + 11) {
            return EST_ERR_RSA_BAD_INPUT_DATA;
        }
        nb_pad = olen - 3 - ilen;
        *p++ = 0;
        *p++ = RSA_CRYPT;

        while (nb_pad-- > 0) {
            do {
                *p = (uchar)rand();
            } while (*p == 0);
            p++;
        }
        *p++ = 0;
        memcpy(p, input, ilen);
        break;

    default:
        return EST_ERR_RSA_INVALID_PADDING;
    }
    return (mode == RSA_PUBLIC) ? rsa_public(ctx, output, output) : rsa_private(ctx, output, output);
}


/*
    Do an RSA operation, then remove the message padding
 */
int rsa_pkcs1_decrypt(rsa_context *ctx, int mode, int *olen, uchar *input, uchar *output, int output_max_len)
{
    int ret, ilen;
    uchar *p;
    uchar buf[512];

    ilen = ctx->len;

    if (ilen < 16 || ilen > (int)sizeof(buf)) {
        return EST_ERR_RSA_BAD_INPUT_DATA;
    }
    ret = (mode == RSA_PUBLIC) ? rsa_public(ctx, input, buf) : rsa_private(ctx, input, buf);

    if (ret != 0) {
        return ret;
    }
    p = buf;

    switch (ctx->padding) {
    case RSA_PKCS_V15:

        if (*p++ != 0 || *p++ != RSA_CRYPT) {
            return EST_ERR_RSA_INVALID_PADDING;
        }
        while (*p != 0) {
            if (p >= buf + ilen - 1) {
                return EST_ERR_RSA_INVALID_PADDING;
            }
            p++;
        }
        p++;
        break;

    default:
        return EST_ERR_RSA_INVALID_PADDING;
    }
    if (ilen - (int)(p - buf) > output_max_len) {
        return EST_ERR_RSA_OUTPUT_TO_LARGE;
    }
    *olen = ilen - (int)(p - buf);
    memcpy(output, p, *olen);
    return 0;
}


/*
    Do an RSA operation to sign the message digest
 */
int rsa_pkcs1_sign(rsa_context *ctx, int mode, int hash_id, int hashlen, uchar *hash, uchar *sig)
{
    int nb_pad, olen;
    uchar *p = sig;

    olen = ctx->len;

    switch (ctx->padding) {
    case RSA_PKCS_V15:

        switch (hash_id) {
        case RSA_RAW:
            nb_pad = olen - 3 - hashlen;
            break;

        case RSA_MD2:
        case RSA_MD4:
        case RSA_MD5:
            nb_pad = olen - 3 - 34;
            break;

        case RSA_SHA1:
            nb_pad = olen - 3 - 35;
            break;

        default:
            return EST_ERR_RSA_BAD_INPUT_DATA;
        }
        if (nb_pad < 8) {
            return EST_ERR_RSA_BAD_INPUT_DATA;
        }
        *p++ = 0;
        *p++ = RSA_SIGN;
        memset(p, 0xFF, nb_pad);
        p += nb_pad;
        *p++ = 0;
        break;

    default:
        return EST_ERR_RSA_INVALID_PADDING;
    }

    switch (hash_id) {
    case RSA_RAW:
        memcpy(p, hash, hashlen);
        break;

    case RSA_MD2:
        memcpy(p, EST_ASN1_HASH_MDX, 18);
        memcpy(p + 18, hash, 16);
        p[13] = 2;
        break;

    case RSA_MD4:
        memcpy(p, EST_ASN1_HASH_MDX, 18);
        memcpy(p + 18, hash, 16);
        p[13] = 4;
        break;

    case RSA_MD5:
        memcpy(p, EST_ASN1_HASH_MDX, 18);
        memcpy(p + 18, hash, 16);
        p[13] = 5;
        break;

    case RSA_SHA1:
        memcpy(p, EST_ASN1_HASH_SHA1, 15);
        memcpy(p + 15, hash, 20);
        break;

    default:
        return EST_ERR_RSA_BAD_INPUT_DATA;
    }
    return (mode == RSA_PUBLIC) ? rsa_public(ctx, sig, sig) : rsa_private(ctx, sig, sig);
}


/*
    Do an RSA operation and check the message digest
 */
int rsa_pkcs1_verify(rsa_context *ctx, int mode, int hash_id, int hashlen, uchar *hash, uchar *sig)
{
    uchar   *p, c, buf[512];
    int     ret, len, siglen;

    siglen = ctx->len;

    if (siglen < 16 || siglen > (int)sizeof(buf)) {
        return EST_ERR_RSA_BAD_INPUT_DATA;
    }
    ret = (mode == RSA_PUBLIC) ? rsa_public(ctx, sig, buf) : rsa_private(ctx, sig, buf);

    if (ret != 0) {
        return ret;
    }
    p = buf;

    switch (ctx->padding) {
    case RSA_PKCS_V15:
        if (*p++ != 0 || *p++ != RSA_SIGN) {
            return EST_ERR_RSA_INVALID_PADDING;
        }
        while (*p != 0) {
            if (p >= buf + siglen - 1 || *p != 0xFF)
                return EST_ERR_RSA_INVALID_PADDING;
            p++;
        }
        p++;
        break;

    default:
        return EST_ERR_RSA_INVALID_PADDING;
    }
    len = siglen - (int)(p - buf);

    if (len == 34) {
        c = p[13];
        p[13] = 0;

        if (memcmp(p, EST_ASN1_HASH_MDX, 18) != 0) {
            return EST_ERR_RSA_VERIFY_FAILED;
        }
        if ((c == 2 && hash_id == RSA_MD2) || (c == 4 && hash_id == RSA_MD4) || (c == 5 && hash_id == RSA_MD5)) {
            if (memcmp(p + 18, hash, 16) == 0) {
                return 0;
            } else {
                return EST_ERR_RSA_VERIFY_FAILED;
            }
        }
    }
    if (len == 35 && hash_id == RSA_SHA1) {
        if (memcmp(p, EST_ASN1_HASH_SHA1, 15) == 0 && memcmp(p + 15, hash, 20) == 0) {
            return 0;
        } else {
            return EST_ERR_RSA_VERIFY_FAILED;
        }
    }
    if (len == hashlen && hash_id == RSA_RAW) {
        if (memcmp(p, hash, hashlen) == 0) {
            return 0;
        } else {
            return EST_ERR_RSA_VERIFY_FAILED;
        }
    }
    return EST_ERR_RSA_INVALID_PADDING;
}


/*
    Free the components of an RSA key
 */
void rsa_free(rsa_context *ctx)
{
    mpi_free(&ctx->RQ, &ctx->RP, &ctx->RN, &ctx->QP, &ctx->DQ, &ctx->DP, &ctx->Q, &ctx->P, &ctx->D, &ctx->E, &ctx->N, NULL);
}


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



/********* Start of file src/sha1.c ************/


/*
    sha1.c -- FIPS-180-1 compliant SHA-1 implementation

    The SHA-1 standard was published by NIST in 1993.
    http://www.itl.nist.gov/fipspubs/fip180-1.htm

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_SHA1

/*
    32-bit integer manipulation macros (big endian)
 */
#ifndef GET_ULONG_BE
#define GET_ULONG_BE(n,b,i)                     \
    {                                           \
        (n) = ( (ulong) (b)[(i)    ] << 24 )    \
            | ( (ulong) (b)[(i) + 1] << 16 )    \
            | ( (ulong) (b)[(i) + 2] <<  8 )    \
            | ( (ulong) (b)[(i) + 3]       );   \
    }
#endif

#ifndef PUT_ULONG_BE
#define PUT_ULONG_BE(n,b,i)                     \
    {                                           \
        (b)[(i)    ] = (uchar) ( (n) >> 24 );   \
        (b)[(i) + 1] = (uchar) ( (n) >> 16 );   \
        (b)[(i) + 2] = (uchar) ( (n) >>  8 );   \
        (b)[(i) + 3] = (uchar) ( (n)       );   \
    }
#endif


/*
    SHA-1 context setup
 */
void sha1_starts(sha1_context *ctx)
{
    ctx->total[0] = 0;
    ctx->total[1] = 0;
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
}


static void sha1_process(sha1_context *ctx, uchar data[64])
{
    ulong temp, W[16], A, B, C, D, E;

    GET_ULONG_BE(W[0], data, 0);
    GET_ULONG_BE(W[1], data, 4);
    GET_ULONG_BE(W[2], data, 8);
    GET_ULONG_BE(W[3], data, 12);
    GET_ULONG_BE(W[4], data, 16);
    GET_ULONG_BE(W[5], data, 20);
    GET_ULONG_BE(W[6], data, 24);
    GET_ULONG_BE(W[7], data, 28);
    GET_ULONG_BE(W[8], data, 32);
    GET_ULONG_BE(W[9], data, 36);
    GET_ULONG_BE(W[10], data, 40);
    GET_ULONG_BE(W[11], data, 44);
    GET_ULONG_BE(W[12], data, 48);
    GET_ULONG_BE(W[13], data, 52);
    GET_ULONG_BE(W[14], data, 56);
    GET_ULONG_BE(W[15], data, 60);

#define S(x,n) ((x << n) | ((x & 0xFFFFFFFF) >> (32 - n)))

#define R(t)                                            \
    (                                                   \
        temp = W[(t -  3) & 0x0F] ^ W[(t - 8) & 0x0F] ^ \
        W[(t - 14) & 0x0F] ^ W[t & 0x0F],               \
        ( W[t & 0x0F] = S(temp,1) )                     \
        )

#define P(a,b,c,d,e,x)                                  \
    {                                                   \
        e += S(a,5) + F(b,c,d) + K + x; b = S(b,30);    \
    }

    A = ctx->state[0];
    B = ctx->state[1];
    C = ctx->state[2];
    D = ctx->state[3];
    E = ctx->state[4];

#define F(x,y,z) (z ^ (x & (y ^ z)))
#define K 0x5A827999

    P(A, B, C, D, E, W[0]);
    P(E, A, B, C, D, W[1]);
    P(D, E, A, B, C, W[2]);
    P(C, D, E, A, B, W[3]);
    P(B, C, D, E, A, W[4]);
    P(A, B, C, D, E, W[5]);
    P(E, A, B, C, D, W[6]);
    P(D, E, A, B, C, W[7]);
    P(C, D, E, A, B, W[8]);
    P(B, C, D, E, A, W[9]);
    P(A, B, C, D, E, W[10]);
    P(E, A, B, C, D, W[11]);
    P(D, E, A, B, C, W[12]);
    P(C, D, E, A, B, W[13]);
    P(B, C, D, E, A, W[14]);
    P(A, B, C, D, E, W[15]);
    P(E, A, B, C, D, R(16));
    P(D, E, A, B, C, R(17));
    P(C, D, E, A, B, R(18));
    P(B, C, D, E, A, R(19));

#undef K
#undef F

#define F(x,y,z) (x ^ y ^ z)
#define K 0x6ED9EBA1

    P(A, B, C, D, E, R(20));
    P(E, A, B, C, D, R(21));
    P(D, E, A, B, C, R(22));
    P(C, D, E, A, B, R(23));
    P(B, C, D, E, A, R(24));
    P(A, B, C, D, E, R(25));
    P(E, A, B, C, D, R(26));
    P(D, E, A, B, C, R(27));
    P(C, D, E, A, B, R(28));
    P(B, C, D, E, A, R(29));
    P(A, B, C, D, E, R(30));
    P(E, A, B, C, D, R(31));
    P(D, E, A, B, C, R(32));
    P(C, D, E, A, B, R(33));
    P(B, C, D, E, A, R(34));
    P(A, B, C, D, E, R(35));
    P(E, A, B, C, D, R(36));
    P(D, E, A, B, C, R(37));
    P(C, D, E, A, B, R(38));
    P(B, C, D, E, A, R(39));

#undef K
#undef F

#define F(x,y,z) ((x & y) | (z & (x | y)))
#define K 0x8F1BBCDC

    P(A, B, C, D, E, R(40));
    P(E, A, B, C, D, R(41));
    P(D, E, A, B, C, R(42));
    P(C, D, E, A, B, R(43));
    P(B, C, D, E, A, R(44));
    P(A, B, C, D, E, R(45));
    P(E, A, B, C, D, R(46));
    P(D, E, A, B, C, R(47));
    P(C, D, E, A, B, R(48));
    P(B, C, D, E, A, R(49));
    P(A, B, C, D, E, R(50));
    P(E, A, B, C, D, R(51));
    P(D, E, A, B, C, R(52));
    P(C, D, E, A, B, R(53));
    P(B, C, D, E, A, R(54));
    P(A, B, C, D, E, R(55));
    P(E, A, B, C, D, R(56));
    P(D, E, A, B, C, R(57));
    P(C, D, E, A, B, R(58));
    P(B, C, D, E, A, R(59));

#undef K
#undef F

#define F(x,y,z) (x ^ y ^ z)
#define K 0xCA62C1D6

    P(A, B, C, D, E, R(60));
    P(E, A, B, C, D, R(61));
    P(D, E, A, B, C, R(62));
    P(C, D, E, A, B, R(63));
    P(B, C, D, E, A, R(64));
    P(A, B, C, D, E, R(65));
    P(E, A, B, C, D, R(66));
    P(D, E, A, B, C, R(67));
    P(C, D, E, A, B, R(68));
    P(B, C, D, E, A, R(69));
    P(A, B, C, D, E, R(70));
    P(E, A, B, C, D, R(71));
    P(D, E, A, B, C, R(72));
    P(C, D, E, A, B, R(73));
    P(B, C, D, E, A, R(74));
    P(A, B, C, D, E, R(75));
    P(E, A, B, C, D, R(76));
    P(D, E, A, B, C, R(77));
    P(C, D, E, A, B, R(78));
    P(B, C, D, E, A, R(79));

#undef K
#undef F

    ctx->state[0] += A;
    ctx->state[1] += B;
    ctx->state[2] += C;
    ctx->state[3] += D;
    ctx->state[4] += E;
}


/*
    SHA-1 process buffer
 */
void sha1_update(sha1_context *ctx, uchar *input, int ilen)
{
    int fill;
    ulong left;

    if (ilen <= 0) {
        return;
    }
    left = ctx->total[0] & 0x3F;
    fill = 64 - left;

    ctx->total[0] += ilen;
    ctx->total[0] &= 0xFFFFFFFF;

    if (ctx->total[0] < (ulong) ilen) {
        ctx->total[1]++;
    }
    if (left && ilen >= fill) {
        memcpy((void *)(ctx->buffer + left), (void *)input, fill);
        sha1_process(ctx, ctx->buffer);
        input += fill;
        ilen -= fill;
        left = 0;
    }
    while (ilen >= 64) {
        sha1_process(ctx, input);
        input += 64;
        ilen -= 64;
    }
    if (ilen > 0) {
        memcpy((void *)(ctx->buffer + left), (void *)input, ilen);
    }
}


static const uchar sha1_padding[64] = {
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


/*
    SHA-1 final digest
 */
void sha1_finish(sha1_context *ctx, uchar output[20])
{
    ulong last, padn, high, low;
    uchar msglen[8];

    high = (ctx->total[0] >> 29) | (ctx->total[1] << 3);
    low = (ctx->total[0] << 3);

    PUT_ULONG_BE(high, msglen, 0);
    PUT_ULONG_BE(low, msglen, 4);

    last = ctx->total[0] & 0x3F;
    padn = (last < 56) ? (56 - last) : (120 - last);

    sha1_update(ctx, (uchar *)sha1_padding, padn);
    sha1_update(ctx, msglen, 8);

    PUT_ULONG_BE(ctx->state[0], output, 0);
    PUT_ULONG_BE(ctx->state[1], output, 4);
    PUT_ULONG_BE(ctx->state[2], output, 8);
    PUT_ULONG_BE(ctx->state[3], output, 12);
    PUT_ULONG_BE(ctx->state[4], output, 16);
}


/*
    output = SHA-1( input buffer )
 */
void sha1(uchar *input, int ilen, uchar output[20])
{
    sha1_context ctx;

    sha1_starts(&ctx);
    sha1_update(&ctx, input, ilen);
    sha1_finish(&ctx, output);
    memset(&ctx, 0, sizeof(sha1_context));
}


/*
    output = SHA-1( file contents )
 */
int sha1_file(char *path, uchar output[20])
{
    FILE *f;
    size_t n;
    sha1_context ctx;
    uchar buf[1024];

    if ((f = fopen(path, "rb")) == NULL) {
        return 1;
    }
    sha1_starts(&ctx);

    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        sha1_update(&ctx, buf, (int)n);
    }
    sha1_finish(&ctx, output);
    memset(&ctx, 0, sizeof(sha1_context));

    if (ferror(f) != 0) {
        fclose(f);
        return 2;
    }
    fclose(f);
    return 0;
}


/*
    SHA-1 HMAC context setup
 */
void sha1_hmac_starts(sha1_context *ctx, uchar *key, int keylen)
{
    int i;
    uchar sum[20];

    if (keylen > 64) {
        sha1(key, keylen, sum);
        keylen = 20;
        key = sum;
    }
    memset(ctx->ipad, 0x36, 64);
    memset(ctx->opad, 0x5C, 64);

    for (i = 0; i < keylen; i++) {
        ctx->ipad[i] = (uchar)(ctx->ipad[i] ^ key[i]);
        ctx->opad[i] = (uchar)(ctx->opad[i] ^ key[i]);
    }
    sha1_starts(ctx);
    sha1_update(ctx, ctx->ipad, 64);
    memset(sum, 0, sizeof(sum));
}


/*
    SHA-1 HMAC process buffer
 */
void sha1_hmac_update(sha1_context *ctx, uchar *input, int ilen)
{
    sha1_update(ctx, input, ilen);
}


/*
    SHA-1 HMAC final digest
 */
void sha1_hmac_finish(sha1_context *ctx, uchar output[20])
{
    uchar tmpbuf[20];

    sha1_finish(ctx, tmpbuf);
    sha1_starts(ctx);
    sha1_update(ctx, ctx->opad, 64);
    sha1_update(ctx, tmpbuf, 20);
    sha1_finish(ctx, output);
    memset(tmpbuf, 0, sizeof(tmpbuf));
}


/*
    output = HMAC-SHA-1( hmac key, input buffer )
 */
void sha1_hmac(uchar *key, int keylen, uchar *input, int ilen, uchar output[20])
{
    sha1_context ctx;

    sha1_hmac_starts(&ctx, key, keylen);
    sha1_hmac_update(&ctx, input, ilen);
    sha1_hmac_finish(&ctx, output);
    memset(&ctx, 0, sizeof(sha1_context));
}


#undef P
#undef R
#undef SHR
#undef ROTR
#undef S0
#undef S1
#undef S2
#undef S3
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



/********* Start of file src/sha2.c ************/


/*
    sha2.c -- FIPS-180-2 compliant SHA-256 implementation

    The SHA-256 Secure Hash Standard was published by NIST in 2002.
    http://csrc.nist.gov/publications/fips/fips180-2/fips180-2.pdf

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_SHA2

/*
    32-bit integer manipulation macros (big endian)
 */
#ifndef GET_ULONG_BE
#define GET_ULONG_BE(n,b,i)                     \
    {                                           \
        (n) = ( (ulong) (b)[(i)    ] << 24 )    \
            | ( (ulong) (b)[(i) + 1] << 16 )    \
            | ( (ulong) (b)[(i) + 2] <<  8 )    \
            | ( (ulong) (b)[(i) + 3]       );   \
    }
#endif

#ifndef PUT_ULONG_BE
#define PUT_ULONG_BE(n,b,i)                     \
    {                                           \
        (b)[(i)    ] = (uchar) ( (n) >> 24 );   \
        (b)[(i) + 1] = (uchar) ( (n) >> 16 );   \
        (b)[(i) + 2] = (uchar) ( (n) >>  8 );   \
        (b)[(i) + 3] = (uchar) ( (n)       );   \
    }
#endif

/*
    SHA-256 context setup
 */
void sha2_starts(sha2_context *ctx, int is224)
{
    ctx->total[0] = 0;
    ctx->total[1] = 0;

    if (is224 == 0) {
        /* SHA-256 */
        ctx->state[0] = 0x6A09E667;
        ctx->state[1] = 0xBB67AE85;
        ctx->state[2] = 0x3C6EF372;
        ctx->state[3] = 0xA54FF53A;
        ctx->state[4] = 0x510E527F;
        ctx->state[5] = 0x9B05688C;
        ctx->state[6] = 0x1F83D9AB;
        ctx->state[7] = 0x5BE0CD19;
    } else {
        /* SHA-224 */
        ctx->state[0] = 0xC1059ED8;
        ctx->state[1] = 0x367CD507;
        ctx->state[2] = 0x3070DD17;
        ctx->state[3] = 0xF70E5939;
        ctx->state[4] = 0xFFC00B31;
        ctx->state[5] = 0x68581511;
        ctx->state[6] = 0x64F98FA7;
        ctx->state[7] = 0xBEFA4FA4;
    }

    ctx->is224 = is224;
}


static void sha2_process(sha2_context *ctx, uchar data[64])
{
    ulong temp1, temp2, W[64];
    ulong A, B, C, D, E, F, G, H;

    GET_ULONG_BE(W[0], data, 0);
    GET_ULONG_BE(W[1], data, 4);
    GET_ULONG_BE(W[2], data, 8);
    GET_ULONG_BE(W[3], data, 12);
    GET_ULONG_BE(W[4], data, 16);
    GET_ULONG_BE(W[5], data, 20);
    GET_ULONG_BE(W[6], data, 24);
    GET_ULONG_BE(W[7], data, 28);
    GET_ULONG_BE(W[8], data, 32);
    GET_ULONG_BE(W[9], data, 36);
    GET_ULONG_BE(W[10], data, 40);
    GET_ULONG_BE(W[11], data, 44);
    GET_ULONG_BE(W[12], data, 48);
    GET_ULONG_BE(W[13], data, 52);
    GET_ULONG_BE(W[14], data, 56);
    GET_ULONG_BE(W[15], data, 60);

#define  SHR(x,n) ((x & 0xFFFFFFFF) >> n)
#define ROTR(x,n) (SHR(x,n) | (x << (32 - n)))

#define S0(x) (ROTR(x, 7) ^ ROTR(x,18) ^  SHR(x, 3))
#define S1(x) (ROTR(x,17) ^ ROTR(x,19) ^  SHR(x,10))

#define S2(x) (ROTR(x, 2) ^ ROTR(x,13) ^ ROTR(x,22))
#define S3(x) (ROTR(x, 6) ^ ROTR(x,11) ^ ROTR(x,25))

#define F0(x,y,z) ((x & y) | (z & (x | y)))
#define F1(x,y,z) (z ^ (x & (y ^ z)))

#define R(t)                                    \
    (                                           \
        W[t] = S1(W[t -  2]) + W[t -  7] +      \
        S0(W[t - 15]) + W[t - 16]               \
        )

#define P(a,b,c,d,e,f,g,h,x,K)                  \
    {                                           \
        temp1 = h + S3(e) + F1(e,f,g) + K + x;  \
        temp2 = S2(a) + F0(a,b,c);              \
        d += temp1; h = temp1 + temp2;          \
    }

    A = ctx->state[0];
    B = ctx->state[1];
    C = ctx->state[2];
    D = ctx->state[3];
    E = ctx->state[4];
    F = ctx->state[5];
    G = ctx->state[6];
    H = ctx->state[7];

    P(A, B, C, D, E, F, G, H, W[0], 0x428A2F98);
    P(H, A, B, C, D, E, F, G, W[1], 0x71374491);
    P(G, H, A, B, C, D, E, F, W[2], 0xB5C0FBCF);
    P(F, G, H, A, B, C, D, E, W[3], 0xE9B5DBA5);
    P(E, F, G, H, A, B, C, D, W[4], 0x3956C25B);
    P(D, E, F, G, H, A, B, C, W[5], 0x59F111F1);
    P(C, D, E, F, G, H, A, B, W[6], 0x923F82A4);
    P(B, C, D, E, F, G, H, A, W[7], 0xAB1C5ED5);
    P(A, B, C, D, E, F, G, H, W[8], 0xD807AA98);
    P(H, A, B, C, D, E, F, G, W[9], 0x12835B01);
    P(G, H, A, B, C, D, E, F, W[10], 0x243185BE);
    P(F, G, H, A, B, C, D, E, W[11], 0x550C7DC3);
    P(E, F, G, H, A, B, C, D, W[12], 0x72BE5D74);
    P(D, E, F, G, H, A, B, C, W[13], 0x80DEB1FE);
    P(C, D, E, F, G, H, A, B, W[14], 0x9BDC06A7);
    P(B, C, D, E, F, G, H, A, W[15], 0xC19BF174);
    P(A, B, C, D, E, F, G, H, R(16), 0xE49B69C1);
    P(H, A, B, C, D, E, F, G, R(17), 0xEFBE4786);
    P(G, H, A, B, C, D, E, F, R(18), 0x0FC19DC6);
    P(F, G, H, A, B, C, D, E, R(19), 0x240CA1CC);
    P(E, F, G, H, A, B, C, D, R(20), 0x2DE92C6F);
    P(D, E, F, G, H, A, B, C, R(21), 0x4A7484AA);
    P(C, D, E, F, G, H, A, B, R(22), 0x5CB0A9DC);
    P(B, C, D, E, F, G, H, A, R(23), 0x76F988DA);
    P(A, B, C, D, E, F, G, H, R(24), 0x983E5152);
    P(H, A, B, C, D, E, F, G, R(25), 0xA831C66D);
    P(G, H, A, B, C, D, E, F, R(26), 0xB00327C8);
    P(F, G, H, A, B, C, D, E, R(27), 0xBF597FC7);
    P(E, F, G, H, A, B, C, D, R(28), 0xC6E00BF3);
    P(D, E, F, G, H, A, B, C, R(29), 0xD5A79147);
    P(C, D, E, F, G, H, A, B, R(30), 0x06CA6351);
    P(B, C, D, E, F, G, H, A, R(31), 0x14292967);
    P(A, B, C, D, E, F, G, H, R(32), 0x27B70A85);
    P(H, A, B, C, D, E, F, G, R(33), 0x2E1B2138);
    P(G, H, A, B, C, D, E, F, R(34), 0x4D2C6DFC);
    P(F, G, H, A, B, C, D, E, R(35), 0x53380D13);
    P(E, F, G, H, A, B, C, D, R(36), 0x650A7354);
    P(D, E, F, G, H, A, B, C, R(37), 0x766A0ABB);
    P(C, D, E, F, G, H, A, B, R(38), 0x81C2C92E);
    P(B, C, D, E, F, G, H, A, R(39), 0x92722C85);
    P(A, B, C, D, E, F, G, H, R(40), 0xA2BFE8A1);
    P(H, A, B, C, D, E, F, G, R(41), 0xA81A664B);
    P(G, H, A, B, C, D, E, F, R(42), 0xC24B8B70);
    P(F, G, H, A, B, C, D, E, R(43), 0xC76C51A3);
    P(E, F, G, H, A, B, C, D, R(44), 0xD192E819);
    P(D, E, F, G, H, A, B, C, R(45), 0xD6990624);
    P(C, D, E, F, G, H, A, B, R(46), 0xF40E3585);
    P(B, C, D, E, F, G, H, A, R(47), 0x106AA070);
    P(A, B, C, D, E, F, G, H, R(48), 0x19A4C116);
    P(H, A, B, C, D, E, F, G, R(49), 0x1E376C08);
    P(G, H, A, B, C, D, E, F, R(50), 0x2748774C);
    P(F, G, H, A, B, C, D, E, R(51), 0x34B0BCB5);
    P(E, F, G, H, A, B, C, D, R(52), 0x391C0CB3);
    P(D, E, F, G, H, A, B, C, R(53), 0x4ED8AA4A);
    P(C, D, E, F, G, H, A, B, R(54), 0x5B9CCA4F);
    P(B, C, D, E, F, G, H, A, R(55), 0x682E6FF3);
    P(A, B, C, D, E, F, G, H, R(56), 0x748F82EE);
    P(H, A, B, C, D, E, F, G, R(57), 0x78A5636F);
    P(G, H, A, B, C, D, E, F, R(58), 0x84C87814);
    P(F, G, H, A, B, C, D, E, R(59), 0x8CC70208);
    P(E, F, G, H, A, B, C, D, R(60), 0x90BEFFFA);
    P(D, E, F, G, H, A, B, C, R(61), 0xA4506CEB);
    P(C, D, E, F, G, H, A, B, R(62), 0xBEF9A3F7);
    P(B, C, D, E, F, G, H, A, R(63), 0xC67178F2);

    ctx->state[0] += A;
    ctx->state[1] += B;
    ctx->state[2] += C;
    ctx->state[3] += D;
    ctx->state[4] += E;
    ctx->state[5] += F;
    ctx->state[6] += G;
    ctx->state[7] += H;
}


/*
    SHA-256 process buffer
 */
void sha2_update(sha2_context *ctx, uchar *input, int ilen)
{
    int fill;
    ulong left;

    if (ilen <= 0) {
        return;
    }
    left = ctx->total[0] & 0x3F;
    fill = 64 - left;

    ctx->total[0] += ilen;
    ctx->total[0] &= 0xFFFFFFFF;

    if (ctx->total[0] < (ulong) ilen) {
        ctx->total[1]++;
    }
    if (left && ilen >= fill) {
        memcpy((void *)(ctx->buffer + left), (void *)input, fill);
        sha2_process(ctx, ctx->buffer);
        input += fill;
        ilen -= fill;
        left = 0;
    }
    while (ilen >= 64) {
        sha2_process(ctx, input);
        input += 64;
        ilen -= 64;
    }
    if (ilen > 0) {
        memcpy((void *)(ctx->buffer + left), (void *)input, ilen);
    }
}


static const uchar sha2_padding[64] = {
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


/*
    SHA-256 final digest
 */
void sha2_finish(sha2_context *ctx, uchar output[32])
{
    ulong last, padn;
    ulong high, low;
    uchar msglen[8];

    high = (ctx->total[0] >> 29) | (ctx->total[1] << 3);
    low = (ctx->total[0] << 3);

    PUT_ULONG_BE(high, msglen, 0);
    PUT_ULONG_BE(low, msglen, 4);

    last = ctx->total[0] & 0x3F;
    padn = (last < 56) ? (56 - last) : (120 - last);

    sha2_update(ctx, (uchar *)sha2_padding, padn);
    sha2_update(ctx, msglen, 8);

    PUT_ULONG_BE(ctx->state[0], output, 0);
    PUT_ULONG_BE(ctx->state[1], output, 4);
    PUT_ULONG_BE(ctx->state[2], output, 8);
    PUT_ULONG_BE(ctx->state[3], output, 12);
    PUT_ULONG_BE(ctx->state[4], output, 16);
    PUT_ULONG_BE(ctx->state[5], output, 20);
    PUT_ULONG_BE(ctx->state[6], output, 24);

    if (ctx->is224 == 0)
        PUT_ULONG_BE(ctx->state[7], output, 28);
}


/*
    output = SHA-256( input buffer )
 */
void sha2(uchar *input, int ilen, uchar output[32], int is224)
{
    sha2_context ctx;

    sha2_starts(&ctx, is224);
    sha2_update(&ctx, input, ilen);
    sha2_finish(&ctx, output);
    memset(&ctx, 0, sizeof(sha2_context));
}


/*
    output = SHA-256( file contents )
 */
int sha2_file(char *path, uchar output[32], int is224)
{
    FILE *f;
    size_t n;
    sha2_context ctx;
    uchar buf[1024];

    if ((f = fopen(path, "rb")) == NULL) {
        return 1;
    }
    sha2_starts(&ctx, is224);

    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        sha2_update(&ctx, buf, (int)n);
    }
    sha2_finish(&ctx, output);
    memset(&ctx, 0, sizeof(sha2_context));
    if (ferror(f) != 0) {
        fclose(f);
        return 2;
    }
    fclose(f);
    return 0;
}

/*
    SHA-256 HMAC context setup
 */
void sha2_hmac_starts(sha2_context *ctx, uchar *key, int keylen, int is224)
{
    int i;
    uchar sum[32];

    if (keylen > 64) {
        sha2(key, keylen, sum, is224);
        keylen = (is224) ? 28 : 32;
        key = sum;
    }
    memset(ctx->ipad, 0x36, 64);
    memset(ctx->opad, 0x5C, 64);
    for (i = 0; i < keylen; i++) {
        ctx->ipad[i] = (uchar)(ctx->ipad[i] ^ key[i]);
        ctx->opad[i] = (uchar)(ctx->opad[i] ^ key[i]);
    }
    sha2_starts(ctx, is224);
    sha2_update(ctx, ctx->ipad, 64);
    memset(sum, 0, sizeof(sum));
}


/*
    SHA-256 HMAC process buffer
 */
void sha2_hmac_update(sha2_context *ctx, uchar *input, int ilen)
{
    sha2_update(ctx, input, ilen);
}


/*
    SHA-256 HMAC final digest
 */
void sha2_hmac_finish(sha2_context *ctx, uchar output[32])
{
    int is224, hlen;
    uchar tmpbuf[32];

    is224 = ctx->is224;
    hlen = (is224 == 0) ? 32 : 28;

    sha2_finish(ctx, tmpbuf);
    sha2_starts(ctx, is224);
    sha2_update(ctx, ctx->opad, 64);
    sha2_update(ctx, tmpbuf, hlen);
    sha2_finish(ctx, output);
    memset(tmpbuf, 0, sizeof(tmpbuf));
}


/*
    output = HMAC-SHA-256( hmac key, input buffer )
 */
void sha2_hmac(uchar *key, int keylen, uchar *input, int ilen, uchar output[32], int is224)
{
    sha2_context ctx;

    sha2_hmac_starts(&ctx, key, keylen, is224);
    sha2_hmac_update(&ctx, input, ilen);
    sha2_hmac_finish(&ctx, output);
    memset(&ctx, 0, sizeof(sha2_context));
}


#undef P
#undef R
#undef SHR
#undef ROTR
#undef S0
#undef S1
#undef S2
#undef S3
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



/********* Start of file src/sha4.c ************/


/*
    sha4.c -- FIPS-180-2 compliant SHA-384/512 implementation

    The SHA-512 Secure Hash Standard was published by NIST in 2002.
    http://csrc.nist.gov/publications/fips/fips180-2/fips180-2.pdf

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_SHA4

/*
    64-bit integer manipulation macros (big endian)
 */
#ifndef GET_UINT64_BE
#define GET_UINT64_BE(n,b,i)                    \
    {                                           \
        (n) = ( (uint64) (b)[(i)    ] << 56 )   \
            | ( (uint64) (b)[(i) + 1] << 48 )   \
            | ( (uint64) (b)[(i) + 2] << 40 )   \
            | ( (uint64) (b)[(i) + 3] << 32 )   \
            | ( (uint64) (b)[(i) + 4] << 24 )   \
            | ( (uint64) (b)[(i) + 5] << 16 )   \
            | ( (uint64) (b)[(i) + 6] <<  8 )   \
            | ( (uint64) (b)[(i) + 7]       );  \
    }
#endif

#ifndef PUT_UINT64_BE
#define PUT_UINT64_BE(n,b,i)                    \
    {                                           \
        (b)[(i)    ] = (uchar) ( (n) >> 56 );   \
        (b)[(i) + 1] = (uchar) ( (n) >> 48 );   \
        (b)[(i) + 2] = (uchar) ( (n) >> 40 );   \
        (b)[(i) + 3] = (uchar) ( (n) >> 32 );   \
        (b)[(i) + 4] = (uchar) ( (n) >> 24 );   \
        (b)[(i) + 5] = (uchar) ( (n) >> 16 );   \
        (b)[(i) + 6] = (uchar) ( (n) >>  8 );   \
        (b)[(i) + 7] = (uchar) ( (n)       );   \
    }
#endif

/*
    Round constants
 */
static const uint64 K[80] = {
    UL64(0x428A2F98D728AE22), UL64(0x7137449123EF65CD),
    UL64(0xB5C0FBCFEC4D3B2F), UL64(0xE9B5DBA58189DBBC),
    UL64(0x3956C25BF348B538), UL64(0x59F111F1B605D019),
    UL64(0x923F82A4AF194F9B), UL64(0xAB1C5ED5DA6D8118),
    UL64(0xD807AA98A3030242), UL64(0x12835B0145706FBE),
    UL64(0x243185BE4EE4B28C), UL64(0x550C7DC3D5FFB4E2),
    UL64(0x72BE5D74F27B896F), UL64(0x80DEB1FE3B1696B1),
    UL64(0x9BDC06A725C71235), UL64(0xC19BF174CF692694),
    UL64(0xE49B69C19EF14AD2), UL64(0xEFBE4786384F25E3),
    UL64(0x0FC19DC68B8CD5B5), UL64(0x240CA1CC77AC9C65),
    UL64(0x2DE92C6F592B0275), UL64(0x4A7484AA6EA6E483),
    UL64(0x5CB0A9DCBD41FBD4), UL64(0x76F988DA831153B5),
    UL64(0x983E5152EE66DFAB), UL64(0xA831C66D2DB43210),
    UL64(0xB00327C898FB213F), UL64(0xBF597FC7BEEF0EE4),
    UL64(0xC6E00BF33DA88FC2), UL64(0xD5A79147930AA725),
    UL64(0x06CA6351E003826F), UL64(0x142929670A0E6E70),
    UL64(0x27B70A8546D22FFC), UL64(0x2E1B21385C26C926),
    UL64(0x4D2C6DFC5AC42AED), UL64(0x53380D139D95B3DF),
    UL64(0x650A73548BAF63DE), UL64(0x766A0ABB3C77B2A8),
    UL64(0x81C2C92E47EDAEE6), UL64(0x92722C851482353B),
    UL64(0xA2BFE8A14CF10364), UL64(0xA81A664BBC423001),
    UL64(0xC24B8B70D0F89791), UL64(0xC76C51A30654BE30),
    UL64(0xD192E819D6EF5218), UL64(0xD69906245565A910),
    UL64(0xF40E35855771202A), UL64(0x106AA07032BBD1B8),
    UL64(0x19A4C116B8D2D0C8), UL64(0x1E376C085141AB53),
    UL64(0x2748774CDF8EEB99), UL64(0x34B0BCB5E19B48A8),
    UL64(0x391C0CB3C5C95A63), UL64(0x4ED8AA4AE3418ACB),
    UL64(0x5B9CCA4F7763E373), UL64(0x682E6FF3D6B2B8A3),
    UL64(0x748F82EE5DEFB2FC), UL64(0x78A5636F43172F60),
    UL64(0x84C87814A1F0AB72), UL64(0x8CC702081A6439EC),
    UL64(0x90BEFFFA23631E28), UL64(0xA4506CEBDE82BDE9),
    UL64(0xBEF9A3F7B2C67915), UL64(0xC67178F2E372532B),
    UL64(0xCA273ECEEA26619C), UL64(0xD186B8C721C0C207),
    UL64(0xEADA7DD6CDE0EB1E), UL64(0xF57D4F7FEE6ED178),
    UL64(0x06F067AA72176FBA), UL64(0x0A637DC5A2C898A6),
    UL64(0x113F9804BEF90DAE), UL64(0x1B710B35131C471B),
    UL64(0x28DB77F523047D84), UL64(0x32CAAB7B40C72493),
    UL64(0x3C9EBE0A15C9BEBC), UL64(0x431D67C49C100D4C),
    UL64(0x4CC5D4BECB3E42B6), UL64(0x597F299CFC657E2A),
    UL64(0x5FCB6FAB3AD6FAEC), UL64(0x6C44198C4A475817)
};


/*
    SHA-512 context setup
 */
void sha4_starts(sha4_context *ctx, int is384)
{
    ctx->total[0] = 0;
    ctx->total[1] = 0;

    if (is384 == 0) {
        /* SHA-512 */
        ctx->state[0] = UL64(0x6A09E667F3BCC908);
        ctx->state[1] = UL64(0xBB67AE8584CAA73B);
        ctx->state[2] = UL64(0x3C6EF372FE94F82B);
        ctx->state[3] = UL64(0xA54FF53A5F1D36F1);
        ctx->state[4] = UL64(0x510E527FADE682D1);
        ctx->state[5] = UL64(0x9B05688C2B3E6C1F);
        ctx->state[6] = UL64(0x1F83D9ABFB41BD6B);
        ctx->state[7] = UL64(0x5BE0CD19137E2179);
    } else {
        /* SHA-384 */
        ctx->state[0] = UL64(0xCBBB9D5DC1059ED8);
        ctx->state[1] = UL64(0x629A292A367CD507);
        ctx->state[2] = UL64(0x9159015A3070DD17);
        ctx->state[3] = UL64(0x152FECD8F70E5939);
        ctx->state[4] = UL64(0x67332667FFC00B31);
        ctx->state[5] = UL64(0x8EB44A8768581511);
        ctx->state[6] = UL64(0xDB0C2E0D64F98FA7);
        ctx->state[7] = UL64(0x47B5481DBEFA4FA4);
    }

    ctx->is384 = is384;
}


static void sha4_process(sha4_context *ctx, uchar data[128])
{
    int i;
    uint64 temp1, temp2, W[80];
    uint64 A, B, C, D, E, F, G, H;

#define  SHR(x,n) (x >> n)
#define ROTR(x,n) (SHR(x,n) | (x << (64 - n)))

#define S0(x) (ROTR(x, 1) ^ ROTR(x, 8) ^  SHR(x, 7))
#define S1(x) (ROTR(x,19) ^ ROTR(x,61) ^  SHR(x, 6))

#define S2(x) (ROTR(x,28) ^ ROTR(x,34) ^ ROTR(x,39))
#define S3(x) (ROTR(x,14) ^ ROTR(x,18) ^ ROTR(x,41))

#define F0(x,y,z) ((x & y) | (z & (x | y)))
#define F1(x,y,z) (z ^ (x & (y ^ z)))

#define P(a,b,c,d,e,f,g,h,x,K)                  \
    {                                           \
        temp1 = h + S3(e) + F1(e,f,g) + K + x;  \
        temp2 = S2(a) + F0(a,b,c);              \
        d += temp1; h = temp1 + temp2;          \
    }

    for (i = 0; i < 16; i++) {
        GET_UINT64_BE(W[i], data, i << 3);
    }

    for (; i < 80; i++) {
        W[i] = S1(W[i - 2]) + W[i - 7] + S0(W[i - 15]) + W[i - 16];
    }

    A = ctx->state[0];
    B = ctx->state[1];
    C = ctx->state[2];
    D = ctx->state[3];
    E = ctx->state[4];
    F = ctx->state[5];
    G = ctx->state[6];
    H = ctx->state[7];
    i = 0;

    do {
        P(A, B, C, D, E, F, G, H, W[i], K[i]);
        i++;
        P(H, A, B, C, D, E, F, G, W[i], K[i]);
        i++;
        P(G, H, A, B, C, D, E, F, W[i], K[i]);
        i++;
        P(F, G, H, A, B, C, D, E, W[i], K[i]);
        i++;
        P(E, F, G, H, A, B, C, D, W[i], K[i]);
        i++;
        P(D, E, F, G, H, A, B, C, W[i], K[i]);
        i++;
        P(C, D, E, F, G, H, A, B, W[i], K[i]);
        i++;
        P(B, C, D, E, F, G, H, A, W[i], K[i]);
        i++;
    } while (i < 80);

    ctx->state[0] += A;
    ctx->state[1] += B;
    ctx->state[2] += C;
    ctx->state[3] += D;
    ctx->state[4] += E;
    ctx->state[5] += F;
    ctx->state[6] += G;
    ctx->state[7] += H;
}


/*
    SHA-512 process buffer
 */
void sha4_update(sha4_context *ctx, uchar *input, int ilen)
{
    int fill;
    uint64 left;

    if (ilen <= 0)
        return;

    left = ctx->total[0] & 0x7F;
    fill = (int)(128 - left);

    ctx->total[0] += ilen;

    if (ctx->total[0] < (uint64)ilen)
        ctx->total[1]++;

    if (left && ilen >= fill) {
        memcpy((void *)(ctx->buffer + left), (void *)input, fill);
        sha4_process(ctx, ctx->buffer);
        input += fill;
        ilen -= fill;
        left = 0;
    }
    while (ilen >= 128) {
        sha4_process(ctx, input);
        input += 128;
        ilen -= 128;
    }
    if (ilen > 0) {
        memcpy((void *)(ctx->buffer + left), (void *)input, ilen);
    }
}


static const uchar sha4_padding[128] = {
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};


/*
    SHA-512 final digest
 */
void sha4_finish(sha4_context *ctx, uchar output[64])
{
    int last, padn;
    uint64 high, low;
    uchar msglen[16];

    high = (ctx->total[0] >> 61) | (ctx->total[1] << 3);
    low = (ctx->total[0] << 3);

    PUT_UINT64_BE(high, msglen, 0);
    PUT_UINT64_BE(low, msglen, 8);

    last = (int)(ctx->total[0] & 0x7F);
    padn = (last < 112) ? (112 - last) : (240 - last);

    sha4_update(ctx, (uchar *)sha4_padding, padn);
    sha4_update(ctx, msglen, 16);

    PUT_UINT64_BE(ctx->state[0], output, 0);
    PUT_UINT64_BE(ctx->state[1], output, 8);
    PUT_UINT64_BE(ctx->state[2], output, 16);
    PUT_UINT64_BE(ctx->state[3], output, 24);
    PUT_UINT64_BE(ctx->state[4], output, 32);
    PUT_UINT64_BE(ctx->state[5], output, 40);

    if (ctx->is384 == 0) {
        PUT_UINT64_BE(ctx->state[6], output, 48);
        PUT_UINT64_BE(ctx->state[7], output, 56);
    }
}


/*
    output = SHA-512( input buffer )
 */
void sha4(uchar *input, int ilen, uchar output[64], int is384)
{
    sha4_context ctx;

    sha4_starts(&ctx, is384);
    sha4_update(&ctx, input, ilen);
    sha4_finish(&ctx, output);
    memset(&ctx, 0, sizeof(sha4_context));
}


/*
    output = SHA-512( file contents )
 */
int sha4_file(char *path, uchar output[64], int is384)
{
    FILE *f;
    size_t n;
    sha4_context ctx;
    uchar buf[1024];

    if ((f = fopen(path, "rb")) == NULL) {
        return 1;
    }
    sha4_starts(&ctx, is384);

    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        sha4_update(&ctx, buf, (int)n);
    }
    sha4_finish(&ctx, output);
    memset(&ctx, 0, sizeof(sha4_context));

    if (ferror(f) != 0) {
        fclose(f);
        return 2;
    }
    fclose(f);
    return 0;
}


/*
    SHA-512 HMAC context setup
 */
void sha4_hmac_starts(sha4_context *ctx, uchar *key, int keylen, int is384)
{
    int i;
    uchar sum[64];

    if (keylen > 128) {
        sha4(key, keylen, sum, is384);
        keylen = (is384) ? 48 : 64;
        key = sum;
    }
    memset(ctx->ipad, 0x36, 128);
    memset(ctx->opad, 0x5C, 128);
    for (i = 0; i < keylen; i++) {
        ctx->ipad[i] = (uchar)(ctx->ipad[i] ^ key[i]);
        ctx->opad[i] = (uchar)(ctx->opad[i] ^ key[i]);
    }
    sha4_starts(ctx, is384);
    sha4_update(ctx, ctx->ipad, 128);
    memset(sum, 0, sizeof(sum));
}


/*
    SHA-512 HMAC process buffer
 */
void sha4_hmac_update(sha4_context *ctx, uchar *input, int ilen)
{
    sha4_update(ctx, input, ilen);
}


/*
    SHA-512 HMAC final digest
 */
void sha4_hmac_finish(sha4_context *ctx, uchar output[64])
{
    int is384, hlen;
    uchar tmpbuf[64];

    is384 = ctx->is384;
    hlen = (is384 == 0) ? 64 : 48;
    sha4_finish(ctx, tmpbuf);
    sha4_starts(ctx, is384);
    sha4_update(ctx, ctx->opad, 128);
    sha4_update(ctx, tmpbuf, hlen);
    sha4_finish(ctx, output);
    memset(tmpbuf, 0, sizeof(tmpbuf));
}


/*
    output = HMAC-SHA-512( hmac key, input buffer )
 */
void sha4_hmac(uchar *key, int keylen, uchar *input, int ilen, uchar output[64], int is384)
{
    sha4_context ctx;

    sha4_hmac_starts(&ctx, key, keylen, is384);
    sha4_hmac_update(&ctx, input, ilen);
    sha4_hmac_finish(&ctx, output);
    memset(&ctx, 0, sizeof(sha4_context));
}


#undef P
#undef R
#undef SHR
#undef ROTR
#undef S0
#undef S1
#undef S2
#undef S3
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



/********* Start of file src/ssl_cli.c ************/


/*
    ssl_cli.c -- SSLv3/TLSv1 client-side functions

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */



#if ME_EST_CLIENT

static int ssl_write_client_hello(ssl_context * ssl)
{
    int ret, i, n;
    uchar *buf;
    uchar *p;
    time_t t;

    SSL_DEBUG_MSG(2, ("=> write client hello"));

    ssl->major_ver = SSL_MAJOR_VERSION_3;
    ssl->minor_ver = SSL_MINOR_VERSION_0;

    ssl->max_major_ver = SSL_MAJOR_VERSION_3;
    ssl->max_minor_ver = SSL_MINOR_VERSION_1;

    /*
           0  .   0       handshake type
           1  .   3       handshake length
           4  .   5       highest version supported
           6  .   9       current UNIX time
          10  .  37       random bytes
     */
    buf = ssl->out_msg;
    p = buf + 4;

    *p++ = (uchar)ssl->max_major_ver;
    *p++ = (uchar)ssl->max_minor_ver;

    SSL_DEBUG_MSG(3, ("client hello, max version: [%d:%d]", buf[4], buf[5]));

    t = time(NULL);
    *p++ = (uchar)(t >> 24);
    *p++ = (uchar)(t >> 16);
    *p++ = (uchar)(t >> 8);
    *p++ = (uchar)(t);

    SSL_DEBUG_MSG(3, ("client hello, current time: %lu", t));

    for (i = 28; i > 0; i--) {
        *p++ = (uchar)ssl->f_rng(ssl->p_rng);
    }
    memcpy(ssl->randbytes, buf + 6, 32);

    SSL_DEBUG_BUF(3, "client hello, random bytes", buf + 6, 32);

    /*
          38  .  38       session id length
          39  . 39+n  session id
         40+n . 41+n  cipherlist length
         42+n . ..        cipherlist
         ..       . ..    compression alg. (0)
         ..       . ..    extensions (unused)
     */
    n = ssl->session->length;

    if (n < 16 || n > 32 || ssl->resume == 0 || t - ssl->session->start > ssl->timeout) {
        n = 0;
    }
    *p++ = (uchar)n;

    for (i = 0; i < n; i++) {
        *p++ = ssl->session->id[i];
    }
    SSL_DEBUG_MSG(3, ("client hello, session id len.: %d", n));
    SSL_DEBUG_BUF(3, "client hello, session id", buf + 39, n);

    for (n = 0; ssl->ciphers[n] != 0; n++) {}

    *p++ = (uchar)(n >> 7);
    *p++ = (uchar)(n << 1);

    SSL_DEBUG_MSG(3, ("client hello, got %d ciphers", n));

    for (i = 0; i < n; i++) {
        SSL_DEBUG_MSG(3, ("client hello, add cipher: %2d", ssl->ciphers[i]));
        *p++ = (uchar)(ssl->ciphers[i] >> 8);
        *p++ = (uchar)(ssl->ciphers[i]);
    }

    SSL_DEBUG_MSG(3, ("client hello, compress len.: %d", 1));
    SSL_DEBUG_MSG(3, ("client hello, compress alg.: %d", 0));

    *p++ = 1;
    *p++ = SSL_COMPRESS_NULL;

    if (ssl->hostname != NULL) {
        SSL_DEBUG_MSG(3, ("client hello, server name extension: %s", ssl->hostname));

        *p++ = (uchar)(((ssl->hostname_len + 9) >> 8) & 0xFF);
        *p++ = (uchar)(((ssl->hostname_len + 9)) & 0xFF);

        *p++ = (uchar)((TLS_EXT_SERVERNAME >> 8) & 0xFF);
        *p++ = (uchar)((TLS_EXT_SERVERNAME) & 0xFF);

        *p++ = (uchar)(((ssl->hostname_len + 5) >> 8) & 0xFF);
        *p++ = (uchar)(((ssl->hostname_len + 5)) & 0xFF);

        *p++ = (uchar)(((ssl->hostname_len + 3) >> 8) & 0xFF);
        *p++ = (uchar)(((ssl->hostname_len + 3)) & 0xFF);

        *p++ = (uchar)((TLS_EXT_SERVERNAME_HOSTNAME) & 0xFF);
        *p++ = (uchar)((ssl->hostname_len >> 8) & 0xFF);
        *p++ = (uchar)((ssl->hostname_len) & 0xFF);

        memcpy(p, ssl->hostname, ssl->hostname_len);
        p += ssl->hostname_len;
    }
    ssl->out_msglen = p - buf;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0] = SSL_HS_CLIENT_HELLO;
    ssl->state++;

    if ((ret = ssl_write_record(ssl)) != 0) {
        SSL_DEBUG_RET(1, "ssl_write_record", ret);
        return ret;
    }
    SSL_DEBUG_MSG(2, ("<= write client hello"));
    return 0;
}


static int ssl_parse_server_hello(ssl_context * ssl)
{
    time_t t;
    int ret, i, n;
    int ext_len;
    uchar *buf;

    SSL_DEBUG_MSG(2, ("=> parse server hello"));

    /*
           0  .   0       handshake type
           1  .   3       handshake length
           4  .   5       protocol version
           6  .   9       UNIX time()
          10  .  37       random bytes
     */
    buf = ssl->in_msg;

    if ((ret = ssl_read_record(ssl)) != 0) {
        SSL_DEBUG_RET(3, "ssl_read_record", ret);
        return ret;
    }
    if (ssl->in_msgtype != SSL_MSG_HANDSHAKE) {
        SSL_DEBUG_MSG(1, ("bad server hello message"));
        return EST_ERR_SSL_UNEXPECTED_MESSAGE;
    }
    SSL_DEBUG_MSG(3, ("server hello, chosen version: [%d:%d]", buf[4], buf[5]));

    if (ssl->in_hslen < 42 ||
        buf[0] != SSL_HS_SERVER_HELLO || buf[4] != SSL_MAJOR_VERSION_3) {
        SSL_DEBUG_MSG(1, ("bad server hello message"));
        return EST_ERR_SSL_BAD_HS_SERVER_HELLO;
    }
    if (buf[5] != SSL_MINOR_VERSION_0 && buf[5] != SSL_MINOR_VERSION_1) {
        SSL_DEBUG_MSG(1, ("bad server hello message"));
        return EST_ERR_SSL_BAD_HS_SERVER_HELLO;
    }
    ssl->minor_ver = buf[5];

    t = ((time_t) buf[6] << 24) | ((time_t) buf[7] << 16) | ((time_t) buf[8] << 8) | ((time_t) buf[9]);
    memcpy(ssl->randbytes + 32, buf + 6, 32);

    n = buf[38];
    SSL_DEBUG_MSG(3, ("server hello, current time: %lu", t));
    SSL_DEBUG_BUF(3, "server hello, random bytes", buf + 6, 32);

    /*
          38  .  38       session id length
          39  . 38+n  session id
         39+n . 40+n  chosen cipher
         41+n . 41+n  chosen compression alg.
         42+n . 43+n  extensions length
         44+n . 44+n+m extensions
     */
    if (n < 0 || n > 32 || ssl->in_hslen > 42 + n) {
        ext_len = ((buf[42 + n] << 8) | (buf[43 + n])) + 2;
    } else {
        ext_len = 0;
    }
    if (n < 0 || n > 32 || ssl->in_hslen != 42 + n + ext_len) {
        SSL_DEBUG_MSG(1, ("bad server hello message"));
        return EST_ERR_SSL_BAD_HS_SERVER_HELLO;
    }
    i = (buf[39 + n] << 8) | buf[40 + n];
    SSL_DEBUG_MSG(3, ("server hello, session id len.: %d", n));
    SSL_DEBUG_BUF(3, "server hello, session id", buf + 39, n);

    /*
       Check if the session can be resumed
     */
    if (ssl->resume == 0 || n == 0 || ssl->session->cipher != i || ssl->session->length != n ||
            memcmp(ssl->session->id, buf + 39, n) != 0) {
        ssl->state++;
        ssl->resume = 0;
        ssl->session->start = time(NULL);
        ssl->session->cipher = i;
        ssl->session->length = n;
        memcpy(ssl->session->id, buf + 39, n);
    } else {
        ssl->state = SSL_SERVER_CHANGE_CIPHER_SPEC;
        ssl_derive_keys(ssl);
    }
    SSL_DEBUG_MSG(3, ("%s session has been resumed", ssl->resume ? "a" : "no"));
    SSL_DEBUG_MSG(3, ("server hello, chosen cipher: %d", i));
    SSL_DEBUG_MSG(3, ("server hello, compress alg.: %d", buf[41 + n]));

    i = 0;
    while (1) {
        if (ssl->ciphers[i] == 0) {
            SSL_DEBUG_MSG(1, ("bad server hello message"));
            return EST_ERR_SSL_BAD_HS_SERVER_HELLO;
        }
        if (ssl->ciphers[i++] == ssl->session->cipher)
            break;
    }
    if (buf[41 + n] != SSL_COMPRESS_NULL) {
        SSL_DEBUG_MSG(1, ("bad server hello message"));
        return EST_ERR_SSL_BAD_HS_SERVER_HELLO;
    }
    SSL_DEBUG_MSG(2, ("<= parse server hello"));
    return 0;
}


static int ssl_parse_server_key_exchange(ssl_context * ssl)
{
    int ret, n;
    uchar *p, *end;
    uchar hash[36];
    md5_context md5;
    sha1_context sha1;

    SSL_DEBUG_MSG(2, ("=> parse server key exchange"));

    if (ssl->session->cipher != TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA &&
        ssl->session->cipher != TLS_DHE_RSA_WITH_AES_256_CBC_SHA &&
        ssl->session->cipher != TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA) {
        SSL_DEBUG_MSG(2, ("<= skip parse server key exchange"));
        ssl->state++;
        return 0;
    }
#if !ME_EST_DHM
    SSL_DEBUG_MSG(1, ("support for dhm in not available"));
    return EST_ERR_SSL_FEATURE_UNAVAILABLE;
#else
    if ((ret = ssl_read_record(ssl)) != 0) {
        SSL_DEBUG_RET(3, "ssl_read_record", ret);
        return ret;
    }
    if (ssl->in_msgtype != SSL_MSG_HANDSHAKE) {
        SSL_DEBUG_MSG(1, ("bad server key exchange message"));
        return EST_ERR_SSL_UNEXPECTED_MESSAGE;
    }
    if (ssl->in_msg[0] != SSL_HS_SERVER_KEY_EXCHANGE) {
        SSL_DEBUG_MSG(1, ("bad server key exchange message"));
        return EST_ERR_SSL_BAD_HS_SERVER_KEY_EXCHANGE;
    }

    /*
       Ephemeral DH parameters:
      
       struct {
               opaque dh_p<1..2^16-1>;
               opaque dh_g<1..2^16-1>;
               opaque dh_Ys<1..2^16-1>;
       } ServerDHParams;
     */
    p = ssl->in_msg + 4;
    end = ssl->in_msg + ssl->in_hslen;

    if ((ret = dhm_read_params(&ssl->dhm_ctx, &p, end)) != 0) {
        SSL_DEBUG_MSG(1, ("bad server key exchange message"));
        return EST_ERR_SSL_BAD_HS_SERVER_KEY_EXCHANGE;
    }
    if ((int)(end - p) != ssl->peer_cert->rsa.len) {
        SSL_DEBUG_MSG(1, ("bad server key exchange message"));
        return EST_ERR_SSL_BAD_HS_SERVER_KEY_EXCHANGE;
    }
    if (ssl->dhm_ctx.len < 64 || ssl->dhm_ctx.len > 256) {
        SSL_DEBUG_MSG(1, ("bad server key exchange message"));
        return EST_ERR_SSL_BAD_HS_SERVER_KEY_EXCHANGE;
    }
    SSL_DEBUG_MPI(3, "DHM: P ", &ssl->dhm_ctx.P);
    SSL_DEBUG_MPI(3, "DHM: G ", &ssl->dhm_ctx.G);
    SSL_DEBUG_MPI(3, "DHM: GY", &ssl->dhm_ctx.GY);

    /*
       digitally-signed struct {
               opaque md5_hash[16];
               opaque sha_hash[20];
       };
      
       md5_hash MD5(ClientHello.random + ServerHello.random + ServerParams);
       sha_hash SHA(ClientHello.random + ServerHello.random + ServerParams);
     */
    n = ssl->in_hslen - (end - p) - 6;
    md5_starts(&md5);
    md5_update(&md5, ssl->randbytes, 64);
    md5_update(&md5, ssl->in_msg + 4, n);
    md5_finish(&md5, hash);

    sha1_starts(&sha1);
    sha1_update(&sha1, ssl->randbytes, 64);
    sha1_update(&sha1, ssl->in_msg + 4, n);
    sha1_finish(&sha1, hash + 16);

    SSL_DEBUG_BUF(3, "parameters hash", hash, 36);

    if ((ret = rsa_pkcs1_verify(&ssl->peer_cert->rsa, RSA_PUBLIC, RSA_RAW, 36, hash, p)) != 0) {
        SSL_DEBUG_RET(1, "rsa_pkcs1_verify", ret);
        return ret;
    }
    ssl->state++;
    SSL_DEBUG_MSG(2, ("<= parse server key exchange"));
    return 0;
#endif
}


static int ssl_parse_certificate_request(ssl_context * ssl)
{
    int ret;

    SSL_DEBUG_MSG(2, ("=> parse certificate request"));

    /*
           0  .   0       handshake type
           1  .   3       handshake length
           4  .   5       SSL version
           6  .   6       cert type count
           7  .. n-1  cert types
           n  .. n+1  length of all DNs
          n+2 .. n+3  length of DN 1
          n+4 .. ...  Distinguished Name #1
          ... .. ...  length of DN 2, etc.
     */
    if ((ret = ssl_read_record(ssl)) != 0) {
        SSL_DEBUG_RET(3, "ssl_read_record", ret);
        return ret;
    }
    if (ssl->in_msgtype != SSL_MSG_HANDSHAKE) {
        SSL_DEBUG_MSG(1, ("bad certificate request message"));
        return EST_ERR_SSL_UNEXPECTED_MESSAGE;
    }
    ssl->client_auth = 0;
    ssl->state++;

    if (ssl->in_msg[0] == SSL_HS_CERTIFICATE_REQUEST) {
        ssl->client_auth++;
    }
    SSL_DEBUG_MSG(3, ("got %s certificate request", ssl->client_auth ? "a" : "no"));
    SSL_DEBUG_MSG(2, ("<= parse certificate request"));
    return 0;
}


static int ssl_parse_server_hello_done(ssl_context * ssl)
{
    int ret;

    SSL_DEBUG_MSG(2, ("=> parse server hello done"));

    if (ssl->client_auth != 0) {
        if ((ret = ssl_read_record(ssl)) != 0) {
            SSL_DEBUG_RET(3, "ssl_read_record", ret);
            return ret;
        }
        if (ssl->in_msgtype != SSL_MSG_HANDSHAKE) {
            SSL_DEBUG_MSG(1, ("bad server hello done message"));
            return EST_ERR_SSL_UNEXPECTED_MESSAGE;
        }
    }
    if (ssl->in_hslen != 4 || ssl->in_msg[0] != SSL_HS_SERVER_HELLO_DONE) {
        SSL_DEBUG_MSG(1, ("bad server hello done message"));
        return EST_ERR_SSL_BAD_HS_SERVER_HELLO_DONE;
    }
    ssl->state++;
    SSL_DEBUG_MSG(2, ("<= parse server hello done"));
    return 0;
}


static int ssl_write_client_key_exchange(ssl_context * ssl)
{
    int ret, i, n;

    SSL_DEBUG_MSG(2, ("=> write client key exchange"));

    if (ssl->session->cipher == TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA ||
        ssl->session->cipher == TLS_DHE_RSA_WITH_AES_256_CBC_SHA ||
        ssl->session->cipher == TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA) {
#if !ME_EST_DHM
        SSL_DEBUG_MSG(1, ("support for dhm in not available"));
        return EST_ERR_SSL_FEATURE_UNAVAILABLE;
#else
        /*
           DHM key exchange -- send G^X mod P
         */
        n = ssl->dhm_ctx.len;
        ssl->out_msg[4] = (uchar)(n >> 8);
        ssl->out_msg[5] = (uchar)(n);
        i = 6;

        ret = dhm_make_public(&ssl->dhm_ctx, 256, &ssl->out_msg[i], n, ssl->f_rng, ssl->p_rng);
        if (ret != 0) {
            SSL_DEBUG_RET(1, "dhm_make_public", ret);
            return ret;
        }
        SSL_DEBUG_MPI(3, "DHM: X ", &ssl->dhm_ctx.X);
        SSL_DEBUG_MPI(3, "DHM: GX", &ssl->dhm_ctx.GX);
        ssl->pmslen = ssl->dhm_ctx.len;

        if ((ret = dhm_calc_secret(&ssl->dhm_ctx, ssl->premaster, &ssl->pmslen)) != 0) {
            SSL_DEBUG_RET(1, "dhm_calc_secret", ret);
            return ret;
        }
        SSL_DEBUG_MPI(3, "DHM: K ", &ssl->dhm_ctx.K);
#endif
    } else {
        /*
           RSA key exchange -- send rsa_public(pkcs1 v1.5(premaster))
         */
        ssl->premaster[0] = (uchar)ssl->max_major_ver;
        ssl->premaster[1] = (uchar)ssl->max_minor_ver;
        ssl->pmslen = 48;

        for (i = 2; i < ssl->pmslen; i++) {
            ssl->premaster[i] = (uchar)ssl->f_rng(ssl->p_rng);
        }
        i = 4;
        n = ssl->peer_cert->rsa.len;

        if (ssl->minor_ver != SSL_MINOR_VERSION_0) {
            i += 2;
            ssl->out_msg[4] = (uchar)(n >> 8);
            ssl->out_msg[5] = (uchar)(n);
        }
        ret = rsa_pkcs1_encrypt(&ssl->peer_cert->rsa, RSA_PUBLIC, ssl->pmslen, ssl->premaster, ssl->out_msg + i);
        if (ret != 0) {
            SSL_DEBUG_RET(1, "rsa_pkcs1_encrypt", ret);
            return ret;
        }
    }
    ssl_derive_keys(ssl);
    ssl->out_msglen = i + n;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0] = SSL_HS_CLIENT_KEY_EXCHANGE;
    ssl->state++;

    if ((ret = ssl_write_record(ssl)) != 0) {
        SSL_DEBUG_RET(1, "ssl_write_record", ret);
        return ret;
    }
    SSL_DEBUG_MSG(2, ("<= write client key exchange"));
    return 0;
}


static int ssl_write_certificate_verify(ssl_context * ssl)
{
    int ret, n;
    uchar hash[36];

    SSL_DEBUG_MSG(2, ("=> write certificate verify"));

    if (ssl->client_auth == 0) {
        SSL_DEBUG_MSG(2, ("<= skip write certificate verify"));
        ssl->state++;
        return 0;
    }
    if (ssl->rsa_key == NULL) {
        SSL_DEBUG_MSG(1, ("got no private key"));
        return EST_ERR_SSL_PRIVATE_KEY_REQUIRED;
    }
    /*
        Make an RSA signature of the handshake digests
     */
    ssl_calc_verify(ssl, hash);

    n = ssl->rsa_key->len;
    ssl->out_msg[4] = (uchar)(n >> 8);
    ssl->out_msg[5] = (uchar)(n);

    if ((ret = rsa_pkcs1_sign(ssl->rsa_key, RSA_PRIVATE, RSA_RAW, 36, hash, ssl->out_msg + 6)) != 0) {
        SSL_DEBUG_RET(1, "rsa_pkcs1_sign", ret);
        return ret;
    }
    ssl->out_msglen = 6 + n;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0] = SSL_HS_CERTIFICATE_VERIFY;
    ssl->state++;

    if ((ret = ssl_write_record(ssl)) != 0) {
        SSL_DEBUG_RET(1, "ssl_write_record", ret);
        return ret;
    }
    SSL_DEBUG_MSG(2, ("<= write certificate verify"));
    return 0;
}


/*
    SSL handshake -- client side
 */
int ssl_handshake_client(ssl_context * ssl)
{
    int ret = 0;

    SSL_DEBUG_MSG(2, ("=> handshake client"));

    while (ssl->state != SSL_HANDSHAKE_OVER) {
        SSL_DEBUG_MSG(2, ("client state: %d", ssl->state));

        if ((ret = ssl_flush_output(ssl)) != 0) {
            break;
        }
        switch (ssl->state) {
        case SSL_HELLO_REQUEST:
            ssl->state = SSL_CLIENT_HELLO;
            break;

            /*
                ==>       ClientHello
             */
        case SSL_CLIENT_HELLO:
            ret = ssl_write_client_hello(ssl);
            break;

            /*
                <==       ServerHello
                          Certificate
                        ( ServerKeyExchange      )
                        ( CertificateRequest )
                          ServerHelloDone
             */
        case SSL_SERVER_HELLO:
            ret = ssl_parse_server_hello(ssl);
            break;

        case SSL_SERVER_CERTIFICATE:
            ret = ssl_parse_certificate(ssl);
            break;

        case SSL_SERVER_KEY_EXCHANGE:
            ret = ssl_parse_server_key_exchange(ssl);
            break;

        case SSL_CERTIFICATE_REQUEST:
            ret = ssl_parse_certificate_request(ssl);
            break;

        case SSL_SERVER_HELLO_DONE:
            ret = ssl_parse_server_hello_done(ssl);
            break;

            /*
                ==> ( Certificate/Alert )
                      ClientKeyExchange
                    ( CertificateVerify )
                      ChangeCipherSpec
                      Finished
             */
        case SSL_CLIENT_CERTIFICATE:
            ret = ssl_write_certificate(ssl);
            break;

        case SSL_CLIENT_KEY_EXCHANGE:
            ret = ssl_write_client_key_exchange(ssl);
            break;

        case SSL_CERTIFICATE_VERIFY:
            ret = ssl_write_certificate_verify(ssl);
            break;

        case SSL_CLIENT_CHANGE_CIPHER_SPEC:
            ret = ssl_write_change_cipher_spec(ssl);
            break;

        case SSL_CLIENT_FINISHED:
            ret = ssl_write_finished(ssl);
            break;

            /*
                <== ChangeCipherSpec
                    Finished
             */
        case SSL_SERVER_CHANGE_CIPHER_SPEC:
            ret = ssl_parse_change_cipher_spec(ssl);
            break;

        case SSL_SERVER_FINISHED:
            ret = ssl_parse_finished(ssl);
            break;

        case SSL_FLUSH_BUFFERS:
            SSL_DEBUG_MSG(2, ("handshake: done"));
            ssl->state = SSL_HANDSHAKE_OVER;
            break;

        default:
            SSL_DEBUG_MSG(1, ("invalid state %d", ssl->state));
            return EST_ERR_SSL_BAD_INPUT_DATA;
        }
        if (ret != 0) {
            break;
        }
    }
    SSL_DEBUG_MSG(2, ("<= handshake client"));
    return ret;
}

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



/********* Start of file src/ssl_srv.c ************/


/*
    ssl_srv.c -- SSLv3/TLSv1 server-side functions

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_SERVER

static int ssl_parse_client_hello(ssl_context * ssl)
{
    int ret, i, j, n;
    int ciph_len, sess_len;
    int chal_len, comp_len;
    uchar *buf, *p;

    SSL_DEBUG_MSG(2, ("=> parse client hello"));

    if ((ret = ssl_fetch_input(ssl, 5)) != 0) {
        SSL_DEBUG_RET(3, "ssl_fetch_input", ret);
        return ret;
    }
    buf = ssl->in_hdr;

    if ((buf[0] & 0x80) != 0) {
        SSL_DEBUG_BUF(4, "record header", buf, 5);

        SSL_DEBUG_MSG(3, ("client hello v2, message type: %d", buf[2]));
        SSL_DEBUG_MSG(3, ("client hello v2, message len.: %d", ((buf[0] & 0x7F) << 8) | buf[1]));
        SSL_DEBUG_MSG(3, ("client hello v2, max. version: [%d:%d]", buf[3], buf[4]));

        /*
         * SSLv2 Client Hello
         *
         * Record layer:
         *     0  .   1   message length
         *
         * SSL layer:
         *     2  .   2   message type
         *     3  .   4   protocol version
         */
        if (buf[2] != SSL_HS_CLIENT_HELLO || buf[3] != SSL_MAJOR_VERSION_3) {
            SSL_DEBUG_MSG(1, ("bad client hello message"));
            return EST_ERR_SSL_BAD_HS_CLIENT_HELLO;
        }
        n = ((buf[0] << 8) | buf[1]) & 0x7FFF;

        if (n < 17 || n > 512) {
            SSL_DEBUG_MSG(1, ("bad client hello message"));
            return EST_ERR_SSL_BAD_HS_CLIENT_HELLO;
        }
        ssl->max_major_ver = buf[3];
        ssl->max_minor_ver = buf[4];

        ssl->major_ver = SSL_MAJOR_VERSION_3;
        ssl->minor_ver = (buf[4] <= SSL_MINOR_VERSION_1) ? buf[4] : SSL_MINOR_VERSION_1;

        if ((ret = ssl_fetch_input(ssl, 2 + n)) != 0) {
            SSL_DEBUG_RET(3, "ssl_fetch_input", ret);
            return ret;
        }
        md5_update(&ssl->fin_md5, buf + 2, n);
        sha1_update(&ssl->fin_sha1, buf + 2, n);

        buf = ssl->in_msg;
        n = ssl->in_left - 5;

        /*
         *    0  .   1   cipherlist length
         *    2  .   3   session id length
         *    4  .   5   challenge length
         *    6  .  ..   cipherlist
         *   ..  .  ..   session id
         *   ..  .  ..   challenge
         */
        SSL_DEBUG_BUF(4, "record contents", buf, n);

        ciph_len = (buf[0] << 8) | buf[1];
        sess_len = (buf[2] << 8) | buf[3];
        chal_len = (buf[4] << 8) | buf[5];

        SSL_DEBUG_MSG(3, ("ciph_len: %d, sess_len: %d, chal_len: %d", ciph_len, sess_len, chal_len));

        /*
            Make sure each parameter length is valid
         */
        if (ciph_len < 3 || (ciph_len % 3) != 0) {
            SSL_DEBUG_MSG(1, ("bad client hello message"));
            return EST_ERR_SSL_BAD_HS_CLIENT_HELLO;
        }
        if (sess_len < 0 || sess_len > 32) {
            SSL_DEBUG_MSG(1, ("bad client hello message"));
            return EST_ERR_SSL_BAD_HS_CLIENT_HELLO;
        }
        if (chal_len < 8 || chal_len > 32) {
            SSL_DEBUG_MSG(1, ("bad client hello message"));
            return EST_ERR_SSL_BAD_HS_CLIENT_HELLO;
        }
        if (n != 6 + ciph_len + sess_len + chal_len) {
            SSL_DEBUG_MSG(1, ("bad client hello message"));
            return EST_ERR_SSL_BAD_HS_CLIENT_HELLO;
        }
        SSL_DEBUG_BUF(3, "client hello, cipherlist", buf + 6, ciph_len);
        SSL_DEBUG_BUF(3, "client hello, session id", buf + 6 + ciph_len, sess_len);
        SSL_DEBUG_BUF(3, "client hello, challenge", buf + 6 + ciph_len + sess_len, chal_len);

        p = buf + 6 + ciph_len;
        ssl->session->length = sess_len;
        memset(ssl->session->id, 0, sizeof(ssl->session->id));
        memcpy(ssl->session->id, p, ssl->session->length);

        p += sess_len;
        memset(ssl->randbytes, 0, 64);
        memcpy(ssl->randbytes + 32 - chal_len, p, chal_len);

        for (i = 0; ssl->ciphers[i] != 0; i++) {
            for (j = 0, p = buf + 6; j < ciph_len; j += 3, p += 3) {
                if (p[0] == 0 &&
                    p[1] == 0 && p[2] == ssl->ciphers[i])
                    goto have_cipher;
            }
        }
    } else {
        SSL_DEBUG_BUF(4, "record header", buf, 5);
        SSL_DEBUG_MSG(3, ("client hello v3, message type: %d", buf[0]));
        SSL_DEBUG_MSG(3, ("client hello v3, message len.: %d", (buf[3] << 8) | buf[4]));
        SSL_DEBUG_MSG(3, ("client hello v3, protocol ver: [%d:%d]", buf[1], buf[2]));

        /*
         * SSLv3 Client Hello
         *
         * Record layer:
         *     0  .   0   message type
         *     1  .   2   protocol version
         *     3  .   4   message length
         */
        if (buf[0] != SSL_MSG_HANDSHAKE || buf[1] != SSL_MAJOR_VERSION_3) {
            SSL_DEBUG_MSG(1, ("bad client hello message"));
            return EST_ERR_SSL_BAD_HS_CLIENT_HELLO;
        }
        n = (buf[3] << 8) | buf[4];
        if (n < 45 || n > 512) {
            SSL_DEBUG_MSG(1, ("bad client hello message"));
            return EST_ERR_SSL_BAD_HS_CLIENT_HELLO;
        }
        if ((ret = ssl_fetch_input(ssl, 5 + n)) != 0) {
            SSL_DEBUG_RET(3, "ssl_fetch_input", ret);
            return ret;
        }
        buf = ssl->in_msg;
        n = ssl->in_left - 5;

        md5_update(&ssl->fin_md5, buf, n);
        sha1_update(&ssl->fin_sha1, buf, n);

        /*
         * SSL layer:
         *     0  .   0   handshake type
         *     1  .   3   handshake length
         *     4  .   5   protocol version
         *     6  .   9   UNIX time()
         *    10  .  37   random bytes
         *    38  .  38   session id length
         *    39  . 38+x  session id
         *   39+x . 40+x  cipherlist length
         *   41+x .  ..   cipherlist
         *    ..  .  ..   compression alg.
         *    ..  .  ..   extensions
         */
        SSL_DEBUG_BUF(4, "record contents", buf, n);
        SSL_DEBUG_MSG(3, ("client hello v3, handshake type: %d", buf[0]));
        SSL_DEBUG_MSG(3, ("client hello v3, handshake len.: %d", (buf[1] << 16) | (buf[2] << 8) | buf[3]));
        SSL_DEBUG_MSG(3, ("client hello v3, max. version: [%d:%d]", buf[4], buf[5]));

        /*
         * Check the handshake type and protocol version
         */
        if (buf[0] != SSL_HS_CLIENT_HELLO || buf[4] != SSL_MAJOR_VERSION_3) {
            SSL_DEBUG_MSG(1, ("bad client hello message"));
            return EST_ERR_SSL_BAD_HS_CLIENT_HELLO;
        }
        ssl->major_ver = SSL_MAJOR_VERSION_3;
        ssl->minor_ver = (buf[5] <= SSL_MINOR_VERSION_1) ? buf[5] : SSL_MINOR_VERSION_1;
        ssl->max_major_ver = buf[4];
        ssl->max_minor_ver = buf[5];

        memcpy(ssl->randbytes, buf + 6, 32);

        /*
            Check the handshake message length
         */
        if (buf[1] != 0 || n != 4 + ((buf[2] << 8) | buf[3])) {
            SSL_DEBUG_MSG(1, ("bad client hello message"));
            return EST_ERR_SSL_BAD_HS_CLIENT_HELLO;
        }
        /*
            Check the session length
         */
        sess_len = buf[38];

        if (sess_len < 0 || sess_len > 32) {
            SSL_DEBUG_MSG(1, ("bad client hello message"));
            return EST_ERR_SSL_BAD_HS_CLIENT_HELLO;
        }
        ssl->session->length = sess_len;
        memset(ssl->session->id, 0, sizeof(ssl->session->id));
        memcpy(ssl->session->id, buf + 39, ssl->session->length);

        /*
            Check the cipherlist length
         */
        ciph_len = (buf[39 + sess_len] << 8) | (buf[40 + sess_len]);

        if (ciph_len < 2 || ciph_len > 256 || (ciph_len % 2) != 0) {
            SSL_DEBUG_MSG(1, ("bad client hello message"));
            return EST_ERR_SSL_BAD_HS_CLIENT_HELLO;
        }
        /*
            Check the compression algorithms length
         */
        comp_len = buf[41 + sess_len + ciph_len];

        if (comp_len < 1 || comp_len > 16) {
            SSL_DEBUG_MSG(1, ("bad client hello message"));
            return EST_ERR_SSL_BAD_HS_CLIENT_HELLO;
        }
        SSL_DEBUG_BUF(3, "client hello, random bytes", buf + 6, 32);
        SSL_DEBUG_BUF(3, "client hello, session id", buf + 38, sess_len);
        SSL_DEBUG_BUF(3, "client hello, cipherlist", buf + 41 + sess_len, ciph_len);
        SSL_DEBUG_BUF(3, "client hello, compression", buf + 42 + sess_len + ciph_len, comp_len);

        /*
         * Search for a matching cipher
         */
        for (i = 0; ssl->ciphers[i] != 0; i++) {
            for (j = 0, p = buf + 41 + sess_len; j < ciph_len;
                 j += 2, p += 2) {
                if (p[0] == 0 && p[1] == ssl->ciphers[i])
                    goto have_cipher;
            }
        }
    }
    SSL_DEBUG_MSG(1, ("got no ciphers in common"));
    return EST_ERR_SSL_NO_CIPHER_CHOSEN;

have_cipher:
    ssl->session->cipher = ssl->ciphers[i];
    ssl->in_left = 0;
    ssl->state++;
    SSL_DEBUG_MSG(2, ("<= parse client hello"));
    return 0;
}


static int ssl_write_server_hello(ssl_context * ssl)
{
    time_t t;
    int ret, i, n;
    uchar *buf, *p;

    SSL_DEBUG_MSG(2, ("=> write server hello"));

    /*
     *     0  .   0   handshake type
     *     1  .   3   handshake length
     *     4  .   5   protocol version
     *     6  .   9   UNIX time()
     *    10  .  37   random bytes
     */
    buf = ssl->out_msg;
    p = buf + 4;

    *p++ = (uchar)ssl->major_ver;
    *p++ = (uchar)ssl->minor_ver;

    SSL_DEBUG_MSG(3, ("server hello, chosen version: [%d:%d]", buf[4], buf[5]));

    t = time(NULL);
    *p++ = (uchar)(t >> 24);
    *p++ = (uchar)(t >> 16);
    *p++ = (uchar)(t >> 8);
    *p++ = (uchar)(t);

    SSL_DEBUG_MSG(3, ("server hello, current time: %lu", t));

    for (i = 28; i > 0; i--) {
        *p++ = (uchar)ssl->f_rng(ssl->p_rng);
    }
    memcpy(ssl->randbytes + 32, buf + 6, 32);

    SSL_DEBUG_BUF(3, "server hello, random bytes", buf + 6, 32);

    /*
     *    38  .  38   session id length
     *    39  . 38+n  session id
     *   39+n . 40+n  chosen cipher
     *   41+n . 41+n  chosen compression alg.
     */
    ssl->session->length = n = 32;
    *p++ = (uchar)ssl->session->length;

    if (ssl->s_get == NULL || ssl->s_get(ssl) != 0) {
        /*
         * Not found, create a new session id
         */
        ssl->resume = 0;
        ssl->state++;

        for (i = 0; i < n; i++) {
            ssl->session->id[i] = (uchar)ssl->f_rng(ssl->p_rng);
        }
    } else {
        /*
         * Found a matching session, resume it
         */
        ssl->resume = 1;
        ssl->state = SSL_SERVER_CHANGE_CIPHER_SPEC;
        ssl_derive_keys(ssl);
    }

    memcpy(p, ssl->session->id, ssl->session->length);
    p += ssl->session->length;

    SSL_DEBUG_MSG(3, ("server hello, session id len.: %d", n));
    SSL_DEBUG_BUF(3, "server hello, session id", buf + 39, n);
    SSL_DEBUG_MSG(3, ("%s session has been resumed", ssl->resume ? "a" : "no"));

    *p++ = (uchar)(ssl->session->cipher >> 8);
    *p++ = (uchar)(ssl->session->cipher);
    *p++ = SSL_COMPRESS_NULL;

    SSL_DEBUG_MSG(3, ("server hello, chosen cipher: %d", ssl->session->cipher));
    SSL_DEBUG_MSG(3, ("server hello, compress alg.: %d", 0));

    ssl->out_msglen = p - buf;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0] = SSL_HS_SERVER_HELLO;

    ret = ssl_write_record(ssl);

    SSL_DEBUG_MSG(2, ("<= write server hello"));
    return ret;
}


static int ssl_write_certificate_request(ssl_context * ssl)
{
    int ret, n;
    uchar *buf, *p;
    x509_cert *crt;

    SSL_DEBUG_MSG(2, ("=> write certificate request"));
    ssl->state++;

    if (ssl->authmode == SSL_VERIFY_NO_CHECK) {
        SSL_DEBUG_MSG(2, ("<= skip write certificate request"));
        return 0;
    }
    /*
     *     0  .   0   handshake type
     *     1  .   3   handshake length
     *     4  .   4   cert type count
     *     5  .. n-1  cert types
     *     n  .. n+1  length of all DNs
     *    n+2 .. n+3  length of DN 1
     *    n+4 .. ...  Distinguished Name #1
     *    ... .. ...  length of DN 2, etc.
     */
    buf = ssl->out_msg;
    p = buf + 4;

    /*
     * At the moment, only RSA certificates are supported
     */
    *p++ = 1;
    *p++ = 1;

    p += 2;
    crt = ssl->ca_chain;

    while (crt != NULL && crt->next != NULL) {
        if (p - buf > 4096) {
            break;
        }
        n = crt->subject_raw.len;
        *p++ = (uchar)(n >> 8);
        *p++ = (uchar)(n);
        memcpy(p, crt->subject_raw.p, n);

        SSL_DEBUG_BUF(3, "requested DN", p, n);
        p += n;
        crt = crt->next;
    }
    ssl->out_msglen = n = p - buf;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0] = SSL_HS_CERTIFICATE_REQUEST;
    ssl->out_msg[6] = (uchar)((n - 8) >> 8);
    ssl->out_msg[7] = (uchar)((n - 8));

    ret = ssl_write_record(ssl);
    SSL_DEBUG_MSG(2, ("<= write certificate request"));
    return ret;
}

static int ssl_write_server_key_exchange(ssl_context * ssl)
{
    int ret, n;
    uchar hash[36];
    md5_context md5;
    sha1_context sha1;

    SSL_DEBUG_MSG(2, ("=> write server key exchange"));

    if (ssl->session->cipher != TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA &&
        ssl->session->cipher != TLS_DHE_RSA_WITH_AES_256_CBC_SHA &&
        ssl->session->cipher != TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA) {
        SSL_DEBUG_MSG(2, ("<= skip write server key exchange"));
        ssl->state++;
        return 0;
    }
#if !ME_EST_DHM
    SSL_DEBUG_MSG(1, ("support for dhm is not available"));
    return EST_ERR_SSL_FEATURE_UNAVAILABLE;
#else
    /*
     * Ephemeral DH parameters:
     *
     * struct {
     *     opaque dh_p<1..2^16-1>;
     *     opaque dh_g<1..2^16-1>;
     *     opaque dh_Ys<1..2^16-1>;
     * } ServerDHParams;
     */
    if ((ret = dhm_make_params(&ssl->dhm_ctx, 256, ssl->out_msg + 4, &n, ssl->f_rng, ssl->p_rng)) != 0) {
        SSL_DEBUG_RET(1, "dhm_make_params", ret);
        return ret;
    }

    SSL_DEBUG_MPI(3, "DHM: X ", &ssl->dhm_ctx.X);
    SSL_DEBUG_MPI(3, "DHM: P ", &ssl->dhm_ctx.P);
    SSL_DEBUG_MPI(3, "DHM: G ", &ssl->dhm_ctx.G);
    SSL_DEBUG_MPI(3, "DHM: GX", &ssl->dhm_ctx.GX);

    /*
     * digitally-signed struct {
     *     opaque md5_hash[16];
     *     opaque sha_hash[20];
     * };
     *
     * md5_hash
     *     MD5(ClientHello.random + ServerHello.random
     *                            + ServerParams);
     * sha_hash
     *     SHA(ClientHello.random + ServerHello.random
     *                            + ServerParams);
     */
    md5_starts(&md5);
    md5_update(&md5, ssl->randbytes, 64);
    md5_update(&md5, ssl->out_msg + 4, n);
    md5_finish(&md5, hash);

    sha1_starts(&sha1);
    sha1_update(&sha1, ssl->randbytes, 64);
    sha1_update(&sha1, ssl->out_msg + 4, n);
    sha1_finish(&sha1, hash + 16);

    SSL_DEBUG_BUF(3, "parameters hash", hash, 36);

    ssl->out_msg[4 + n] = (uchar)(ssl->rsa_key->len >> 8);
    ssl->out_msg[5 + n] = (uchar)(ssl->rsa_key->len);

    ret = rsa_pkcs1_sign(ssl->rsa_key, RSA_PRIVATE, RSA_RAW, 36, hash, ssl->out_msg + 6 + n);
    if (ret != 0) {
        SSL_DEBUG_RET(1, "rsa_pkcs1_sign", ret);
        return ret;
    }

    SSL_DEBUG_BUF(3, "my RSA sig", ssl->out_msg + 6 + n, ssl->rsa_key->len);

    ssl->out_msglen = 6 + n + ssl->rsa_key->len;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0] = SSL_HS_SERVER_KEY_EXCHANGE;

    ssl->state++;

    if ((ret = ssl_write_record(ssl)) != 0) {
        SSL_DEBUG_RET(1, "ssl_write_record", ret);
        return ret;
    }
    SSL_DEBUG_MSG(2, ("<= write server key exchange"));
    return 0;
#endif
}

static int ssl_write_server_hello_done(ssl_context * ssl)
{
    int ret;

    SSL_DEBUG_MSG(2, ("=> write server hello done"));

    ssl->out_msglen = 4;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0] = SSL_HS_SERVER_HELLO_DONE;

    ssl->state++;

    if ((ret = ssl_write_record(ssl)) != 0) {
        SSL_DEBUG_RET(1, "ssl_write_record", ret);
        return ret;
    }
    SSL_DEBUG_MSG(2, ("<= write server hello done"));
    return 0;
}

static int ssl_parse_client_key_exchange(ssl_context * ssl)
{
    int ret, i, n;

    SSL_DEBUG_MSG(2, ("=> parse client key exchange"));

    if ((ret = ssl_read_record(ssl)) != 0) {
        SSL_DEBUG_RET(3, "ssl_read_record", ret);
        return ret;
    }
    if (ssl->in_msgtype != SSL_MSG_HANDSHAKE) {
        SSL_DEBUG_MSG(1, ("bad client key exchange message"));
        return EST_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE;
    }
    if (ssl->in_msg[0] != SSL_HS_CLIENT_KEY_EXCHANGE) {
        SSL_DEBUG_MSG(1, ("bad client key exchange message"));
        return EST_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE;
    }
    if (ssl->session->cipher == TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA ||
        ssl->session->cipher == TLS_DHE_RSA_WITH_AES_256_CBC_SHA ||
        ssl->session->cipher == TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA) {
#if !ME_EST_DHM
        SSL_DEBUG_MSG(1, ("support for dhm is not available"));
        return EST_ERR_SSL_FEATURE_UNAVAILABLE;
#else
        /*
           Receive G^Y mod P, premaster = (G^Y)^X mod P
         */
        n = (ssl->in_msg[4] << 8) | ssl->in_msg[5];

        if (n < 1 || n > ssl->dhm_ctx.len || n + 6 != ssl->in_hslen) {
            SSL_DEBUG_MSG(1, ("bad client key exchange message"));
            return EST_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE;
        }
        if ((ret = dhm_read_public(&ssl->dhm_ctx, ssl->in_msg + 6, n)) != 0) {
            SSL_DEBUG_RET(1, "dhm_read_public", ret);
            return EST_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE | ret;
        }
        SSL_DEBUG_MPI(3, "DHM: GY", &ssl->dhm_ctx.GY);
        ssl->pmslen = ssl->dhm_ctx.len;

        if ((ret = dhm_calc_secret(&ssl->dhm_ctx, ssl->premaster, &ssl->pmslen)) != 0) {
            SSL_DEBUG_RET(1, "dhm_calc_secret", ret);
            return EST_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE | ret;
        }

        SSL_DEBUG_MPI(3, "DHM: K ", &ssl->dhm_ctx.K);
#endif
    } else {
        /*
           Decrypt the premaster using own private RSA key
         */
        i = 4;
        n = ssl->rsa_key->len;
        ssl->pmslen = 48;

        if (ssl->minor_ver != SSL_MINOR_VERSION_0) {
            i += 2;
            if (ssl->in_msg[4] != ((n >> 8) & 0xFF) ||
                ssl->in_msg[5] != ((n) & 0xFF)) {
                SSL_DEBUG_MSG(1, ("bad client key exchange message"));
                return EST_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE;
            }
        }
        if (ssl->in_hslen != i + n) {
            SSL_DEBUG_MSG(1, ("bad client key exchange message"));
            return EST_ERR_SSL_BAD_HS_CLIENT_KEY_EXCHANGE;
        }
        ret = rsa_pkcs1_decrypt(ssl->rsa_key, RSA_PRIVATE, &ssl->pmslen, ssl->in_msg + i, ssl->premaster,
                sizeof(ssl->premaster));

        if (ret != 0 || ssl->pmslen != 48 || ssl->premaster[0] != ssl->max_major_ver ||
                ssl->premaster[1] != ssl->max_minor_ver) {
            SSL_DEBUG_MSG(1, ("bad client key exchange message"));

            /*
               Protection against Bleichenbacher's attack: invalid PKCS#1 v1.5 padding must not cause
               the connection to end immediately; instead, send a bad_record_mac later in the handshake.
             */
            ssl->pmslen = 48;
            for (i = 0; i < ssl->pmslen; i++) {
                ssl->premaster[i] = (uchar)ssl->f_rng(ssl->p_rng);
            }
        }
    }
    ssl_derive_keys(ssl);

    if (ssl->s_set != NULL) {
        ssl->s_set(ssl);
    }
    ssl->state++;
    SSL_DEBUG_MSG(2, ("<= parse client key exchange"));
    return 0;
}


static int ssl_parse_certificate_verify(ssl_context * ssl)
{
    int n1, n2, ret;
    uchar hash[36];

    SSL_DEBUG_MSG(2, ("=> parse certificate verify"));

    if (ssl->peer_cert == NULL) {
        SSL_DEBUG_MSG(2, ("<= skip parse certificate verify"));
        ssl->state++;
        return 0;
    }
    ssl_calc_verify(ssl, hash);

    if ((ret = ssl_read_record(ssl)) != 0) {
        SSL_DEBUG_RET(3, "ssl_read_record", ret);
        return ret;
    }
    ssl->state++;

    if (ssl->in_msgtype != SSL_MSG_HANDSHAKE) {
        SSL_DEBUG_MSG(1, ("bad certificate verify message"));
        return EST_ERR_SSL_BAD_HS_CERTIFICATE_VERIFY;
    }
    if (ssl->in_msg[0] != SSL_HS_CERTIFICATE_VERIFY) {
        SSL_DEBUG_MSG(1, ("bad certificate verify message"));
        return EST_ERR_SSL_BAD_HS_CERTIFICATE_VERIFY;
    }
    n1 = ssl->peer_cert->rsa.len;
    n2 = (ssl->in_msg[4] << 8) | ssl->in_msg[5];

    if (n1 + 6 != ssl->in_hslen || n1 != n2) {
        SSL_DEBUG_MSG(1, ("bad certificate verify message"));
        return EST_ERR_SSL_BAD_HS_CERTIFICATE_VERIFY;
    }
    ret = rsa_pkcs1_verify(&ssl->peer_cert->rsa, RSA_PUBLIC, RSA_RAW, 36, hash, ssl->in_msg + 6);
    if (ret != 0) {
        SSL_DEBUG_RET(1, "rsa_pkcs1_verify", ret);
        return ret;
    }
    SSL_DEBUG_MSG(2, ("<= parse certificate verify"));
    return 0;
}


/*
    SSL handshake -- server side
 */
int ssl_handshake_server(ssl_context * ssl)
{
    int ret = 0;

    SSL_DEBUG_MSG(2, ("=> handshake server"));

    while (ssl->state != SSL_HANDSHAKE_OVER) {
        SSL_DEBUG_MSG(2, ("server state: %d", ssl->state));

        if ((ret = ssl_flush_output(ssl)) != 0) {
            break;
        }
        switch (ssl->state) {
        case SSL_HELLO_REQUEST:
            ssl->state = SSL_CLIENT_HELLO;
            break;

        /*
            <==   ClientHello
         */
        case SSL_CLIENT_HELLO:
            ret = ssl_parse_client_hello(ssl);
            break;

        /*
            ==>   ServerHello
                  Certificate
                ( ServerKeyExchange  )
                ( CertificateRequest )
                  ServerHelloDone
         */
        case SSL_SERVER_HELLO:
            ret = ssl_write_server_hello(ssl);
            break;

        case SSL_SERVER_CERTIFICATE:
            ret = ssl_write_certificate(ssl);
            break;

        case SSL_SERVER_KEY_EXCHANGE:
            ret = ssl_write_server_key_exchange(ssl);
            break;

        case SSL_CERTIFICATE_REQUEST:
            ret = ssl_write_certificate_request(ssl);
            break;

        case SSL_SERVER_HELLO_DONE:
            ret = ssl_write_server_hello_done(ssl);
            break;

        /*
            <== ( Certificate/Alert  )
                  ClientKeyExchange
                ( CertificateVerify  )
                  ChangeCipherSpec
                  Finished
         */
        case SSL_CLIENT_CERTIFICATE:
            ret = ssl_parse_certificate(ssl);
            break;

        case SSL_CLIENT_KEY_EXCHANGE:
            ret = ssl_parse_client_key_exchange(ssl);
            break;

        case SSL_CERTIFICATE_VERIFY:
            ret = ssl_parse_certificate_verify(ssl);
            break;

        case SSL_CLIENT_CHANGE_CIPHER_SPEC:
            ret = ssl_parse_change_cipher_spec(ssl);
            break;

        case SSL_CLIENT_FINISHED:
            ret = ssl_parse_finished(ssl);
            break;

        /*
            ==>   ChangeCipherSpec
                  Finished
         */
        case SSL_SERVER_CHANGE_CIPHER_SPEC:
            ret = ssl_write_change_cipher_spec(ssl);
            break;

        case SSL_SERVER_FINISHED:
            ret = ssl_write_finished(ssl);
            break;

        case SSL_FLUSH_BUFFERS:
            SSL_DEBUG_MSG(2, ("handshake: done"));
            ssl->state = SSL_HANDSHAKE_OVER;
            break;

        default:
            SSL_DEBUG_MSG(1, ("invalid state %d", ssl->state));
            return EST_ERR_SSL_BAD_INPUT_DATA;
        }
        if (ret != 0) {
            break;
        }
    }
    SSL_DEBUG_MSG(2, ("<= handshake server"));
    return ret;
}

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



/********* Start of file src/ssl_tls.c ************/


/*
    ssl_tls.c -- SSLv3/TLSv1 shared functions

    The SSL 3.0 specification was drafted by Netscape in 1996, and became an IETF standard in 1999.
    http://wp.netscape.com/eng/ssl3/
    http://www.ietf.org/rfc/rfc2246.txt
    http://www.ietf.org/rfc/rfc4346.txt

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_SSL

int ssl_default_ciphers[] = {
#if ME_EST_DHM
#if ME_EST_AES
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA,
#endif
#if ME_EST_CAMELLIA
    TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA,
#endif
#if ME_EST_DES
    TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA,
#endif
#endif
#if ME_EST_AES
    TLS_RSA_WITH_AES_128_CBC_SHA,
    TLS_RSA_WITH_AES_256_CBC_SHA,
#endif
#if ME_EST_CAMELLIA
    TLS_RSA_WITH_CAMELLIA_128_CBC_SHA,
    TLS_RSA_WITH_CAMELLIA_256_CBC_SHA,
#endif
#if ME_EST_DES
    TLS_RSA_WITH_3DES_EDE_CBC_SHA,
#endif
#if ME_EST_RC4
    TLS_RSA_WITH_RC4_128_SHA,
    TLS_RSA_WITH_RC4_128_MD5,
#endif
    0
};


/* 
    Supported ciphers. Ordered in preference order
    See: http://www.iana.org/assignments/tls-parameters/tls-parameters.xml
 */
static EstCipher cipherList[] = {
#if ME_EST_RSA && ME_EST_AES
    { "TLS_RSA_WITH_AES_128_CBC_SHA",           TLS_RSA_WITH_AES_128_CBC_SHA            /* 0x2F */ },
    { "TLS_RSA_WITH_AES_256_CBC_SHA",           TLS_RSA_WITH_AES_256_CBC_SHA            /* 0x35 */ },
#endif
#if ME_EST_RSA && ME_EST_RC4
    { "TLS_RSA_WITH_RC4_128_SHA",               TLS_RSA_WITH_RC4_128_SHA                /* 0x05 */ },
#endif
#if ME_EST_RSA && ME_EST_DES
    { "TLS_RSA_WITH_3DES_EDE_CBC_SHA",          TLS_RSA_WITH_3DES_EDE_CBC_SHA           /* 0x0A */ },
#endif
#if ME_EST_RSA && ME_EST_RC4
    { "TLS_RSA_WITH_RC4_128_MD5",               TLS_RSA_WITH_RC4_128_MD5                /* 0x04 */ },
#endif
#if ME_EST_RSA && ME_EST_AES && ME_EST_DHM
    { "TLS_DHE_RSA_WITH_AES_256_CBC_SHA",       TLS_DHE_RSA_WITH_AES_256_CBC_SHA        /* 0x39 */ },
#endif
#if ME_EST_RSA && ME_EST_DES && ME_EST_DHM
    { "TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA",      TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA       /* 0x16 */ },
#endif
#if ME_EST_RSA && ME_EST_CAMELLIA
    { "TLS_RSA_WITH_CAMELLIA_128_CBC_SHA",      TLS_RSA_WITH_CAMELLIA_128_CBC_SHA       /* 0x41 */ },
    { "TLS_RSA_WITH_CAMELLIA_256_CBC_SHA",      TLS_RSA_WITH_CAMELLIA_256_CBC_SHA       /* 0x88 */ },
    #if ME_EST_DHM
    { "TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA",  TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA   /* 0x84 */ },
    #endif
#endif
    { 0, 0 },
};


char *ssl_get_cipher(ssl_context *ssl)
{
    EstCipher   *cp;

    for (cp = cipherList; cp->name; cp++) {
        if (cp->code == ssl->session->cipher) {
            return cp->name;
        }
    }
    return 0;
}


static char *stok(char *str, char *delim, char **last)
{
    char    *start, *end;
    ssize   i;

    start = (str || *last == 0) ? str : *last;

    if (start == 0) {
        if (last) {
            *last = 0;
        }
        return 0;
    }
    i = strspn(start, delim);
    start += i;
    if (*start == '\0') {
        if (last) {
            *last = 0;
        }
        return 0;
    }
    end = strpbrk(start, delim);
    if (end) {
        *end++ = '\0';
        i = strspn(end, delim);
        end += i;
    }
    if (last) {
        *last = end;
    }
    return start;
}


int *ssl_create_ciphers(cchar *cipherSuite)
{
    EstCipher   *cp;
    char        *suite, *cipher, *next;
    int         nciphers, i, *ciphers;

    nciphers = sizeof(cipherList) / sizeof(EstCipher);
    ciphers = malloc((nciphers + 1) * sizeof(int));

    if (!cipherSuite || cipherSuite[0] == '\0') {
        memcpy(ciphers, ssl_default_ciphers, nciphers * sizeof(int));
        ciphers[nciphers] = 0;
        return ciphers;
    }
    suite = strdup(cipherSuite);
    i = 0;
    while ((cipher = stok(suite, ":, \t", &next)) != 0) {
        for (cp = cipherList; cp->name; cp++) {
            if (strcmp(cp->name, cipher) == 0) {
                break;
            }
        }
        if (cp) {
            ciphers[i++] = cp->code;
        } else {
            // SSL_DEBUG_MSG(0, ("Requested cipher %s is not supported", cipher));
        }
        suite = 0;
    }
    if (i == 0) {
        for (i = 0; i < nciphers; i++) {
            ciphers[i] = ssl_default_ciphers[i];
        }
        ciphers[i] = 0;
    }
    return ciphers;
}


/*
   Key material generation
 */
static int tls1_prf(uchar *secret, int slen, char *label, uchar *random, int rlen, uchar *dstbuf, int dlen)
{
    int nb, hs;
    int i, j, k;
    uchar *S1, *S2;
    uchar tmp[128];
    uchar h_i[20];

    if (sizeof(tmp) < 20 + strlen(label) + rlen) {
        return EST_ERR_SSL_BAD_INPUT_DATA;
    }
    hs = (slen + 1) / 2;
    S1 = secret;
    S2 = secret + slen - hs;

    nb = strlen(label);
    memcpy(tmp + 20, label, nb);
    memcpy(tmp + 20 + nb, random, rlen);
    nb += rlen;

    /*
       First compute P_md5(secret,label+random)[0..dlen]
     */
    md5_hmac(S1, hs, tmp + 20, nb, 4 + tmp);

    for (i = 0; i < dlen; i += 16) {
        md5_hmac(S1, hs, 4 + tmp, 16 + nb, h_i);
        md5_hmac(S1, hs, 4 + tmp, 16, 4 + tmp);
        k = (i + 16 > dlen) ? dlen % 16 : 16;
        for (j = 0; j < k; j++) {
            dstbuf[i + j] = h_i[j];
        }
    }

    /*
        XOR out with P_sha1(secret,label+random)[0..dlen]
     */
    sha1_hmac(S2, hs, tmp + 20, nb, tmp);

    for (i = 0; i < dlen; i += 20) {
        sha1_hmac(S2, hs, tmp, 20 + nb, h_i);
        sha1_hmac(S2, hs, tmp, 20, tmp);
        k = (i + 20 > dlen) ? dlen % 20 : 20;
        for (j = 0; j < k; j++) {
            dstbuf[i + j] = (uchar)(dstbuf[i + j] ^ h_i[j]);
        }
    }
    memset(tmp, 0, sizeof(tmp));
    memset(h_i, 0, sizeof(h_i));
    return 0;
}


int ssl_derive_keys(ssl_context * ssl)
{
    int i;
    md5_context md5;
    sha1_context sha1;
    uchar tmp[64];
    uchar padding[16];
    uchar sha1sum[20];
    uchar keyblk[256];
    uchar *key1;
    uchar *key2;

    SSL_DEBUG_MSG(2, ("=> derive keys"));

    /*
       SSLv3:
         master =
           MD5(premaster + SHA1( 'A'   + premaster + randbytes)) +
           MD5(premaster + SHA1( 'BB'  + premaster + randbytes)) +
           MD5(premaster + SHA1( 'CCC' + premaster + randbytes))
      
       TLSv1:
         master = PRF( premaster, "master secret", randbytes )[0..47]
     */
    if (ssl->resume == 0) {
        int len = ssl->pmslen;

        SSL_DEBUG_BUF(3, "premaster secret", ssl->premaster, len);

        if (ssl->minor_ver == SSL_MINOR_VERSION_0) {
            for (i = 0; i < 3; i++) {
                memset(padding, 'A' + i, 1 + i);

                sha1_starts(&sha1);
                sha1_update(&sha1, padding, 1 + i);
                sha1_update(&sha1, ssl->premaster, len);
                sha1_update(&sha1, ssl->randbytes, 64);
                sha1_finish(&sha1, sha1sum);

                md5_starts(&md5);
                md5_update(&md5, ssl->premaster, len);
                md5_update(&md5, sha1sum, 20);
                md5_finish(&md5, ssl->session->master + i * 16);
            }
        } else {
            tls1_prf(ssl->premaster, len, "master secret", ssl->randbytes, 64, ssl->session->master, 48);
        }
        memset(ssl->premaster, 0, sizeof(ssl->premaster));
    } else {
        SSL_DEBUG_MSG(3, ("no premaster (session resumed)"));
    }

    /*
       Swap the client and server random values.
     */
    memcpy(tmp, ssl->randbytes, 64);
    memcpy(ssl->randbytes, tmp + 32, 32);
    memcpy(ssl->randbytes + 32, tmp, 32);
    memset(tmp, 0, sizeof(tmp));

    /*
        SSLv3:
          key block =
            MD5( master + SHA1( 'A'    + master + randbytes ) ) +
            MD5( master + SHA1( 'BB'   + master + randbytes ) ) +
            MD5( master + SHA1( 'CCC'  + master + randbytes ) ) +
            MD5( master + SHA1( 'DDDD' + master + randbytes ) ) +
            ...
        TLSv1:
          key block = PRF( master, "key expansion", randbytes )
     */
    if (ssl->minor_ver == SSL_MINOR_VERSION_0) {
        for (i = 0; i < 16; i++) {
            memset(padding, 'A' + i, 1 + i);

            sha1_starts(&sha1);
            sha1_update(&sha1, padding, 1 + i);
            sha1_update(&sha1, ssl->session->master, 48);
            sha1_update(&sha1, ssl->randbytes, 64);
            sha1_finish(&sha1, sha1sum);

            md5_starts(&md5);
            md5_update(&md5, ssl->session->master, 48);
            md5_update(&md5, sha1sum, 20);
            md5_finish(&md5, keyblk + i * 16);
        }
        memset(&md5, 0, sizeof(md5));
        memset(&sha1, 0, sizeof(sha1));
        memset(padding, 0, sizeof(padding));
        memset(sha1sum, 0, sizeof(sha1sum));
    } else {
        tls1_prf(ssl->session->master, 48, "key expansion", ssl->randbytes, 64, keyblk, 256);
    }
    SSL_DEBUG_MSG(3, ("cipher = %s", ssl_get_cipher(ssl)));
    SSL_DEBUG_BUF(3, "master secret", ssl->session->master, 48);
    SSL_DEBUG_BUF(4, "random bytes", ssl->randbytes, 64);
    SSL_DEBUG_BUF(4, "key block", keyblk, 256);

    memset(ssl->randbytes, 0, sizeof(ssl->randbytes));

    /*
     * Determine the appropriate key, IV and MAC length.
     */
    switch (ssl->session->cipher) {
#if ME_EST_RC4
    case TLS_RSA_WITH_RC4_128_MD5:
        ssl->keylen = 16;
        ssl->minlen = 16;
        ssl->ivlen = 0;
        ssl->maclen = 16;
        break;

    case TLS_RSA_WITH_RC4_128_SHA:
        ssl->keylen = 16;
        ssl->minlen = 20;
        ssl->ivlen = 0;
        ssl->maclen = 20;
        break;
#endif

#if ME_EST_DES
    case TLS_RSA_WITH_3DES_EDE_CBC_SHA:
    case TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA:
        ssl->keylen = 24;
        ssl->minlen = 24;
        ssl->ivlen = 8;
        ssl->maclen = 20;
        break;
#endif

#if ME_EST_AES
    case TLS_RSA_WITH_AES_128_CBC_SHA:
        ssl->keylen = 16;
        ssl->minlen = 32;
        ssl->ivlen = 16;
        ssl->maclen = 20;
        break;

    case TLS_RSA_WITH_AES_256_CBC_SHA:
    case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
        ssl->keylen = 32;
        ssl->minlen = 32;
        ssl->ivlen = 16;
        ssl->maclen = 20;
        break;
#endif

#if ME_EST_CAMELLIA
    case TLS_RSA_WITH_CAMELLIA_128_CBC_SHA:
        ssl->keylen = 16;
        ssl->minlen = 32;
        ssl->ivlen = 16;
        ssl->maclen = 20;
        break;

    case TLS_RSA_WITH_CAMELLIA_256_CBC_SHA:
    case TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA:
        ssl->keylen = 32;
        ssl->minlen = 32;
        ssl->ivlen = 16;
        ssl->maclen = 20;
        break;
#endif

    default:
        SSL_DEBUG_MSG(1, ("cipher %s is not available", ssl_get_cipher(ssl)));
        return EST_ERR_SSL_FEATURE_UNAVAILABLE;
    }

    SSL_DEBUG_MSG(3, ("keylen: %d, minlen: %d, ivlen: %d, maclen: %d", ssl->keylen, ssl->minlen, ssl->ivlen, ssl->maclen));

    /*
        Finally setup the cipher contexts, IVs and MAC secrets.
     */
    if (ssl->endpoint == SSL_IS_CLIENT) {
        key1 = keyblk + ssl->maclen * 2;
        key2 = keyblk + ssl->maclen * 2 + ssl->keylen;

        memcpy(ssl->mac_enc, keyblk, ssl->maclen);
        memcpy(ssl->mac_dec, keyblk + ssl->maclen, ssl->maclen);

        memcpy(ssl->iv_enc, key2 + ssl->keylen, ssl->ivlen);
        memcpy(ssl->iv_dec, key2 + ssl->keylen + ssl->ivlen, ssl->ivlen);

    } else {
        key1 = keyblk + ssl->maclen * 2 + ssl->keylen;
        key2 = keyblk + ssl->maclen * 2;

        memcpy(ssl->mac_dec, keyblk, ssl->maclen);
        memcpy(ssl->mac_enc, keyblk + ssl->maclen, ssl->maclen);

        memcpy(ssl->iv_dec, key1 + ssl->keylen, ssl->ivlen);
        memcpy(ssl->iv_enc, key1 + ssl->keylen + ssl->ivlen, ssl->ivlen);
    }

    switch (ssl->session->cipher) {
#if ME_EST_RC4
    case TLS_RSA_WITH_RC4_128_MD5:
    case TLS_RSA_WITH_RC4_128_SHA:
        arc4_setup((arc4_context *) ssl->ctx_enc, key1, ssl->keylen);
        arc4_setup((arc4_context *) ssl->ctx_dec, key2, ssl->keylen);
        break;
#endif

#if ME_EST_DES
    case TLS_RSA_WITH_3DES_EDE_CBC_SHA:
    case TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA:
        des3_set3key_enc((des3_context *) ssl->ctx_enc, key1);
        des3_set3key_dec((des3_context *) ssl->ctx_dec, key2);
        break;
#endif

#if ME_EST_AES
    case TLS_RSA_WITH_AES_128_CBC_SHA:
        aes_setkey_enc((aes_context *) ssl->ctx_enc, key1, 128);
        aes_setkey_dec((aes_context *) ssl->ctx_dec, key2, 128);
        break;

    case TLS_RSA_WITH_AES_256_CBC_SHA:
    case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
        aes_setkey_enc((aes_context *) ssl->ctx_enc, key1, 256);
        aes_setkey_dec((aes_context *) ssl->ctx_dec, key2, 256);
        break;
#endif

#if ME_EST_CAMELLIA
    case TLS_RSA_WITH_CAMELLIA_128_CBC_SHA:
        camellia_setkey_enc((camellia_context *) ssl->ctx_enc, key1, 128);
        camellia_setkey_dec((camellia_context *) ssl->ctx_dec, key2, 128);
        break;

    case TLS_RSA_WITH_CAMELLIA_256_CBC_SHA:
    case TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA:
        camellia_setkey_enc((camellia_context *) ssl->ctx_enc, key1, 256);
        camellia_setkey_dec((camellia_context *) ssl->ctx_dec, key2, 256);
        break;
#endif

    default:
        return EST_ERR_SSL_FEATURE_UNAVAILABLE;
    }
    memset(keyblk, 0, sizeof(keyblk));
    SSL_DEBUG_MSG(2, ("<= derive keys"));
    return 0;
}

void ssl_calc_verify(ssl_context * ssl, uchar hash[36])
{
    md5_context md5;
    sha1_context sha1;
    uchar pad_1[48];
    uchar pad_2[48];

    SSL_DEBUG_MSG(2, ("=> calc verify"));

    memcpy(&md5, &ssl->fin_md5, sizeof(md5_context));
    memcpy(&sha1, &ssl->fin_sha1, sizeof(sha1_context));

    if (ssl->minor_ver == SSL_MINOR_VERSION_0) {
        memset(pad_1, 0x36, 48);
        memset(pad_2, 0x5C, 48);

        md5_update(&md5, ssl->session->master, 48);
        md5_update(&md5, pad_1, 48);
        md5_finish(&md5, hash);

        md5_starts(&md5);
        md5_update(&md5, ssl->session->master, 48);
        md5_update(&md5, pad_2, 48);
        md5_update(&md5, hash, 16);
        md5_finish(&md5, hash);

        sha1_update(&sha1, ssl->session->master, 48);
        sha1_update(&sha1, pad_1, 40);
        sha1_finish(&sha1, hash + 16);

        sha1_starts(&sha1);
        sha1_update(&sha1, ssl->session->master, 48);
        sha1_update(&sha1, pad_2, 40);
        sha1_update(&sha1, hash + 16, 20);
        sha1_finish(&sha1, hash + 16);

    } else {
        /* TLSv1 */
        md5_finish(&md5, hash);
        sha1_finish(&sha1, hash + 16);
    }
    SSL_DEBUG_BUF(3, "calculated verify result", hash, 36);
    SSL_DEBUG_MSG(2, ("<= calc verify"));
    return;
}


/*
    SSLv3.0 MAC functions
 */
static void ssl_mac_md5(uchar *secret, uchar *buf, int len, uchar *ctr, int type)
{
    uchar header[11];
    uchar padding[48];
    md5_context md5;

    memcpy(header, ctr, 8);
    header[8] = (uchar)type;
    header[9] = (uchar)(len >> 8);
    header[10] = (uchar)(len);

    memset(padding, 0x36, 48);
    md5_starts(&md5);
    md5_update(&md5, secret, 16);
    md5_update(&md5, padding, 48);
    md5_update(&md5, header, 11);
    md5_update(&md5, buf, len);
    md5_finish(&md5, buf + len);

    memset(padding, 0x5C, 48);
    md5_starts(&md5);
    md5_update(&md5, secret, 16);
    md5_update(&md5, padding, 48);
    md5_update(&md5, buf + len, 16);
    md5_finish(&md5, buf + len);
}


static void ssl_mac_sha1(uchar *secret, uchar *buf, int len, uchar *ctr, int type)
{
    uchar header[11];
    uchar padding[40];
    sha1_context sha1;

    memcpy(header, ctr, 8);
    header[8] = (uchar)type;
    header[9] = (uchar)(len >> 8);
    header[10] = (uchar)(len);

    memset(padding, 0x36, 40);
    sha1_starts(&sha1);
    sha1_update(&sha1, secret, 20);
    sha1_update(&sha1, padding, 40);
    sha1_update(&sha1, header, 11);
    sha1_update(&sha1, buf, len);
    sha1_finish(&sha1, buf + len);

    memset(padding, 0x5C, 40);
    sha1_starts(&sha1);
    sha1_update(&sha1, secret, 20);
    sha1_update(&sha1, padding, 40);
    sha1_update(&sha1, buf + len, 20);
    sha1_finish(&sha1, buf + len);
}

/*
    Encryption/decryption functions
 */
static int ssl_encrypt_buf(ssl_context * ssl)
{
    int i, padlen;

    SSL_DEBUG_MSG(2, ("=> encrypt buf"));

    /*
        Add MAC then encrypt
     */
    if (ssl->minor_ver == SSL_MINOR_VERSION_0) {
        if (ssl->maclen == 16) {
            ssl_mac_md5(ssl->mac_enc, ssl->out_msg, ssl->out_msglen, ssl->out_ctr, ssl->out_msgtype);
        }
        if (ssl->maclen == 20) {
            ssl_mac_sha1(ssl->mac_enc, ssl->out_msg, ssl->out_msglen, ssl->out_ctr, ssl->out_msgtype);
        }

    } else {
        if (ssl->maclen == 16) {
            md5_hmac(ssl->mac_enc, 16, ssl->out_ctr, ssl->out_msglen + 13, ssl->out_msg + ssl->out_msglen);
        }
        if (ssl->maclen == 20) {
            sha1_hmac(ssl->mac_enc, 20, ssl->out_ctr, ssl->out_msglen + 13, ssl->out_msg + ssl->out_msglen);
        }
    }
    SSL_DEBUG_BUF(4, "computed mac", ssl->out_msg + ssl->out_msglen, ssl->maclen);

    ssl->out_msglen += ssl->maclen;

    for (i = 7; i >= 0; i--) {
        if (++ssl->out_ctr[i] != 0) {
            break;
        }
    }
    if (ssl->ivlen == 0) {
#if ME_EST_RC4
        padlen = 0;
        SSL_DEBUG_MSG(3, ("before encrypt: msglen = %d, " "including %d bytes of padding", ssl->out_msglen, 0));
        SSL_DEBUG_BUF(4, "before encrypt: output payload", ssl->out_msg, ssl->out_msglen);
        arc4_crypt((arc4_context *) ssl->ctx_enc, ssl->out_msg, ssl->out_msglen);
#else
        return EST_ERR_SSL_FEATURE_UNAVAILABLE;
#endif
    } else {
        padlen = ssl->ivlen - (ssl->out_msglen + 1) % ssl->ivlen;
        if (padlen == ssl->ivlen) {
            padlen = 0;
        }
        for (i = 0; i <= padlen; i++) {
            ssl->out_msg[ssl->out_msglen + i] = (uchar)padlen;
        }
        ssl->out_msglen += padlen + 1;

        SSL_DEBUG_MSG(3, ("before encrypt: msglen = %d, " "including %d bytes of padding", ssl->out_msglen, padlen + 1));
        SSL_DEBUG_BUF(4, "before encrypt: output payload", ssl->out_msg, ssl->out_msglen);

        switch (ssl->ivlen) {
        case 8:
#if ME_EST_DES
            des3_crypt_cbc((des3_context *) ssl->ctx_enc, DES_ENCRYPT, ssl->out_msglen,
                ssl->iv_enc, ssl->out_msg, ssl->out_msg);
            break;
#endif

        case 16:
#if ME_EST_AES
            if (ssl->session->cipher == TLS_RSA_WITH_AES_128_CBC_SHA ||
                ssl->session->cipher == TLS_RSA_WITH_AES_256_CBC_SHA ||
                ssl->session->cipher == TLS_DHE_RSA_WITH_AES_256_CBC_SHA) {
                    aes_crypt_cbc((aes_context *) ssl->ctx_enc, AES_ENCRYPT, ssl->out_msglen, ssl->iv_enc, ssl->out_msg,
                          ssl->out_msg);
                break;
            }
#endif

#if ME_EST_CAMELLIA
            if (ssl->session->cipher == TLS_RSA_WITH_CAMELLIA_128_CBC_SHA ||
                ssl->session->cipher == TLS_RSA_WITH_CAMELLIA_256_CBC_SHA ||
                ssl->session->cipher == TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA) {
                    camellia_crypt_cbc((camellia_context *) ssl->ctx_enc, CAMELLIA_ENCRYPT,
                    ssl->out_msglen, ssl->iv_enc, ssl->out_msg, ssl->out_msg);
                break;
            }
#endif
        default:
            return EST_ERR_SSL_FEATURE_UNAVAILABLE;
        }
    }
    SSL_DEBUG_MSG(2, ("<= encrypt buf"));
    return 0;
}


static int ssl_decrypt_buf(ssl_context * ssl)
{
    int i, padlen;
    uchar tmp[20];

    SSL_DEBUG_MSG(2, ("=> decrypt buf"));

    if (ssl->in_msglen < ssl->minlen) {
        SSL_DEBUG_MSG(1, ("in_msglen (%d) < minlen (%d)", ssl->in_msglen, ssl->minlen));
        return EST_ERR_SSL_INVALID_MAC;
    }
    if (ssl->ivlen == 0) {
#if ME_EST_RC4
        padlen = 0;
        arc4_crypt((arc4_context *) ssl->ctx_dec, ssl->in_msg, ssl->in_msglen);
#else
        return EST_ERR_SSL_FEATURE_UNAVAILABLE;
#endif
    } else {
        /*
            Decrypt and check the padding
         */
        if (ssl->in_msglen % ssl->ivlen != 0) {
            SSL_DEBUG_MSG(1, ("msglen (%d) %% ivlen (%d) != 0", ssl->in_msglen, ssl->ivlen));
            return EST_ERR_SSL_INVALID_MAC;
        }
        switch (ssl->ivlen) {
#if ME_EST_DES
        case 8:
            des3_crypt_cbc((des3_context *) ssl->ctx_dec, DES_DECRYPT, ssl->in_msglen,
                ssl->iv_dec, ssl->in_msg, ssl->in_msg);
            break;
#endif

        case 16:
#if ME_EST_AES
            if (ssl->session->cipher == TLS_RSA_WITH_AES_128_CBC_SHA ||
                ssl->session->cipher == TLS_RSA_WITH_AES_256_CBC_SHA ||
                ssl->session->cipher == TLS_DHE_RSA_WITH_AES_256_CBC_SHA) {
                aes_crypt_cbc((aes_context *) ssl->ctx_dec, AES_DECRYPT, ssl->in_msglen, ssl->iv_dec, ssl->in_msg,
                    ssl->in_msg);
                break;
            }
#endif

#if ME_EST_CAMELLIA
            if (ssl->session->cipher == TLS_RSA_WITH_CAMELLIA_128_CBC_SHA ||
                ssl->session->cipher == TLS_RSA_WITH_CAMELLIA_256_CBC_SHA ||
                ssl->session->cipher == TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA) {
                camellia_crypt_cbc((camellia_context *) ssl->ctx_dec, CAMELLIA_DECRYPT,
                    ssl->in_msglen, ssl->iv_dec, ssl->in_msg, ssl->in_msg);
                break;
            }
#endif
        default:
            return EST_ERR_SSL_FEATURE_UNAVAILABLE;
        }
        padlen = 1 + ssl->in_msg[ssl->in_msglen - 1];

        if (ssl->minor_ver == SSL_MINOR_VERSION_0) {
            if (padlen > ssl->ivlen) {
                SSL_DEBUG_MSG(1, ("bad padding length: is %d, " "should be no more than %d", padlen, ssl->ivlen));
                padlen = 0;
            }
        } else {
            /*
                TLSv1: always check the padding
             */
            for (i = 1; i <= padlen; i++) {
                if (ssl->in_msg[ssl->in_msglen - i] != padlen - 1) {
                    SSL_DEBUG_MSG(1, ("bad padding byte: should be %02x, but is %02x",
                       padlen - 1, ssl-> in_msg[ssl->in_msglen - i]));
                    padlen = 0;
                }
            }
        }
    }
    SSL_DEBUG_BUF(4, "raw buffer after decryption", ssl->in_msg, ssl->in_msglen);

    /*
        Always compute the MAC (RFC4346, CBCTIME)
     */
    ssl->in_msglen -= (ssl->maclen + padlen);

    ssl->in_hdr[3] = (uchar)(ssl->in_msglen >> 8);
    ssl->in_hdr[4] = (uchar)(ssl->in_msglen);

    memcpy(tmp, ssl->in_msg + ssl->in_msglen, 20);

    if (ssl->minor_ver == SSL_MINOR_VERSION_0) {
        if (ssl->maclen == 16) {
            ssl_mac_md5(ssl->mac_dec, ssl->in_msg, ssl->in_msglen, ssl->in_ctr, ssl->in_msgtype);
        } else {
            ssl_mac_sha1(ssl->mac_dec, ssl->in_msg, ssl->in_msglen, ssl->in_ctr, ssl->in_msgtype);
        }
    } else {
        if (ssl->maclen == 16) {
            md5_hmac(ssl->mac_dec, 16, ssl->in_ctr, ssl->in_msglen + 13, ssl->in_msg + ssl->in_msglen);
        } else {
            sha1_hmac(ssl->mac_dec, 20, ssl->in_ctr, ssl->in_msglen + 13, ssl->in_msg + ssl->in_msglen);
        }
    }
    SSL_DEBUG_BUF(4, "message  mac", tmp, ssl->maclen);
    SSL_DEBUG_BUF(4, "computed mac", ssl->in_msg + ssl->in_msglen, ssl->maclen);

    if (memcmp(tmp, ssl->in_msg + ssl->in_msglen, ssl->maclen) != 0) {
        SSL_DEBUG_MSG(1, ("message mac does not match"));
        return EST_ERR_SSL_INVALID_MAC;
    }

    /*
        Finally check the padding length; bad padding will produce the same error as an invalid MAC.
     */
    if (ssl->ivlen != 0 && padlen == 0) {
        return EST_ERR_SSL_INVALID_MAC;
    }

    if (ssl->in_msglen == 0) {
        ssl->nb_zero++;
        /*
            Three or more empty messages may be a DoS attack (excessive CPU consumption).
         */
        if (ssl->nb_zero > 3) {
            SSL_DEBUG_MSG(1, ("received four consecutive empty " "messages, possible DoS attack"));
            return EST_ERR_SSL_INVALID_MAC;
        }
    } else
        ssl->nb_zero = 0;

    for (i = 7; i >= 0; i--) {
        if (++ssl->in_ctr[i] != 0) {
            break;
        }
    }
    SSL_DEBUG_MSG(2, ("<= decrypt buf"));
    return 0;
}


/*
    Fill the input message buffer
 */
int ssl_fetch_input(ssl_context * ssl, int nb_want)
{
    int ret, len;

    SSL_DEBUG_MSG(2, ("=> fetch input"));

    while (ssl->in_left < nb_want) {
        len = nb_want - ssl->in_left;
        ret = ssl->f_recv(ssl->p_recv, ssl->in_hdr + ssl->in_left, len);

        SSL_DEBUG_MSG(4, ("in_left: %d, nb_want: %d", ssl->in_left, nb_want));
        SSL_DEBUG_RET(4, "ssl->f_recv", ret);

        if (ret < 0) {
            return ret;
        }
        ssl->in_left += ret;
    }
    SSL_DEBUG_MSG(4, ("<= fetch input"));
    return 0;
}


/*
    Flush any data not yet written
 */
int ssl_flush_output(ssl_context * ssl)
{
    int ret;
    uchar *buf;

    SSL_DEBUG_MSG(2, ("=> flush output"));

    while (ssl->out_left > 0) {
        SSL_DEBUG_MSG(2, ("message length: %d, out_left: %d", 5 + ssl->out_msglen, ssl->out_left));

        buf = ssl->out_hdr + 5 + ssl->out_msglen - ssl->out_left;
        ret = ssl->f_send(ssl->p_send, buf, ssl->out_left);
        SSL_DEBUG_RET(2, "ssl->f_send", ret);

        if (ret <= 0) {
            return ret;
        }
        ssl->out_left -= ret;
    }
    SSL_DEBUG_MSG(2, ("<= flush output"));
    return 0;
}


/*
    Record layer functions
 */
int ssl_write_record(ssl_context * ssl)
{
    int ret, len = ssl->out_msglen;

    SSL_DEBUG_MSG(2, ("=> write record"));

    ssl->out_hdr[0] = (uchar)ssl->out_msgtype;
    ssl->out_hdr[1] = (uchar)ssl->major_ver;
    ssl->out_hdr[2] = (uchar)ssl->minor_ver;
    ssl->out_hdr[3] = (uchar)(len >> 8);
    ssl->out_hdr[4] = (uchar)(len);

    if (ssl->out_msgtype == SSL_MSG_HANDSHAKE) {
        ssl->out_msg[1] = (uchar)((len - 4) >> 16);
        ssl->out_msg[2] = (uchar)((len - 4) >> 8);
        ssl->out_msg[3] = (uchar)((len - 4));
        md5_update(&ssl->fin_md5, ssl->out_msg, len);
        sha1_update(&ssl->fin_sha1, ssl->out_msg, len);
    }
    if (ssl->do_crypt != 0) {
        if ((ret = ssl_encrypt_buf(ssl)) != 0) {
            SSL_DEBUG_RET(1, "ssl_encrypt_buf", ret);
            return ret;
        }
        len = ssl->out_msglen;
        ssl->out_hdr[3] = (uchar)(len >> 8);
        ssl->out_hdr[4] = (uchar)(len);
    }
    ssl->out_left = 5 + ssl->out_msglen;

    SSL_DEBUG_MSG(3, ("output record: msgtype = %d, version = [%d:%d], msglen = %d",
        ssl->out_hdr[0], ssl->out_hdr[1], ssl->out_hdr[2], (ssl->out_hdr[3] << 8) | ssl->out_hdr[4]));
    SSL_DEBUG_BUF(4, "output record sent to network", ssl->out_hdr, 5 + ssl->out_msglen);

    if ((ret = ssl_flush_output(ssl)) != 0) {
        SSL_DEBUG_RET(1, "ssl_flush_output", ret);
        return ret;
    }
    SSL_DEBUG_MSG(2, ("<= write record"));
    return 0;
}


int ssl_read_record(ssl_context * ssl)
{
    int ret;

    SSL_DEBUG_MSG(2, ("=> read record"));

    if (ssl->in_hslen != 0 && ssl->in_hslen < ssl->in_msglen) {
        /*
            Get next Handshake message in the current record
         */
        ssl->in_msglen -= ssl->in_hslen;

        memcpy(ssl->in_msg, ssl->in_msg + ssl->in_hslen, ssl->in_msglen);

        ssl->in_hslen = 4;
        ssl->in_hslen += (ssl->in_msg[2] << 8) | ssl->in_msg[3];

        SSL_DEBUG_MSG(3, ("handshake message: msglen = %d, type = %d, hslen = %d",
                  ssl->in_msglen, ssl->in_msg[0], ssl->in_hslen));

        if (ssl->in_msglen < 4 || ssl->in_msg[1] != 0) {
            SSL_DEBUG_MSG(1, ("bad handshake length"));
            return EST_ERR_SSL_INVALID_RECORD;
        }
        if (ssl->in_msglen < ssl->in_hslen) {
            SSL_DEBUG_MSG(1, ("bad handshake length"));
            return EST_ERR_SSL_INVALID_RECORD;
        }
        md5_update(&ssl->fin_md5, ssl->in_msg, ssl->in_hslen);
        sha1_update(&ssl->fin_sha1, ssl->in_msg, ssl->in_hslen);
        return 0;
    }

    ssl->in_hslen = 0;

    /*
        Read the record header and validate it
     */
    if ((ret = ssl_fetch_input(ssl, 5)) != 0) {
        SSL_DEBUG_RET(3, "ssl_fetch_input", ret);
        return ret;
    }
    ssl->in_msgtype = ssl->in_hdr[0];
    ssl->in_msglen = (ssl->in_hdr[3] << 8) | ssl->in_hdr[4];

    SSL_DEBUG_MSG(3, ("input record: msgtype = %d, version = [%d:%d], msglen = %d",
          ssl->in_hdr[0], ssl->in_hdr[1], ssl->in_hdr[2], (ssl->in_hdr[3] << 8) | ssl->in_hdr[4]));

    if (ssl->in_hdr[1] != ssl->major_ver) {
        SSL_DEBUG_MSG(1, ("major version mismatch"));
        return EST_ERR_SSL_INVALID_RECORD;
    }
    if (ssl->in_hdr[2] != SSL_MINOR_VERSION_0 && ssl->in_hdr[2] != SSL_MINOR_VERSION_1) {
        SSL_DEBUG_MSG(1, ("minor version mismatch"));
        return EST_ERR_SSL_INVALID_RECORD;
    }
    /*
        Make sure the message length is acceptable
     */
    if (ssl->do_crypt == 0) {
        if (ssl->in_msglen < 1 || ssl->in_msglen > SSL_MAX_CONTENT_LEN) {
            SSL_DEBUG_MSG(1, ("bad message length"));
            return EST_ERR_SSL_INVALID_RECORD;
        }
    } else {
        if (ssl->in_msglen < ssl->minlen) {
            SSL_DEBUG_MSG(1, ("bad message length"));
            return EST_ERR_SSL_INVALID_RECORD;
        }
        if (ssl->minor_ver == SSL_MINOR_VERSION_0 && ssl->in_msglen > ssl->minlen + SSL_MAX_CONTENT_LEN) {
            SSL_DEBUG_MSG(1, ("bad message length"));
            return EST_ERR_SSL_INVALID_RECORD;
        }
        /*
            TLS encrypted messages can have up to 256 bytes of padding
         */
        if (ssl->minor_ver == SSL_MINOR_VERSION_1 &&
            ssl->in_msglen > ssl->minlen + SSL_MAX_CONTENT_LEN + 256) {
            SSL_DEBUG_MSG(1, ("bad message length"));
            return EST_ERR_SSL_INVALID_RECORD;
        }
    }
    /*
        Read and optionally decrypt the message contents
     */
    if ((ret = ssl_fetch_input(ssl, 5 + ssl->in_msglen)) != 0) {
        SSL_DEBUG_RET(3, "ssl_fetch_input", ret);
        return ret;
    }
    SSL_DEBUG_BUF(4, "input record from network", ssl->in_hdr, 5 + ssl->in_msglen);

    if (ssl->do_crypt != 0) {
        if ((ret = ssl_decrypt_buf(ssl)) != 0) {
            SSL_DEBUG_RET(1, "ssl_decrypt_buf", ret);
            return ret;
        }
        SSL_DEBUG_BUF(4, "input payload after decrypt", ssl->in_msg, ssl->in_msglen);

        if (ssl->in_msglen > SSL_MAX_CONTENT_LEN) {
            SSL_DEBUG_MSG(1, ("bad message length"));
            return EST_ERR_SSL_INVALID_RECORD;
        }
    }

    if (ssl->in_msgtype == SSL_MSG_HANDSHAKE) {
        ssl->in_hslen = 4;
        ssl->in_hslen += (ssl->in_msg[2] << 8) | ssl->in_msg[3];

        SSL_DEBUG_MSG(3, ("handshake message: msglen = %d, type = %d, hslen = %d",
            ssl->in_msglen, ssl->in_msg[0], ssl->in_hslen));

        /*
            Additional checks to validate the handshake header
         */
        if (ssl->in_msglen < 4 || ssl->in_msg[1] != 0) {
            SSL_DEBUG_MSG(1, ("bad handshake length"));
            return EST_ERR_SSL_INVALID_RECORD;
        }
        if (ssl->in_msglen < ssl->in_hslen) {
            SSL_DEBUG_MSG(1, ("bad handshake length"));
            return EST_ERR_SSL_INVALID_RECORD;
        }
        md5_update(&ssl->fin_md5, ssl->in_msg, ssl->in_hslen);
        sha1_update(&ssl->fin_sha1, ssl->in_msg, ssl->in_hslen);
    }

    if (ssl->in_msgtype == SSL_MSG_ALERT) {
        SSL_DEBUG_MSG(2, ("got an alert message, type: [%d:%d]", ssl->in_msg[0], ssl->in_msg[1]));
        /*
            Ignore non-fatal alerts, except close_notify
         */
        if (ssl->in_msg[0] == SSL_ALERT_FATAL) {
            SSL_DEBUG_MSG(1, ("is a fatal alert message"));
            return EST_ERR_SSL_FATAL_ALERT_MESSAGE | ssl->in_msg[1];
        }
        if (ssl->in_msg[0] == SSL_ALERT_WARNING && ssl->in_msg[1] == SSL_ALERT_CLOSE_NOTIFY) {
            SSL_DEBUG_MSG(2, ("is a close notify message"));
            return EST_ERR_SSL_PEER_CLOSE_NOTIFY;
        }
    }
    ssl->in_left = 0;
    SSL_DEBUG_MSG(2, ("<= read record"));
    return 0;
}


/*
    Handshake functions
 */
int ssl_write_certificate(ssl_context * ssl)
{
    int ret, i, n;
    x509_cert *crt;

    SSL_DEBUG_MSG(2, ("=> write certificate"));

    if (ssl->endpoint == SSL_IS_CLIENT) {
        if (ssl->client_auth == 0) {
            SSL_DEBUG_MSG(2, ("<= skip write certificate"));
            ssl->state++;
            return 0;
        }
        /*
           If using SSLv3 and got no cert, send an Alert message (otherwise an empty Certificate message will be sent).
         */
        if (ssl->own_cert == NULL && ssl->minor_ver == SSL_MINOR_VERSION_0) {
            ssl->out_msglen = 2;
            ssl->out_msgtype = SSL_MSG_ALERT;
            ssl->out_msg[0] = SSL_ALERT_WARNING;
            ssl->out_msg[1] = SSL_ALERT_NO_CERTIFICATE;
            SSL_DEBUG_MSG(2, ("got no certificate to send"));
            goto write_msg;
        }
    } else {
        /* SSL_IS_SERVER */
        if (ssl->own_cert == NULL) {
            SSL_DEBUG_MSG(1, ("got no certificate to send"));
            return EST_ERR_SSL_CERTIFICATE_REQUIRED;
        }
    }

    SSL_DEBUG_CRT(3, "own certificate", ssl->own_cert);

    /*
           0  .  0    handshake type
           1  .  3    handshake length
           4  .  6    length of all certs
           7  .  9    length of cert. 1
          10  . n-1   peer certificate
           n  . n+2   length of cert. 2
          n+3 . ...   upper level cert, etc.
     */
    i = 7;
    crt = ssl->own_cert;

    while (crt != NULL && crt->next != NULL) {
        n = crt->raw.len;
        if (i + 3 + n > SSL_MAX_CONTENT_LEN) {
            SSL_DEBUG_MSG(1, ("certificate too large, %d > %d", i + 3 + n, SSL_MAX_CONTENT_LEN));
            return EST_ERR_SSL_CERTIFICATE_TOO_LARGE;
        }
        ssl->out_msg[i] = (uchar)(n >> 16);
        ssl->out_msg[i + 1] = (uchar)(n >> 8);
        ssl->out_msg[i + 2] = (uchar)(n);
        i += 3;
        memcpy(ssl->out_msg + i, crt->raw.p, n);
        i += n;
        crt = crt->next;
    }
    ssl->out_msg[4] = (uchar)((i - 7) >> 16);
    ssl->out_msg[5] = (uchar)((i - 7) >> 8);
    ssl->out_msg[6] = (uchar)((i - 7));

    ssl->out_msglen = i;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0] = SSL_HS_CERTIFICATE;

write_msg:
    ssl->state++;
    if ((ret = ssl_write_record(ssl)) != 0) {
        SSL_DEBUG_RET(1, "ssl_write_record", ret);
        return ret;
    }
    SSL_DEBUG_MSG(2, ("<= write certificate"));
    return 0;
}


int ssl_parse_certificate(ssl_context * ssl)
{
    int ret, i, n;

    SSL_DEBUG_MSG(2, ("=> parse certificate"));

    if (ssl->endpoint == SSL_IS_SERVER && ssl->authmode == SSL_VERIFY_NO_CHECK) {
        SSL_DEBUG_MSG(2, ("<= skip parse certificate"));
        ssl->state++;
        return 0;
    }
    if ((ret = ssl_read_record(ssl)) != 0) {
        SSL_DEBUG_RET(3, "ssl_read_record", ret);
        return ret;
    }
    ssl->state++;

    /*
       Check if the client sent an empty certificate
     */
    if (ssl->endpoint == SSL_IS_SERVER && ssl->minor_ver == SSL_MINOR_VERSION_0) {
        if (ssl->in_msglen == 2 &&
            ssl->in_msgtype == SSL_MSG_ALERT &&
            ssl->in_msg[0] == SSL_ALERT_WARNING &&
            ssl->in_msg[1] == SSL_ALERT_NO_CERTIFICATE) {
            SSL_DEBUG_MSG(1, ("SSLv3 client has no certificate"));

            if (ssl->authmode == SSL_VERIFY_OPTIONAL) {
                return 0;
            } else {
                return EST_ERR_SSL_NO_CLIENT_CERTIFICATE;
            }
        }
    }
    if (ssl->endpoint == SSL_IS_SERVER && ssl->minor_ver != SSL_MINOR_VERSION_0) {
        if (ssl->in_hslen == 7 &&
            ssl->in_msgtype == SSL_MSG_HANDSHAKE &&
            ssl->in_msg[0] == SSL_HS_CERTIFICATE &&
            memcmp(ssl->in_msg + 4, "\0\0\0", 3) == 0) {
            SSL_DEBUG_MSG(1, ("TLSv1 client has no certificate"));

            if (ssl->authmode == SSL_VERIFY_REQUIRED) {
                return EST_ERR_SSL_NO_CLIENT_CERTIFICATE;
            } else {
                return 0;
            }
        }
    }
    if (ssl->in_msgtype != SSL_MSG_HANDSHAKE) {
        SSL_DEBUG_MSG(1, ("bad certificate message"));
        return EST_ERR_SSL_UNEXPECTED_MESSAGE;
    }
    if (ssl->in_msg[0] != SSL_HS_CERTIFICATE || ssl->in_hslen < 10) {
        SSL_DEBUG_MSG(1, ("bad certificate message"));
        return EST_ERR_SSL_BAD_HS_CERTIFICATE;
    }

    /*
        Same message structure as in ssl_write_certificate()
     */
    n = (ssl->in_msg[5] << 8) | ssl->in_msg[6];

    if (ssl->in_msg[4] != 0 || ssl->in_hslen != 7 + n) {
        SSL_DEBUG_MSG(1, ("bad certificate message"));
        return EST_ERR_SSL_BAD_HS_CERTIFICATE;
    }
    if ((ssl->peer_cert = (x509_cert *) malloc(sizeof(x509_cert))) == NULL) {
        SSL_DEBUG_MSG(1, ("malloc(%d bytes) failed", sizeof(x509_cert)));
        return 1;
    }
    memset(ssl->peer_cert, 0, sizeof(x509_cert));

    i = 7;
    while (i < ssl->in_hslen) {
        if (ssl->in_msg[i] != 0) {
            SSL_DEBUG_MSG(1, ("bad certificate message"));
            return EST_ERR_SSL_BAD_HS_CERTIFICATE;
        }
        n = ((uint)ssl->in_msg[i + 1] << 8) | (uint)ssl->in_msg[i + 2];
        i += 3;

        if (n < 128 || i + n > ssl->in_hslen) {
            SSL_DEBUG_MSG(1, ("bad certificate message"));
            return EST_ERR_SSL_BAD_HS_CERTIFICATE;
        }
        ret = x509parse_crt(ssl->peer_cert, ssl->in_msg + i, n);
        if (ret != 0) {
            SSL_DEBUG_RET(1, " x509parse_crt", ret);
            return ret;
        }
        i += n;
    }
    SSL_DEBUG_CRT(3, "peer certificate", ssl->peer_cert);

    if (ssl->authmode != SSL_VERIFY_NO_CHECK) {
        if (ssl->ca_chain == NULL) {
            SSL_DEBUG_MSG(1, ("got no CA chain"));
            return EST_ERR_SSL_CA_CHAIN_REQUIRED;
        }
        ret = x509parse_verify(ssl->peer_cert, ssl->ca_chain, ssl->peer_cn, &ssl->verify_result);
        if (ret != 0) {
            SSL_DEBUG_MSG(3, ("x509_verify_cert %d, verify_result %d", ret, ssl->verify_result));
        }
        if (ssl->authmode != SSL_VERIFY_REQUIRED) {
            ret = 0;
        }
    }
    SSL_DEBUG_MSG(2, ("<= parse certificate"));
    return ret;
}


int ssl_write_change_cipher_spec(ssl_context * ssl)
{
    int ret;

    SSL_DEBUG_MSG(2, ("=> write change cipher spec"));

    ssl->out_msgtype = SSL_MSG_CHANGE_CIPHER_SPEC;
    ssl->out_msglen = 1;
    ssl->out_msg[0] = 1;
    ssl->do_crypt = 0;
    ssl->state++;

    if ((ret = ssl_write_record(ssl)) != 0) {
        SSL_DEBUG_RET(1, "ssl_write_record", ret);
        return ret;
    }
    SSL_DEBUG_MSG(2, ("<= write change cipher spec"));
    return 0;
}


int ssl_parse_change_cipher_spec(ssl_context * ssl)
{
    int ret;

    SSL_DEBUG_MSG(2, ("=> parse change cipher spec"));
    ssl->do_crypt = 0;

    if ((ret = ssl_read_record(ssl)) != 0) {
        SSL_DEBUG_RET(3, "ssl_read_record", ret);
        return ret;
    }
    if (ssl->in_msgtype != SSL_MSG_CHANGE_CIPHER_SPEC) {
        SSL_DEBUG_MSG(1, ("bad change cipher spec message"));
        return EST_ERR_SSL_UNEXPECTED_MESSAGE;
    }
    if (ssl->in_msglen != 1 || ssl->in_msg[0] != 1) {
        SSL_DEBUG_MSG(1, ("bad change cipher spec message"));
        return EST_ERR_SSL_BAD_HS_CHANGE_CIPHER_SPEC;
    }
    ssl->state++;
    SSL_DEBUG_MSG(2, ("<= parse change cipher spec"));
    return 0;
}


static void ssl_calc_finished(ssl_context * ssl, uchar *buf, int from, md5_context * md5, sha1_context * sha1)
{
    int len = 12;
    char *sender;
    uchar padbuf[48];
    uchar md5sum[16];
    uchar sha1sum[20];

    SSL_DEBUG_MSG(2, ("=> calc  finished"));

    /*
       SSLv3:
         hash =
            MD5( master + pad2 +
                MD5( handshake + sender + master + pad1 ) )
         + SHA1( master + pad2 +
               SHA1( handshake + sender + master + pad1 ) )
      
       TLSv1:
         hash = PRF( master, finished_label,
                     MD5( handshake ) + SHA1( handshake ) )[0..11]
     */

    SSL_DEBUG_BUF(4, "finished  md5 state", (uchar *) md5->state, sizeof(md5->state));
    SSL_DEBUG_BUF(4, "finished sha1 state", (uchar *) sha1->state, sizeof(sha1->state));

    if (ssl->minor_ver == SSL_MINOR_VERSION_0) {
        sender = (from == SSL_IS_CLIENT) ? (char *)"CLNT" : (char *)"SRVR";

        memset(padbuf, 0x36, 48);
        md5_update(md5, (uchar *)sender, 4);
        md5_update(md5, ssl->session->master, 48);
        md5_update(md5, padbuf, 48);
        md5_finish(md5, md5sum);

        sha1_update(sha1, (uchar *)sender, 4);
        sha1_update(sha1, ssl->session->master, 48);
        sha1_update(sha1, padbuf, 40);
        sha1_finish(sha1, sha1sum);

        memset(padbuf, 0x5C, 48);

        md5_starts(md5);
        md5_update(md5, ssl->session->master, 48);
        md5_update(md5, padbuf, 48);
        md5_update(md5, md5sum, 16);
        md5_finish(md5, buf);

        sha1_starts(sha1);
        sha1_update(sha1, ssl->session->master, 48);
        sha1_update(sha1, padbuf, 40);
        sha1_update(sha1, sha1sum, 20);
        sha1_finish(sha1, buf + 16);
        len += 24;

    } else {
        sender = (from == SSL_IS_CLIENT) ? (char *)"client finished" : (char *)"server finished";
        md5_finish(md5, padbuf);
        sha1_finish(sha1, padbuf + 16);
        tls1_prf(ssl->session->master, 48, sender, padbuf, 36, buf, len);
    }
    SSL_DEBUG_BUF(3, "calc finished result", buf, len);

    memset(md5, 0, sizeof(md5_context));
    memset(sha1, 0, sizeof(sha1_context));
    memset(padbuf, 0, sizeof(padbuf));
    memset(md5sum, 0, sizeof(md5sum));
    memset(sha1sum, 0, sizeof(sha1sum));

    SSL_DEBUG_MSG(2, ("<= calc  finished"));
}


int ssl_write_finished(ssl_context * ssl)
{
    int ret, hash_len;
    md5_context md5;
    sha1_context sha1;

    SSL_DEBUG_MSG(2, ("=> write finished"));

    memcpy(&md5, &ssl->fin_md5, sizeof(md5_context));
    memcpy(&sha1, &ssl->fin_sha1, sizeof(sha1_context));

    ssl_calc_finished(ssl, ssl->out_msg + 4, ssl->endpoint, &md5, &sha1);

    hash_len = (ssl->minor_ver == SSL_MINOR_VERSION_0) ? 36 : 12;

    ssl->out_msglen = 4 + hash_len;
    ssl->out_msgtype = SSL_MSG_HANDSHAKE;
    ssl->out_msg[0] = SSL_HS_FINISHED;

    /*
       In case of session resuming, invert the client and server
       ChangeCipherSpec messages order.
     */
    if (ssl->resume != 0) {
        if (ssl->endpoint == SSL_IS_CLIENT) {
            ssl->state = SSL_HANDSHAKE_OVER;
        } else {
            ssl->state = SSL_CLIENT_CHANGE_CIPHER_SPEC;
        }
    } else {
        ssl->state++;
    }
    ssl->do_crypt = 1;

    if ((ret = ssl_write_record(ssl)) != 0) {
        SSL_DEBUG_RET(1, "ssl_write_record", ret);
        return ret;
    }
    SSL_DEBUG_MSG(2, ("<= write finished"));
    return 0;
}


int ssl_parse_finished(ssl_context * ssl)
{
    int ret, hash_len;
    md5_context md5;
    sha1_context sha1;
    uchar buf[36];

    SSL_DEBUG_MSG(2, ("=> parse finished"));

    memcpy(&md5, &ssl->fin_md5, sizeof(md5_context));
    memcpy(&sha1, &ssl->fin_sha1, sizeof(sha1_context));

    ssl->do_crypt = 1;

    if ((ret = ssl_read_record(ssl)) != 0) {
        SSL_DEBUG_RET(3, "ssl_read_record", ret);
        return ret;
    }
    if (ssl->in_msgtype != SSL_MSG_HANDSHAKE) {
        SSL_DEBUG_MSG(1, ("bad finished message"));
        return EST_ERR_SSL_UNEXPECTED_MESSAGE;
    }

    hash_len = (ssl->minor_ver == SSL_MINOR_VERSION_0) ? 36 : 12;

    if (ssl->in_msg[0] != SSL_HS_FINISHED || ssl->in_hslen != 4 + hash_len) {
        SSL_DEBUG_MSG(1, ("bad finished message"));
        return EST_ERR_SSL_BAD_HS_FINISHED;
    }
    ssl_calc_finished(ssl, buf, ssl->endpoint ^ 1, &md5, &sha1);

    if (memcmp(ssl->in_msg + 4, buf, hash_len) != 0) {
        SSL_DEBUG_MSG(1, ("bad finished message"));
        return EST_ERR_SSL_BAD_HS_FINISHED;
    }

    if (ssl->resume != 0) {
        if (ssl->endpoint == SSL_IS_CLIENT) {
            ssl->state = SSL_CLIENT_CHANGE_CIPHER_SPEC;
        }
        if (ssl->endpoint == SSL_IS_SERVER) {
            ssl->state = SSL_HANDSHAKE_OVER;
        }
    } else {
        ssl->state++;
    }
    SSL_DEBUG_MSG(2, ("<= parse finished"));
    return 0;
}


#if EMBEDTHIS || 1
/*
    Default single-threaded session mgmt functios
 */
static ssl_session *slist = NULL;
static ssl_session *cur, *prv;

static int getSession(ssl_context *ssl)
{
    time_t  t;
   
    t = time(NULL);

    if (ssl->resume == 0) {
        return 1;
    }
    cur = slist;
    prv = NULL;

    while (cur) {
        prv = cur;
        cur = cur->next;
        if (ssl->timeout != 0 && t - prv->start > ssl->timeout) {
            continue;
        }
        if (ssl->session->cipher != prv->cipher || ssl->session->length != prv->length) {
            continue;
        }
        if (memcmp(ssl->session->id, prv->id, prv->length) != 0) {
            continue;
        }
        memcpy(ssl->session->master, prv->master, 48);
        return 0;
    }
    return 1;
}

static int setSession(ssl_context *ssl)
{
    time_t  t;
   
    t = time(NULL);

    cur = slist;
    prv = NULL;
    while (cur) {
        if (ssl->timeout != 0 && t - cur->start > ssl->timeout) {
            /* expired, reuse this slot */
            break;
        }
        if (memcmp(ssl->session->id, cur->id, cur->length) == 0) {
            /* client reconnected */
            break;  
        }
        prv = cur;
        cur = cur->next;
    }
    if (cur == NULL) {
        cur = (ssl_session*) malloc(sizeof(ssl_session));
        if (cur == NULL) {
            return 1;
        }
        if (prv == NULL) {
            slist = cur;
        } else {
            prv->next = cur;
        }
    }
    memcpy(cur, ssl->session, sizeof(ssl_session));
    return 0;
}
#endif


/*
    Initialize an SSL context
 */
int ssl_init(ssl_context * ssl)
{
    int len = SSL_BUFFER_LEN;

    memset(ssl, 0, sizeof(ssl_context));

    ssl->in_ctr = (uchar *)malloc(len);
    ssl->in_hdr = ssl->in_ctr + 8;
    ssl->in_msg = ssl->in_ctr + 13;

    if (ssl->in_ctr == NULL) {
        SSL_DEBUG_MSG(1, ("malloc(%d bytes) failed", len));
        return 1;
    }
    ssl->out_ctr = (uchar *)malloc(len);
    ssl->out_hdr = ssl->out_ctr + 8;
    ssl->out_msg = ssl->out_ctr + 13;

    if (ssl->out_ctr == NULL) {
        SSL_DEBUG_MSG(1, ("malloc(%d bytes) failed", len));
        free(ssl->in_ctr);
        return 1;
    }
    memset(ssl->in_ctr, 0, SSL_BUFFER_LEN);
    memset(ssl->out_ctr, 0, SSL_BUFFER_LEN);

    ssl->hostname = NULL;
    ssl->hostname_len = 0;

#if EMBEDTHIS || 1
    ssl->s_get = getSession;
    ssl->s_set = setSession;
#endif

    md5_starts(&ssl->fin_md5);
    sha1_starts(&ssl->fin_sha1);
    return 0;
}


/*
   SSL set accessors
 */
void ssl_set_endpoint(ssl_context * ssl, int endpoint)
{
    ssl->endpoint = endpoint;
}


void ssl_set_authmode(ssl_context * ssl, int authmode)
{
    ssl->authmode = authmode;
}


void ssl_set_rng(ssl_context * ssl, int (*f_rng) (void *), void *p_rng)
{
    ssl->f_rng = f_rng;
    ssl->p_rng = p_rng;
}


void ssl_set_dbg(ssl_context * ssl, void (*f_dbg) (void *, int, char *), void *p_dbg)
{
    ssl->f_dbg = f_dbg;
    ssl->p_dbg = p_dbg;
}

void ssl_set_bio(ssl_context * ssl,
     int (*f_recv) (void *, uchar *, int), void *p_recv,
     int (*f_send) (void *, uchar *, int), void *p_send)
{
    ssl->f_recv = f_recv;
    ssl->f_send = f_send;
    ssl->p_recv = p_recv;
    ssl->p_send = p_send;
}


void ssl_set_scb(ssl_context * ssl, int (*s_get) (ssl_context *), int (*s_set) (ssl_context *))
{
    ssl->s_get = s_get;
    ssl->s_set = s_set;
}


void ssl_set_session(ssl_context * ssl, int resume, int timeout, ssl_session * session)
{
    ssl->resume = resume;
    ssl->timeout = timeout;
    ssl->session = session;
}


void ssl_set_ciphers(ssl_context * ssl, int *ciphers)
{
    ssl->ciphers = ciphers;
}


void ssl_set_ca_chain(ssl_context * ssl, x509_cert * ca_chain, char *peer_cn)
{
    ssl->ca_chain = ca_chain;
    ssl->peer_cn = peer_cn;
}


void ssl_set_own_cert(ssl_context * ssl, x509_cert * own_cert, rsa_context * rsa_key)
{
    ssl->own_cert = own_cert;
    ssl->rsa_key = rsa_key;
}


int ssl_set_dh_param(ssl_context * ssl, char *dhm_P, char *dhm_G)
{
    int ret;

    if ((ret = mpi_read_string(&ssl->dhm_ctx.P, 16, dhm_P)) != 0) {
        SSL_DEBUG_RET(1, "mpi_read_string", ret);
        return ret;
    }
    if ((ret = mpi_read_string(&ssl->dhm_ctx.G, 16, dhm_G)) != 0) {
        SSL_DEBUG_RET(1, "mpi_read_string", ret);
        return ret;
    }
    return 0;
}


int ssl_set_hostname(ssl_context * ssl, char *hostname)
{
    if (hostname == NULL) {
        return EST_ERR_SSL_BAD_INPUT_DATA;
    }
    ssl->hostname_len = strlen(hostname);
    ssl->hostname = (uchar *)malloc(ssl->hostname_len + 1);
    memcpy(ssl->hostname, (uchar *)hostname, ssl->hostname_len);
    return 0;
}


/*
    SSL get accessors
 */
int ssl_get_bytes_avail(ssl_context * ssl)
{
    return ssl->in_offt == NULL ? 0 : ssl->in_msglen;
}


int ssl_get_verify_result(ssl_context * ssl)
{
    return ssl->verify_result;
}


#if UNUSED
char *ssl_get_cipher(ssl_context * ssl)
{
    switch (ssl->session->cipher) {
#if ME_EST_RC4
    case TLS_RSA_WITH_RC4_128_MD5:
        return "TLS_RSA_WITH_RC4_128_MD5";

    case TLS_RSA_WITH_RC4_128_SHA:
        return "TLS_RSA_WITH_RC4_128_SHA";
#endif

#if ME_EST_DES
    case TLS_RSA_WITH_3DES_EDE_CBC_SHA:
        return "TLS_RSA_WITH_3DES_EDE_CBC_SHA";

    case TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA:
        return "TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA";
#endif

#if ME_EST_AES
    case TLS_RSA_WITH_AES_128_CBC_SHA:
        return "TLS_RSA_WITH_AES_128_CBC_SHA";

    case TLS_RSA_WITH_AES_256_CBC_SHA:
        return "TLS_RSA_WITH_AES_256_CBC_SHA";

    case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
        return "TLS_DHE_RSA_WITH_AES_256_CBC_SHA";
#endif

#if ME_EST_CAMELLIA
    case TLS_RSA_WITH_CAMELLIA_128_CBC_SHA:
        return "TLS_RSA_WITH_CAMELLIA_128_CBC_SHA";

    case TLS_RSA_WITH_CAMELLIA_256_CBC_SHA:
        return "TLS_RSA_WITH_CAMELLIA_256_CBC_SHA";

    case TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA:
        return "TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA";
#endif

    default:
        break;
    }
    return "unknown";
}
#endif


/*
   Perform the SSL handshake
 */
PUBLIC int ssl_handshake(ssl_context * ssl)
{
#if ME_EST_LOGGING
    char    cbuf[4096];
#endif
    int old_state = ssl->state;
    int ret = EST_ERR_SSL_FEATURE_UNAVAILABLE;

    SSL_DEBUG_MSG(2, ("=> handshake"));

#if ME_EST_CLIENT
    if (ssl->endpoint == SSL_IS_CLIENT)
        ret = ssl_handshake_client(ssl);
#endif

#if ME_EST_SERVER
    if (ssl->endpoint == SSL_IS_SERVER)
        ret = ssl_handshake_server(ssl);
#endif
    SSL_DEBUG_MSG(2, ("<= handshake"));
    
#if ME_EST_LOGGING
    if (ssl->state == SSL_HANDSHAKE_OVER && old_state != SSL_HANDSHAKE_OVER) {
        SSL_DEBUG_MSG(1, ("using cipher: %s", ssl_get_cipher(ssl)));
        if (ssl->peer_cert) {
            SSL_DEBUG_MSG(1, ("Peer certificate: %s", x509parse_cert_info("", cbuf, sizeof(cbuf), ssl->peer_cert)));
        } else {
            SSL_DEBUG_MSG(1, ("Peer supplied no certificate"));
        }
    }
#endif
    return ret;
}


/*
   Receive application data decrypted from the SSL layer
 */
int ssl_read(ssl_context * ssl, uchar *buf, int len)
{
    int ret, n;

    SSL_DEBUG_MSG(2, ("=> read"));

    if (ssl->state != SSL_HANDSHAKE_OVER) {
        if ((ret = ssl_handshake(ssl)) != 0) {
            SSL_DEBUG_RET(1, "ssl_handshake", ret);
            return ret;
        }
    }
    if (ssl->in_offt == NULL) {
        if ((ret = ssl_read_record(ssl)) != 0) {
            SSL_DEBUG_RET(3, "ssl_read_record", ret);
            return ret;
        }
        if (ssl->in_msglen == 0 && ssl->in_msgtype == SSL_MSG_APPLICATION_DATA) {
            /*
               OpenSSL sends empty messages to randomize the IV
             */
            if ((ret = ssl_read_record(ssl)) != 0) {
                SSL_DEBUG_RET(3, "ssl_read_record", ret);
                return ret;
            }
        }
        if (ssl->in_msgtype != SSL_MSG_APPLICATION_DATA) {
            SSL_DEBUG_MSG(1, ("bad application data message"));
            return EST_ERR_SSL_UNEXPECTED_MESSAGE;
        }
        ssl->in_offt = ssl->in_msg;
    }
    n = (len < ssl->in_msglen) ? len : ssl->in_msglen;

    memcpy(buf, ssl->in_offt, n);
    ssl->in_msglen -= n;

    if (ssl->in_msglen == 0) {
        /* all bytes consumed  */
        ssl->in_offt = NULL;
    } else {
        /* more data available */
        ssl->in_offt += n;
    }
    SSL_DEBUG_MSG(2, ("<= read"));
    return n;
}


/*
   Send application data to be encrypted by the SSL layer
 */
int ssl_write(ssl_context * ssl, uchar *buf, int len)
{
    int ret, n;

    SSL_DEBUG_MSG(2, ("=> write"));

    if (ssl->state != SSL_HANDSHAKE_OVER) {
        if ((ret = ssl_handshake(ssl)) != 0) {
            SSL_DEBUG_RET(1, "ssl_handshake", ret);
            return ret;
        }
    }
    n = (len < SSL_MAX_CONTENT_LEN) ? len : SSL_MAX_CONTENT_LEN;

    if (ssl->out_left != 0) {
        if ((ret = ssl_flush_output(ssl)) != 0) {
            SSL_DEBUG_RET(1, "ssl_flush_output", ret);
            return ret;
        }
    } else {
        ssl->out_msglen = n;
        ssl->out_msgtype = SSL_MSG_APPLICATION_DATA;
        memcpy(ssl->out_msg, buf, n);

        if ((ret = ssl_write_record(ssl)) != 0) {
            SSL_DEBUG_RET(1, "ssl_write_record", ret);
            return ret;
        }
    }
    SSL_DEBUG_MSG(2, ("<= write"));
    return n;
}


/*
   Notify the peer that the connection is being closed
 */
int ssl_close_notify(ssl_context * ssl)
{
    int ret;

    SSL_DEBUG_MSG(2, ("=> write close notify"));

    if ((ret = ssl_flush_output(ssl)) != 0) {
        SSL_DEBUG_RET(1, "ssl_flush_output", ret);
        return ret;
    }
    if (ssl->state == SSL_HANDSHAKE_OVER) {
        ssl->out_msgtype = SSL_MSG_ALERT;
        ssl->out_msglen = 2;
        ssl->out_msg[0] = SSL_ALERT_WARNING;
        ssl->out_msg[1] = SSL_ALERT_CLOSE_NOTIFY;

        if ((ret = ssl_write_record(ssl)) != 0) {
            SSL_DEBUG_RET(1, "ssl_write_record", ret);
            return ret;
        }
    }
    SSL_DEBUG_MSG(2, ("<= write close notify"));
    return ret;
}


/*
   Free an SSL context
 */
void ssl_free(ssl_context * ssl)
{
    SSL_DEBUG_MSG(2, ("=> free"));

    if (ssl->peer_cert != NULL) {
        x509_free(ssl->peer_cert);
        memset(ssl->peer_cert, 0, sizeof(x509_cert));
        free(ssl->peer_cert);
    }
    if (ssl->out_ctr != NULL) {
        memset(ssl->out_ctr, 0, SSL_BUFFER_LEN);
        free(ssl->out_ctr);
    }
    if (ssl->in_ctr != NULL) {
        memset(ssl->in_ctr, 0, SSL_BUFFER_LEN);
        free(ssl->in_ctr);
    }
#if ME_EST_DHM
    dhm_free(&ssl->dhm_ctx);
#endif
    if (ssl->hostname != NULL) {
        memset(ssl->hostname, 0, ssl->hostname_len);
        free(ssl->hostname);
        ssl->hostname_len = 0;
    }
    memset(ssl, 0, sizeof(ssl_context));
    SSL_DEBUG_MSG(2, ("<= free"));
}


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



/********* Start of file src/timing.c ************/


/*
    timing.c -- Portable interface to the CPU cycle counter

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_TIMING

#if WINDOWS
struct _hr_time {
    LARGE_INTEGER start;
};
#else
struct _hr_time {
    struct timeval start;
};
#endif

#if WINDOWS
ulong hardclock(void)
{
    LARGE_INTEGER  now;
    QueryPerformanceCounter(&now);
    return (ulong) (((uint64) now.HighPart) << 32) + now.LowPart;
}

#elif (defined(_MSC_VER) && defined(_M_IX86)) || defined(__WATCOMC__)

#elif WINDOWS
ulong hardclock(void)
{
    ulong tsc;
    __asm rdtsc 
    __asm mov[tsc], eax 
    return (tsc);
}

#elif defined(__GNUC__) && defined(__i386__)

ulong hardclock(void)
{
    ulong tsc;
    asm("rdtsc":"=a"(tsc));
    return (tsc);
}

#elif defined(__GNUC__) && (defined(__amd64__) || defined(__x86_64__))

ulong hardclock(void)
{
    ulong lo, hi;
    asm("rdtsc":"=a"(lo), "=d"(hi));
    return (lo | (hi << 32));
}

#elif defined(__GNUC__) && (defined(__powerpc__) || defined(__ppc__))

ulong hardclock(void)
{
    ulong tbl, tbu0, tbu1;

    do {
        asm("mftbu %0":"=r"(tbu0));
        asm("mftb   %0":"=r"(tbl));
        asm("mftbu %0":"=r"(tbu1));
    } while (tbu0 != tbu1);
    return (tbl);
}

#elif defined(__GNUC__) && defined(__sparc__)

ulong hardclock(void)
{
    ulong tick;
    asm(".byte 0x83, 0x41, 0x00, 0x00");
    asm("mov    %%g1, %0":"=r"(tick));
    return (tick);
}

#elif defined(__GNUC__) && defined(__alpha__)

ulong hardclock(void)
{
    ulong cc;
    asm("rpcc %0":"=r"(cc));
    return (cc & 0xFFFFFFFF);
}

#elif defined(__GNUC__) && defined(__ia64__)

ulong hardclock(void)
{
    ulong itc;
    asm("mov %0 = ar.itc":"=r"(itc));
    return (itc);
}

#else

static int hardclock_init = 0;
static struct timeval tv_init;

ulong hardclock(void)
{
    struct timeval tv_cur;

    if (hardclock_init == 0) {
        gettimeofday(&tv_init, NULL);
        hardclock_init = 1;
    }
    gettimeofday(&tv_cur, NULL);
    return ((tv_cur.tv_sec - tv_init.tv_sec) * 1000000 + (tv_cur.tv_usec - tv_init.tv_usec));
}

#endif

int alarmed = 0;

#if WINDOWS

ulong get_timer(struct hr_time *val, int reset)
{
    ulong delta;
    LARGE_INTEGER offset, hfreq;
    struct _hr_time *t = (struct _hr_time *)val;

    QueryPerformanceCounter(&offset);
    QueryPerformanceFrequency(&hfreq);

    delta = (ulong)((1000 * (offset.QuadPart - t->start.QuadPart)) / hfreq.QuadPart);
    if (reset) {
        QueryPerformanceCounter(&t->start);
    }
    return (delta);
}


DWORD WINAPI TimerProc(LPVOID uElapse)
{
    Sleep((DWORD) uElapse);
    alarmed = 1;
    return (TRUE);
}


#if UNUSED
void set_alarm(int seconds)
{
    DWORD ThreadId;
    alarmed = 0;
    CloseHandle(CreateThread(NULL, 0, TimerProc, (LPVOID) (seconds * 1000), 0, &ThreadId));
}
#endif


void m_sleep(int milliseconds)
{
    Sleep(milliseconds);
}


PUBLIC int gettimeofday(struct timeval *tv, struct timezone *tz)
{
    #if ME_WIN_LIKE
        FILETIME        fileTime;
        Time            now;
        static int      tzOnce;

        if (NULL != tv) {
            /* Convert from 100-nanosec units to microsectonds */
            GetSystemTimeAsFileTime(&fileTime);
            now = ((((Time) fileTime.dwHighDateTime) << BITS(uint)) + ((Time) fileTime.dwLowDateTime));
            now /= 10;
            now -= TIME_GENESIS;
            tv->tv_sec = (long) (now / 1000000);
            tv->tv_usec = (long) (now % 1000000);
        }
        if (NULL != tz) {
            TIME_ZONE_INFORMATION   zone;
            int                     rc, bias;
            rc = GetTimeZoneInformation(&zone);
            bias = (int) zone.Bias;
            if (rc == TIME_ZONE_ID_DAYLIGHT) {
                tz->tz_dsttime = 1;
            } else {
                tz->tz_dsttime = 0;
            }
            tz->tz_minuteswest = bias;
        }
        return 0;

    #elif VXWORKS
        struct tm       tm;
        struct timespec now;
        time_t          t;
        char            *tze, *p;
        int             rc;

        if ((rc = clock_gettime(CLOCK_REALTIME, &now)) == 0) {
            tv->tv_sec  = now.tv_sec;
            tv->tv_usec = (now.tv_nsec + 500) / MS_PER_SEC;
            if ((tze = getenv("TIMEZONE")) != 0) {
                if ((p = strchr(tze, ':')) != 0) {
                    if ((p = strchr(tze, ':')) != 0) {
                        tz->tz_minuteswest = stoi(++p);
                    }
                }
                t = tickGet();
                tz->tz_dsttime = (localtime_r(&t, &tm) == 0) ? tm.tm_isdst : 0;
            }
        }
        return rc;
    #endif
}

#else

ulong get_timer(struct hr_time *val, int reset)
{
    ulong delta;
    struct timeval offset;
    struct _hr_time *t = (struct _hr_time *)val;

    gettimeofday(&offset, NULL);
    delta = (offset.tv_sec - t->start.tv_sec) * 1000 + (offset.tv_usec - t->start.tv_usec) / 1000;
    if (reset) {
        t->start.tv_sec = offset.tv_sec;
        t->start.tv_usec = offset.tv_usec;
    }
    return (delta);
}


static void sighandler(int signum)
{
    alarmed = 1;
    signal(signum, sighandler);
}


#if UNUSED
void set_alarm(int seconds)
{
    alarmed = 0;
    signal(SIGALRM, sighandler);
    alarm(seconds);
}
#endif


void m_sleep(int milliseconds)
{
    struct timeval tv;

    tv.tv_sec = milliseconds / 1000;
    tv.tv_usec = milliseconds * 1000;
    select(0, NULL, NULL, NULL, &tv);
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



/********* Start of file src/x509parse.c ************/


/*
    x509parse.c -- X.509 certificate and private key decoding

    The ITU-T X.509 standard defines a certificat format for PKI.
  
    http://www.ietf.org/rfc/rfc2459.txt
    http://www.ietf.org/rfc/rfc3279.txt
  
    ftp://ftp.rsasecurity.com/pub/pkcs/ascii/pkcs-1v2.asc
  
    http://www.itu.int/ITU-T/studygroups/com17/languages/X.680-0207.pdf
    http://www.itu.int/ITU-T/studygroups/com17/languages/X.690-0207.pdf
*/



#if ME_EST_X509

/*
    ASN.1 DER decoding routines
 */
static int asn1_get_len(uchar **p, uchar *end, int *len)
{
    if ((end - *p) < 1) {
        return EST_ERR_ASN1_OUT_OF_DATA;
    }

    if ((**p & 0x80) == 0) {
        *len = *(*p)++;
    } else {
        switch (**p & 0x7F) {
        case 1:
            if ((end - *p) < 2) {
                return EST_ERR_ASN1_OUT_OF_DATA;
            }
            *len = (*p)[1];
            (*p) += 2;
            break;

        case 2:
            if ((end - *p) < 3) {
                return EST_ERR_ASN1_OUT_OF_DATA;
            }
            *len = ((*p)[1] << 8) | (*p)[2];
            (*p) += 3;
            break;

        default:
            return EST_ERR_ASN1_INVALID_LENGTH;
        }
    }
    if (*len > (int)(end - *p)) {
        return EST_ERR_ASN1_OUT_OF_DATA;
    }
    return 0;
}


static int asn1_get_tag(uchar **p, uchar *end, int *len, int tag)
{
    if ((end - *p) < 1) {
        return EST_ERR_ASN1_OUT_OF_DATA;
    }
    if (**p != tag) {
        return EST_ERR_ASN1_UNEXPECTED_TAG;
    }
    (*p)++;
    return asn1_get_len(p, end, len);
}


static int asn1_get_bool(uchar **p, uchar *end, int *val)
{
    int ret, len;

    if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_BOOLEAN)) != 0) {
        return ret;
    }
    if (len != 1) {
        return EST_ERR_ASN1_INVALID_LENGTH;
    }
    *val = (**p != 0) ? 1 : 0;
    (*p)++;
    return 0;
}


static int asn1_get_int(uchar **p, uchar *end, int *val)
{
    int ret, len;

    if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_INTEGER)) != 0) {
        return ret;
    }
    if (len > (int)sizeof(int) || (**p & 0x80) != 0) {
        return EST_ERR_ASN1_INVALID_LENGTH;
    }
    *val = 0;
    while (len-- > 0) {
        *val = (*val << 8) | **p;
        (*p)++;
    }
    return 0;
}


static int asn1_get_mpi(uchar **p, uchar *end, mpi * X)
{
    int ret, len;

    if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_INTEGER)) != 0) {
        return ret;
    }
    ret = mpi_read_binary(X, *p, len);
    *p += len;
    return ret;
}


/*
    Version  ::=  INTEGER  {  v1(0), v2(1), v3(2)  }
 */
static int x509_get_version(uchar **p, uchar *end, int *ver)
{
    int ret, len;

    if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_CONTEXT_SPECIFIC | EST_ASN1_CONSTRUCTED | 0)) != 0) {
        if (ret == EST_ERR_ASN1_UNEXPECTED_TAG) {
            return *ver = 0;
        }
        return ret;
    }
    end = *p + len;

    if ((ret = asn1_get_int(p, end, ver)) != 0) {
        return EST_ERR_X509_CERT_INVALID_VERSION | ret;
    }
    if (*p != end) {
        return EST_ERR_X509_CERT_INVALID_VERSION | EST_ERR_ASN1_LENGTH_MISMATCH;
    }
    return 0;
}


/*
    CertificateSerialNumber  ::=  INTEGER
 */
static int x509_get_serial(uchar **p, uchar *end, x509_buf * serial)
{
    int ret;

    if ((end - *p) < 1) {
        return EST_ERR_X509_CERT_INVALID_SERIAL | EST_ERR_ASN1_OUT_OF_DATA;
    }
    if (**p != (EST_ASN1_CONTEXT_SPECIFIC | EST_ASN1_PRIMITIVE | 2) && **p != EST_ASN1_INTEGER) {
        return EST_ERR_X509_CERT_INVALID_SERIAL | EST_ERR_ASN1_UNEXPECTED_TAG;
    }
    serial->tag = *(*p)++;

    if ((ret = asn1_get_len(p, end, &serial->len)) != 0) {
        return EST_ERR_X509_CERT_INVALID_SERIAL | ret;
    }
    serial->p = *p;
    *p += serial->len;
    return 0;
}


/*
    AlgorithmIdentifier  ::=  SEQUENCE  {
         algorithm               OBJECT IDENTIFIER,
         parameters              ANY DEFINED BY algorithm OPTIONAL  }
 */
static int x509_get_alg(uchar **p, uchar *end, x509_buf * alg)
{
    int ret, len;

    if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_CONSTRUCTED | EST_ASN1_SEQUENCE)) != 0) {
        return EST_ERR_X509_CERT_INVALID_ALG | ret;
    }
    end = *p + len;
    alg->tag = **p;

    if ((ret = asn1_get_tag(p, end, &alg->len, EST_ASN1_OID)) != 0) {
        return EST_ERR_X509_CERT_INVALID_ALG | ret;
    }
    alg->p = *p;
    *p += alg->len;

    if (*p == end) {
        return 0;
    }

    /*
        assume the algorithm parameters must be NULL
     */
    if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_NULL)) != 0) {
        return EST_ERR_X509_CERT_INVALID_ALG | ret;
    }
    if (*p != end) {
        return EST_ERR_X509_CERT_INVALID_ALG | EST_ERR_ASN1_LENGTH_MISMATCH;
    }
    return 0;
}


/*
    RelativeDistinguishedName ::=
      SET OF AttributeTypeAndValue
  
    AttributeTypeAndValue ::= SEQUENCE {
      type     AttributeType,
      value    AttributeValue }
  
    AttributeType ::= OBJECT IDENTIFIER
  
    AttributeValue ::= ANY DEFINED BY AttributeType
 */
static int x509_get_name(uchar **p, uchar *end, x509_name * cur)
{
    int ret, len;
    uchar *end2;
    x509_buf *oid;
    x509_buf *val;

    if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_CONSTRUCTED | EST_ASN1_SET)) != 0) {
        return EST_ERR_X509_CERT_INVALID_NAME | ret;
    }
    end2 = end;
    end = *p + len;

    if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_CONSTRUCTED | EST_ASN1_SEQUENCE)) != 0) {
        return EST_ERR_X509_CERT_INVALID_NAME | ret;
    }
    if (*p + len != end) {
        return EST_ERR_X509_CERT_INVALID_NAME | EST_ERR_ASN1_LENGTH_MISMATCH;
    }
    oid = &cur->oid;
    oid->tag = **p;

    if ((ret = asn1_get_tag(p, end, &oid->len, EST_ASN1_OID)) != 0) {
        return EST_ERR_X509_CERT_INVALID_NAME | ret;
    }
    oid->p = *p;
    *p += oid->len;

    if ((end - *p) < 1) {
        return EST_ERR_X509_CERT_INVALID_NAME | EST_ERR_ASN1_OUT_OF_DATA;
    }
    if (**p != EST_ASN1_BMP_STRING && **p != EST_ASN1_UTF8_STRING &&
            **p != EST_ASN1_T61_STRING && **p != EST_ASN1_PRINTABLE_STRING &&
            **p != EST_ASN1_IA5_STRING && **p != EST_ASN1_UNIVERSAL_STRING) {
        return EST_ERR_X509_CERT_INVALID_NAME | EST_ERR_ASN1_UNEXPECTED_TAG;
    }

    val = &cur->val;
    val->tag = *(*p)++;

    if ((ret = asn1_get_len(p, end, &val->len)) != 0) {
        return EST_ERR_X509_CERT_INVALID_NAME | ret;
    }
    val->p = *p;
    *p += val->len;

    cur->next = NULL;

    if (*p != end) {
        return EST_ERR_X509_CERT_INVALID_NAME | EST_ERR_ASN1_LENGTH_MISMATCH;
    }

    /*
       recurse until end of SEQUENCE is reached
     */
    if (*p == end2) {
        return 0;
    }
    cur->next = (x509_name *) malloc(sizeof(x509_name));

    if (cur->next == NULL) {
        return 1;
    }
    return x509_get_name(p, end2, cur->next);
}


/*
    Validity ::= SEQUENCE {
         notBefore      Time,
         notAfter       Time }
  
    Time ::= CHOICE {
         utcTime        UTCTime,
         generalTime    GeneralizedTime }
 */
static int x509_get_dates(uchar **p, uchar *end, x509_time *from, x509_time *to)
{
    int ret, len;
    char date[64];

    if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_CONSTRUCTED | EST_ASN1_SEQUENCE)) != 0) {
        return EST_ERR_X509_CERT_INVALID_DATE | ret;
    }
    end = *p + len;

    if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_UTC_TIME)) != 0) {
        return EST_ERR_X509_CERT_INVALID_DATE | ret;
    }
    memset(date, 0, sizeof(date));
    memcpy(date, *p, (len < (int)sizeof(date) - 1) ?  len : (int)sizeof(date) - 1);

    if (sscanf(date, "%2d%2d%2d%2d%2d%2d", &from->year, &from->mon, &from->day, &from->hour, &from->min, &from->sec) < 5) {
        return EST_ERR_X509_CERT_INVALID_DATE;
    }
    from->year += 100 * (from->year < 90);
    from->year += 1900;
    *p += len;

    if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_UTC_TIME)) != 0) {
        return EST_ERR_X509_CERT_INVALID_DATE | ret;
    }
    memset(date, 0, sizeof(date));
    memcpy(date, *p, (len < (int)sizeof(date) - 1) ?  len : (int)sizeof(date) - 1);

    if (sscanf(date, "%2d%2d%2d%2d%2d%2d", &to->year, &to->mon, &to->day, &to->hour, &to->min, &to->sec) < 5) {
        return EST_ERR_X509_CERT_INVALID_DATE;
    }
    to->year += 100 * (to->year < 90);
    to->year += 1900;
    *p += len;

    if (*p != end) {
        return EST_ERR_X509_CERT_INVALID_DATE | EST_ERR_ASN1_LENGTH_MISMATCH;
    }
    return 0;
}


/*
    SubjectPublicKeyInfo  ::=  SEQUENCE  {
         algorithm            AlgorithmIdentifier,
         subjectPublicKey     BIT STRING }
 */
static int x509_get_pubkey(uchar **p, uchar *end, x509_buf * pk_alg_oid, mpi * N, mpi * E)
{
    int ret, len;
    uchar *end2;

    if ((ret = x509_get_alg(p, end, pk_alg_oid)) != 0) {
        return ret;
    }

    /*
     * only RSA public keys handled at this time
     */
    if (pk_alg_oid->len != 9 || memcmp(pk_alg_oid->p, OID_PKCS1_RSA, 9) != 0) {
        return EST_ERR_X509_CERT_UNKNOWN_PK_ALG;
    }
    if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_ME_STRING)) != 0) {
        return EST_ERR_X509_CERT_INVALID_PUBKEY | ret;
    }
    if ((end - *p) < 1) {
        return EST_ERR_X509_CERT_INVALID_PUBKEY | EST_ERR_ASN1_OUT_OF_DATA;
    }
    end2 = *p + len;

    if (*(*p)++ != 0) {
        return EST_ERR_X509_CERT_INVALID_PUBKEY;
    }

    /*
            RSAPublicKey ::= SEQUENCE {
                    modulus                   INTEGER,      -- n
                    publicExponent    INTEGER       -- e
            }
     */
    if ((ret = asn1_get_tag(p, end2, &len, EST_ASN1_CONSTRUCTED | EST_ASN1_SEQUENCE)) != 0) {
        return EST_ERR_X509_CERT_INVALID_PUBKEY | ret;
    }
    if (*p + len != end2) {
        return EST_ERR_X509_CERT_INVALID_PUBKEY | EST_ERR_ASN1_LENGTH_MISMATCH;
    }
    if ((ret = asn1_get_mpi(p, end2, N)) != 0 || (ret = asn1_get_mpi(p, end2, E)) != 0) {
        return EST_ERR_X509_CERT_INVALID_PUBKEY | ret;
    }
    if (*p != end) {
        return EST_ERR_X509_CERT_INVALID_PUBKEY | EST_ERR_ASN1_LENGTH_MISMATCH;
    }
    return 0;
}


static int x509_get_sig(uchar **p, uchar *end, x509_buf * sig)
{
    int ret, len;

    sig->tag = **p;

    if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_ME_STRING)) != 0) {
        return EST_ERR_X509_CERT_INVALID_SIGNATURE | ret;
    }
    if (--len < 1 || *(*p)++ != 0) {
        return EST_ERR_X509_CERT_INVALID_SIGNATURE;
    }
    sig->len = len;
    sig->p = *p;
    *p += len;
    return 0;
}


/*
    X.509 v2/v3 unique identifier (not parsed)
 */
static int x509_get_uid(uchar **p, uchar *end, x509_buf * uid, int n)
{
    int ret;

    if (*p == end) {
        return 0;
    }
    uid->tag = **p;

    if ((ret = asn1_get_tag(p, end, &uid->len, EST_ASN1_CONTEXT_SPECIFIC | EST_ASN1_CONSTRUCTED | n)) != 0) {
        if (ret == EST_ERR_ASN1_UNEXPECTED_TAG) {
            return 0;
        }
        return ret;
    }
    uid->p = *p;
    *p += uid->len;
    return 0;
}


/*
   X.509 v3 extensions (only BasicConstraints are parsed)
 */
static int x509_get_ext(uchar **p, uchar *end, x509_buf * ext, int *ca_istrue, int *max_pathlen)
{
    int ret, len;
    int is_critical = 1;
    int is_cacert = 0;
    uchar *end2;

    if (*p == end) {
        return 0;
    }
    ext->tag = **p;

    if ((ret = asn1_get_tag(p, end, &ext->len, EST_ASN1_CONTEXT_SPECIFIC | EST_ASN1_CONSTRUCTED | 3)) != 0) {
        if (ret == EST_ERR_ASN1_UNEXPECTED_TAG) {
            return 0;
        }
        return ret;
    }
    ext->p = *p;
    end = *p + ext->len;

    /*
       Extensions  ::=      SEQUENCE SIZE (1..MAX) OF Extension
      
       Extension  ::=  SEQUENCE      {
                    extnID          OBJECT IDENTIFIER,
                    critical        BOOLEAN DEFAULT FALSE,
                    extnValue       OCTET STRING  }
     */
    if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_CONSTRUCTED | EST_ASN1_SEQUENCE)) != 0) {
        return EST_ERR_X509_CERT_INVALID_EXTENSIONS | ret;
    }
    if (end != *p + len) {
        return EST_ERR_X509_CERT_INVALID_EXTENSIONS | EST_ERR_ASN1_LENGTH_MISMATCH;
    }

    while (*p < end) {
        if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_CONSTRUCTED | EST_ASN1_SEQUENCE)) != 0) {
            return EST_ERR_X509_CERT_INVALID_EXTENSIONS | ret;
        }
        if (memcmp(*p, "\x06\x03\x55\x1D\x13", 5) != 0) {
            *p += len;
            continue;
        }

        *p += 5;

        if ((ret = asn1_get_bool(p, end, &is_critical)) != 0 && (ret != EST_ERR_ASN1_UNEXPECTED_TAG)) {
            return EST_ERR_X509_CERT_INVALID_EXTENSIONS | ret;
        }
        if ((ret = asn1_get_tag(p, end, &len, EST_ASN1_OCTET_STRING)) != 0) {
            return EST_ERR_X509_CERT_INVALID_EXTENSIONS | ret;
        }
        /*
           BasicConstraints ::= SEQUENCE {
                        cA                                              BOOLEAN DEFAULT FALSE,
                        pathLenConstraint               INTEGER (0..MAX) OPTIONAL }
         */
        end2 = *p + len;

        if ((ret = asn1_get_tag(p, end2, &len, EST_ASN1_CONSTRUCTED | EST_ASN1_SEQUENCE)) != 0) {
            return EST_ERR_X509_CERT_INVALID_EXTENSIONS | ret;
        }
        if (*p == end2) {
            continue;
        }
        if ((ret = asn1_get_bool(p, end2, &is_cacert)) != 0) {
            if (ret == EST_ERR_ASN1_UNEXPECTED_TAG) {
                ret = asn1_get_int(p, end2, &is_cacert);
            }
            if (ret != 0) {
                return EST_ERR_X509_CERT_INVALID_EXTENSIONS | ret;
            }
            if (is_cacert != 0) {
                is_cacert = 1;
            }
        }
        if (*p == end2) {
            continue;
        }
        if ((ret = asn1_get_int(p, end2, max_pathlen)) != 0) {
            return EST_ERR_X509_CERT_INVALID_EXTENSIONS | ret;
        }
        if (*p != end2) {
            return EST_ERR_X509_CERT_INVALID_EXTENSIONS | EST_ERR_ASN1_LENGTH_MISMATCH;
        }
        max_pathlen++;
    }

    if (*p != end) {
        return EST_ERR_X509_CERT_INVALID_EXTENSIONS | EST_ERR_ASN1_LENGTH_MISMATCH;
    }
    *ca_istrue = is_critical & is_cacert;
    return 0;
}


/*
   Parse one or more certificates and add them to the chained list
 */
int x509parse_crt(x509_cert * chain, uchar *buf, int buflen)
{
    int ret, len;
    uchar *s1, *s2, *oldbuf;
    uchar *p, *end;
    x509_cert *crt;

    crt = chain;

    while (crt->version != 0) {
        crt = crt->next;
    }

    /*
       check if the certificate is encoded in base64
     */
    s1 = (uchar *)strstr((char *)buf, "-----BEGIN CERTIFICATE-----");

    if (s1 != NULL) {
        s2 = (uchar *)strstr((char *)buf, "-----END CERTIFICATE-----");

        if (s2 == NULL || s2 <= s1) {
            return EST_ERR_X509_CERT_INVALID_PEM;
        }
        s1 += 27;
        if (*s1 == '\r') {
            s1++;
        }
        if (*s1 == '\n') {
            s1++;
        } else {
            return EST_ERR_X509_CERT_INVALID_PEM;
        }

        /*
           get the DER data length and decode the buffer
         */
        len = 0;
        ret = base64_decode(NULL, &len, s1, s2 - s1);

        if (ret == EST_ERR_BASE64_INVALID_CHARACTER) {
            return EST_ERR_X509_CERT_INVALID_PEM | ret;
        }
        if ((p = (uchar *)malloc(len)) == NULL) {
            return 1;
        }
        if ((ret = base64_decode(p, &len, s1, s2 - s1)) != 0) {
            free(p);
            return EST_ERR_X509_CERT_INVALID_PEM | ret;
        }

        /*
            update the buffer size and offset
         */
        s2 += 25;
        if (*s2 == '\r') {
            s2++;
        }
        if (*s2 == '\n') {
            s2++;
        } else {
            free(p);
            return EST_ERR_X509_CERT_INVALID_PEM;
        }
        oldbuf = buf;
        buflen -= s2 - buf;
        buf = s2;

    } else {
        /*
           nope, copy the raw DER data
         */
        p = (uchar*) malloc(len = buflen);
        if (p == NULL) {
            return 1;
        }
        memcpy(p, buf, buflen);
        oldbuf = buf;
        buflen = 0;
    }

    crt->raw.p = p;
    crt->raw.len = len;
    end = p + len;

    /*
       Certificate  ::=      SEQUENCE  {
                    tbsCertificate           TBSCertificate,
                    signatureAlgorithm       AlgorithmIdentifier,
                    signatureValue           BIT STRING      }
     */
    if ((ret = asn1_get_tag(&p, end, &len, EST_ASN1_CONSTRUCTED | EST_ASN1_SEQUENCE)) != 0) {
        x509_free(crt);
        return EST_ERR_X509_CERT_INVALID_FORMAT;
    }
    if (len != (int)(end - p)) {
        x509_free(crt);
        return EST_ERR_X509_CERT_INVALID_FORMAT | EST_ERR_ASN1_LENGTH_MISMATCH;
    }

    /*
       TBSCertificate  ::=  SEQUENCE  {
     */
    crt->tbs.p = p;
    if ((ret = asn1_get_tag(&p, end, &len, EST_ASN1_CONSTRUCTED | EST_ASN1_SEQUENCE)) != 0) {
        x509_free(crt);
        return EST_ERR_X509_CERT_INVALID_FORMAT | ret;
    }
    end = p + len;
    crt->tbs.len = end - crt->tbs.p;

    /*
       Version      ::=      INTEGER  {      v1(0), v2(1), v3(2)  }
      
       CertificateSerialNumber      ::=      INTEGER
      
       signature                    AlgorithmIdentifier
     */
    if ((ret = x509_get_version(&p, end, &crt->version)) != 0 ||
        (ret = x509_get_serial(&p, end, &crt->serial)) != 0 ||
        (ret = x509_get_alg(&p, end, &crt->sig_oid1)) != 0) {
        x509_free(crt);
        return ret;
    }
    crt->version++;
    if (crt->version > 3) {
        x509_free(crt);
        return EST_ERR_X509_CERT_UNKNOWN_VERSION;
    }
    if (crt->sig_oid1.len != 9 || memcmp(crt->sig_oid1.p, OID_PKCS1, 8) != 0) {
        x509_free(crt);
#if UNUSED
if (buflen > 0) {
    goto error;
}
#endif
        return EST_ERR_X509_CERT_UNKNOWN_SIG_ALG;
    }
    if (crt->sig_oid1.p[8] < 2 || crt->sig_oid1.p[8] > 5) {
        x509_free(crt);
#if UNUSED
if (buflen > 0) {
    goto error;
}
#endif
        return EST_ERR_X509_CERT_UNKNOWN_SIG_ALG;
    }
    /*
        issuer Name
     */
    crt->issuer_raw.p = p;
    if ((ret = asn1_get_tag(&p, end, &len, EST_ASN1_CONSTRUCTED | EST_ASN1_SEQUENCE)) != 0) {
        x509_free(crt);
        return EST_ERR_X509_CERT_INVALID_FORMAT | ret;
    }
    if ((ret = x509_get_name(&p, p + len, &crt->issuer)) != 0) {
        x509_free(crt);
        return ret;
    }
    crt->issuer_raw.len = p - crt->issuer_raw.p;

    /*
       Validity ::= SEQUENCE {
                    notBefore          Time,
                    notAfter           Time }
      
     */
    if ((ret = x509_get_dates(&p, end, &crt->valid_from, &crt->valid_to)) != 0) {
        x509_free(crt);
#if UNUSED
if (buflen > 0) {
    goto error;
}
#endif
        return ret;
    }

    /*
        subject Name
     */
    crt->subject_raw.p = p;

    if ((ret = asn1_get_tag(&p, end, &len, EST_ASN1_CONSTRUCTED | EST_ASN1_SEQUENCE)) != 0) {
        x509_free(crt);
        return EST_ERR_X509_CERT_INVALID_FORMAT | ret;
    }
    if ((ret = x509_get_name(&p, p + len, &crt->subject)) != 0) {
        x509_free(crt);
        return ret;
    }
    crt->subject_raw.len = p - crt->subject_raw.p;

    /*
       SubjectPublicKeyInfo  ::=  SEQUENCE
            algorithm             AlgorithmIdentifier,
            subjectPublicKey      BIT STRING      }
     */
    if ((ret = asn1_get_tag(&p, end, &len, EST_ASN1_CONSTRUCTED | EST_ASN1_SEQUENCE)) != 0) {
        x509_free(crt);
        return EST_ERR_X509_CERT_INVALID_FORMAT | ret;
    }
    if ((ret = x509_get_pubkey(&p, p + len, &crt->pk_oid, &crt->rsa.N, &crt->rsa.E)) != 0) {
        x509_free(crt);
        return ret;
    }
    if ((ret = rsa_check_pubkey(&crt->rsa)) != 0) {
        x509_free(crt);
        return ret;
    }
    crt->rsa.len = mpi_size(&crt->rsa.N);

    /*
        issuerUniqueID  [1]      IMPLICIT UniqueIdentifier OPTIONAL,
                                 -- If present, version shall be v2 or v3
        subjectUniqueID [2]      IMPLICIT UniqueIdentifier OPTIONAL,
                                 -- If present, version shall be v2 or v3
        extensions      [3]      EXPLICIT Extensions OPTIONAL
                                 -- If present, version shall be v3
     */
    if (crt->version == 2 || crt->version == 3) {
        ret = x509_get_uid(&p, end, &crt->issuer_id, 1);
        if (ret != 0) {
            x509_free(crt);
            return ret;
        }
    }
    if (crt->version == 2 || crt->version == 3) {
        ret = x509_get_uid(&p, end, &crt->subject_id, 2);
        if (ret != 0) {
            x509_free(crt);
            return ret;
        }
    }
    if (crt->version == 3) {
        ret = x509_get_ext(&p, end, &crt->v3_ext, &crt->ca_istrue, &crt->max_pathlen);
        if (ret != 0) {
            x509_free(crt);
            return ret;
        }
    }
    if (p != end) {
        x509_free(crt);
        return EST_ERR_X509_CERT_INVALID_FORMAT | EST_ERR_ASN1_LENGTH_MISMATCH;
    }
    end = crt->raw.p + crt->raw.len;

    /*
        signatureAlgorithm       AlgorithmIdentifier,
        signatureValue           BIT STRING
     */
    if ((ret = x509_get_alg(&p, end, &crt->sig_oid2)) != 0) {
        x509_free(crt);
        return ret;
    }
    if (memcmp(crt->sig_oid1.p, crt->sig_oid2.p, 9) != 0) {
        x509_free(crt);
        return EST_ERR_X509_CERT_SIG_MISMATCH;
    }
    if ((ret = x509_get_sig(&p, end, &crt->sig)) != 0) {
        x509_free(crt);
        return ret;
    }
    if (p != end) {
        x509_free(crt);
        return EST_ERR_X509_CERT_INVALID_FORMAT | EST_ERR_ASN1_LENGTH_MISMATCH;
    }
    crt->next = (x509_cert *) malloc(sizeof(x509_cert));

    if (crt->next == NULL) {
        x509_free(crt);
        return 1;
    }
    crt = crt->next;
    memset(crt, 0, sizeof(x509_cert));

#if UNUSED
more:
#endif
    if (buflen > 0) {
        int rc = x509parse_crt(crt, buf, buflen);
        return 0;
    }
    return 0;

#if UNUSED
error:
    {
        char msg[80], *cp;
        strncpy(msg, (char*) &oldbuf[1], sizeof(msg) - 1);
        if ((cp = strchr(msg, '\n')) != 0) {
            *cp = '\0';
        }
        printf("FAILED to parse %s\n", msg);
        memset(crt, 0, sizeof(x509_cert));
        goto more;
    }
#endif
}


/*
    Load one or more certificates and add them to the chained list
 */
int x509parse_crtfile(x509_cert * chain, char *path)
{
    int ret;
    FILE *f;
    size_t n;
    uchar *buf;

    if ((f = fopen(path, "rb")) == NULL) {
        return 1;
    }
    fseek(f, 0, SEEK_END);
    n = (size_t) ftell(f);
    fseek(f, 0, SEEK_SET);

    if ((buf = (uchar *)malloc(n + 1)) == NULL) {
        return 1;
    }
    if (fread(buf, 1, n, f) != n) {
        fclose(f);
        free(buf);
        return 1;
    }
    buf[n] = '\0';
    ret = x509parse_crt(chain, buf, (int)n);
    memset(buf, 0, n + 1);
    free(buf);
    fclose(f);
    return ret;
}


#if ME_EST_DES
/*
    Read a 16-byte hex string and convert it to binary
 */
static int x509_get_iv(uchar *s, uchar iv[8])
{
    int i, j, k;

    memset(iv, 0, 8);

    for (i = 0; i < 16; i++, s++) {
        if (*s >= '0' && *s <= '9') {
            j = *s - '0';
        } else if (*s >= 'A' && *s <= 'F') {
            j = *s - '7';
        } else if (*s >= 'a' && *s <= 'f') {
            j = *s - 'W';
        } else {
            return EST_ERR_X509_KEY_INVALID_ENC_IV;
        }
        k = ((i & 1) != 0) ? j : j << 4;
        iv[i >> 1] = (uchar)(iv[i >> 1] | k);
    }

    return 0;
}


/*
    Decrypt with 3DES-CBC, using PBKDF1 for key derivation
 */
static void x509_des3_decrypt(uchar des3_iv[8], uchar *buf, int buflen, uchar *pwd, int pwdlen)
{
    md5_context md5_ctx;
    des3_context des3_ctx;
    uchar md5sum[16];
    uchar des3_key[24];

    /*
       3DES key[ 0..15] = MD5(pwd || IV)
            key[16..23] = MD5(pwd || IV || 3DES key[ 0..15])
     */
    md5_starts(&md5_ctx);
    md5_update(&md5_ctx, pwd, pwdlen);
    md5_update(&md5_ctx, des3_iv, 8);
    md5_finish(&md5_ctx, md5sum);
    memcpy(des3_key, md5sum, 16);

    md5_starts(&md5_ctx);
    md5_update(&md5_ctx, md5sum, 16);
    md5_update(&md5_ctx, pwd, pwdlen);
    md5_update(&md5_ctx, des3_iv, 8);
    md5_finish(&md5_ctx, md5sum);
    memcpy(des3_key + 16, md5sum, 8);

    des3_set3key_dec(&des3_ctx, des3_key);
    des3_crypt_cbc(&des3_ctx, DES_DECRYPT, buflen, des3_iv, buf, buf);

    memset(&md5_ctx, 0, sizeof(md5_ctx));
    memset(&des3_ctx, 0, sizeof(des3_ctx));
    memset(md5sum, 0, 16);
    memset(des3_key, 0, 24);
}
#endif


/*
    Parse a private RSA key
 */
int x509parse_key(rsa_context * rsa, uchar *buf, int buflen, uchar *pwd, int pwdlen)
{
    int     ret, len, enc;
    uchar   *s1, *s2, *p, *end;
#if ME_EST_DES
    uchar   des3_iv[8];
#endif

    s1 = (uchar *)strstr((char *)buf, "-----BEGIN RSA PRIVATE KEY-----");

    if (s1 != NULL) {
        s2 = (uchar *)strstr((char *)buf, "-----END RSA PRIVATE KEY-----");

        if (s2 == NULL || s2 <= s1) {
            return EST_ERR_X509_KEY_INVALID_PEM;
        }
        s1 += 31;
        if (*s1 == '\r') {
            s1++;
        }
        if (*s1 == '\n') {
            s1++;
        } else {
            return EST_ERR_X509_KEY_INVALID_PEM;
        }
        enc = 0;

        if (memcmp(s1, "Proc-Type: 4,ENCRYPTED", 22) == 0) {
#if ME_EST_DES
            enc++;
            s1 += 22;
            if (*s1 == '\r') {
                s1++;
            }
            if (*s1 == '\n') {
                s1++;
            } else {
                return EST_ERR_X509_KEY_INVALID_PEM;
            }
            if (memcmp(s1, "DEK-Info: DES-EDE3-CBC,", 23) != 0) {
                return EST_ERR_X509_KEY_UNKNOWN_ENC_ALG;
            }
            s1 += 23;
            if (x509_get_iv(s1, des3_iv) != 0) {
                return EST_ERR_X509_KEY_INVALID_ENC_IV;
            }
            s1 += 16;
            if (*s1 == '\r') {
                s1++;
            }
            if (*s1 == '\n') {
                s1++;
            } else {
                return EST_ERR_X509_KEY_INVALID_PEM;
            }
#else
            return EST_ERR_X509_FEATURE_UNAVAILABLE;
#endif
        }
        len = 0;
        ret = base64_decode(NULL, &len, s1, s2 - s1);

        if (ret == EST_ERR_BASE64_INVALID_CHARACTER) {
            return ret | EST_ERR_X509_KEY_INVALID_PEM;
        }
        if ((buf = (uchar *)malloc(len)) == NULL) {
            return 1;
        }
        if ((ret = base64_decode(buf, &len, s1, s2 - s1)) != 0) {
            free(buf);
            return ret | EST_ERR_X509_KEY_INVALID_PEM;
        }
        buflen = len;

        if (enc != 0) {
#if ME_EST_DES
            if (pwd == NULL) {
                free(buf);
                return EST_ERR_X509_KEY_PASSWORD_REQUIRED;
            }
            x509_des3_decrypt(des3_iv, buf, buflen, pwd, pwdlen);

            if (buf[0] != 0x30 || buf[1] != 0x82 || buf[4] != 0x02 || buf[5] != 0x01) {
                free(buf);
                return EST_ERR_X509_KEY_PASSWORD_MISMATCH;
            }
#else
            return EST_ERR_X509_FEATURE_UNAVAILABLE;
#endif
        }
    }
    memset(rsa, 0, sizeof(rsa_context));

    p = buf;
    end = buf + buflen;

    /*
            RSAPrivateKey ::= SEQUENCE {
                    version                   Version,
                    modulus                   INTEGER,      -- n
                    publicExponent    INTEGER,      -- e
                    privateExponent   INTEGER,      -- d
                    prime1                    INTEGER,      -- p
                    prime2                    INTEGER,      -- q
                    exponent1                 INTEGER,      -- d mod (p-1)
                    exponent2                 INTEGER,      -- d mod (q-1)
                    coefficient               INTEGER,      -- (inverse of q) mod p
                    otherPrimeInfos   OtherPrimeInfos OPTIONAL
            }
     */
    if ((ret = asn1_get_tag(&p, end, &len, EST_ASN1_CONSTRUCTED | EST_ASN1_SEQUENCE)) != 0) {
        if (s1 != NULL) {
            free(buf);
        }
        rsa_free(rsa);
        return EST_ERR_X509_KEY_INVALID_FORMAT | ret;
    }
    end = p + len;

    if ((ret = asn1_get_int(&p, end, &rsa->ver)) != 0) {
        if (s1 != NULL) {
            free(buf);
        }
        rsa_free(rsa);
        return EST_ERR_X509_KEY_INVALID_FORMAT | ret;
    }
    if (rsa->ver != 0) {
        if (s1 != NULL) {
            free(buf);
        }
        rsa_free(rsa);
        return ret | EST_ERR_X509_KEY_INVALID_VERSION;
    }

    if ((ret = asn1_get_mpi(&p, end, &rsa->N)) != 0 ||
        (ret = asn1_get_mpi(&p, end, &rsa->E)) != 0 ||
        (ret = asn1_get_mpi(&p, end, &rsa->D)) != 0 ||
        (ret = asn1_get_mpi(&p, end, &rsa->P)) != 0 ||
        (ret = asn1_get_mpi(&p, end, &rsa->Q)) != 0 ||
        (ret = asn1_get_mpi(&p, end, &rsa->DP)) != 0 ||
        (ret = asn1_get_mpi(&p, end, &rsa->DQ)) != 0 ||
        (ret = asn1_get_mpi(&p, end, &rsa->QP)) != 0) {
        if (s1 != NULL) {
            free(buf);
        }
        rsa_free(rsa);
        return ret | EST_ERR_X509_KEY_INVALID_FORMAT;
    }

    rsa->len = mpi_size(&rsa->N);

    if (p != end) {
        if (s1 != NULL) {
            free(buf);
        }
        rsa_free(rsa);
        return EST_ERR_X509_KEY_INVALID_FORMAT | EST_ERR_ASN1_LENGTH_MISMATCH;
    }
    if ((ret = rsa_check_privkey(rsa)) != 0) {
        if (s1 != NULL) {
            free(buf);
        }
        rsa_free(rsa);
        return ret;
    }
    if (s1 != NULL) {
        free(buf);
    }
    return 0;
}


/*
    Load and parse a private RSA key
 */
int x509parse_keyfile(rsa_context * rsa, char *path, char *pwd)
{
    int ret;
    FILE *f;
    size_t n;
    uchar *buf;

    if ((f = fopen(path, "rb")) == NULL) {
        return 1;
    }

    fseek(f, 0, SEEK_END);
    n = (size_t) ftell(f);
    fseek(f, 0, SEEK_SET);

    if ((buf = (uchar *)malloc(n + 1)) == NULL) {
        return 1;
    }
    if (fread(buf, 1, n, f) != n) {
        fclose(f);
        free(buf);
        return 1;
    }
    buf[n] = '\0';

    if (pwd == NULL) {
        ret = x509parse_key(rsa, buf, (int)n, NULL, 0);
    } else {
        ret = x509parse_key(rsa, buf, (int)n, (uchar *)pwd, strlen(pwd));
    }
    memset(buf, 0, n + 1);
    free(buf);
    fclose(f);
    return ret;
}


/*
    Store the name in printable form into buf; no more than (end - buf) characters will be written
 */
int x509parse_dn_gets(char *prefix, char *buf, int bufsize, x509_name * dn)
{
    x509_name   *name;
    char        *end, s[128], *p;
    int         i;
    uchar       c;

    memset(s, 0, sizeof(s));
    name = dn;
    p = buf;
    end = &buf[bufsize];

    while (name != NULL) {
        p += snfmt(p, end - p, "%s", prefix);
        if (memcmp(name->oid.p, OID_X520, 2) == 0) {
            switch (name->oid.p[2]) {
            case X520_COMMON_NAME:
                p += snfmt(p, end - p, "CN=");
                break;

            case X520_COUNTRY:
                p += snfmt(p, end - p, "C=");
                break;

            case X520_LOCALITY:
                p += snfmt(p, end - p, "L=");
                break;

            case X520_STATE:
                p += snfmt(p, end - p, "ST=");
                break;

            case X520_ORGANIZATION:
                p += snfmt(p, end - p, "O=");
                break;

            case X520_ORG_UNIT:
                p += snfmt(p, end - p, "OU=");
                break;

            default:
                p += snfmt(p, end - p, "0x%02X=", name->oid.p[2]);
                break;
            }
        } else if (memcmp(name->oid.p, OID_PKCS9, 8) == 0) {
            switch (name->oid.p[8]) {
            case PKCS9_EMAIL:
                p += snfmt(p, end - p, "EMAIL=");
                break;

            default:
                p += snfmt(p, end - p, "0x%02X=", name->oid.p[8]);
                break;
            }
        } else {
            p += snfmt(p, end - p, "\?\?=");
        }
        for (i = 0; i < name->val.len; i++) {
            if (i >= (int)sizeof(s) - 1) {
                break;
            }
            c = name->val.p[i];
            if (c < 32 || c == 127 || (c > 128 && c < 160)) {
                s[i] = '?';
            } else {
                s[i] = c;
            }
        }
        s[i] = '\0';
        p += snfmt(p, end - p, "%s", s);
        name = name->next;
        p += snfmt(p, end - p, ",");
    }
    return p - buf;
}


/*
    Return an informational string about the certificate, or NULL if memory allocation failed
 */
char *x509parse_cert_info(char *prefix, char *buf, int bufsize, x509_cert *crt)
{
    char    *end, *p, *cipher, pbuf[5120];
    int     i, n;

    p = buf;
    end = &buf[bufsize];
    p += snfmt(p, end - p, "%sVERSION=%d,%sSERIAL=", prefix, crt->version, prefix);
    n = (crt->serial.len <= 32) ? crt->serial.len : 32;
    for (i = 0; i < n; i++) {
        p += snfmt(p, end - p, "%02X%s", crt->serial.p[i], (i < n - 1) ? ":" : "");
    }
    p += snfmt(p, end - p, ",");

    snfmt(pbuf, sizeof(pbuf), "%sS_", prefix);
    p += x509parse_dn_gets(pbuf, p, end - p, &crt->subject);

    snfmt(pbuf, sizeof(pbuf), "%sI_", prefix);
    p += x509parse_dn_gets(pbuf, p, end - p, &crt->issuer);

    p += snfmt(p, end - p, "%sSTART=%04d-%02d-%02d %02d:%02d:%02d,", prefix, crt->valid_from.year, crt->valid_from.mon,
        crt->valid_from.day, crt->valid_from.hour, crt->valid_from.min, crt->valid_from.sec);

    p += snfmt(p, end - p, "%sEND=%04d-%02d-%02d %02d:%02d:%02d,", prefix, crt->valid_to.year, crt->valid_to.mon, 
        crt->valid_to.day, crt->valid_to.hour, crt->valid_to.min, crt->valid_to.sec);

    switch (crt->sig_oid1.p[8]) {
    case RSA_MD2:
        cipher = "RSA_MD2";
        break;
    case RSA_MD4:
        cipher = "RSA_MD4";
        break;
    case RSA_MD5:
        cipher = "RSA_MD5";
        break;
    case RSA_SHA1:
        cipher = "RSA_SHA1";
        break;
    default:
        cipher = "RSA";
        break;
    }
    p += snfmt(p, end - p, "%sCIPHER=%s,", prefix, cipher);
    p += snfmt(p, end - p, "%sKEYSIZE=%d,", prefix, crt->rsa.N.n * (int)sizeof(ulong) * 8);
    return buf;
}


/*
    Return 0 if the certificate is still valid, or BADCERT_EXPIRED
 */
int x509parse_expired(x509_cert * crt)
{
    struct tm *lt;
    time_t tt;

    tt = time(NULL);
    lt = localtime(&tt);

    if (lt->tm_year > crt->valid_to.year - 1900) {
        return BADCERT_EXPIRED;
    }
    if (lt->tm_year == crt->valid_to.year - 1900 && lt->tm_mon > crt->valid_to.mon - 1) {
        return BADCERT_EXPIRED;
    }
    if (lt->tm_year == crt->valid_to.year - 1900 && lt->tm_mon == crt->valid_to.mon - 1 && lt->tm_mday > crt->valid_to.day) {
        return BADCERT_EXPIRED;
    }
    return 0;
}

static void x509_hash(uchar *in, int len, int alg, uchar *out)
{
    switch (alg) {
#if ME_EST_MD2
    case RSA_MD2:
        md2(in, len, out);
        break;
#endif
#if ME_EST_MD4
    case RSA_MD4:
        md4(in, len, out);
        break;
#endif
    case RSA_MD5:
        md5(in, len, out);
        break;
    case RSA_SHA1:
        sha1(in, len, out);
        break;
    default:
        memset(out, '\xFF', len);
        break;
    }
}


/*
    Verify the certificate validity
 */
int x509parse_verify(x509_cert *crt, x509_cert *trust_ca, char *cn, int *flags)
{
    x509_cert   *cur;
    x509_name   *name;
    uchar       hash[20];
    char        *domain, *peer;
    int         cn_len, hash_id, pathlen;

    *flags = x509parse_expired(crt);

    if (cn != NULL) {
        name = &crt->subject;
        cn_len = strlen(cn);

        while (name != NULL) {
            if (memcmp(name->oid.p, OID_CN, 3) == 0) {
                peer = (char*) name->val.p;
                if (name->val.len == cn_len && memcmp(peer, cn, cn_len) == 0) {
                    break;
                }
                /* 
                    Cert peer name must be of the form *.domain.tld. i.e. *.com is not valid 
                 */
                if (name->val.len > 2 && peer[0] == '*' && peer[1] == '.' && strchr(&peer[2], '.')) {
                    /* 
                        Required peer name must have a domain portion. i.e. domain.tld 
                     */
                    if ((domain = strchr(cn, '.')) != 0 && strncmp(&peer[2], &domain[1], name->val.len - 2) == 0) {
                        break;
                    }
                }
            }
            name = name->next;
        }
        if (name == NULL) {
            *flags |= BADCERT_CN_MISMATCH;
        }
    }
    *flags |= BADCERT_NOT_TRUSTED;

    /*
        Iterate upwards in the given cert chain, ignoring any upper cert with CA != TRUE.
     */
    cur = crt->next;
    pathlen = 1;

    while (cur->version != 0) {
        if (cur->ca_istrue == 0 || crt->issuer_raw.len != cur->subject_raw.len ||
            memcmp(crt->issuer_raw.p, cur->subject_raw.p, crt->issuer_raw.len) != 0) {
            cur = cur->next;
            continue;
        }
        hash_id = crt->sig_oid1.p[8];
        x509_hash(crt->tbs.p, crt->tbs.len, hash_id, hash);
        if (rsa_pkcs1_verify(&cur->rsa, RSA_PUBLIC, hash_id, 0, hash, crt->sig.p) != 0) {
            return EST_ERR_X509_CERT_VERIFY_FAILED;
        }
        pathlen++;
        crt = cur;
        cur = crt->next;
    }

    /*
        Atempt to validate topmost cert with our CA chain.
     */
    while (trust_ca->version != 0) {
        if (crt->issuer_raw.len != trust_ca->subject_raw.len ||
                memcmp(crt->issuer_raw.p, trust_ca->subject_raw.p, crt->issuer_raw.len) != 0) {
            trust_ca = trust_ca->next;
            continue;
        }
        if (trust_ca->max_pathlen > 0 && trust_ca->max_pathlen < pathlen) {
            break;
        }
        hash_id = crt->sig_oid1.p[8];
        x509_hash(crt->tbs.p, crt->tbs.len, hash_id, hash);

        if (rsa_pkcs1_verify(&trust_ca->rsa, RSA_PUBLIC, hash_id, 0, hash, crt->sig.p) == 0) {
            /*
                cert. is signed by a trusted CA
             */
            *flags &= ~BADCERT_NOT_TRUSTED;
            break;
        }
        trust_ca = trust_ca->next;
    }
    if (*flags & BADCERT_NOT_TRUSTED) {
        if (crt->issuer_raw.len == crt->subject_raw.len && 
                memcmp(crt->issuer_raw.p, crt->subject_raw.p, crt->issuer_raw.len) == 0) {
            *flags |= BADCERT_SELF_SIGNED;
        }
    }
    if (*flags != 0) {
        return EST_ERR_X509_CERT_VERIFY_FAILED;
    }
    return 0;
}


/*
    Unallocate all certificate data
 */
void x509_free(x509_cert * crt)
{
    x509_cert *cert_cur = crt;
    x509_cert *cert_prv;
    x509_name *name_cur;
    x509_name *name_prv;

    if (crt == NULL) {
        return;
    }
    do {
        rsa_free(&cert_cur->rsa);

        name_cur = cert_cur->issuer.next;
        while (name_cur != NULL) {
            name_prv = name_cur;
            name_cur = name_cur->next;
            memset(name_prv, 0, sizeof(x509_name));
            free(name_prv);
        }
        name_cur = cert_cur->subject.next;
        while (name_cur != NULL) {
            name_prv = name_cur;
            name_cur = name_cur->next;
            memset(name_prv, 0, sizeof(x509_name));
            free(name_prv);
        }
        if (cert_cur->raw.p != NULL) {
            memset(cert_cur->raw.p, 0, cert_cur->raw.len);
            free(cert_cur->raw.p);
        }
        cert_cur = cert_cur->next;
    } while (cert_cur != NULL);

    cert_cur = crt;
    do {
        cert_prv = cert_cur;
        cert_cur = cert_cur->next;

        memset(cert_prv, 0, sizeof(x509_cert));
        if (cert_prv != crt) {
            free(cert_prv);
        }
    } while (cert_cur != NULL);
}


#endif



/********* Start of file src/xtea.c ************/


/*
    xtea.c -- An 32-bit implementation of the XTEA algorithm

    Copyright (c) All Rights Reserved. See details at the end of the file.
 */


#if ME_EST_XTEA

/*
   32-bit integer manipulation macros (big endian)
 */
#ifndef GET_ULONG_BE
#define GET_ULONG_BE(n,b,i)                     \
    {                                           \
        (n) = ( (ulong) (b)[(i)    ] << 24 )    \
            | ( (ulong) (b)[(i) + 1] << 16 )    \
            | ( (ulong) (b)[(i) + 2] <<  8 )    \
            | ( (ulong) (b)[(i) + 3]       );   \
    }
#endif

#ifndef PUT_ULONG_BE
#define PUT_ULONG_BE(n,b,i)                     \
    {                                           \
        (b)[(i)    ] = (uchar) ( (n) >> 24 );   \
        (b)[(i) + 1] = (uchar) ( (n) >> 16 );   \
        (b)[(i) + 2] = (uchar) ( (n) >>  8 );   \
        (b)[(i) + 3] = (uchar) ( (n)       );   \
    }
#endif

/*
   XTEA key schedule
 */
void xtea_setup(xtea_context *ctx, uchar key[16])
{
    int i;

    memset(ctx, 0, sizeof(xtea_context));
    for (i = 0; i < 4; i++) {
        GET_ULONG_BE(ctx->k[i], key, i << 2);
    }
}


/*
   XTEA encrypt function
 */
void xtea_crypt_ecb(xtea_context *ctx, int mode, uchar input[8], uchar output[8])
{
    ulong *k, v0, v1, i;

    k = ctx->k;
    GET_ULONG_BE(v0, input, 0);
    GET_ULONG_BE(v1, input, 4);

    if (mode == XTEA_ENCRYPT) {
        ulong sum = 0, delta = 0x9E3779B9;

        for (i = 0; i < 32; i++) {
            v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + k[sum & 3]);
            sum += delta;
            v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + k[(sum >> 11) & 3]);
        }
    } else {
        /* XTEA_DECRYPT */
        ulong delta = 0x9E3779B9, sum = delta * 32;

        for (i = 0; i < 32; i++) {
            v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + k[(sum >> 11) & 3]);
            sum -= delta;
            v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + k[sum & 3]);
        }
    }
    PUT_ULONG_BE(v0, output, 0);
    PUT_ULONG_BE(v1, output, 4);
}

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

#endif /* ME_COM_EST */