#pragma once

#include <vector>
#include <memory>
#include <glm/glm.hpp>

namespace lillugsi::planet {

/// VertexData represents a single vertex in our planetary surface mesh.
/// It stores elevation data and maintains relationships with neighboring vertices,
/// providing efficient slope calculations through caching and dirty flags.
class VertexData {
public:
	/// Create a vertex at the given position with default elevation of 0
	explicit VertexData(const glm::dvec3& position);

	/// Rule of five - we manage complex relationships between vertices
	~VertexData() = default;
	VertexData(const VertexData& other) = delete;
	VertexData& operator=(const VertexData& other) = delete;
	VertexData(VertexData&& other) noexcept = default;
	VertexData& operator=(VertexData&& other) noexcept = default;

	/// Elevation accessors
	[[nodiscard]] double getElevation() const { return elevation; }
	void setElevation(double newElevation);

	/// Position accessors
	[[nodiscard]] glm::dvec3 getPosition() const { return position; }
	/// Get the current normal vector, recalculating if necessary
	/// This provides an efficient way to access the normal while ensuring it's up to date
	[[nodiscard]] glm::dvec3 getNormal();

	/// Neighbor management
	/// We store weak_ptrs to neighbors to avoid circular reference issues
	void addNeighbor(const std::shared_ptr<VertexData>& neighbor);
	[[nodiscard]] std::vector<std::shared_ptr<VertexData>> getNeighbors() const;

	/// Get the slope to a specific neighbor
	/// Calculates and caches the slope if needed
	[[nodiscard]] double getSlope(size_t neighborIndex);

	/// Force recalculation of normal vector based on neighbor positions
	void recalculateNormal();

	/// Clear all neighbor relationships for this vertex
	/// Called before rebuilding neighbors after subdivision
	void clearNeighbors();

private:
	/// Calculate slope to specific neighbor and cache the result
	void calculateSlope(size_t neighborIndex);

	/// Mark all slopes involving this vertex as needing recalculation
	void markSlopesDirty();

	/// Mark slope to specific neighbor as dirty
	/// Called by neighbors when their elevation changes
	void markSlopeDirty(const VertexData* neighbor);

	/// Mark this vertex's normal as needing recalculation
	/// We call this when our elevation changes
	void markNormalDirty();

	/// Notify neighbors that their normals need recalculation
	/// We call this when our elevation changes since it affects their normals
	void notifyNeighborsNormalsDirty() const;

	/// Calculate initial distance to a neighbor
	/// Called once during neighbor setup since lateral positions don't change
	[[nodiscard]] double calculateDistanceToNeighbor(const VertexData& neighbor) const;

	/// Physical properties
	double elevation{-2.0};

	/// Vector fields
	glm::dvec3 position;
	glm::dvec3 normal{0.0, 1.0, 0.0};  /// Default normal points up

	/// Topology and cached calculations
	std::vector<std::weak_ptr<VertexData>> neighbors;
	/// Store distances to neighbors - these remain constant after initialization
	/// since only elevation changes, not lateral positions
	std::vector<double> neighborDistances;
	/// Cache calculated slopes to avoid repeated calculations
	std::vector<double> neighborSlopes;
	/// Track which slopes need recalculation due to elevation changes
	std::vector<bool> slopeDirtyFlags;
	/// Track if the normal needs recalculation due to elevation changes
	/// We use this to avoid unnecessary normal recalculations
	bool normalDirty{true};

	/// Constant used for floating point comparisons
	static constexpr double EPSILON = 0.0000001;
};

} /// namespace lillugsi::planet