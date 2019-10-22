#include "texture/TextureCache.h"
#include "Application.h"


TextureCache* TextureCache::TextureCacheInstance = NULL;

TextureCache* TextureCache::getInstance()
{
	if (TextureCacheInstance == NULL)
	{
		TextureCacheInstance = new TextureCache();
	}
	return TextureCacheInstance;
}

void TextureCache::destroy()
{
	if (TextureCacheInstance)
	{
		delete TextureCacheInstance;
		TextureCacheInstance = NULL;
	}
}

TextureCache::~TextureCache()
{
	this->releaseAll();
}

ImTextureID TextureCache::loadTexture(const std::string& textureName)
{
	auto it = textureCacheMap.find(textureName);
	if (it == textureCacheMap.end())
	{
		auto path = this->getPath(textureName);
		ImTextureID texture = Application_LoadTexture(path.c_str());
		if (texture == NULL)
		{
			assert(0);
			return NULL;
		}
		textureCacheMap.insert(std::make_pair(textureName, texture));
		return texture;
	}
	return it->second;
}

void TextureCache::releaseTexture(ImTextureID textureId)
{
	for (auto it = textureCacheMap.begin(); it != textureCacheMap.end(); ++it)
	{
		if (it->second == textureId)
		{
			Application_DestroyTexture(it->second);
			textureCacheMap.erase(it);
			break;
		}
	}
}

void TextureCache::releaseTextureByName(const std::string& textureName)
{
	auto it = textureCacheMap.find(textureName);
	if (it != textureCacheMap.end())
	{
		Application_DestroyTexture(it->second);
		textureCacheMap.erase(it);
	}
}

void TextureCache::releaseAll()
{
	for (auto& it : textureCacheMap)
	{
		Application_DestroyTexture(it.second);
	}
	textureCacheMap.clear();
}

void TextureCache::addSearchPath(const std::string& path)
{
	if (path.back() != '/' && path.back() != '\\')
	{
		auto str = path;
		str.append("/");
		searchPaths.push_back(str);
	}
	else
	{
		searchPaths.push_back(path);
	}
}

std::string TextureCache::getPath(const std::string& path)
{
	auto fileExist = [](const std::string& filepath) -> bool
	{
		FILE* fp = fopen(filepath.c_str(), "rb");
		if (fp)
		{
			fclose(fp);
			return true;
		}
		return false;
	};

	if (fileExist(path))
	{
		return path;
	}

	for (auto i = 0; i < searchPaths.size(); ++i)
	{
		std::string str = searchPaths[i] + path;
		if (fileExist(str))
		{
			return std::move(str);
		}
	}

	assert(0);
	return "";
}

