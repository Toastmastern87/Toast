#include "tpch.h"
#include "Font.h"

#include "MSDFData.h"

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

		Ref<Texture2D> texture = CreateRef<Texture2D>(DXGI_FORMAT::DXGI_FORMAT_R32G32B32A32_FLOAT, bitmap.width, bitmap.height, D3D11_USAGE_DYNAMIC, D3D11_BIND_SHADER_RESOURCE);
		texture->SetData((void*)bitmap.pixels, ((bitmap.width * bitmap.height) * 4 * sizeof(float)));
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
		TightAtlasPacker::DimensionsConstraint atlasSizeConstraint = TightAtlasPacker::DimensionsConstraint::MULTIPLE_OF_FOUR_SQUARE;
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
			glyphsLoaded = mMSDFData->FontGeometry.loadCharset(font, fontInput.fontScale, msdf_atlas::Charset::ASCII, true, true);
			anyCodepointsAvailable |= glyphsLoaded > 0;

			if (glyphsLoaded < 0)
				TOAST_CORE_ERROR("No glyphs loaded!");
			TOAST_CORE_INFO("Loaded font gemometry of %d out of %d glyphs", glyphsLoaded, msdf_atlas::Charset::ASCII.size());

			if (fontInput.fontName)
				mMSDFData->FontGeometry.setName(fontInput.fontName);

			// Determine final atlas dimensions, scale and range, pack glyphs
			{
				double pxRange = rangeValue;
				TightAtlasPacker atlasPacker;

				atlasPacker.setDimensionsConstraint(atlasSizeConstraint);
				atlasPacker.setPadding(0);
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
						TOAST_CORE_ERROR("Error: Could not fit %d out of %d glyphs into the atlast", remaining, (int)mMSDFData->Glyphs.size());
						TOAST_CORE_ASSERT(false, "");
					}
				}
				atlasPacker.getDimensions(config.width, config.height);
				TOAST_CORE_ASSERT(config.width > 0 && config.height > 0, "");
				config.emSize = atlasPacker.getScale();
				config.pxRange = atlasPacker.getPixelRange();
				TOAST_CORE_INFO("Glyph size: %f pixels/EM", config.emSize);
				TOAST_CORE_INFO("Atlas dimensions: %d x %d", config.width, config.height);
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
		sDefaultFont = CreateRef<Font>("..\\Toaster\\assets\\fonts\\Roboto Mono\\RobotoMono-Regular.ttf");
	}

	Toast::Ref<Font> Font::GetDefaultFont()
	{
		return sDefaultFont;
	}

}