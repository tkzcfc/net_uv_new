#pragma once

#include "imgui.h"
#include <map>
#include <vector>

class TextureCache
{
	static TextureCache* TextureCacheInstance;
public:

	static TextureCache* getInstance();
	
	static void destroy();

	~TextureCache();

	ImTextureID loadTexture(const std::string& textureName);

	void releaseTexture(ImTextureID textureId);

	void releaseTextureByName(const std::string& textureName);

	void releaseAll();

	void addSearchPath(const std::string& path);

	std::string getPath(const std::string& path);
	
protected:

	std::map<std::string, ImTextureID> textureCacheMap;

	std::vector<std::string> searchPaths;
};


