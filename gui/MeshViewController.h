#ifndef CANDIDATE_MC_GUI_MESH_VIEW_CONTROLLER_H__
#define CANDIDATE_MC_GUI_MESH_VIEW_CONTROLLER_H__

#include <crag/Crag.h>
#include <crag/CragVolumes.h>
#include <inference/CragSolution.h>
#include <scopegraph/Agent.h>
#include <sg_gui/VolumeView.h>
#include <sg_gui/MeshView.h>
#include <sg_gui/Meshes.h>
#include <sg_gui/KeySignals.h>
#include "Signals.h"

template <typename EV>
class ExplicitVolumeAdaptor {

public:

	typedef typename EV::value_type value_type;

	ExplicitVolumeAdaptor(const EV& ev) :
		_ev(ev) {}

	const util::box<float,3>& getBoundingBox() const { return _ev.getBoundingBox(); }

	float operator()(float x, float y, float z) const {

		if (!getBoundingBox().contains(x, y, z))
			return 0;

		unsigned int dx, dy, dz;

		_ev.getDiscreteCoordinates(x, y, z, dx, dy, dz);

		return _ev(dx, dy, dz);
	}

private:

	const EV& _ev;

};

class MeshViewController :
		public sg::Agent<
				MeshViewController,
				sg::Accepts<
						sg_gui::VolumePointSelected,
						sg_gui::MouseDown,
						sg_gui::KeyDown
				>,
				sg::Provides<
						sg_gui::SetMeshes,
						SetCandidate,
						SetEdge
				>
		> {

public:

	MeshViewController(
			const Crag&                            crag,
			const CragVolumes&                     volumes,
			std::shared_ptr<ExplicitVolume<float>> labels);

	void loadMeshes(const std::vector<Crag::CragNode>& nodes);

	void setSolution(std::shared_ptr<CragSolution> solution);

	void onSignal(sg_gui::VolumePointSelected& signal);

	void onSignal(sg_gui::MouseDown& signal);

	void onSignal(sg_gui::KeyDown& signal);

private:

	void nextVolume();

	void prevVolume();

	void nextNeighbor();

	void prevNeighbor();

	void showSingleMesh(Crag::CragNode n);

	void showNeighbor(Crag::CragNode n);

	void addMesh(Crag::CragNode n);

	void removeMesh(Crag::CragNode n);

	Crag::CragNode parentOf(Crag::CragNode n);

	const Crag& _crag;

	const CragVolumes& _volumes;

	std::shared_ptr<ExplicitVolume<float>> _labels;

	std::shared_ptr<sg_gui::Meshes> _meshes;

	std::map<Crag::CragNode, std::shared_ptr<sg_gui::Mesh>> _meshCache;

	Crag::CragNode _current;

	std::vector<Crag::CragNode> _path;

	std::vector<Crag::CragNode> _neighbors;

	int _currentNeighbor;

	std::shared_ptr<CragSolution> _solution;
};

#endif // CANDIDATE_MC_GUI_MESH_VIEW_CONTROLLER_H__

