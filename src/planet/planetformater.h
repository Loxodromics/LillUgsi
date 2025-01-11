#include "spdlog/fmt/fmt.h"
#include "face.h"

/// Custom formatter for Face class
template<>
struct fmt::formatter<lillugsi::planet::Face> {
	constexpr auto parse(format_parse_context& ctx) -> format_parse_context::iterator {
		return ctx.begin();
	}

	template<typename FormatContext>
	auto format(const lillugsi::planet::Face& face, FormatContext& ctx) -> format_context::iterator {
		return fmt::format_to(ctx.out(), "Face(Vertices: [{}, {}, {}], Data: {})",
			face.getVertexIndices()[0],
			face.getVertexIndices()[1],
			face.getVertexIndices()[2],
			face.getData());
	}
};