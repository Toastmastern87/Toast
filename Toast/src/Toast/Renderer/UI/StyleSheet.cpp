#include "tpch.h"
#include "StyleSheet.h"

#include "Toast/Assets/AssetManager.h"

namespace Toast {

	class StyleReader
	{
	public:
		StyleReader(const std::string& source)
			: mSource(source) {
		}

		// Skips white spaces and comments
		void SkipTrivia()
		{
			while (mPos < mSource.size())
			{
				const char c = mSource[mPos];

				if (c == '\n')
				{
					mLine++;
					mPos++;
				}
				else if (std::isspace((unsigned char)c))
					mPos++;
				else if (c == '/' && mPos + 1 < mSource.size() && mSource[mPos + 1] == '*')
				{
					mPos += 2;

					while (mPos + 1 < mSource.size() && !(mSource[mPos] == '*' && mSource[mPos + 1] == '/'))
					{
						if (mSource[mPos] == '\n')
							mLine++;

						mPos++;
					}
					mPos = std::min(mPos + 2, mSource.size());
				}
				else
					break;
			}
		}

		bool AtEnd() const { return mPos >= mSource.size(); }
		char Peek() const { return mPos < mSource.size() ? mSource[mPos] : '\0'; }

		bool Match(char expected)
		{
			if (Peek() != expected)
				return false;

			mPos++;
			return true;
		}

		// Identifiers allow '-' so 'corner-radius' is one token, not three
		std::string ReadIdentifier() 
		{
			size_t start = mPos;

			while (mPos < mSource.size() && (std::isalnum((unsigned char)mSource[mPos]) || mSource[mPos] == '-'))
				mPos++;

			return mSource.substr(start, mPos - start);
		}

		// Reads up to but not including the delimiter, trimming trailing space.
		std::string ReadUntil(char delimiter)
		{
			size_t start = mPos;

			while (mPos < mSource.size() && mSource[mPos] != delimiter)
			{
				if (mSource[mPos] == '\n')
					mLine++;

				mPos++;
			}

			std::string value = mSource.substr(start, mPos - start);
			while (!value.empty() && std::isspace((unsigned char)value.back()))
				value.pop_back();

			return value;
		}
		// Recovery: after an error, run to the next ';' so one bad declaration
		// doesn't discard the rest of the file.
		void SkipToDeclerationEnd() 
		{
			while (mPos < mSource.size() && mSource[mPos] != ';') 
			{
				if (mSource[mPos] == '\n')
					mLine++;

				mPos++;
			}

			Match(';');
		}

		uint32_t GetLine() const { return mLine; }
	private:
		const std::string mSource;
		size_t mPos = 0;
		uint32_t mLine = 1;
	};

	// #RGB, #RRGGBB, #RRGGBBAA, or rgb()/rgba().
	// No gamma conversion - the result must match what ImGui::ColorEdit4 
	static bool ParseColor(const std::string& text, DirectX::XMFLOAT4& out)
	{
		auto hexDigit = [](char c) -> int
			{
				if (c >= '0' && c <= '9')
					return c - '0';
				if (c >= 'a' && c <= 'f')
					return c - 'a' + 10;
				if (c >= 'A' && c <= 'F')
					return c - 'A' + 10;

				return -1;
			};

		if (!text.empty() && text[0] == '#')
		{
			std::string hex = text.substr(1);
			uint32_t channels[4] = { 0, 0, 0, 255 };

			if (hex.size() == 3)
			{
				for (int i = 0; i < 3; i++)
				{
					int d = hexDigit(hex[i]);

					if (d < 0)
						return false;

					channels[i] = (uint32_t)(d * 16 + d);
				}
			}
			else if (hex.size() == 6 || hex.size() == 8)
			{
				for (size_t i = 0; i < hex.size(); i += 2)
				{
					int hi = hexDigit(hex[i]);
					int lo = hexDigit(hex[i + 1]);
					if (hi < 0 || lo < 0)
						return false;

					channels[i / 2] = (uint32_t)(hi * 16 + lo);
				}
			}
			else 
				return false;

			out = { channels[0] / 255.0f, channels[1] / 255.0f, channels[2] / 255.0f, channels[3] / 255.0f };
			return true;
		}

		if (text.rfind("rgba(", 0) == 0 || text.rfind("rgb(", 0) == 0)
		{
			size_t open = text.find('(');
			size_t close = text.find(')');
			if (close == std::string::npos)
				return false;

			std::string inner = text.substr(open + 1, close - open - 1);
			float values[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
			int count = 0;

			std::stringstream ss(inner);
			std::string token;
			while (std::getline(ss, token, ',') && count < 4)
			{
				try
				{
					values[count++] = std::stof(token);
				}
				catch (...)
				{
					return false;
				}
			}

			if (count < 3)
				return false;

			out = { values[0] / 255.0f, values[1] / 255.0f, values[2] / 255.0f, count >= 4 ? values[3] : 1.0f };
			return true;
		}

		return false;
	}

	static bool ParseNumber(const std::string& text, float& out)
	{
		try
		{
			out = std::stof(text);
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	static bool ParseBool(const std::string& text, bool& out)
	{
		if (text == "true" || text == "1")
		{
			out = true;
			return true;
		}

		if (text == "false" || text == "0")
		{
			out = false;
			return true;
		}

		return false;
	}

#define UI_TEXTURE_DIRECTORY "Texture/UI"
	
	static bool ResolveUITexture(const std::string& filename, AssetHandle& outHandle)
	{
		std::filesystem::path relativePath = (std::filesystem::path(UI_TEXTURE_DIRECTORY) / filename).lexically_normal();

		outHandle = AssetManager::GetHandleFromPath(relativePath);

		if (outHandle == AssetHandle(0))
		{
			TOAST_CORE_WARN("StyleSheet: '%s' not found in '%s'. Put the texture in that folder so it gets imported at startup.", filename.c_str(), UI_TEXTURE_DIRECTORY);
			return false;
		}

		return true;
	}

	static bool ApplyDeclaration(StyleBlock& block, const std::string& property, const std::string& value)
	{
		if (property == "color")
		{
			DirectX::XMFLOAT4 c;
			if (!ParseColor(value, c))
				return false;

			block.Color.Assign(c);
			return true;
		}

		if (property == "background")
		{
			DirectX::XMFLOAT4 c;
			if (!ParseColor(value, c))
				return false;

			block.Background.Assign(c);
			return true;
		}

		if (property == "background-click")
		{
			DirectX::XMFLOAT4 c;
			if (!ParseColor(value, c))
				return false;

			block.BackgroundClick.Assign(c);
			return true;
		}

		if (property == "corner-radius")
		{
			float f;
			if (!ParseNumber(value, f))
				return false;

			block.CornerRadius.Assign(f);
			return true;
		}

		if (property == "visible")
		{
			bool b;
			if (!ParseBool(value, b))
				return false;

			block.Visible.Assign(b);
			return true;
		}

		if (property == "use-color")
		{
			bool b;
			if (!ParseBool(value, b))
				return false;


			block.UseColor.Assign(b);
			return true;
		}

		if (property == "background-image")
		{
			AssetHandle handle;
			if (!ResolveUITexture(value, handle))
				return false;

			block.BackgroundImage.Assign(handle);
			return true;
		}

		return false;
	}

	bool StyleSheet::ParseFromFile(const std::filesystem::path& filepath)
	{
		std::ifstream in(filepath);
		if (!in.is_open())
		{
			TOAST_CORE_ERROR("StyleSheet: Could not open '%s'", filepath.string().c_str());
			return false;
		}

		std::stringstream buffer;
		buffer << in.rdbuf();
		const std::string source = buffer.str();

		mBlock = StyleBlock{};

		StyleReader reader(source);
		uint32_t declerations = 0;
		uint32_t errors = 0;

		while (true)
		{
			reader.SkipTrivia();
			if (reader.AtEnd())
				break;

			const uint32_t line = reader.GetLine();
			const std::string property = reader.ReadIdentifier();

			if (property.empty())
			{
				TOAST_CORE_WARN("StyleSheet '%s' line %u, expected a property, skipping to the next ';'.", filepath.string().c_str(), line);
				errors++;
				reader.SkipToDeclerationEnd();
				continue;
			}

			reader.SkipTrivia();

			if (!reader.Match(':'))
			{
				TOAST_CORE_WARN("StyleSheet '%s' line %u, expected ':', after '%s'.", filepath.string().c_str(), line, property.c_str());
				errors++;
				reader.SkipToDeclerationEnd();
				continue;
			}

			reader.SkipTrivia();

			const std::string value = reader.ReadUntil(';');
			reader.Match(';');

			if (!ApplyDeclaration(mBlock, property, value))
			{
				TOAST_CORE_WARN("StyleSheet '%s' line %u, unknown or invalid declaration '%s: %s'.", filepath.string().c_str(), line, property.c_str(), value.c_str());
				errors++;
				continue;
			}

			declerations++;
		}

		TOAST_CORE_INFO("StyleSheet: Parsed '%s' - %u declerations, %u errors", filepath.string().c_str(), declerations, errors);

		return true;
	}

	bool StyleSheet::SaveToFile(const std::filesystem::path& filepath) const
	{
		std::ofstream out(filepath);
		if (!out.is_open())
		{
			TOAST_CORE_ERROR("StyleSheet: Could not open '%s' for writing", filepath.string().c_str());
			return false;
		}

		auto writeColor = [&out](const char* name, const StyleValue<DirectX::XMFLOAT4>& v)
			{
				if (!v.Set)
					return;

				out << name << ": #"
					<< std::hex << std::setfill('0') << std::setw(2) << (int)(v.Value.x * 255.0f + 0.5f)
					<< std::setw(2) << (int)(v.Value.y * 255.0f + 0.5f)
					<< std::setw(2) << (int)(v.Value.z * 255.0f + 0.5f)
					<< std::setw(2) << (int)(v.Value.w * 255.0f + 0.5f)
					<< std::dec << ";\n";
			};

		writeColor("color", mBlock.Color);
		writeColor("background", mBlock.Background);
		writeColor("background-click", mBlock.BackgroundClick);

		if (mBlock.CornerRadius.Set)
			out << "corner-radius: " << mBlock.CornerRadius.Value << ";\n";
		if (mBlock.Visible.Set)
			out << "visible: " << mBlock.Visible.Value << ";\n";
		if (mBlock.UseColor.Set)
			out << "use-color: " << mBlock.UseColor.Value << ";\n";

		if (mBlock.BackgroundImage.Set)
		{
			if(const AssetMetadata* metadata = AssetManager::GetMetadata(mBlock.BackgroundImage.Value))
				out << "background-image: " << metadata->FilePath.filename().string() << ";\n";
		}

		return true;
	}

}