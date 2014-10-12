// Copyright (C) 2014, Panagiotis Christopoulos Charitos.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include "anki/resource/Image.h"
#include "anki/util/Exception.h"
#include "anki/util/Logger.h"
#include "anki/util/File.h"
#include "anki/util/Filesystem.h"
#include "anki/util/Assert.h"
#include "anki/util/Array.h"
#include "anki/misc/Xml.h"

namespace anki {

//==============================================================================
// TGA                                                                         =
//==============================================================================

//==============================================================================
static U8 tgaHeaderUncompressed[12] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static U8 tgaHeaderCompressed[12] = {0, 0, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0};

//==============================================================================
static ANKI_USE_RESULT Error loadUncompressedTga(
	File& fs, U32& width, U32& height, U32& bpp, ResourceDArray<U8>& data,
	ResourceAllocator<U8>& alloc)
{
	U8 header6[6];
	I bytesPerPxl;
	I imageSize;

	// read the info from header
	Error err = fs.read((char*)&header6[0], sizeof(header6));
	if(err)
	{
		goto cleanup;
	}

	width  = header6[1] * 256 + header6[0];
	height = header6[3] * 256 + header6[2];
	bpp = header6[4];

	if((width <= 0) || (height <= 0) || ((bpp != 24) && (bpp != 32)))
	{
		ANKI_LOGE("Invalid image information");
		err = ErrorCode::USER_DATA;
		goto cleanup;
	}

	// read the data
	bytesPerPxl = (bpp / 8);
	imageSize = bytesPerPxl * width * height;
	err = data.create(alloc, imageSize);
	if(err)
	{
		goto cleanup;
	}

	err = fs.read(reinterpret_cast<char*>(&data[0]), imageSize);
	if(err)
	{
		goto cleanup;
	}

	// swap red with blue
	for(I i = 0; i < imageSize; i += bytesPerPxl)
	{
		U32 temp = data[i];
		data[i] = data[i + 2];
		data[i + 2] = temp;
	}

cleanup:
	if(err)
	{
		data.destroy(alloc);
	}

	return err;
}

//==============================================================================
static ANKI_USE_RESULT Error loadCompressedTga(
	File& fs, U32& width, U32& height, U32& bpp, ResourceDArray<U8>& data,
	ResourceAllocator<U8>& alloc)
{
	U8 header6[6];
	
	I bytesPerPxl;
	I imageSize;

	U32 pixelcount;
	U32 currentpixel;
	U32 currentbyte;
	U8 colorbuffer[4];

	Error err = fs.read(reinterpret_cast<char*>(&header6[0]), sizeof(header6));
	if(err)
	{
		goto cleanup;
	}

	width  = header6[1] * 256 + header6[0];
	height = header6[3] * 256 + header6[2];
	bpp	= header6[4];

	if((width <= 0) || (height <= 0) || ((bpp != 24) && (bpp != 32)))
	{
		ANKI_LOGE("Invalid texture information");
		err = ErrorCode::USER_DATA;
		goto cleanup;
	}

	bytesPerPxl = (bpp / 8);
	imageSize = bytesPerPxl * width * height;
	err = data.create(alloc, imageSize);
	if(err)
	{
		goto cleanup;
	}

	pixelcount = height * width;
	currentpixel = 0;
	currentbyte = 0;

	do
	{
		U8 chunkheader = 0;

		err = fs.read((char*)&chunkheader, sizeof(U8));
		if(err)
		{
			goto cleanup;
		}

		if(chunkheader < 128)
		{
			chunkheader++;
			for(int counter = 0; counter < chunkheader; counter++)
			{
				err = fs.read((char*)&colorbuffer[0], bytesPerPxl);
				if(err)
				{
					goto cleanup;
				}

				data[currentbyte] = colorbuffer[2];
				data[currentbyte + 1] = colorbuffer[1];
				data[currentbyte + 2] = colorbuffer[0];

				if(bytesPerPxl == 4)
				{
					data[currentbyte + 3] = colorbuffer[3];
				}

				currentbyte += bytesPerPxl;
				currentpixel++;

				if(currentpixel > pixelcount)
				{
					ANKI_LOGE("Too many pixels read");
					err = ErrorCode::USER_DATA;
					goto cleanup;
				}
			}
		}
		else
		{
			chunkheader -= 127;
			err = fs.read((char*)&colorbuffer[0], bytesPerPxl);
			if(err)
			{
				goto cleanup;
			}

			for(int counter = 0; counter < chunkheader; counter++)
			{
				data[currentbyte] = colorbuffer[2];
				data[currentbyte + 1] = colorbuffer[1];
				data[currentbyte + 2] = colorbuffer[0];

				if(bytesPerPxl == 4)
				{
					data[currentbyte + 3] = colorbuffer[3];
				}

				currentbyte += bytesPerPxl;
				currentpixel++;

				if(currentpixel > pixelcount)
				{
					ANKI_LOGE("Too many pixels read");
					err = ErrorCode::USER_DATA;
					goto cleanup;
				}
			}
		}
	} while(currentpixel < pixelcount);

cleanup:
	if(err)
	{
		data.destroy(alloc);
	}

	return err;
}

//==============================================================================
static ANKI_USE_RESULT Error loadTga(const CString& filename, 
	U32& width, U32& height, U32& bpp, ResourceDArray<U8>& data,
	ResourceAllocator<U8>& alloc)
{
	File fs;
	char myTgaHeader[12];

	Error err = fs.open(filename, 
		File::OpenFlag::READ | File::OpenFlag::BINARY);

	if(!err)
	{
		err = fs.read(&myTgaHeader[0], sizeof(myTgaHeader));
	}

	if(!err)
	{
		if(std::memcmp(
			tgaHeaderUncompressed, &myTgaHeader[0], sizeof(myTgaHeader)) == 0)
		{
			err = loadUncompressedTga(fs, width, height, bpp, data, alloc);
		}
		else if(std::memcmp(tgaHeaderCompressed, &myTgaHeader[0],
			sizeof(myTgaHeader)) == 0)
		{
			err = loadCompressedTga(fs, width, height, bpp, data, alloc);
		}
		else
		{
			ANKI_LOGE("Invalid image header");
			err = ErrorCode::USER_DATA;
		}
	}

	if(!err && bpp != 32 && bpp != 24)
	{
		ANKI_LOGE("Invalid bpp");
		err = ErrorCode::USER_DATA;
	}

	return err;
}

//==============================================================================
// ANKI                                                                        =
//==============================================================================

//==============================================================================
class AnkiTextureHeader
{
public:
	Array<U8, 8> m_magic;
	U32 m_width;
	U32 m_height;
	U32 m_depth;
	Image::TextureType m_type;
	Image::ColorFormat m_colorFormat;
	Image::DataCompression m_compressionFormats;
	U32 m_normal;
	U32 m_mipLevels;
	U8 m_padding[88];
};

static_assert(sizeof(AnkiTextureHeader) == 128, 
	"Check sizeof AnkiTextureHeader");

//==============================================================================
/// Get the size in bytes of a single surface
static PtrSize calcSurfaceSize(const U width, const U height, 
	const Image::DataCompression comp, const Image::ColorFormat cf)
{
	PtrSize out = 0;

	ANKI_ASSERT(width >= 4 || height >= 4);

	switch(comp)
	{
	case Image::DataCompression::RAW:
		out = width * height * ((cf == Image::ColorFormat::RGB8) ? 3 : 4);
		break;
	case Image::DataCompression::S3TC:
		out = (width / 4) * (height / 4) 
			* ((cf == Image::ColorFormat::RGB8) ? 8 : 16); // This is the 
		                                                   // block size
		break;
	case Image::DataCompression::ETC:
		out = (width / 4) * (height / 4) * 8;
		break;
	default:
		ANKI_ASSERT(0);
	}

	ANKI_ASSERT(out > 0);

	return out;
}

//==============================================================================
/// Calculate the size of a compressed or uncomressed color data
static PtrSize calcSizeOfSegment(const AnkiTextureHeader& header, 
	Image::DataCompression comp)
{
	PtrSize out = 0;
	U width = header.m_width;
	U height = header.m_height;
	U mips = header.m_mipLevels;
	U layers = 0;

	switch(header.m_type)
	{
	case Image::TextureType::_2D:
		layers = 1;
		break;
	case Image::TextureType::CUBE:
		layers = 6;
		break;
	case Image::TextureType::_2D_ARRAY:
	case Image::TextureType::_3D:
		layers = header.m_depth;
		break;
	default:
		ANKI_ASSERT(0);
		break;
	}

	while(mips-- != 0)
	{
		U l = layers;

		while(l-- != 0)
		{
			out += calcSurfaceSize(width, height, comp, header.m_colorFormat);
		}

		width /= 2;
		height /= 2;
	}

	return out;
}

//==============================================================================
static ANKI_USE_RESULT Error loadAnkiTexture(
	const CString& filename, 
	U32 maxTextureSize,
	Image::DataCompression& preferredCompression,
	ResourceDArray<Image::Surface>& surfaces,
	ResourceAllocator<U8>& alloc,
	U8& depth, 
	U8& mipLevels, 
	Image::TextureType& textureType,
	Image::ColorFormat& colorFormat)
{
	Error err = ErrorCode::NONE;
	File file;

	U mipWidth;
	U mipHeight;

	U size;
	U maxsize;
	U tmpMipLevels;

	err = file.open(filename, 
		File::OpenFlag::READ | File::OpenFlag::BINARY 
		| File::OpenFlag::LITTLE_ENDIAN);
	if(err)
	{
		goto cleanup;
	}

	//
	// Read and check the header
	//
	AnkiTextureHeader header;
	err = file.read(&header, sizeof(AnkiTextureHeader));
	if(err)
	{
		goto cleanup;
	}

	if(std::memcmp(&header.m_magic[0], "ANKITEX1", 8) != 0)
	{
		ANKI_LOGE("Wrong magic word");
		err = ErrorCode::USER_DATA;
		goto cleanup;
	}

	if(header.m_width == 0 
		|| !isPowerOfTwo(header.m_width) 
		|| header.m_width > 4096
		|| header.m_height == 0 
		|| !isPowerOfTwo(header.m_height) 
		|| header.m_height > 4096)
	{
		ANKI_LOGE("Incorrect width/height value");
		err = ErrorCode::USER_DATA;
		goto cleanup;
	}

	if(header.m_depth < 1 || header.m_depth > 16)
	{
		ANKI_LOGE("Zero or too big depth");
		err = ErrorCode::USER_DATA;
		goto cleanup;
	}

	if(header.m_type < Image::TextureType::_2D 
		|| header.m_type > Image::TextureType::_2D_ARRAY)
	{
		ANKI_LOGE("Incorrect header: texture type");
		err = ErrorCode::USER_DATA;
		goto cleanup;
	}

	if(header.m_colorFormat < Image::ColorFormat::RGB8 
		|| header.m_colorFormat > Image::ColorFormat::RGBA8)
	{
		ANKI_LOGE("Incorrect header: color format");
		err = ErrorCode::USER_DATA;
		goto cleanup;
	}

	if((header.m_compressionFormats & preferredCompression) 
		== Image::DataCompression::NONE)
	{
		ANKI_LOGE("File does not contain the wanted compression");
		err = ErrorCode::USER_DATA;
		goto cleanup;
	}

	if(header.m_normal != 0 && header.m_normal != 1)
	{
		ANKI_LOGE("Incorrect header: normal");
		err = ErrorCode::USER_DATA;
		goto cleanup;
	}

	// Check mip levels
	size = std::min(header.m_width, header.m_height);
	maxsize = std::max(header.m_width, header.m_height);
	mipLevels = 0;
	tmpMipLevels = 0;
	while(size >= 4) // The minimum size is 4x4
	{
		++tmpMipLevels;

		if(maxsize <= maxTextureSize)
		{
			++mipLevels;
		}

		size /= 2;
		maxsize /= 2;
	}

	if(tmpMipLevels != header.m_mipLevels)
	{
		ANKI_LOGE("Incorrect number of mip levels");
		err = ErrorCode::USER_DATA;
		goto cleanup;
	}

	colorFormat = header.m_colorFormat;

	switch(header.m_type)
	{
		case Image::TextureType::_2D:
			depth = 1;
			break;
		case Image::TextureType::CUBE:
			depth = 6;
			break;
		case Image::TextureType::_3D:
		case Image::TextureType::_2D_ARRAY:
			depth = header.m_depth;
			break;
		default:
			ANKI_ASSERT(0);
	}

	textureType = header.m_type;

	//
	// Move file pointer
	//

	if(preferredCompression == Image::DataCompression::RAW)
	{
		// Do nothing
	}
	else if(preferredCompression == Image::DataCompression::S3TC)
	{
		if((header.m_compressionFormats & Image::DataCompression::RAW)
			!= Image::DataCompression::NONE)
		{
			// If raw compression is present then skip it
			err = file.seek(
				calcSizeOfSegment(header, Image::DataCompression::RAW), 
				File::SeekOrigin::CURRENT);
		}
	}
	else if(preferredCompression == Image::DataCompression::ETC)
	{
		if((header.m_compressionFormats & Image::DataCompression::RAW)
			!= Image::DataCompression::NONE)
		{
			// If raw compression is present then skip it
			err = file.seek(
				calcSizeOfSegment(header, Image::DataCompression::RAW), 
				File::SeekOrigin::CURRENT);
		}

		if((header.m_compressionFormats & Image::DataCompression::S3TC)
			!= Image::DataCompression::NONE)
		{
			// If s3tc compression is present then skip it
			err = file.seek(
				calcSizeOfSegment(header, Image::DataCompression::S3TC), 
				File::SeekOrigin::CURRENT);
		}
	}

	if(err)
	{
		goto cleanup;
	}

	//
	// It's time to read
	//

	// Allocate the surfaces 
	err = surfaces.create(alloc, mipLevels * depth);
	if(err)
	{
		goto cleanup;
	}

	// Read all surfaces
	mipWidth = header.m_width;
	mipHeight = header.m_height;
	for(U mip = 0; mip < header.m_mipLevels && !err; mip++)
	{
		for(U d = 0; d < depth && !err; d++)
		{
			U dataSize = calcSurfaceSize(
				mipWidth, mipHeight, preferredCompression, 
				header.m_colorFormat);

			// Check if this mipmap can be skipped because of size
			if(std::max(mipWidth, mipHeight) <= maxTextureSize)
			{
				U index = (mip - tmpMipLevels + mipLevels) * depth + d;
				ANKI_ASSERT(index < surfaces.getSize());
				Image::Surface& surf = surfaces[index];
				surf.m_width = mipWidth;
				surf.m_height = mipHeight;

				err = surf.m_data.create(alloc, dataSize);

				if(!err)
				{
					err = file.read(&surf.m_data[0], dataSize);
				}
			}
			else
			{
				err = file.seek(dataSize, File::SeekOrigin::CURRENT);
			}
		}

		mipWidth /= 2;
		mipHeight /= 2;
	}

cleanup:
	// Cleanup
	if(err)
	{
		for(Image::Surface& surf : surfaces)
		{
			surf.m_data.destroy(alloc);
		}

		surfaces.destroy(alloc);
	}

	return err;
}

//==============================================================================
// Image                                                                       =
//==============================================================================

//==============================================================================
Error Image::load(const CString& filename, U32 maxTextureSize)
{
	Error err = ErrorCode::NONE;

	// get the extension
	String ext = getFileExtension(filename, m_alloc);
	
	if(ext.isEmpty())
	{
		ANKI_LOGE("Failed to get filename extension");
		return ErrorCode::USER_DATA;
	}

	// load from this extension
	U32 bpp;
	m_textureType = TextureType::_2D;
	m_compression = DataCompression::RAW;

	if(ext == "tga")
	{
		err = m_surfaces.create(m_alloc, 1);

		if(!err)
		{
			m_mipLevels = 1;
			m_depth = 1;
			err = loadTga(filename, m_surfaces[0].m_width, 
				m_surfaces[0].m_height, bpp, m_surfaces[0].m_data, m_alloc);
		}

		if(!err)
		{
			if(bpp == 32)
			{
				m_colorFormat = ColorFormat::RGBA8;
			}
			else if(bpp == 24)
			{
				m_colorFormat = ColorFormat::RGB8;
			}
			else
			{
				ANKI_ASSERT(0);
			}
		}
	}
	else if(ext == "ankitex")
	{
#if 0
		compression = Image::DataCompression::RAW;
#elif ANKI_GL == ANKI_GL_DESKTOP
		m_compression = Image::DataCompression::S3TC;
#else
		m_compression = Image::DataCompression::ETC;
#endif

		err = loadAnkiTexture(filename, maxTextureSize, 
			m_compression, m_surfaces, m_alloc, m_depth, 
			m_mipLevels, m_textureType, m_colorFormat);

	}
	else
	{
		ANKI_LOGE("Unsupported extension");
		err = ErrorCode::USER_DATA;
	}

	return err;
}

//==============================================================================
const Image::Surface& Image::getSurface(U mipLevel, U layer) const
{
	ANKI_ASSERT(mipLevel < m_mipLevels);

	U layers = 0;

	switch(m_textureType)
	{
	case TextureType::_2D:
		layers = 1;
		break;
	case TextureType::CUBE:
		layers = 6;
		break;
	case TextureType::_3D:
	case TextureType::_2D_ARRAY:
		layers = m_depth;
		break;
	default:
		ANKI_ASSERT(0);
	}

	// [mip][depthFace]
	U index = mipLevel * layers + layer;
	
	ANKI_ASSERT(index < m_surfaces.getSize());

	return m_surfaces[index];
}

} // end namespace anki
