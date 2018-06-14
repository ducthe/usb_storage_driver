
#ifndef __COMMON_H__
#define __COMMON_H__

/* Inline Functions: */
/** Function to reverse the individual bits in a byte - i.e. bit 7 is moved to bit 0, bit 6 to bit 1,
 *  etc.
 *
 *  \ingroup Group_BitManip
 *
 *  \param[in] Byte   Byte of data whose bits are to be reversed
 */

#define VENDOR_ID   0x090c
#define PRODUCT_ID  0x1000
#define MIN(a,b) ((a) <= (b) ? (a) : (b))
#define BULK_EP_OUT 0x02 // endpoint out address 
#define BULK_EP_IN  0x81 //endpoint in address 
#define MAX_PKT_SIZE 512
#define BULK_ONLY_DEFAULT_LUN_NUMBER            0

#define DEFAULT_TEST_BLOCK  1024


static inline uint8_t BitReverse(uint8_t Byte);
static inline uint8_t BitReverse(uint8_t Byte)
{
	Byte = (((Byte & 0xF0) >> 4) | ((Byte & 0x0F) << 4));
	Byte = (((Byte & 0xCC) >> 2) | ((Byte & 0x33) << 2));
	Byte = (((Byte & 0xAA) >> 1) | ((Byte & 0x55) << 1));

	return Byte;
}

/** Function to reverse the byte ordering of the individual bytes in a 16 bit number.
 *
 *  \ingroup Group_BitManip
 *
 *  \param[in] Word   Word of data whose bytes are to be swapped
 */
static inline uint16_t SwapEndian_16(uint16_t Word) ;
static inline uint16_t SwapEndian_16(uint16_t Word)
{
	return ((Word >> 8) | (Word << 8));				
}

/** Function to reverse the byte ordering of the individual bytes in a 32 bit number.
 *
 *  \ingroup Group_BitManip
 *
 *  \param[in] DWord   Double word of data whose bytes are to be swapped
 */
static inline uint32_t SwapEndian_32(uint32_t DWord) ;
static inline uint32_t SwapEndian_32(uint32_t DWord)
{
	return (((DWord & 0xFF000000) >> 24) |
	        ((DWord & 0x00FF0000) >> 8)  |
			((DWord & 0x0000FF00) << 8)  |
			((DWord & 0x000000FF) << 24));
}

/** Function to reverse the byte ordering of the individual bytes in a n byte number.
 *
 *  \ingroup Group_BitManip
 *
 *  \param[in,out] Data  Pointer to a number containing an even number of bytes to be reversed
 *  \param[in] Bytes  Length of the data in bytes
 */
static inline void SwapEndian_n(uint8_t* Data, uint8_t Bytes);
static inline void SwapEndian_n(uint8_t* Data, uint8_t Bytes)
{
	uint8_t Temp;
	
	while (Bytes)
	{
		Temp = *Data;
		*Data = *(Data + Bytes - 1);
		*(Data + Bytes) = Temp;

		Data++;
		Bytes -= 2;
	}
}

#endif