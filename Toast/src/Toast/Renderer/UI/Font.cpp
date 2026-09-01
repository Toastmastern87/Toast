#include "tpch.h"
#include "Font.h"

#include "MSDFData.h"

#define NOMINMAX
#include <algorithm>

namespace Toast {

	using namespace msdf_atlas;

	struct FontInput 
	{
		const char* fontFilename;
		GlyphIdentifierType glyphIdentifierType;
		const char* charsetFilename;
		double fontScale;
		const char* fontName;
	};

	struct Configuration 
	{
		ImageType imageType;
		msdf_atlas::ImageFormat imageFormat;
		YDirection yDirection;
		int width, height;
		double emSize;
		double pxRange;
		double angleThreshold;
		double miterLimit;
		void (*edgeColoring)(msdfgen::Shape&, double, unsigned long long);
		bool expensiveColoring;
		unsigned long long coloringSeed;
		GeneratorAttributes generatorAttributes;
	};

#define FONT_ATLAS_SIZE 512
#define DEFAULT_ANGLE_THRESHOLD 3.0
#define DEFAULT_MITER_LIMIT 1.0
#define LCG_MULTIPLIER 6364136223846793005ull
#define THREADS 8

	template<typename T, typename S, int N, GeneratorFunction<S, N> GEN_FN>
	static Ref<Texture2D> makeAtlas(const std::vector<GlyphGeometry>& glyphs, const FontGeometry& fontGeometry, const Configuration& config) 
	{
		ImmediateAtlasGenerator<S, N, GEN_FN, BitmapAtlasStorage<T, N>> generator(config.width, config.height);
		generator.setAttributes(config.generatorAttributes);
		generator.setThreadCount(THREADS);
		generator.generate(glyphs.data(), glyphs.size());

		msdfgen::BitmapConstRef<T, N> bitmap = (msdfgen::BitmapConstRef<T, N>) generator.atlasStorage();

		TOAST_CORE_ASSERT(bitmap.width == FONT_ATLAS_SIZE && bitmap.height == FONT_ATLAS_SIZE, "Font atlas is not FONT_ATLAS_SIZE square - texture array slices will mismatch!");

		const size_t dataSize = (size_t)bitmap.width * (size_t)bitmap.height * N * sizeof(T);

		// Create the Texture2D using the fixed 512x512 dimensions.
		Ref<Texture2D> texture = CreateRef<Texture2D>(
			DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT,
			DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT,
			(uint32_t)bitmap.width, (uint32_t)bitmap.height,
			D3D11_USAGE_DYNAMIC,
			D3D11_BIND_SHADER_RESOURCE,
			1,
			D3D11_CPU_ACCESS_WRITE
		);

		texture->SetData((void*)bitmap.pixels, dataSize);

		TOAST_CORE_INFO("Font atlas generated: %dx%d, %zu bytes", (int)bitmap.width, (int)bitmap.height, dataSize);

		return texture;
	}

	Font::Font(const std::string& filepath)
		: mFilePath(filepath), mMSDFData(new MSDFData())
	{
		int result = 0;
		FontInput fontInput = { };
		Configuration config = { };
		fontInput.glyphIdentifierType = GlyphIdentifierType::UNICODE_CODEPOINT;
		fontInput.fontScale = 1;
		config.imageType = ImageType::MTSDF;
		config.imageFormat = msdf_atlas::ImageFormat::BINARY_FLOAT;
		config.yDirection = YDirection::BOTTOM_UP;
		config.edgeColoring = msdfgen::edgeColoringSimple;
		const char* imageFormatName = nullptr;
		int fixedWidth = -1, fixedHeight = -1;
		config.generatorAttributes.config.overlapSupport = true;
		config.generatorAttributes.scanlinePass = true;
		double minEmSize = 0.0;
		double rangeValue = 2.0;
		config.angleThreshold = DEFAULT_ANGLE_THRESHOLD;
		config.miterLimit = DEFAULT_MITER_LIMIT;
		
		fontInput.fontFilename = mFilePath.c_str();

		config.emSize = 40;

		// Load fonts
		bool anyCodepointsAvailable = false;
		{
			class FontHolder 
			{
				msdfgen::FreetypeHandle* ft;
				msdfgen::FontHandle* font;
				const char* fontFilename;
			public:
				FontHolder() : ft(msdfgen::initializeFreetype()), font(nullptr), fontFilename(nullptr) { }
				~FontHolder() {
					if (ft) {
						if (font)
							msdfgen::destroyFont(font);
						msdfgen::deinitializeFreetype(ft);
					}
				}

				bool load(const char* fontFilename) {
					if (ft && fontFilename) {
						if (this->fontFilename && !strcmp(this->fontFilename, fontFilename)) 
							return true;
						if (font)
							msdfgen::destroyFont(font);
						if ((font = msdfgen::loadFont(ft, fontFilename))) {
							this->fontFilename = fontFilename;
							return true;
						}
						this->fontFilename = nullptr;
					}
					return false;
				}
				operator msdfgen::FontHandle* () const {
					return font;
				}
			} font;
			
			if (!font.load(fontInput.fontFilename))
				TOAST_CORE_ERROR("Error loading font file!");

			// Load Glyphs
			mMSDFData->FontGeometry = FontGeometry(&mMSDFData->Glyphs);
			int glyphsLoaded = -1;
			glyphsLoaded = mMSDFData->FontGeometry.loadCharset(font, fontInput.fontScale, msdf_atlas::Charset::ASCII);
			anyCodepointsAvailable |= glyphsLoaded > 0;

			if (glyphsLoaded < 0)
				TOAST_CORE_ERROR("No glyphs loaded!");
			TOAST_CORE_INFO("Loaded font(%s) gemometry of %d out of %d glyphs", mFilePath.c_str(), glyphsLoaded, msdf_atlas::Charset::ASCII.size());

			if (fontInput.fontName)
				mMSDFData->FontGeometry.setName(fontInput.fontName);

			// Determine final atlas dimensions, scale and range, pack glyphs
			{
				double pxRange = rangeValue;
				TightAtlasPacker atlasPacker;

				atlasPacker.setDimensions(FONT_ATLAS_SIZE, FONT_ATLAS_SIZE);
				atlasPacker.setPadding(2);
				atlasPacker.setScale(config.emSize);
				atlasPacker.setPixelRange(pxRange);
				atlasPacker.setMiterLimit(config.miterLimit);

				if (int remaining = atlasPacker.pack(mMSDFData->Glyphs.data(), mMSDFData->Glyphs.size()))
				{
					if (remaining < 0)
					{
						TOAST_CORE_ASSERT(false, "");
					}
					else 
					{
						TOAST_CORE_ERROR("Font(%s): Could not fit %d out of %d glyphs into a %dx%d atlas at emSize %f, padding %d. Lower emSize, reduce the charset, or raise FONT_ATLAS_SIZE.", mFilePath.c_str(), remaining, (int)mMSDFData->Glyphs.size(), FONT_ATLAS_SIZE, FONT_ATLAS_SIZE, config.emSize, 2);
						TOAST_CORE_ASSERT(false, "");
					}
				}
				atlasPacker.getDimensions(config.width, config.height);
				TOAST_CORE_ASSERT(config.width > 0 && config.height > 0, "");
				config.emSize = atlasPacker.getScale();
				config.pxRange = atlasPacker.getPixelRange();
				TOAST_CORE_INFO("Glyph size: %f pixels/EM", config.emSize);
				TOAST_CORE_INFO("Atlas dimensions: %d x %d, pixel range: %f", config.width, config.height, config.pxRange);
			}

			// Edge coloring
			unsigned long long glyphSeed = config.coloringSeed;
			for (GlyphGeometry& glyph : mMSDFData->Glyphs)
			{
				glyphSeed *= LCG_MULTIPLIER;
				glyph.edgeColoring(config.edgeColoring, config.angleThreshold, glyphSeed);
			}
		}

		mTextureAtlas = makeAtlas<float, float, 4, mtsdfGenerator>(mMSDFData->Glyphs, mMSDFData->FontGeometry, config);
	}

	Font::~Font()
	{
		delete mMSDFData;
	}

	static Ref<Font> sDefaultFont;

	void Font::StaticInit()
	{
		sDefaultFont = CreateRef<Font>("..\\Toaster\\assets\\fonts\\1_Roboto Mono\\RobotoMono-Regular.ttf");
	}

	Ref<Font> Font::GetDefaultFont()
	{
		return sDefaultFont;
	}

}