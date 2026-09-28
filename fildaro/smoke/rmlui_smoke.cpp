// Smoke test for the Fildaro RmlUi fork: configures nothing product-specific,
// provides minimal host interfaces, and drives Core through a full
// initialise -> context -> document -> update -> render -> shutdown cycle.
//
// Checks are written to stderr and terminate with exit code 2, in line with
// the Fildaro repos' testing conventions (no CRT assert dialogs).
#include <RmlUi/Core.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/SystemInterface.h>
#include <RmlUi/Core/RenderInterface.h>

#include <cstdio>
#include <string>

namespace {

int g_failures = 0;

void check(bool condition, const char* what)
{
	if (!condition)
	{
		std::fprintf(stderr, "FAIL: %s\n", what);
		g_failures++;
	}
}

// Monotonic clock so animations and updates observe advancing time.
double smoke_time()
{
	static double t = 0.0;
	t += 1.0 / 60.0;
	return t;
}

class SmokeSystemInterface : public Rml::SystemInterface {
public:
	double GetElapsedTime() override { return smoke_time(); }
};

// Records compiled geometry sizes in the handle itself so RenderGeometry can
// count drawn vertices without keeping a side table.
class SmokeRenderInterface : public Rml::RenderInterface {
public:
	Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
		Rml::Span<const int> /*indices*/) override
	{
		compiled_batches_++;
		last_compile_vertices_ = static_cast<uintptr_t>(vertices.size());
		return static_cast<Rml::CompiledGeometryHandle>(last_compile_vertices_);
	}
	void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f /*translation*/,
		Rml::TextureHandle /*texture*/) override
	{
		rendered_batches_++;
		rendered_vertices_ += static_cast<size_t>(geometry);
	}
	void ReleaseGeometry(Rml::CompiledGeometryHandle /*geometry*/) override {}
	Rml::TextureHandle LoadTexture(Rml::Vector2i& /*texture_dimensions*/,
		const Rml::String& /*source*/) override
	{
		return 0;
	}
	Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> /*source*/,
		Rml::Vector2i /*source_dimensions*/) override
	{
		return 0;
	}
	void ReleaseTexture(Rml::TextureHandle /*texture*/) override {}
	void EnableScissorRegion(bool /*enable*/) override {}
	void SetScissorRegion(Rml::Rectanglei /*region*/) override {}

	size_t rendered_batches() const { return rendered_batches_; }
	size_t rendered_vertices() const { return rendered_vertices_; }
	size_t last_compile_vertices() const { return last_compile_vertices_; }

private:
	size_t compiled_batches_ = 0;
	size_t rendered_batches_ = 0;
	size_t rendered_vertices_ = 0;
	size_t last_compile_vertices_ = 0;
};

std::string load_first_existing(const char* const* candidates, size_t count)
{
	for (size_t i = 0; i < count; i++)
	{
		FILE* f = std::fopen(candidates[i], "rb");
		if (f)
		{
			std::fclose(f);
			return candidates[i];
		}
	}
	return {};
}

} // namespace

int main(int argc, char** argv)
{
	// Optional font path: argv[1] or a Windows default. Without a font the
	// text checks are skipped; box geometry still has to render.
	std::string font_path;
	if (argc > 1)
		font_path = argv[1];
	if (font_path.empty())
	{
#ifdef _WIN32
		const char* candidates[] = {"C:/Windows/Fonts/arial.ttf", "C:/Windows/Fonts/segoeui.ttf"};
		font_path = load_first_existing(candidates, 2);
#endif
	}

	SmokeSystemInterface system_interface;
	SmokeRenderInterface render_interface;
	Rml::SetSystemInterface(&system_interface);
	Rml::SetRenderInterface(&render_interface);

	check(Rml::Initialise(), "Rml::Initialise");
	if (g_failures)
		return 2;

	Rml::Context* context = Rml::CreateContext("smoke", Rml::Vector2i(400, 300));
	check(context != nullptr, "Rml::CreateContext");
	if (!context)
	{
		Rml::Shutdown();
		return 2;
	}

	const bool have_font =
		!font_path.empty() &&
		Rml::LoadFontFace(font_path.c_str(), "FildaroSmoke", Rml::Style::FontStyle::Normal);

	Rml::ElementDocument* document = context->LoadDocumentFromMemory(R"RML(
<rml>
	<head>
		<style>
			/* RmlUi ships no user-agent stylesheet: display and font must be
			   declared, like every upstream sample does through rml.rcss. */
			body { margin: 0; display: block; font-family: FildaroSmoke; font-size: 16px; }
			div { display: block; }
			#box { width: 120px; height: 40px; background-color: #f00; }
			#label { color: #fff; }
		</style>
	</head>
	<body>
		<div id="box"></div>
		<div id="label">Fildaro RmlUi smoke</div>
	</body>
</rml>
)RML");
	check(document != nullptr, "LoadDocumentMemory");
	if (document)
	{
		document->Show();

		context->Update();
		context->Render();

		Rml::Element* box = document->GetElementById("box");
		check(box != nullptr, "GetElementById(box)");
		if (box)
		{
			check(box->GetOffsetWidth() == 120, "box offset width is 120px");
			check(box->GetOffsetHeight() == 40, "box offset height is 40px");
		}
		check(render_interface.rendered_batches() > 0, "geometry batches were rendered");
		check(render_interface.rendered_vertices() >= 4, "at least one quad of vertices was rendered");

		if (have_font)
		{
			// Glyphs produce far more vertices than the flat box alone.
			check(render_interface.rendered_vertices() > 40,
				"text glyphs produced additional geometry (font engine live)");
		}
		else
		{
			std::fprintf(stderr, "info: no font available, text checks skipped\n");
		}

		// Input pipeline sanity: deliver events without a window backend.
		context->ProcessMouseMove(10, 10, 0);
		context->ProcessMouseButtonDown(0, 0);
		context->ProcessMouseButtonUp(0, 0);

		context->Update();
		context->Render();
		check(render_interface.rendered_batches() > 0, "still rendering after input");

		context->UnloadDocument(document);
	}

	Rml::RemoveContext("smoke");
	Rml::Shutdown();

	if (g_failures)
	{
		std::fprintf(stderr, "fildaro_rmlui_smoke: %d failure(s)\n", g_failures);
		return 2;
	}
	std::fprintf(stderr, "fildaro_rmlui_smoke: ok (font=%s batches=%zu vertices=%zu)\n",
		have_font ? "yes" : "no", render_interface.rendered_batches(),
		render_interface.rendered_vertices());
	return 0;
}
