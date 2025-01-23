#pragma once

#include "vertexdata.h"
#include <memory>

namespace lillugsi::planet {

/// Base class for all vertex visitors
/// Provides interface for algorithms that need to process or modify vertex data
class VertexVisitor {
public:
	virtual ~VertexVisitor() = default;
	
	/// Called for each vertex in the mesh
	/// Derived visitors implement this to perform their specific operations
	virtual void visit(std::shared_ptr<VertexData> vertex) = 0;
};

} /// namespace lillugsi::planet