/* md4.cpp - RSA Data Security, Inc., MD4 message-digest algorithm
 */

/* Copyright (C) 1990-2, RSA Data Security, Inc. All rights reserved.

   License to copy and use this software is granted provided that it
   is identified as the "RSA Data Security, Inc. MD4 Message-Digest
   Algorithm" in all material mentioning or referencing this software
   or this function.

   License is also granted to make and use derivative works provided
   that such works are identified as "derived from the RSA Data
   Security, Inc. MD4 Message-Digest Algorithm" in all material
   mentioning or referencing the derived work.

   RSA Data Security, Inc. makes no representations concerning either
   the merchantability of this software or the suitability of this
   software for any particular purpose. It is provided "as is"
   without express or implied warranty of any kind.

   These notices must be retained in any copies of any part of this
   documentation and/or software.
 */

#include <string>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <sstream>
#include "md4.h"
#include "../logger.h"

// Static flag to log only once
static bool s_md4Logged = false;


// F, G and H are basic MD4 functions.
MD4::UINT4 MD4::F(UINT4 x, UINT4 y, UINT4 z)
{
	// Log only once when first MD4 function is called
	if (!s_md4Logged) {
		Logger::log("MD4 hashing system initialized [md4.cpp]", "", 0);
		s_md4Logged = true;
	}
	return (((x) & (y)) | ((~x) & (z)));
}

MD4::UINT4 MD4::G(UINT4 x, UINT4 y, UINT4 z)
{
	return (((x) & (y)) | ((x) & (z)) | ((y) & (z)));
}

MD4::UINT4 MD4::H(UINT4 x, UINT4 y, UINT4 z)
{
	return ((x) ^ (y) ^ (z));
}

// ROTATE_LEFT rotates x left n bits.
MD4::UINT4 MD4::ROTATE_LEFT(UINT4 x, int n)
{
	return (((x) << (n)) | ((x) >> (32-(n))));
}

/* FF, GG and HH are transformations for rounds 1, 2 and 3 */
/* Rotation is separate from addition to prevent recomputation */

MD4::UINT4 MD4::FF(UINT4 a, UINT4 b, UINT4 c, UINT4 d, UINT4 x, int s)
{
	a += F((b), (c), (d)) + (x);
	return a = ROTATE_LEFT((a), (s));
}

MD4::UINT4 MD4::GG(UINT4 a, UINT4 b, UINT4 c, UINT4 d, UINT4 x, int s)
{
	a += G((b), (c), (d)) + (x) + (UINT4)0x5a827999;
	return a = ROTATE_LEFT((a), (s));
}

MD4::UINT4 MD4::HH(UINT4 a, UINT4 b, UINT4 c, UINT4 d, UINT4 x, int s)
{
	a += H((b), (c), (d)) + (x) + (UINT4)0x6ed9eba1;
	return a = ROTATE_LEFT((a), (s));
}

/* MD4 initialization. Begins an MD4 operation, writing a new context.
 */
void MD4::Init(MD4_CTX * context)
{
	context->count[0] = context->count[1] = 0;

	// Load magic initialization constants.
	context->state[0] = 0x67452301;
	context->state[1] = 0xefcdab89;
	context->state[2] = 0x98badcfe;
	context->state[3] = 0x10325476;

	step = 0;
}

/* MD4 block update operation. Continues an MD4 message-digest
     operation, processing another message block, and updating the
     context.
 */
void MD4::Update(MD4_CTX *context, unsigned char *input, unsigned int inputLen)
{
	unsigned int i, index, partLen;

	// Compute number of bytes mod 64
	index = (unsigned int)((context->count[0] >> 3) & 0x3F);
	// Update number of bits
	if((context->count[0] += ((UINT4)inputLen << 3)) < ((UINT4)inputLen << 3))
	{
		context->count[1]++;
	}
	context->count[1] += ((UINT4)inputLen >> 29);

	partLen = 64 - index;

	// Transform as many timse as possible.
	if(inputLen >= partLen)
	{
		memcpy(&context->buffer[index], (POINTER)input, partLen);
		Transform(context->state, context->buffer);

		for(i = partLen; i + 63 < inputLen; i += 64)
		{
			Transform(context->state, &input[i]);
		}

		index = 0;
	}
	else
	{
		i = 0;
	}

	// Buffer remaining input
	memcpy(&context->buffer[index], (POINTER)&input[i], inputLen-i);
	step++;
}

/* MD4 finalization. Ends an MD4 message-digest operation, writing
     the message digest and zeroizing the context.
 */
void MD4::Final(unsigned char digest[16], MD4_CTX *context)
{
	unsigned char bits[8];
	unsigned int index, padLen;

	// Save number of bits
	Encode(bits, context->count, 8);

	// Pad out to 56 mod 64.
	index = (unsigned int)((context->count[0] >> 3) & 0x3f);
	padLen = (index < 56) ? (56 - index) : (120 - index);
	Update(context, PADDING, padLen);

	// Append length (before padding)
	Update(context, bits, 8);
	// Store state in digest
	Encode(digest, context->state, 16);

	// Zeroize sensitive information.
	memset((unsigned char *)&context, 0, sizeof(context));
}

// MD4 basic transformation. Transforms state based on block.
void MD4::Transform(UINT4 state[4], unsigned char block[64])
{
	UINT4 a = state[0], b = state[1], c = state[2], d = state[3], x[16];

	Decode(x, block, 64);

	// Round 1
	a = FF(a, b, c, d, x[ 0], S11); /* 1 */
	d = FF(d, a, b, c, x[ 1], S12); /* 2 */
	c = FF(c, d, a, b, x[ 2], S13); /* 3 */
	b = FF(b, c, d, a, x[ 3], S14); /* 4 */
	a = FF(a, b, c, d, x[ 4], S11); /* 5 */
	d = FF(d, a, b, c, x[ 5], S12); /* 6 */
	c = FF(c, d, a, b, x[ 6], S13); /* 7 */
	b = FF(b, c, d, a, x[ 7], S14); /* 8 */
	a = FF(a, b, c, d, x[ 8], S11); /* 9 */
	d = FF(d, a, b, c, x[ 9], S12); /* 10 */
	c = FF(c, d, a, b, x[10], S13); /* 11 */
	b = FF(b, c, d, a, x[11], S14); /* 12 */
	a = FF(a, b, c, d, x[12], S11); /* 13 */
	d = FF(d, a, b, c, x[13], S12); /* 14 */
	c = FF(c, d, a, b, x[14], S13); /* 15 */
	b = FF(b, c, d, a, x[15], S14); /* 16 */

	// Round 2
	a = GG(a, b, c, d, x[ 0], S21); /* 17 */
	d = GG(d, a, b, c, x[ 4], S22); /* 18 */
	c = GG(c, d, a, b, x[ 8], S23); /* 19 */
	b = GG(b, c, d, a, x[12], S24); /* 20 */
	a = GG(a, b, c, d, x[ 1], S21); /* 21 */
	d = GG(d, a, b, c, x[ 5], S22); /* 22 */
	c = GG(c, d, a, b, x[ 9], S23); /* 23 */
	b = GG(b, c, d, a, x[13], S24); /* 24 */
	a = GG(a, b, c, d, x[ 2], S21); /* 25 */
	d = GG(d, a, b, c, x[ 6], S22); /* 26 */
	c = GG(c, d, a, b, x[10], S23); /* 27 */
	b = GG(b, c, d, a, x[14], S24); /* 28 */
	a = GG(a, b, c, d, x[ 3], S21); /* 29 */
	d = GG(d, a, b, c, x[ 7], S22); /* 30 */
	c = GG(c, d, a, b, x[11], S23); /* 31 */
	b = GG(b, c, d, a, x[15], S24); /* 32 */

	// Round 3
	a = HH(a, b, c, d, x[ 0], S31); /* 33 */
	d = HH(d, a, b, c, x[ 8], S32); /* 34 */
	c = HH(c, d, a, b, x[ 4], S33); /* 35 */
	b = HH(b, c, d, a, x[12], S34); /* 36 */
	a = HH(a, b, c, d, x[ 2], S31); /* 37 */
	d = HH(d, a, b, c, x[10], S32); /* 38 */
	c = HH(c, d, a, b, x[ 6], S33); /* 39 */
	b = HH(b, c, d, a, x[14], S34); /* 40 */
	a = HH(a, b, c, d, x[ 1], S31); /* 41 */
	d = HH(d, a, b, c, x[ 9], S32); /* 42 */
	c = HH(c, d, a, b, x[ 5], S33); /* 43 */
	b = HH(b, c, d, a, x[13], S34); /* 44 */
	a = HH(a, b, c, d, x[ 3], S31); /* 45 */
	d = HH(d, a, b, c, x[11], S32); /* 46 */
	c = HH(c, d, a, b, x[ 7], S33); /* 47 */
	b = HH(b, c, d, a, x[15], S34); /* 48 */

	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;

	// Zeroize sensitive information.
	memset((POINTER)x, 0, sizeof(x));
}

/* Encodes input (UINT4) into output (unsigned char). Assumes len is
     a multiple of 4.
 */
void MD4::Encode(unsigned char *output, UINT4 *input, unsigned int len)
{
	unsigned int i, j;

	for(i = 0, j = 0; j < len; i++, j += 4)
	{
		output[j] = (unsigned char)(input[i] & 0xff);
		output[j+1] = (unsigned char)((input[i] >> 8) & 0xff);
		output[j+2] = (unsigned char)((input[i] >> 16) & 0xff);
		output[j+3] = (unsigned char)((input[i] >> 24) & 0xff);
	}
}

/* Decodes input (unsigned char) into output (UINT4). Assumes len is
     a multiple of 4.
 */
void MD4::Decode(UINT4 *output, unsigned char *input, unsigned int len)
{
	unsigned int i, j;

	for(i = 0, j = 0; j < len; i++, j += 4)
	{
		output[i] = ((UINT4)input[j]) | (((UINT4)input[j+1]) << 8) | (((UINT4)input[j+2]) << 16) | (((UINT4)input[j+3]) << 24);
	}
}

// Note: Replace "for loop" with standard memcpy if possible.
void MD4::memcpy(POINTER output, POINTER input, unsigned int len)
{
	unsigned int i;

	for(i = 0; i < len; i++)
	{
		output[i] = input[i];
	}
}

// Note: Replace "for loop" with standard memset if possible.
void MD4::memset(POINTER output, int value, unsigned int len)
{
	unsigned int i;

	for(i = 0; i < len; i++)
	{
		((char *)output)[i] = (char)value;
	}
}

// Digests a string and prints the result.
void MD4::String(char *string)
{
	unsigned int len = strlen(string);

	Init(&context);
	Update(&context, (unsigned char *)string, len);
	Final(digest, &context);

	std::cout << "MD4 (\"" << string << "\") = " << HexDigest() << std::endl;
}

// Digests a file and prints the result.
void MD4::File(char *filename)
{
	FILE *file;
	int len;
	unsigned char buffer[1024]/*, digest[16]*/;

	if((file = fopen(filename, "rb")) == NULL)
	{
		std::cout << filename << " can't be opened" << std::endl;
	}

	else
	{
		Init(&context);
		while((len = fread(buffer, 1, 1024, file)))
		{
			Update(&context, buffer, len);
		}
		Final(digest, &context);

		fclose(file);

		std::cout << "MD4 (" << filename << ") = " << HexDigest() << std::endl;
	}
}

unsigned char * MD4::Digest()
{
	return digest;
}

// Initializes constants
MD4::MD4()
{
	S11 = 3;
	S12 = 7;
	S13 = 11;
	S14 = 19;
	S21 = 3;
	S22 = 5;
	S23 = 9;
	S24 = 13;
	S31 = 3;
	S32 = 9;
	S33 = 11;
	S34 = 15;
	PADDING[0] = 0x80;
	memset((POINTER)PADDING+1, 0, sizeof(PADDING)-1);
}

// Returns a hexadecimal message digest.
char * MD4::HexDigest()
{
	unsigned int i;
	std::stringstream ss;

	for(i = 0; i < 16; i++)
	{
		ss << std::setfill('0') << std::setw(2) << std::hex << (int)digest[i];
	}
	hexDigestCache = ss.str();
	return (char *)hexDigestCache.c_str();
}
