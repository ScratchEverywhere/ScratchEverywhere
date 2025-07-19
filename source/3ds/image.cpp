#include "image.hpp"
#include <algorithm>
#include <vector>
#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "../scratch/unzip.hpp"

using u32 = uint32_t;
using u8 = uint8_t;

std::unordered_map<std::string, ImageData> imageC2Ds;
std::vector<Image::ImageRGBA> Image::imageRGBAS;
static std::vector<std::string> toDelete;

struct MemoryStats{
  size_t totalRamUsage = 0;
  size_t totalVRamUsage = 0;
  size_t imageCount = 0;
  size_t c2dImageCount = 0;
};

static MemoryStats memStats;


const u32 next_pow2(u32 n) {
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++;
    return n;
  }
  const u32 clamp(u32 n, u32 lower, u32 upper) {
    if (n < lower)
      return lower;
    if (n > upper)
      return upper;
    return n;
  }
  const u32 rgba_to_abgr(u32 px) {
    u8 r = (px & 0xff000000) >> 24;
    u8 g = (px & 0x00ff0000) >> 16;
    u8 b = (px & 0x0000ff00) >> 8;
    u8 a = px & 0x000000ff;
    return (a << 24) | (b << 16) | (g << 8) | r;
  }

void Image::loadImages(mz_zip_archive*zip){
// Loop through all files in the ZIP
std::cout << "Loading images..." << std::endl;

int file_count = (int)mz_zip_reader_get_num_files(zip);

for (int i = 0; i < file_count; i++) {
    mz_zip_archive_file_stat file_stat;
    if (!mz_zip_reader_file_stat(zip, i, &file_stat)) continue;

    std::string zipFileName = file_stat.m_filename;

    // Check if file is a PNG or JPG
    if (zipFileName.size() >= 4 && 
        (zipFileName.substr(zipFileName.size() - 4) == ".png" || zipFileName.substr(zipFileName.size() - 4) == ".PNG"\
        || zipFileName.substr(zipFileName.size() - 4) == ".jpg" || zipFileName.substr(zipFileName.size() - 4) == ".JPG")) {


        size_t png_size;
        void* png_data = mz_zip_reader_extract_to_heap(zip, i, &png_size, 0);
        if (!png_data) {
            printf("Failed to extract %s\n", zipFileName.c_str());
            continue;
        }

        // Load image from memory into RGBA
        int width, height, channels;
        unsigned char* rgba_data = stbi_load_from_memory(
            (unsigned char*)png_data, png_size,
            &width, &height, &channels, 4
        );

        if (!rgba_data) {
            printf("Failed to decode PNG: %s\n", zipFileName.c_str());
            mz_free(png_data);
            continue;
        }

        Image::ImageRGBA newRGBA;
        newRGBA.name = zipFileName.substr(0, zipFileName.find_last_of('.'));
        newRGBA.width = width;
        newRGBA.height = height;
        newRGBA.data = rgba_data;

        size_t imageSize = width * height * 4;
        memStats.totalRamUsage += imageSize;
        memStats.imageCount++;

        Image::imageRGBAS.push_back(newRGBA);
        mz_free(png_data);
    }
}
}

void Image::loadImageFromFile(std::string filePath){
  
  auto it = std::find_if(imageRGBAS.begin(), imageRGBAS.end(), [&](const ImageRGBA& img) {
    return img.name == filePath;
  });
  if (it != imageRGBAS.end()) return;
    
  int width,height,channels;
  FILE* file = fopen(("romfs:/project/"+filePath + ".png").c_str(), "rb");
  if (!file) {
    file = fopen(("romfs:/project/"+filePath + ".jpg").c_str(), "rb");
    if (!file) {
      std::cerr << "Invalid image file name " << filePath << std::endl;
      return;
    }
  }

  unsigned char* rgba_data = stbi_load_from_file(file, &width, &height, &channels, 4);
  fclose(file);

  if (!rgba_data) {
    std::cerr << "Failed to decode image: " << filePath << std::endl;
    return;
  }

      // std::cout << "Adding PNG: " << zipFileName << std::endl;
    ImageRGBA newRGBA;
    newRGBA.name = filePath;
    newRGBA.width = width;
    newRGBA.height = height;
    newRGBA.data = rgba_data;
    //memorySize += sizeof(newRGBA);

    size_t imageSize = width * height * 4;
    memStats.totalRamUsage += imageSize;
    memStats.imageCount++;

    imageRGBAS.push_back(newRGBA);

}


/** Read an RGBA image from `path` with dimensions `image_width`x`image_height`
 * and return a `C2D_Image` object.
 * Assumes image data is stored left->right, top->bottom.
 * Dimensions must be within 64x64 and 1024x1024.
 * Code here originally from https://gbatemp.net/threads/citro2d-c2d_image-example.668574/
 * then edited to fit my code
 */
C2D_Image get_C2D_Image(Image::ImageRGBA rgba) {
    //std::cout << "Creating C2D_Image from RGBA " << rgba.name << std::endl;

    u32 px_count = rgba.width * rgba.height;
    u32 *rgba_raw = reinterpret_cast<u32*>(rgba.data);

  
    // Image data
    C2D_Image image;
  
    // Base texture
  // std::cout << "Creating C3D_Tex..." << std::endl;
    C3D_Tex *tex = (C3D_Tex *)malloc(sizeof(C3D_Tex));
    image.tex = tex;
    // Texture dimensions must be square powers of two between 64x64 and 1024x1024
    tex->width = clamp(next_pow2(rgba.width), 64, 1024);
    tex->height = clamp(next_pow2(rgba.height), 64, 1024);
  
    size_t textureSize = tex->width * tex->height * 4;
    memStats.totalVRamUsage += textureSize;
    memStats.imageCount++;

    // Subtexture
   //std::cout << "Creating C3D_SubTex..." << std::endl;
    Tex3DS_SubTexture *subtex = (Tex3DS_SubTexture *)malloc(sizeof(Tex3DS_SubTexture));
    image.subtex = subtex;
    subtex->width = rgba.width;
    subtex->height = rgba.height;
    // (U, V) coordinates
    subtex->left = 0.0f;
    subtex->top = 1.0f;
    subtex->right = (float)rgba.width / (float)tex->width;
    subtex->bottom = 1.0 - ((float)rgba.height / (float)tex->height);
  
    //std::cout << "Allocating texture data..." << std::endl;
    C3D_TexInit(tex, tex->width, tex->height, GPU_RGBA8);
   // std::cout << "Setting Texture Filter..." << std::endl;
    C3D_TexSetFilter(tex, GPU_NEAREST, GPU_NEAREST);
    // GPU_LINEAR TODO try that later on real hardware

  
   // std::cout << "Setting Texture Wrap..." << std::endl;
    memset(tex->data, 0, px_count * 4);
    for (u32 i = 0; i < (u32)rgba.width; i++) {
    for (u32 j = 0; j < (u32)rgba.height; j++) {
        u32 src_idx = (j * rgba.width) + i;
        u32 rgba_px = rgba_raw[src_idx];
        u32 abgr_px = rgba_to_abgr(rgba_px);
  
        // Swizzle magic to convert into a t3x format
        u32 dst_ptr_offset = ((((j >> 3) * (tex->width >> 3) + (i >> 3)) << 6) +
                              ((i & 1) | ((j & 1) << 1) | ((i & 2) << 1) |
                               ((j & 2) << 2) | ((i & 4) << 2) | ((j & 4) << 3)));
        ((u32 *)tex->data)[dst_ptr_offset] = abgr_px;
      }
    }
  
    std::cout << "Image Loaded! total VRAM: " << memStats.totalVRamUsage << std::endl;

    return image;
  }

void Image::freeImage(const std::string& costumeId) {
    auto it = imageC2Ds.find(costumeId);
    if (it != imageC2Ds.end()) {
        if (it->second.image.tex) {

            size_t textureSize = it->second.image.tex->width * it->second.image.tex->height * 4;
            memStats.totalVRamUsage -= textureSize;
            memStats.c2dImageCount--;

            C3D_TexDelete(it->second.image.tex);
            //free(it->second.image.tex);
        }
        // if (it->second.image.subtex) {
        //     free((Tex3DS_SubTexture*)it->second.image.subtex);
        // }
        imageC2Ds.erase(it);
        std::cout << "freed image!" << std::endl;
    }
}

void Image::queueFreeImage(const std::string& costumeId){
  toDelete.push_back(costumeId);
}

void Image::FlushImages(){
    
    if(memStats.totalVRamUsage > 24000000){
      ImageData* imgToDelete = nullptr;
      std::string toDeleteStr;
      for(auto& [id, img] : imageC2Ds){
        if(imgToDelete == nullptr) imgToDelete = &img;
        
        if(img.freeTimer < imgToDelete->freeTimer){
          imgToDelete = &img;
          toDeleteStr = id;
        }
    }
    toDelete.push_back(toDeleteStr);
    } else{
    for(auto& [id, img] : imageC2Ds){
        if(img.freeTimer <= 0){
            toDelete.push_back(id);
        } else {
            img.freeTimer -= 1;
        }
    }
  }
    
    for(const std::string& id : toDelete){
        Image::freeImage(id);
    }
    toDelete.clear();
}