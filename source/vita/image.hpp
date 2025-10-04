#pragma once
#include "../scratch/image.hpp"
#include <vita2d.h>
#include <string>
#include <unordered_map>
#include <vector>

class VitaImage {
public:
	int width;
	int height;
	size_t imageUsageCount = 0;
	int maxFreeTime = 480;
	virtual void render(double xPos, double yPos,
				double scale, double opacity,
				double rotation, bool centered = true,
				int scaleX = 1);
	
	virtual ~VitaImage() = default;
};

class BitmapImage : public VitaImage {
private:
	vita2d_texture* tex;
public:
	BitmapImage(vita2d_texture* tex);
	~BitmapImage();
	void render(double xPos, double yPos,
				double scale, double opacity,
				double rotation, bool centered = true,
				int scaleX = 1);
};

class VectorImage : public VitaImage {
	// TODO: Load SVG
	// TODO: Convert SVG shape to pointmap
	// TODO: Store all pointmaps in vertexListMain
	// TODO: Copy and translate pointmap to vertexListRender when required (
	// TODO: Render pointmaps when it's time to render
	// TODO: Offset and scale rendering (SceGxmContext?)
private:
	float *vertexListMain;
	vita2d_color_vertex *vertexListRender;
	bool renderListNeedsUpdating = true;
};

extern std::unordered_map<std::string, VitaImage> imageData;
