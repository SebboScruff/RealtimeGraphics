#pragma once

#include "Entity.hpp"
#include "ShaderProgram.hpp"

namespace glhelper {

//!\brief Abstract class encapsulating an Entity capable of being rendered.
class Renderable : public Entity
{
public:
	// Seb Additions, to allow per-renderable querying in runtime
	std::string name;
	GLuint query;
	// End of Seb Additions

	explicit Renderable(std::string name, const Eigen::Matrix4f& modelToWorld = Eigen::Matrix4f::Identity());

	virtual void render() = 0;
	virtual void render(ShaderProgram&) = 0;

	// Seb Additions, for optionally using the previously defined query objects. Implementations in Renderable.cpp are also by Seb

	void renderWithQuery(); // Alternative Render method that begins a query, renders, then ends the query
	float getRenderTime(); // returns a Render Time in Milliseconds
	~Renderable(); // Class destructor to delete the query object at the end of runtime

	// End of Seb Additions

	virtual bool castsShadow() = 0;
};

}

