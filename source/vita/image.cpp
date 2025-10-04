#include "image.hpp"
#include "os.hpp"
#include <algorithm>
#include <vector>
#define STBI_NO_GIF
#define STB_IMAGE_IMPLEMENTATION
#include "unzip.hpp"
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#include "stb_image.h"

std::unordered_map<std::string, VitaImage> imageData;
static std::vector<std::string> toDelete;

Image::Image(std::string filePath) {
    if (!loadImageFromFile(filePath, false)) return;

    // Find the matching VitaImage in the vector
    std::string filename = filePath.substr(filePath.find_last_of('/') + 1);
    std::string path2 = filename.substr(0, filename.find_last_of('.'));
    for (const auto &imgPair : imageData) {
		auto img = imgPair.second;
        if (imgPair.first == path2) {
            imageId = path2;
            width = img.width;
            height = img.height;
            scale = 1.0;
            rotation = 0.0;
            opacity = 1.0;
			img.imageUsageCount++;
            return;
        }
    }
}

Image::~Image() {
    if (imageData.find(imageId) != imageData.end()) {
		imageData[imageId].imageUsageCount--;
        if (imageData[imageId].imageUsageCount <= 0)
            freeImage(imageId);
    }
}

void Image::render(double xPos, double yPos, bool centered) {
    if (imageData.find(imageId) != imageData.end()) {
		imageData[imageId].render(xPos, yPos, scale, opacity, rotation, centered);
	}
}

/**
 * Turns a single image from an unzipped Scratch project into RGBA data
 */
bool Image::loadImageFromFile(std::string filePath, bool fromScratchProject) {
    std::string filename = filePath.substr(filePath.find_last_of('/') + 1);
    std::string path2 = filename.substr(0, filename.find_last_of('.'));

	auto it = std::find_if(imageData.begin(), imageData.end(), [&](const std::pair<std::string, VitaImage> &p) {
		return p.first == path2;
	});
    if (it != imageData.end()) return true;
	
	std::string finalPath;

	finalPath = OS::getRomFSLocation();
	if (fromScratchProject)
		finalPath = finalPath + "project/";
	
	finalPath = finalPath + filePath;
	if (Unzip::UnpackedInSD) finalPath = Unzip::filePath + filePath;
	
    FILE *file = fopen(finalPath.c_str(), "rb");
    if (!file) {
        Log::logWarning("Image file not found: " + finalPath);
        return false;
    }

    int width, height;
	
	std::string ext;
	if (filePath.size() >= 4) ext = filePath.substr(filePath.size() - 4);
	else ext = filePath;

    bool isSVG = filePath.size() >= 4 && (ext == ".svg" || ext == ".SVG");

    if (isSVG) {
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);

        char *svg_data = (char *)malloc(file_size);
        if (!svg_data) {
            Log::logWarning("Failed to allocate memory for SVG file: " + finalPath);
            fclose(file);
            return false;
        }

        size_t read_size = fread(svg_data, 1, file_size, file);
        fclose(file);

        if (read_size != (size_t)file_size) {
            Log::logWarning("Failed to read SVG file completely: " + finalPath);
            free(svg_data);
            return false;
        }
		
		// TODO: see image.h
		return false;
    } else {
		vita2d_texture* tex = nullptr;
		if (filePath.size() >= 4) {
			if (ext == ".jpg" || ext == ".JPG" || ext == "jpeg" || ext == "JPEG") {
				tex = vita2d_load_JPEG_file(finalPath.c_str());
			} else if (ext == ".png" || ext == ".PNG") {
				tex = vita2d_load_PNG_file(finalPath.c_str());
			} else if (ext == ".bmp" || ext == ".BMP") {
				tex = vita2d_load_BMP_file(finalPath.c_str());
			}
			if (tex) goto skipSTBI;
		}
		{ // This coding is cursed, and I need for this stuff to fall
		  // out of scope immediately before hitting the skip goto
			int channels;
			unsigned char *rgba_data = stbi_load_from_file(file, &width, &height, &channels, 4);
			if (!rgba_data) {
				Log::logWarning("Failed to decode image: " + finalPath);
				return false;
			}
			// Now we do the funny
			tex = vita2d_create_empty_texture_rendertarget(width, height, SCE_GXM_TEXTURE_FORMAT_A8R8G8B8);
			vita2d_start_drawing_advanced(tex, 0);
			unsigned int channelMask;
			switch (channels) {
				case 1:
					channelMask = 0x000000ff;
					break;
				case 2:
					channelMask = 0x0000ffff;
					break;
				case 3:
					channelMask = 0x00ffffff;
					break;
				case 4:
					channelMask = 0xffffffff;
					break;
			}
			/**
			 * Explanation of what I'm doing here:
			 * Above this I set up a mask for the channels
			 * before writing them to the vita2d texture.
			 * Here, I'm using pointer manipulation to make
			 * vita2d interpret the RGBA value as an integer.
			 * Thanks to the Vita being little-endian, the values
			 * stored in memory in the order R, G, B, A will
			 * be interpreted in the order ABGR, which is exactly
			 * what the `vita2d_draw_pixel` function expects.
			 * This is done for every pixel in the image.
			 * What this does is effectively copy the image to VRAM,
			 * so now the memory doesn't need to be hogging main
			 * system RAM.
			 * Thankfully I don't need to worry about accidentally
			 * reading memory I don't have access to; I only need to
			 * read 4 bytes, and the 4 bytes both before and behind
			 * `rgba_data` are taken up by `channels` and
			 * `channelMask`, meaning those might get stepped on.
			 */
			for (int x = 0; x < width; x++)
				for (int y = 0; y < height; y++)
					vita2d_draw_pixel(x, y, channelMask &
									  *(unsigned int*)(rgba_data + (x + y * width + 1) *
													  channels - sizeof(unsigned int)));
			vita2d_end_drawing();
		}
		skipSTBI:
		fclose(file);
		BitmapImage image(tex);
		imageData[path2] = image;
    }
    return true;
}

/**
 * Loads a single image from a Scratch sb3 zip file by filename.
 * @param zip Pointer to the zip archive
 * @param costumeId The filename of the image to load (e.g., "sprite1.png")
 */
void Image::loadImageFromSB3(mz_zip_archive *zip, const std::string &costumeId) {
	// TODO: loadImageFromSB3
	/*
    std::string imageId = costumeId.substr(0, costumeId.find_last_of('.'));

    // Check if image already exists
    auto it = std::find_if(imageRGBAS.begin(), imageRGBAS.end(), [&](const imageRGBA &img) {
        return img.name == imageId;
    });
    if (it != imageRGBAS.end()) return;

    // Log::log("Loading single image: " + costumeId);

    // Find the file in the zip
    int file_index = mz_zip_reader_locate_file(zip, costumeId.c_str(), nullptr, 0);
    if (file_index < 0) {
        Log::logWarning("Image file not found in zip: " + costumeId);
        return;
    }

    // Get file stats
    mz_zip_archive_file_stat file_stat;
    if (!mz_zip_reader_file_stat(zip, file_index, &file_stat)) {
        Log::logWarning("Failed to get file stats for: " + costumeId);
        return;
    }

    // Check if file is bitmap or SVG
    bool isBitmap = costumeId.size() > 4 && ([](std::string ext) {
                        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                        return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" ||
                               ext == ".bmp" || ext == ".psd" || ext == ".gif" || ext == ".hdr" ||
                               ext == ".pic" || ext == ".ppm" || ext == ".pgm";
                    }(costumeId.substr(costumeId.find_last_of('.'))));
    bool isSVG = costumeId.size() >= 4 &&
                 (costumeId.substr(costumeId.size() - 4) == ".svg" ||
                  costumeId.substr(costumeId.size() - 4) == ".SVG");

    if (!isBitmap && !isSVG) {
        Log::logWarning("File is not a supported image format: " + costumeId);
        return;
    }

    // Extract file data
    size_t file_size;
    void *file_data = mz_zip_reader_extract_to_heap(zip, file_index, &file_size, 0);
    if (!file_data) {
        Log::logWarning("Failed to extract: " + costumeId);
        return;
    }

    int width, height;
    unsigned char *rgba_data = nullptr;

    imageRGBA newRGBA;

    if (isSVG) {
        newRGBA.isSVG = true;
        rgba_data = SVGToRGBA(file_data, file_size, width, height);
        if (!rgba_data) {
            Log::logWarning("Failed to decode SVG: " + costumeId);
            mz_free(file_data);
            Image::cleanupImages();
            return;
        }
    } else {
        // Handle bitmap files (PNG, JPG)
        int channels;
        rgba_data = stbi_load_from_memory(
            (unsigned char *)file_data, file_size,
            &width, &height, &channels, 4);

        if (!rgba_data) {
            Log::logWarning("Failed to decode image: " + costumeId);
            mz_free(file_data);
            Image::cleanupImages();
            return;
        }
    }

    // Set up the image data structure
    newRGBA.name = imageId;
    newRGBA.fullName = costumeId;
    newRGBA.width = width;
    newRGBA.height = height;
    newRGBA.textureWidth = clamp(next_pow2(newRGBA.width), 64, 1024);
    newRGBA.textureHeight = clamp(next_pow2(newRGBA.height), 64, 1024);
    newRGBA.textureMemSize = newRGBA.textureWidth * newRGBA.textureHeight * 4;
    newRGBA.data = rgba_data;

    // Track memory usage
    size_t imageSize = width * height * 4;
    MemoryTracker::allocate(imageSize);

    // Log::log("Successfully loaded image!");
    imageRGBAS.push_back(newRGBA);

    // Clean up
    mz_free(file_data);
	 */
}

/**
 * Frees a `VitaImage` from memory using `costumeId` string to find it.
 */
void Image::freeImage(const std::string &costumeId) {
	// TODO: freeImage
	/*
    auto it = imageC2Ds.find(costumeId);
    if (it != imageC2Ds.end()) {
        // Log::log("freed image!");

        if (it->second.sheet) {
            if (it->second.image.tex) {
                size_t textureSize = it->second.image.subtex->width * it->second.image.subtex->height * 4;
                MemoryTracker::deallocateVRAM(textureSize);
            }

            C2D_SpriteSheetFree(it->second.sheet);

            // Log::log("Freed sprite sheet for: " + costumeId);
            goto afterFreeing;
        }

        if (it->second.image.tex) {
            C3D_TexDelete(it->second.image.tex);
            delete it->second.image.tex;
            it->second.image.tex = nullptr;
        }
        if (it->second.image.subtex) {
            delete it->second.image.subtex;
        }

    afterFreeing:

        imageC2Ds.erase(it);
    } else {
        Log::logWarning("cant find image to free: " + costumeId);
    }
    freeRGBA(costumeId);
	 */
}

void Image::cleanupImages() {
	// TODO: cleanupImages
	/*
    std::vector<std::string> keysToDelete;
    keysToDelete.reserve(imageC2Ds.size());

    for (const auto &[id, data] : imageC2Ds) {
        keysToDelete.push_back(id);
    }

    for (const std::string &id : keysToDelete) {
        freeImage(id);
    }

    // Clear maps & queues to prevent dangling references
    imageC2Ds.clear();
    imageLoadQueue.clear();
    toDelete.clear();

    // Log::log("Image cleanup completed.");
	 */
}

/**
 * Queues a `VitaImage` to be freed using `costumeId` to find it.
 * The image will be freed once `FlushImages()` is called.
 */
void Image::queueFreeImage(const std::string &costumeId) {
    toDelete.push_back(costumeId);
}

/**
 * Checks every `VitaImage` in memory to see if they can be freed.
 * A `VitaImage` will get freed if there's too many images in memory.
 */
void Image::FlushImages() {
    std::vector<std::string> keysToDelete;

    for (const std::string &id : keysToDelete) {
        Image::freeImage(id);
    }
}

void VitaImage::render (double xPos, double yPos,
						double scale, double opacity,
						double rotation, bool centered,
						int scaleX) { } // Stub

BitmapImage::BitmapImage(vita2d_texture* tex) {
	this->tex = tex;
	width = vita2d_texture_get_width(tex);
	height = vita2d_texture_get_height(tex);
}

BitmapImage::~BitmapImage() {
	vita2d_free_texture(tex);
}

void BitmapImage::render(double xPos, double yPos,
						 double scale, double opacity,
						 double rotation, bool centered,
						 int scaleX) {
	unsigned int tint = 0xffffffff;
	tint *= opacity;
	tint |= 0x00ffffff;
	float center = centered ? 0.5f : 0;
	vita2d_draw_texture_tint_scale_rotate_hotspot(tex, xPos, yPos, scale * scaleX, scale, rotation, center, center, tint);
}
