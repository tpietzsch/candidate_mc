#include <boost/lexical_cast.hpp>
#include <util/Logger.h>
#include <util/assert.h>
#include "Hdf5CragStore.h"

logger::LogChannel hdf5storelog("hdf5storelog", "[Hdf5CragStore] ");

void
Hdf5CragStore::saveCrag(const Crag& crag) {

	_hdfFile.root();
	_hdfFile.cd_mk("crag");

	_hdfFile.cd_mk("adjacencies");
	writeGraph(crag.getAdjacencyGraph());

	_hdfFile.cd("/crag");
	_hdfFile.cd_mk("subsets");
	writeDigraph(crag.getSubsetGraph());

	_hdfFile.cd("/crag");
	Hdf5GraphWriter::writeNodeMap(crag, crag.nodeTypes(), "node_types", Hdf5GraphWriter::DefaultConverter<Crag::NodeType, int>());

	std::vector<int> edgeTypes;
	for (Crag::CragEdge e : crag.edges()) {

		edgeTypes.push_back(crag.id(e.u()));
		edgeTypes.push_back(crag.id(e.v()));
		edgeTypes.push_back(crag.type(e));
	}
	_hdfFile.write(
			"edge_types",
			vigra::ArrayVectorView<int>(edgeTypes.size(), const_cast<int*>(&edgeTypes[0])));

	_hdfFile.cd("/crag");
	_hdfFile.cd_mk("grid_graph");
	vigra::ArrayVector<int> shape(3);
	shape[0] = crag.getGridGraph().shape()[0];
	shape[1] = crag.getGridGraph().shape()[1];
	shape[2] = crag.getGridGraph().shape()[2];
	_hdfFile.write("shape", shape);

	_hdfFile.cd("/crag");
	_hdfFile.cd_mk("affiliated_edges");
	int numEdges = 0;

	// list of affilitated edges:
	//
	// u v n id_1 ... id_n
	//
	// (u, v) ajacency edge
	// n      number of affiliated edges
	// id_i   id of ith affiliated edge
	std::vector<int> aeIds;
	for (Crag::EdgeIt e(crag); e != lemon::INVALID; ++e) {

			if (numEdges%100 == 0)
				LOG_USER(hdf5storelog) << logger::delline << numEdges << " affiliated egde lists prepared" << std::flush;

		aeIds.push_back(crag.id(crag.u(e)));
		aeIds.push_back(crag.id(crag.v(e)));
		aeIds.push_back(crag.getAffiliatedEdges(e).size());
		for (vigra::GridGraph<3>::Edge ae : crag.getAffiliatedEdges(e))
			aeIds.push_back(crag.getGridGraph().id(ae));

		numEdges++;
	}
	LOG_USER(hdf5storelog) << logger::delline << numEdges << " affiliated egde lists prepared" << std::endl;

	if (aeIds.size() == 0)
		return;

	LOG_USER(hdf5storelog) << "writing affiliated edge lists..." << std::flush;
	_hdfFile.write(
			"list",
			vigra::ArrayVectorView<int>(aeIds.size(), const_cast<int*>(&aeIds[0])));
	LOG_USER(hdf5storelog) << " done." << std::endl;
}

void
Hdf5CragStore::saveVolumes(const CragVolumes& volumes) {

	_hdfFile.cd_mk("/crag");
	_hdfFile.cd_mk("volumes");

	int numNodes = 0;
	for (Crag::NodeIt n(volumes.getCrag()); n != lemon::INVALID; ++n) {

		if (numNodes%100 == 0)
			LOG_USER(hdf5storelog) << logger::delline << numNodes << " node volumes written" << std::flush;

		writeVolume(*volumes[n], boost::lexical_cast<std::string>(volumes.getCrag().id(n)));
		numNodes++;
	}

	LOG_USER(hdf5storelog) << logger::delline << numNodes << " node volumes written" << std::endl;
}

void
Hdf5CragStore::retrieveCrag(Crag& crag) {

	_hdfFile.root();
	_hdfFile.cd("crag");

	_hdfFile.cd("adjacencies");
	readGraph(crag.getAdjacencyGraph());

	_hdfFile.cd("/crag");
	_hdfFile.cd("subsets");
	readDigraph(crag.getSubsetGraph());

	_hdfFile.cd("/crag");
	Hdf5GraphReader::readNodeMap(crag, crag.nodeTypes(), "node_types", Hdf5GraphReader::DefaultConverter<int, Crag::NodeType>());

	vigra::ArrayVector<int> edgeTypes;
	_hdfFile.readAndResize(
			"edge_types",
			edgeTypes);

	for (unsigned int i = 0; i < edgeTypes.size();) {

		Crag::Node u = crag.nodeFromId(edgeTypes[i]);
		Crag::Node v = crag.nodeFromId(edgeTypes[i+1]);
		Crag::EdgeType type = static_cast<Crag::EdgeType>(edgeTypes[i+2]);
		i += 3;

		// find edge in CRAG and set type
		for (Crag::IncEdgeIt e(crag, u); e != lemon::INVALID; ++e)
			if (crag.getAdjacencyGraph().oppositeNode(u, e) == v) {

				crag.edgeTypes()[e] = type;
				break;
			}
	}

	try {

		_hdfFile.cd("/crag");
		_hdfFile.cd("grid_graph");
		vigra::ArrayVector<int> s;
		_hdfFile.readAndResize("shape", s);
		vigra::Shape3 shape;
		shape[0] = s[0];
		shape[1] = s[1];
		shape[2] = s[2];
		vigra::GridGraph<3> gridGraph(shape, vigra::DirectNeighborhood);
		crag.setGridGraph(gridGraph);

		_hdfFile.cd("/crag");
		_hdfFile.cd("affiliated_edges");

		if (!_hdfFile.existsDataset("list"))
			return;

		vigra::ArrayVector<int> aeIds;
		_hdfFile.readAndResize(
				"list",
				aeIds);

		for (unsigned int i = 0; i < aeIds.size();) {

			Crag::Node u = crag.nodeFromId(aeIds[i]);
			Crag::Node v = crag.nodeFromId(aeIds[i+1]);
			int n = aeIds[i+2];
			i += 3;

			std::vector<vigra::GridGraph<3>::Edge> affiliatedEdges;
			for (int j = 0; j < n; j++)
				affiliatedEdges.push_back(crag.getGridGraph().edgeFromId(aeIds[i+j]));
			i += n;

			// find edge in CRAG and set affiliated edge list
			for (Crag::IncEdgeIt e(crag, u); e != lemon::INVALID; ++e)
				if (crag.getAdjacencyGraph().oppositeNode(u, e) == v) {

					crag.setAffiliatedEdges(e, affiliatedEdges);
					break;
				}
		}

	} catch (std::exception& e) {

		LOG_USER(hdf5storelog) << "no grid-graph description found" << std::endl;
	}
}

void
Hdf5CragStore::retrieveVolumes(CragVolumes& volumes) {

	_hdfFile.root();
	_hdfFile.cd("/crag");
	_hdfFile.cd("volumes");

	std::vector<std::string> vols = _hdfFile.ls();

	for (std::string vol : vols) {

		int id = boost::lexical_cast<int>(vol);

		Crag::Node n = volumes.getCrag().nodeFromId(id);
		std::shared_ptr<CragVolume> volume = std::make_shared<CragVolume>();
		readVolume(*volume, vol);
		volumes.setVolume(n, volume);

		UTIL_ASSERT(!volume->getBoundingBox().isZero());
	}
}

void
Hdf5CragStore::saveNodeFeatures(const Crag& crag, const NodeFeatures& features) {

	LOG_USER(hdf5storelog) << "saving node features... " << std::flush;

	_hdfFile.root();
	_hdfFile.cd_mk("crag");
	_hdfFile.cd_mk("features");

	for (Crag::NodeType type : {Crag::VolumeNode, Crag::SliceNode, Crag::AssignmentNode}) {

		int numNodes = 0;
		for (Crag::CragNode n : crag.nodes())
			if (crag.type(n) == type)
				numNodes++;

		if (numNodes == 0)
			continue;

		vigra::MultiArray<2, double> allFeatures(vigra::Shape2(features.dims(type) + 1, numNodes));

		int nodeNum = 0;
		for (Crag::CragNode n : crag.nodes()) {

			if (crag.type(n) != type)
				continue;

			allFeatures(0, nodeNum) = crag.id(n);
			std::copy(
					features[n].begin(),
					features[n].end(),
					allFeatures.bind<1>(nodeNum).begin() + 1);
			nodeNum++;
		}

		_hdfFile.write(std::string("nodes_") + boost::lexical_cast<std::string>(type), allFeatures);
	}

	LOG_USER(hdf5storelog) << "done." << std::endl;
}

void
Hdf5CragStore::retrieveNodeFeatures(const Crag& crag, NodeFeatures& features) {

	_hdfFile.root();
	_hdfFile.cd("crag");
	_hdfFile.cd("features");

	vigra::MultiArray<2, double> allFeatures;

	for (Crag::NodeType type : {Crag::VolumeNode, Crag::SliceNode, Crag::AssignmentNode}) {

		if (!_hdfFile.existsDataset(std::string("nodes_") + boost::lexical_cast<std::string>(type)))
			continue;

		_hdfFile.readAndResize(std::string("nodes_") + boost::lexical_cast<std::string>(type), allFeatures);

		int dims     = allFeatures.shape(0) - 1;
		int numNodes = allFeatures.shape(1);

		for (int i = 0; i < numNodes; i++) {

			Crag::CragNode n = crag.nodeFromId(allFeatures(0, i));

			std::vector<double> f;
			f.resize(dims);
			std::copy(
					allFeatures.bind<1>(i).begin() + 1,
					allFeatures.bind<1>(i).end(),
					f.begin());
			features.set(n, f);
		}
	}
}

void
Hdf5CragStore::saveEdgeFeatures(const Crag& crag, const EdgeFeatures& features) {

	LOG_USER(hdf5storelog) << "saving edge features... " << std::flush;

	_hdfFile.root();
	_hdfFile.cd_mk("crag");
	_hdfFile.cd_mk("features");

	for (Crag::EdgeType type : {Crag::AdjacencyEdge, Crag::NoAssignmentEdge}) {

		int numEdges = 0;
		for (Crag::CragEdge e : crag.edges())
			if (crag.type(e) == type)
				numEdges++;

		if (numEdges == 0)
			continue;

		vigra::MultiArray<2, double> allFeatures(vigra::Shape2(features.dims(type) + 2, numEdges));

		int edgeNum = 0;
		for (Crag::CragEdge e : crag.edges()) {

			if (crag.type(e) != type)
				continue;

			allFeatures(0, edgeNum) = crag.id(e.u());
			allFeatures(1, edgeNum) = crag.id(e.v());
			std::copy(
					features[e].begin(),
					features[e].end(),
					allFeatures.bind<1>(edgeNum).begin() + 2);
			edgeNum++;
		}

		_hdfFile.write(std::string("edges_") + boost::lexical_cast<std::string>(type), allFeatures);
	}

	LOG_USER(hdf5storelog) << "done." << std::endl;
}

void
Hdf5CragStore::retrieveEdgeFeatures(const Crag& crag, EdgeFeatures& features) {

	_hdfFile.root();
	_hdfFile.cd("crag");
	_hdfFile.cd("features");

	for (Crag::EdgeType type : {Crag::AdjacencyEdge, Crag::NoAssignmentEdge}) {

		if (!_hdfFile.existsDataset(std::string("edges_") + boost::lexical_cast<std::string>(type)))
			continue;

		vigra::MultiArray<2, double> allFeatures;

		_hdfFile.readAndResize(std::string("edges_") + boost::lexical_cast<std::string>(type), allFeatures);

		int dims     = allFeatures.shape(0) - 2;
		int numEdges = allFeatures.shape(1);

		for (int i = 0; i < numEdges; i++) {

			Crag::CragNode u = crag.nodeFromId(allFeatures(0, i));
			Crag::CragNode v = crag.nodeFromId(allFeatures(1, i));

			auto e = crag.adjEdges(u).begin();
			for (; e != crag.adjEdges(u).end(); e++)
				if ((*e).opposite(u) == v)
					break;

			if (e == crag.adjEdges(u).end())
				UTIL_THROW_EXCEPTION(
						IOError,
						"can not find edge for nodes " << crag.id(u) << " and " << crag.id(v));

			std::vector<double> f;
			f.resize(dims);
			std::copy(
					allFeatures.bind<1>(i).begin() + 2,
					allFeatures.bind<1>(i).end(),
					f.begin());
			features.set(*e, f);
		}
	}
}

void
Hdf5CragStore::saveSkeletons(const Crag& crag, const Skeletons& skeletons) {

	_hdfFile.root();
	_hdfFile.cd_mk("crag");
	_hdfFile.cd_mk("skeletons");

	for (Crag::NodeIt n(crag); n != lemon::INVALID; ++n) {

		int id                   = crag.id(n);
		const Skeleton& skeleton = skeletons[n];
		std::string     name     = boost::lexical_cast<std::string>(id);

		_hdfFile.cd_mk(name);

		writeGraphVolume(skeleton);
		Hdf5GraphWriter::writeNodeMap(skeleton.graph(), skeleton.diameters(), "diameters");

		_hdfFile.cd_up();
	}
}

void
Hdf5CragStore::retrieveSkeletons(const Crag& crag, Skeletons& skeletons) {

	try {

		_hdfFile.cd("/crag/skeletons");

	} catch (vigra::PreconditionViolation& e) {

		return;
	}

	for (Crag::NodeIt n(crag); n != lemon::INVALID; ++n) {

		LOG_ALL(hdf5storelog) << "reading skeleton for node " << crag.id(n) << std::endl;

		std::string name = boost::lexical_cast<std::string>(crag.id(n));

		try {

			_hdfFile.cd(name);

		} catch (vigra::PreconditionViolation& e) {

			continue;
		}

		Skeleton skeleton;
		readGraphVolume(skeleton);
		Hdf5GraphReader::readNodeMap(skeleton.graph(), skeleton.diameters(), "diameters");
		skeletons[n] = std::move(skeleton);

		_hdfFile.cd_up();
	}
}

void
Hdf5CragStore::saveFeatureWeights(const FeatureWeights& weights) {

	writeWeights(weights, "feature_weights");
}

void
Hdf5CragStore::retrieveFeatureWeights(FeatureWeights& weights) {

	readWeights(weights, "feature_weights");
}

void
Hdf5CragStore::saveFeaturesMin(const FeatureWeights& min) {

	writeWeights(min, "features_min");
}

void
Hdf5CragStore::retrieveFeaturesMin(FeatureWeights& min) {

	readWeights(min, "features_min");
}

void
Hdf5CragStore::saveFeaturesMax(const FeatureWeights& max) {

	writeWeights(max, "features_max");
}

void
Hdf5CragStore::retrieveFeaturesMax(FeatureWeights& max) {

	readWeights(max, "features_max");
}

void
Hdf5CragStore::saveSegmentation(
		const Crag&                              crag,
		const std::vector<std::set<Crag::Node>>& segmentation,
		std::string                              name) {

	_hdfFile.root();
	_hdfFile.cd_mk("segmentations");
	_hdfFile.cd_mk(name);

	for (unsigned int i = 0; i < segmentation.size(); i++) {

		std::string segmentName = "segment_";
		segmentName += boost::lexical_cast<std::string>(i);

		vigra::ArrayVector<int> nodes;
		for (Crag::Node node : segmentation[i])
			nodes.push_back(crag.id(node));
		_hdfFile.write(
				segmentName,
				nodes);
	}
}

void
Hdf5CragStore::retrieveSegmentation(
		const Crag&                        crag,
		std::vector<std::set<Crag::Node>>& segmentation,
		std::string                        name) {

	_hdfFile.root();
	_hdfFile.cd("segmentations");
	_hdfFile.cd(name);

	std::vector<std::string> segmentNames = _hdfFile.ls();

	for (std::string segmentName : segmentNames) {

		vigra::ArrayVector<int> ids;
		_hdfFile.readAndResize(
				segmentName,
				ids);

		std::set<Crag::Node> nodes;
		for (int id : ids)
			nodes.insert(crag.nodeFromId(id));

		segmentation.push_back(nodes);
	}
}

std::vector<std::string>
Hdf5CragStore::getSegmentationNames() {

	try {

		_hdfFile.root();
		_hdfFile.cd("segmentations");

		return _hdfFile.ls();

	} catch (std::exception& e) {

		return std::vector<std::string>();
	}
}

void
Hdf5CragStore::writeGraphVolume(const GraphVolume& graphVolume) {

	PositionConverter positionConverter;

	writeGraph(graphVolume.graph());
	Hdf5GraphWriter::writeNodeMap(graphVolume.graph(), graphVolume.positions(), "positions", positionConverter);

	vigra::MultiArray<1, float> p(3);

	// resolution
	p[0] = graphVolume.getResolutionX();
	p[1] = graphVolume.getResolutionY();
	p[2] = graphVolume.getResolutionZ();
	_hdfFile.write("resolution", p);

	// offset
	p[0] = graphVolume.getOffset().x();
	p[1] = graphVolume.getOffset().y();
	p[2] = graphVolume.getOffset().z();
	_hdfFile.write("offset", p);
}

void
Hdf5CragStore::writeWeights(const FeatureWeights& weights, std::string name) {

	_hdfFile.root();
	_hdfFile.cd_mk(name);

	for (Crag::NodeType type : {Crag::VolumeNode, Crag::SliceNode, Crag::AssignmentNode}) {

		const std::vector<double>& w = weights[type];

		if (w.size() == 0)
			continue;

		_hdfFile.write(
				std::string("node_") + boost::lexical_cast<std::string>(type),
				vigra::ArrayVectorView<double>(w.size(), const_cast<double*>(&w[0])));
	}

	for (Crag::EdgeType type : {Crag::AdjacencyEdge, Crag::NoAssignmentEdge}) {

		const std::vector<double>& w = weights[type];

		if (w.size() == 0)
			continue;

		_hdfFile.write(
				std::string("edge_") + boost::lexical_cast<std::string>(type),
				vigra::ArrayVectorView<double>(w.size(), const_cast<double*>(&w[0])));
	}
}

void
Hdf5CragStore::readWeights(FeatureWeights& weights, std::string name) {

	_hdfFile.root();
	_hdfFile.cd(name);

	for (Crag::NodeType type : {Crag::VolumeNode, Crag::SliceNode, Crag::AssignmentNode}) {

		if (!_hdfFile.existsDataset(std::string("node_") + boost::lexical_cast<std::string>(type)))
			continue;

		vigra::ArrayVector<double> w;
		_hdfFile.readAndResize(
				std::string("node_") + boost::lexical_cast<std::string>(type),
				w);
		weights[type].resize(w.size());
		std::copy(w.begin(), w.end(), weights[type].begin());
	}

	for (Crag::EdgeType type : {Crag::AdjacencyEdge, Crag::NoAssignmentEdge}) {

		if (!_hdfFile.existsDataset(std::string("edge_") + boost::lexical_cast<std::string>(type)))
			continue;

		vigra::ArrayVector<double> w;
		_hdfFile.readAndResize(
				std::string("edge_") + boost::lexical_cast<std::string>(type),
				w);
		weights[type].resize(w.size());
		std::copy(w.begin(), w.end(), weights[type].begin());
	}
}

void
Hdf5CragStore::readGraphVolume(GraphVolume& graphVolume) {

	PositionConverter positionConverter;

	readGraph(graphVolume.graph());
	Hdf5GraphReader::readNodeMap(graphVolume.graph(), graphVolume.positions(), "positions", positionConverter);

	vigra::MultiArray<1, float> p(3);

	// resolution
	_hdfFile.read("resolution", p);
	graphVolume.setResolution(p[0], p[1], p[2]);

	// offset
	_hdfFile.read("offset", p);
	graphVolume.setOffset(p[0], p[1], p[2]);
}
