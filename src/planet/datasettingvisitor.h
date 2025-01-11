#pragma once

#include "face.h"

namespace lillugsi::planet {
class DataSettingVisitor : public FaceVisitor {
public:
	void visit(std::shared_ptr<Face> face) override;

private:
	static float calculateDataForFace(const std::shared_ptr<Face>& face);
};
} /// namespace lillugsi::planet