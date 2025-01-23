#include "face.h"

#include <spdlog/spdlog.h>
#include <cstddef>
#include <iostream>

namespace lillugsi::planet {
Face::Face(const std::array<unsigned int, 3>& vertexIndices)
: vertexIndices(vertexIndices) {

}

std::ostream& operator<<(std::ostream& os, const Face& face) {
	os << "Face(Vertices: [";
	for (size_t i = 0; i < face.vertexIndices.size(); ++i) {
		os << face.vertexIndices[i];
		if (i < face.vertexIndices.size() - 1) os << ", ";
	}
	os << "], Data: " << face.data << ")";
	return os;
}

/// Setters
void Face::setData(const float value) {
	this->data = value;
}

void Face::setNeighbor(const unsigned int index, std::shared_ptr<Face> neighbor) {
	if (index < this->neighbors.size()) {
		this->neighbors[index] = std::move(neighbor);
	}
}

void Face::addNeighbor(const std::shared_ptr<Face>& neighbor) {
	for (const auto & index : this->neighbors) {
		if (neighbor == index) {
			spdlog::trace("addNeighbor: neighbor already exists");
			return;
		}
	}

	for (size_t index = 0; index < this->neighbors.size(); ++index) {
		if (this->neighbors[index] == nullptr) {
			this->setNeighbor(index, neighbor);
			return;
		}
	}
}

void Face::setChild(const unsigned int index, std::shared_ptr<Face> child) {
	if (index < this->children.size()) {
		this->children[index] = std::move(child);
	}
}

void Face::addChild(const std::shared_ptr<Face>& child) {
	for (size_t index = 0; index < this->children.size(); ++index) {
		if (this->children[index] == nullptr) {
			this->setChild(index, child);
			return;
		}
	}
	spdlog::warn("Failed to add child: no empty slots available");
}

void Face::setParent(std::weak_ptr<Face> parent) {
	this->parent = std::move(parent);
}

void Face::setVertexIndices(const std::array<unsigned int, 3>& indices) {
	this->vertexIndices = indices;
}

/// Getters
float Face::getData() const {
	return this->data;
}

std::shared_ptr<Face> Face::getNeighbor(const unsigned int index) const {
	if (index < this->neighbors.size()) {
		return this->neighbors[index];
	}
	return nullptr;
}

std::shared_ptr<Face> Face::getChild(const unsigned int index) const {
	if (index < this->children.size()) {
		return this->children[index];
	}
	return nullptr;
}

std::array<std::shared_ptr<Face>, 4> Face::getChildren() const {
	return this->children;
}

std::shared_ptr<Face> Face::getParent() const {
	if (!this->parent.expired())
		return this->parent.lock();
	else
		return nullptr;
}

std::array<unsigned int, 3> Face::getVertexIndices() const {
	return this->vertexIndices;
}
} /// namespace lillugsi::planet