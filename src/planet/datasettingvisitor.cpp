#include "datasettingvisitor.h"
#include "planetformater.h"

#include <spdlog/spdlog.h>

namespace lillugsi::planet {
void DataSettingVisitor::visit(const std::shared_ptr<Face> face) {
	if (!face) return;

	/// Example operation: Set face data based on some criteria
	/// This is where you implement the logic to set the data for the face
	const float data = DataSettingVisitor::calculateDataForFace(face);
	face->setData(data);
	spdlog::trace("calculateDataForFace Face object: {}", *face);
}

float DataSettingVisitor::calculateDataForFace(const std::shared_ptr<Face>& face) {
	/// Implement your logic to calculate data for a face
	/// This is just a placeholder implementation
	return (float)rand()/(float)(RAND_MAX/1.0f); /// Placeholder return value
}
} /// namespace lillugsi::planet