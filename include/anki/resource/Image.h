#ifndef ANKI_RESOURCE_IMAGE_H
#define ANKI_RESOURCE_IMAGE_H

#include "anki/util/Functions.h"
#include <vector>
#include <iosfwd>
#include <cstdint>

namespace anki {

/// Image class.
/// Used in Texture::load. Supported types: TGA and PNG
class Image
{
public:
	/// The acceptable color types of AnKi
	enum ColorType
	{
		CT_R, ///< Red only
		CT_RGB, ///< RGB
		CT_RGBA ///< RGB plus alpha
	};

	/// The data compression
	enum DataCompression
	{
		DC_NONE,
		DC_DXT1,
		DC_DXT3,
		DC_DXT5
	};

	/// Do nothing
	Image()
	{}

	/// Load an image
	/// @param[in] filename The image file to load
	/// @exception Exception
	Image(const char* filename)
	{
		load(filename);
	}

	/// Do nothing
	~Image()
	{}

	/// @name Accessors
	/// @{
	uint32_t getWidth() const
	{
		return width;
	}

	uint32_t getHeight() const
	{
		return height;
	}

	ColorType getColorType() const
	{
		return type;
	}

	const uint8_t* getData() const
	{
		return &data[0];
	}

	/// Get image size in bytes
	size_t getDataSize() const
	{
		return getVectorSizeInBytes(data);
	}

	DataCompression getDataCompression() const
	{
		return dataCompression;
	}
	/// @}

	/// Load an image file
	/// @param[in] filename The file to load
	/// @exception Exception
	void load(const char* filename);

private:
	uint32_t width = 0; ///< Image width
	uint32_t height = 0; ///< Image height
	ColorType type; ///< Image color type
	std::vector<uint8_t> data; ///< Image data
	DataCompression dataCompression;

	/// @name TGA headers
	/// @{
	static uint8_t tgaHeaderUncompressed[12];
	static uint8_t tgaHeaderCompressed[12];
	/// @}

	/// Load a TGA
	/// @param[in] filename The file to load
	/// @exception Exception
	void loadTga(const char* filename);

	/// Used by loadTga
	/// @param[in] fs The input
	/// @param[out] bpp Bits per pixel
	/// @exception Exception
	void loadUncompressedTga(std::fstream& fs, uint32_t& bpp);

	/// Used by loadTga
	/// @param[in] fs The input
	/// @param[out] bpp Bits per pixel
	/// @exception Exception
	void loadCompressedTga(std::fstream& fs, uint32_t& bpp);

	/// Load PNG. Dont throw exception because libpng is in C
	/// @param[in] filename The file to load
	/// @param[out] err The error message
	/// @return true on success
	bool loadPng(const char* filename, std::string& err) throw();

	/// Load a DDS file
	/// @param[in] filename The file to load
	void loadDds(const char* filename);
};

} // end namespace

#endif