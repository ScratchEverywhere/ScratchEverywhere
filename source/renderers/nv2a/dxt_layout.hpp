#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace NV2ADxtLayout {
struct Layout {int width,height,pitch;};
inline int powerOfTwo(int n){int p=1;while(p<n)p*=2;return p;}
inline Layout make(int width,int height,bool dxt5){
    const int physicalWidth=std::max(dxt5?16:32,powerOfTwo(width));
    return {physicalWidth,std::max(4,powerOfTwo(height)),physicalWidth/4*(dxt5?16:8)};
}
inline uint64_t readBits(const uint8_t *p,int count){uint64_t v=0;for(int i=0;i<count;++i)v|=uint64_t(p[i])<<(i*8);return v;}
inline void writeBits(uint8_t *p,int count,uint64_t v){for(int i=0;i<count;++i)p[i]=uint8_t(v>>(i*8));}
// Input is logical DXT blocks at sourcePitch. File/cache payloads need no change.
// Fill every physical block, clamping selectors rather than recompressing edges.
inline bool upload(uint8_t *destination,const Layout &layout,const uint8_t *source,int sourcePitch,int width,int height,bool dxt5){
    const int blockBytes=dxt5?16:8,columns=(width+3)/4,rows=(height+3)/4;
    if(!destination||!source||width<1||height<1||layout.width<width||layout.height<height||layout.width%4||layout.height%4||layout.pitch!=layout.width/4*blockBytes||sourcePitch<columns*blockBytes)return false;
    const size_t sourceBytes=size_t(rows-1)*sourcePitch+columns*blockBytes,destinationBytes=size_t(layout.pitch)*(layout.height/4);
    const auto srcAddress=reinterpret_cast<uintptr_t>(source),dstAddress=reinterpret_cast<uintptr_t>(destination);
    std::vector<uint8_t> staged;
    if(srcAddress<dstAddress+destinationBytes&&dstAddress<srcAddress+sourceBytes){
        // Preserve source rows for self/overlapping reuploads without reading
        // any caller-owned padding past the last logical block in each row.
        staged.resize(size_t(rows)*columns*blockBytes);
        for(int y=0;y<rows;++y)std::memcpy(staged.data()+size_t(y)*columns*blockBytes,source+size_t(y)*sourcePitch,columns*blockBytes);
        source=staged.data();sourcePitch=columns*blockBytes;
    }
    const int colorOffset=dxt5?8:0;
    for(int by=0;by<layout.height/4;++by)for(int bx=0;bx<layout.width/4;++bx){
        const auto *input=source+size_t(std::min(by,rows-1))*sourcePitch+std::min(bx,columns-1)*blockBytes;
        auto *output=destination+size_t(by)*layout.pitch+bx*blockBytes;
        std::memcpy(output,input,blockBytes);
        if(bx*4+3<width&&by*4+3<height)continue;
        const uint64_t colors=readBits(input+colorOffset+4,4),alphas=dxt5?readBits(input+2,6):0;
        uint64_t outColors=0,outAlphas=0;
        for(int y=0;y<4;++y)for(int x=0;x<4;++x){
            const int src=(std::min(by*4+y,height-1)%4)*4+std::min(bx*4+x,width-1)%4,dst=y*4+x;
            outColors|=((colors>>(src*2))&3)<<(dst*2);
            if(dxt5)outAlphas|=((alphas>>(src*3))&7)<<(dst*3);
        }
        writeBits(output+colorOffset+4,4,outColors);if(dxt5)writeBits(output+2,6,outAlphas);
    }
    return true;
}
} // namespace NV2ADxtLayout
