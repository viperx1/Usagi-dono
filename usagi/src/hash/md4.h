/* md4.h - header file for md4.cpp
 */

/* Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
   rights reserved.

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

class MD4
{
private:
	typedef unsigned char *POINTER;
	typedef unsigned short int UINT2;
	typedef unsigned long int UINT4;

//	MD4_CTX context;
//	unsigned char digest[16];

	/* Constants for MD4Transform routine.
	*/
	int S11, S12, S13, S14;
	int S21, S22, S23, S24;
	int S31, S32, S33, S34;

	void Transform(UINT4 [4], unsigned char [64]);
	void Encode(unsigned char *, UINT4 *, unsigned int);
	void Decode(UINT4 *, unsigned char *, unsigned int);

	void memset(POINTER, int, unsigned int);

	unsigned char PADDING[64];

	// F, G and H are basic MD4 functions.
	UINT4 F(UINT4, UINT4, UINT4);
	UINT4 G(UINT4, UINT4, UINT4);
	UINT4 H(UINT4, UINT4, UINT4);

	// ROTATE_LEFT rotates x left n bits.
	UINT4 ROTATE_LEFT(UINT4, int);

	// FF, GG and HH are transformations for rounds 1, 2 and 3
	// Rotation is separate from addition to prevent recomputation

	UINT4 FF(UINT4, UINT4, UINT4, UINT4, UINT4, int);
	UINT4 GG(UINT4, UINT4, UINT4, UINT4, UINT4, int);
	UINT4 HH(UINT4, UINT4, UINT4, UINT4, UINT4, int);

	int step;

protected:
	// MD4 context
	typedef struct {
		UINT4 state[4];                                   // state (ABCD)
		UINT4 count[2];        // number of bits, modulo 2^64 (lsb first)
		unsigned char buffer[64];                         // input buffer
	} MD4_CTX;
	MD4_CTX context; //make private ~_~
    void memcpy(POINTER, POINTER, unsigned int);
    unsigned char digest[16];

public:
	void String(char *);
	void File(char *);
	unsigned char * Digest();
	char * HexDigest();
	MD4();

	void Init(MD4_CTX *);
	void Update(MD4_CTX *, unsigned char *, unsigned int);
	void Final(unsigned char [16], MD4_CTX *);
};
